#!/bin/bash
# TODO.md 5a, the fifth probe: has s->cost drifted from the cost sx_init built?
#
# shift_to_feasible does `s->cost[v] += need` (src/simplex.c:2098) and
# repay_shifts does `s->cost[v] -= s->shift[v]` (:2383). The bookkeeping is
# exact in real arithmetic and not in floating point: (c + n1 + n2) - (n1 + n2)
# is not c. pilotnov under D118's candidate runs 43041 weight restarts and 156
# stability rebuilds, so it does this a great many times.
#
# A drift of delta in s->cost shows up as delta in every reduced cost computed
# from it -- which the solver would call dual-feasible against its own drifted
# costs and the checker would reject against the model's true ones. That is
# exactly the pair of readings D120 could not reconcile.
#
# The cost sx_init built is snapshotted at the end of the scaling pass and
# compared at the exit. src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-29-cost"
out="$here/cost-drift.txt"
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

snap = """    for (int64_t i = 0; i < s->nrow; i++) {
        s->lo[s->ncol + i] = m->row_lower[i] * rho[i];
        s->up[s->ncol + i] = m->row_upper[i] * rho[i];
        s->cost[s->ncol + i] = 0.0;
    }
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, snap.replace("    return JAOS_OK;",
    "#ifdef JAOS_DIAG\n"
    "    free(g_cost0);\n"
    "    g_cost0 = malloc((size_t)s->nvar * sizeof *g_cost0);\n"
    "    if (g_cost0 != NULL)\n"
    "        memcpy(g_cost0, s->cost, (size_t)s->nvar * sizeof *g_cost0);\n"
    "#endif\n    return JAOS_OK;"))

anchor = "static jaos_status reenter_after_settling(sx *s)\n{"
assert s.count(anchor) == 1
probe = r'''
#ifdef JAOS_DIAG
#include <stdio.h>
static void diag_cost_drift(sx *s, const char *tag)
{
    if (g_cost0 == NULL)
        return;
    double worst = 0.0, worst_rel = 0.0;
    int64_t at = -1, moved = 0, pending = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->shift[v] != 0.0)
            pending++;
        const double d = fabs(s->cost[v] - g_cost0[v]);
        if (d != 0.0)
            moved++;
        const double sc = fabs(g_cost0[v]) > 1.0 ? fabs(g_cost0[v]) : 1.0;
        if (d > worst) { worst = d; worst_rel = d / sc; at = v; }
    }
    fprintf(stderr, "COST %s nvar=%lld moved=%lld shift_still_pending=%lld "
            "worst_abs=%.17g worst_rel=%.17g (v=%lld)\n",
            tag, (long long)s->nvar, (long long)moved, (long long)pending,
            worst, worst_rel, (long long)at);
}
#endif
'''
s = s.replace(anchor, probe + anchor)

# The snapshot pointer, above everything that touches it.
head = "constexpr double ARTIFICIAL_BOUND = 1e10;"
assert s.count(head) == 1
s = s.replace(head, head + "\n\n#ifdef JAOS_DIAG\nstatic double *g_cost0;\n#endif")

ret = "    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;"
assert s.count(ret) == 3
parts = s.split(ret)
tags = ["no-work", "run-not-optimal", "rounds-exhausted"]
out = parts[0]
for k in range(3):
    out += ("#ifdef JAOS_DIAG\n    diag_cost_drift(s, \"%s\");\n#endif\n"
            % tags[k]) + ret + parts[k + 1]
open(p, "w", encoding="utf-8").write(out)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a. s->cost at the exit against the cost sx_init built."
echo "# shift_to_feasible adds to it and repay_shifts subtracts back; the"
echo "# round trip is exact in real arithmetic and not in floating point."
echo
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "######## CANDIDATE / $1 ########"
    ./build/diag/probe "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "COST |objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
