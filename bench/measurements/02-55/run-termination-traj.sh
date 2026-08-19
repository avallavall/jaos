#!/bin/bash
# Where does the hostile degen2 solve lose the true dual, and what did the
# best-point loop see when it kept the wrong vertex? (D146's diagnosis,
# step 1: the trajectory before any value.)
#
# One TRAJ line per settle_shifts call, at run()'s OPTIMAL exit, and at
# classify_optimum's verdict:
#   nlend/lsum/lmax  -> outstanding cost lends (cost - cost0, the rule the
#                       sx comment states; shift[] is NOT read)
#   d0viol/d0max     -> nonbasic sign violations of d0 = d - (cost - cost0),
#                       the reduced cost in the model's own cost space, from
#                       the solver's carried d (drift-bearing, good enough
#                       to see a 5.7% objective error)
#   bstdv/bstobj     -> what the best-point loop believes
#
# Cold degen2 first as the control, then hostile shift 1 (02-54's
# construction). src/ is read and never written; the patch lives in a
# throwaway worktree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-55-traj"
out="$here/termination-traj.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static long long dg_round;
#endif""")

anchor = "static jaos_status reenter_after_settling(sx *s)"
assert s.count(anchor) == 1
s = s.replace(anchor, """#ifdef JAOS_DIAG
static void dg_dump(sx *s, const char *tag)
{
    long long nlend = 0; double lsum = 0.0, lmax = 0.0;
    long long dviol = 0; double dmax = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double lend = s->cost[v] - s->cost0[v];
        if (lend != 0.0) {
            nlend++; lsum += fabs(lend);
            if (fabs(lend) > lmax) lmax = fabs(lend);
        }
        if (s->status[v] == JM_BASIC)
            continue;
        const double d0 = s->d[v] - lend;
        double viol = 0.0;
        if (s->status[v] == JM_AT_LOWER)      viol = d0 < 0.0 ? -d0 : 0.0;
        else if (s->status[v] == JM_AT_UPPER) viol = d0 > 0.0 ?  d0 : 0.0;
        else                                  viol = fabs(d0);
        if (viol > 1e-7) { dviol++; if (viol > dmax) dmax = viol; }
    }
    char b[288];
    int k = snprintf(b, sizeof b,
        "TRAJ tag=%s round=%lld iters=%lld nlend=%lld lsum=%.3e lmax=%.3e "
        "d0viol=%lld d0max=%.3e bstdv=%.3e bstobj=%.17g\\n",
        tag, dg_round, (long long)s->iters, nlend, lsum, lmax,
        dviol, dmax, s->bst_dviol, s->bst_obj);
    if (k > 0 && k < (int)sizeof b) {
        ssize_t w = write(2, b, (size_t)k); (void)w;
    }
}
#endif

""" + anchor)

call = "settle_shifts(s);"
assert s.count(call) == 3
s = s.replace(call, call + """
#ifdef JAOS_DIAG
        dg_round++; dg_dump(s, "settle");
#endif""")

runopt = """            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;"""
assert s.count(runopt) == 1
s = s.replace(runopt, """#ifdef JAOS_DIAG
            dg_dump(s, "runopt");
#endif
""" + runopt)

cls = """    if (blocked < 0)
        return JAOS_SOLVE_OPTIMAL;"""
assert s.count(cls) == 1
s = s.replace(cls, """#ifdef JAOS_DIAG
    dg_dump(s, "classify");
#endif
""" + cls)
open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude src/*.c "$here/probe-traj.c" -o build/diag/probe-traj -lm \
    || { echo "build failed"; exit 2; }

./build/diag/probe-traj bench/instances/degen2.mps 2> "$out.raw"
{
echo "# Trajectory of the hostile degen2 solve against its cold control."
echo "# Read the PHASE markers, then each TRAJ line in order."
echo
cat "$out.raw"
} | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
