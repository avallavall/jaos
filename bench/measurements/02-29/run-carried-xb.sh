#!/bin/bash
# TODO.md 5a, the fourth probe: is the point that gets published the one the
# basis actually solves for?
#
# D20's second opinion refactorizes and re-reads the primal test, and it runs
# BEFORE the re-entry loop. The re-entry then flips nonbasics between bounds
# and updates x_B incrementally (`s->xb[i] -= s->col[i]`, src/simplex.c:2367)
# and does a primal_cleanup, and nothing re-verifies the point afterwards.
#
# So this recomputes x_B from the factorization at the exit and compares it
# against the carried vector, then restores the carried one so the solve is
# unchanged. If the two disagree, the published point is not the one the
# basis solves for, and every optimality reading above it was asked of a
# different point.
#
# Instrumented in a COPY of the tree. src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-29-xb"
out="$here/carried-xb.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

anchor = "static jaos_status reenter_after_settling(sx *s)\n{"
assert s.count(anchor) == 1
probe = r'''
#ifdef JAOS_DIAG
#include <stdio.h>
/* Recompute x_B from the factorization and compare against the carried
 * vector, then put the carried one back so the solve is unchanged. */
static void diag_carried_xb(sx *s, const char *tag)
{
    double *keep = malloc((size_t)s->nrow * sizeof *keep);
    if (keep == NULL)
        return;
    memcpy(keep, s->xb, (size_t)s->nrow * sizeof *keep);

    compute_primal(s, true);

    double worst = 0.0, worst_rel = 0.0;
    int64_t at = -1;
    int64_t out_of_box = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double d = fabs(s->xb[i] - keep[i]);
        const double sc = fabs(keep[i]) > 1.0 ? fabs(keep[i]) : 1.0;
        if (d > worst) { worst = d; at = i; worst_rel = d / sc; }
        const int64_t b = s->basis[i];
        if (keep[i] < s->lo[b] - s->primal_tol ||
            keep[i] > s->up[b] + s->primal_tol)
            out_of_box++;
    }
    fprintf(stderr, "XB %s nrow=%lld worst_abs=%.17g worst_rel=%.17g "
            "(row=%lld)  carried_basics_out_of_bounds=%lld\n",
            tag, (long long)s->nrow, worst, worst_rel, (long long)at,
            (long long)out_of_box);

    memcpy(s->xb, keep, (size_t)s->nrow * sizeof *keep);
    free(keep);
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
    out += ("#ifdef JAOS_DIAG\n    diag_carried_xb(s, \"%s\");\n#endif\n"
            % tags[k]) + ret + parts[k + 1]
open(p, "w", encoding="utf-8").write(out)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a. x_B recomputed from the factorization at the exit,"
echo "# against the vector the solve carried there. The re-entry updates"
echo "# x_B incrementally and nothing re-verifies it afterwards."
echo
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "######## CANDIDATE / $1 ########"
    ./build/diag/probe "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "XB |objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
