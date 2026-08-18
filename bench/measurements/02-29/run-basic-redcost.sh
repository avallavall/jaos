#!/bin/bash
# TODO.md 5a, the decisive reading: the solver's dual measure reads 0 on a
# point the checker rejects at 0.89. Which of the two computations is wrong?
#
# compute_duals ASSIGNS s->d[v] = 0.0 for every basic v (src/simplex.c:1086)
# and never verifies it. The checker recomputes d = c - A'y independently for
# every variable. So this recomputes, for all v including the basics, and
# reports the largest disagreement.
#
# Instrumented in a COPY of the tree. src/ is read and never written.
set -u
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-5b
out="$root/bench/measurements/02-29/basic-redcost.txt"
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
/* Recompute every reduced cost from the solver's own duals, including the
 * basics compute_duals assigns zero to, and report the largest disagreement
 * with what the solver carries. Unscaled, which is the space the caller and
 * the checker read. */
static void diag_basic_redcost(sx *s, const char *tag)
{
    const jaos_model *m = s->m;
    double worst_basic = 0.0, worst_nonbasic = 0.0, carried = 0.0;
    int64_t at_basic = -1, at_nonbasic = -1;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double dtrue = s->cost[v] - price_entry(s, s->y, v);
        const double u = v < s->ncol ? dtrue / m->col_scale[v]
                                     : dtrue * m->row_scale[v - s->ncol];
        if (s->status[v] == JM_BASIC) {
            if (fabs(u) > worst_basic) { worst_basic = fabs(u); at_basic = v; }
            continue;
        }
        double br = 0.0;
        if (s->status[v] == JM_AT_LOWER)      br = u < 0.0 ? -u : 0.0;
        else if (s->status[v] == JM_AT_UPPER) br = u > 0.0 ?  u : 0.0;
        else                                  br = fabs(u);
        if (br > worst_nonbasic) { worst_nonbasic = br; at_nonbasic = v; }
        const double c = v < s->ncol ? s->d[v] / m->col_scale[v]
                                     : s->d[v] * m->row_scale[v - s->ncol];
        double cb = 0.0;
        if (s->status[v] == JM_AT_LOWER)      cb = c < 0.0 ? -c : 0.0;
        else if (s->status[v] == JM_AT_UPPER) cb = c > 0.0 ?  c : 0.0;
        else                                  cb = fabs(c);
        if (cb > carried) carried = cb;
    }
    fprintf(stderr,
            "REDCOST %s nrow=%lld ncol=%lld  worst_basic=%.17g (v=%lld)  "
            "worst_nonbasic_recomputed=%.17g (v=%lld)  "
            "worst_nonbasic_carried=%.17g\n",
            tag, (long long)s->nrow, (long long)s->ncol,
            worst_basic, (long long)at_basic,
            worst_nonbasic, (long long)at_nonbasic, carried);
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
    out += ("#ifdef JAOS_DIAG\n    diag_basic_redcost(s, \"%s\");\n#endif\n"
            % tags[k]) + ret + parts[k + 1]
open(p, "w", encoding="utf-8").write(out)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a. compute_duals assigns d = 0 to every basic variable"
echo "# (src/simplex.c:1086) and never verifies it. This recomputes"
echo "# d = c - A'y from the solver's own duals for every variable, unscaled."
echo
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "######## CANDIDATE / $1 ########"
    ./build/diag/probe "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "REDCOST|objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
