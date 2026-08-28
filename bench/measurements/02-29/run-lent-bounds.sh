#!/bin/bash
# TODO.md 5a, third reading. The reduced solve is dual-feasible on its own
# model to 3.8e-14, and the checker rejects the published answer at 0.89. So
# the disagreement is about which BOUNDS are real.
#
# Dual phase 1 lends a bound of ARTIFICIAL_BOUND = 1e10 to a column the model
# left unbounded (src/simplex.c:785). A column resting on a lent bound is
# INTERIOR in the caller's own box, so any nonzero reduced cost there is a
# dual violation to the checker and legal to the solver.
# classify_optimum only flags one when its reduced cost passes dual_tol.
#
# This lists every nonbasic column sitting on a lent bound at the exit.
# Instrumented in a COPY of the tree. src/ is read and never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
root="$JAOS_ROOT"
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-5c
out="$root/bench/measurements/02-29/lent-bounds.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag "$(dirname "$out")"

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

anchor = "static jaos_status reenter_after_settling(sx *s)\n{"
assert s.count(anchor) == 1
probe = r'''
#ifdef JAOS_DIAG
#include <stdio.h>
static void diag_lent(sx *s, const char *tag)
{
    const jaos_model *m = s->m;
    int64_t lent = 0, resting = 0, flagged = 0;
    double worst = 0.0; int64_t at = -1;
    for (int64_t j = 0; j < s->ncol; j++) {
        if (s->fake[j] == NOT_FAKE)
            continue;
        lent++;
        const bool on = (s->fake[j] == FAKE_LO && s->status[j] == JM_AT_LOWER)
                     || (s->fake[j] == FAKE_UP && s->status[j] == JM_AT_UPPER);
        if (!on)
            continue;
        resting++;
        if (held_by_an_invented_bound(s, j))
            flagged++;
        const double u = s->d[j] / m->col_scale[j];
        if (fabs(u) > worst) { worst = fabs(u); at = j; }
    }
    fprintf(stderr, "LENT %s dual_tol=%.6g  lent=%lld resting=%lld "
            "flagged_by_classify=%lld  worst_redcost_on_a_lent_bound=%.17g "
            "(col=%lld)\n", tag, s->dual_tol, (long long)lent,
            (long long)resting, (long long)flagged, worst, (long long)at);
}
#endif
'''
s = s.replace(anchor, probe + anchor)

ret = "    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;"
assert s.count(ret) == 3
parts = s.split(ret)
tags = ["no-work", "run-not-optimal", "rounds-exhausted"]
out = parts[0]
for k in range(3):
    out += ("#ifdef JAOS_DIAG\n    diag_lent(s, \"%s\");\n#endif\n"
            % tags[k]) + ret + parts[k + 1]
open(p, "w", encoding="utf-8").write(out)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

# held_by_an_invented_bound is defined below the re-entry, so forward-declare.
python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()
mark = "#ifdef JAOS_DIAG\n#include <stdio.h>\nstatic void diag_lent"
assert s.count(mark) == 1
s = s.replace(mark, "#ifdef JAOS_DIAG\n#include <stdio.h>\n"
              "static bool held_by_an_invented_bound(const sx *s, int64_t j);\n"
              "static void diag_lent")
open(p, "w", encoding="utf-8").write(s)
print("forward declaration added")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a. Columns resting on a bound dual phase 1 lent them"
echo "# (ARTIFICIAL_BOUND = 1e10). Such a column is interior in the caller's"
echo "# own box, so a nonzero reduced cost there is a dual violation to the"
echo "# checker and legal to the solver."
echo
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431"; do
    set -- $pair
    echo "######## CANDIDATE / $1 ########"
    ./build/diag/probe "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "LENT|objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
