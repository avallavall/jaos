#!/bin/bash
# TODO.md 5a, the sixth probe: was a loan lost, or is the drift rounding?
#
# s->cost is written at exactly five places (src/simplex.c): the two in the
# scaling pass that build it, shift_to_feasible which lends, and the two that
# repay -- repay_shifts and primal_cleanup. This tallies every lend and every
# repayment per variable and prints the balance beside the drift.
#
#   lent == repaid and drift != 0        -> floating-point rounding
#   lent != repaid                       -> a loan was lost or repaid twice
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-29-loan"
out="$here/loan-balance.txt"
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

head = "constexpr double ARTIFICIAL_BOUND = 1e10;"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
static double *g_cost0, *g_lent, *g_repaid;
static int64_t g_nvar_diag;
#endif""")

# The snapshot, and the tallies, allocated where cost is first built.
snap = """        s->cost[s->ncol + i] = 0.0;
    }
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, """        s->cost[s->ncol + i] = 0.0;
    }
#ifdef JAOS_DIAG
    free(g_cost0); free(g_lent); free(g_repaid);
    g_nvar_diag = s->nvar;
    g_cost0  = malloc((size_t)s->nvar * sizeof *g_cost0);
    g_lent   = calloc((size_t)s->nvar, sizeof *g_lent);
    g_repaid = calloc((size_t)s->nvar, sizeof *g_repaid);
    if (g_cost0 != NULL)
        memcpy(g_cost0, s->cost, (size_t)s->nvar * sizeof *g_cost0);
#endif
    return JAOS_OK;""")

lend = "    s->shift[v] += need;"
assert s.count(lend) == 1, s.count(lend)
s = s.replace(lend, "#ifdef JAOS_DIAG\n    if (g_lent) g_lent[v] += need;\n#endif\n" + lend)

rep1 = """        s->cost[v] -= s->shift[v];
        s->shift[v] = 0.0;"""
assert s.count(rep1) == 1
s = s.replace(rep1, """#ifdef JAOS_DIAG
        if (g_repaid) g_repaid[v] += s->shift[v];
#endif
""" + rep1)

rep2 = """            s->cost[q] -= s->shift[q];
            s->d[q] -= s->shift[q];
            s->shift[q] = 0.0;"""
assert s.count(rep2) == 1
s = s.replace(rep2, """#ifdef JAOS_DIAG
            if (g_repaid) g_repaid[q] += s->shift[q];
#endif
""" + rep2)

anchor = "static jaos_status reenter_after_settling(sx *s)\n{"
assert s.count(anchor) == 1
probe = r'''
#ifdef JAOS_DIAG
#include <stdio.h>
static void diag_loans(sx *s, const char *tag)
{
    if (g_cost0 == NULL || g_lent == NULL || g_repaid == NULL)
        return;
    double worst = 0.0; int64_t at = -1;
    int64_t unbalanced = 0;
    double worst_bal = 0.0; int64_t at_bal = -1;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double drift = s->cost[v] - g_cost0[v];
        if (fabs(drift) > fabs(worst)) { worst = drift; at = v; }
        const double bal = g_lent[v] - g_repaid[v];
        if (bal != 0.0) unbalanced++;
        if (fabs(bal) > fabs(worst_bal)) { worst_bal = bal; at_bal = v; }
    }
    fprintf(stderr, "LOAN %s worst_drift=%.17g (v=%lld: lent=%.17g "
            "repaid=%.17g shift=%.17g)  unbalanced=%lld "
            "worst_balance=%.17g (v=%lld)\n",
            tag, worst, (long long)at,
            at >= 0 ? g_lent[at] : 0.0, at >= 0 ? g_repaid[at] : 0.0,
            at >= 0 ? s->shift[at] : 0.0,
            (long long)unbalanced, worst_bal, (long long)at_bal);
}
#endif
'''
s = s.replace(anchor, probe + anchor)

ret = "    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;"
assert s.count(ret) == 3
parts = s.split(ret)
tags = ["no-work", "run-not-optimal", "rounds-exhausted"]
o = parts[0]
for k in range(3):
    o += ("#ifdef JAOS_DIAG\n    diag_loans(s, \"%s\");\n#endif\n"
          % tags[k]) + ret + parts[k + 1]
open(p, "w", encoding="utf-8").write(o)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/probe -lm || { echo "build failed"; exit 2; }

{
echo "# TODO.md 5a. Every lend and every repayment tallied per variable."
echo "# lent == repaid with a drift means rounding; lent != repaid means a"
echo "# loan was lost or repaid twice."
echo
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "######## CANDIDATE / $1 ########"
    ./build/diag/probe "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "LOAN |objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
