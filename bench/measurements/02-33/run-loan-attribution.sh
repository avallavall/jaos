#!/bin/bash
# TODO.md 5a: 186 loans go missing on pilotnov, and nothing explains it.
#
# 02-29 tallied every lend against every repayment per variable and found 186
# variables where the two totals differ, the worst by 256. That was read as
# loans being lost. This asks a cheaper question first, because the tally
# itself has a defect available to it:
#
#   g_lent[v]   is ONE accumulator over every loan of the whole solve.
#   s->shift[v] is reset to zero at every repayment, so what a repayment
#               hands to g_repaid[v] is a PARTIAL sum, and g_repaid[v] is the
#               sum of those partial sums.
#
# Summing in segments and summing in one accumulator are not the same number
# in floating point. So `lent != repaid` is what a lost loan looks like AND
# what re-association looks like, and 02-29 could not tell them apart.
#
# The discriminator is a third tally, `g_lent_bysegs[v]`: the same loans,
# accumulated the same way the repayments are — per segment, then added up.
#
#   lent_bysegs == repaid, bit for bit   -> nothing is lost; 02-29 measured
#                                           its own instrument
#   lent_bysegs != repaid                -> a loan really is going missing,
#                                           and the counters name where
#
# A fourth check covers the one thing reading the source cannot settle: that
# `s->shift` is written at the three sites this patches and nowhere else. At
# every repayment the segment accumulator must equal `s->shift[v]` exactly.
# Any other write to `shift` breaks that equality; a memcpy would too, and
# grep does not see a memcpy.
#
# The state is only reachable through D118's refused candidate, applied from
# bench/measurements/02-27/candidate.diff. src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-33-loan"
out="$here/loan-attribution.txt"
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

head = "constexpr double ARTIFICIAL_BOUND = 1e10;"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
static double *g_cost0, *g_lent, *g_lent_seg, *g_lent_bysegs, *g_repaid;
static int64_t *g_nlend, *g_nrepay;
static int64_t g_seg_mismatch;
#endif""")

# Allocated beside the one write to cost0, so every solve starts from zero.
snap = """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
#ifdef JAOS_DIAG
    free(g_cost0); free(g_lent); free(g_lent_seg); free(g_lent_bysegs);
    free(g_repaid); free(g_nlend); free(g_nrepay);
    g_seg_mismatch = 0;
    g_cost0       = malloc((size_t)s->nvar * sizeof *g_cost0);
    g_lent        = calloc((size_t)s->nvar, sizeof *g_lent);
    g_lent_seg    = calloc((size_t)s->nvar, sizeof *g_lent_seg);
    g_lent_bysegs = calloc((size_t)s->nvar, sizeof *g_lent_bysegs);
    g_repaid      = calloc((size_t)s->nvar, sizeof *g_repaid);
    g_nlend       = calloc((size_t)s->nvar, sizeof *g_nlend);
    g_nrepay      = calloc((size_t)s->nvar, sizeof *g_nrepay);
    if (g_cost0 != NULL)
        memcpy(g_cost0, s->cost, (size_t)s->nvar * sizeof *g_cost0);
#endif
    return JAOS_OK;""")

# The one lend site.
lend = "    s->shift[v] += need;"
assert s.count(lend) == 1, s.count(lend)
s = s.replace(lend, """#ifdef JAOS_DIAG
    if (g_lent) {
        g_lent[v] += need;
        g_lent_seg[v] += need;
        g_nlend[v]++;
    }
#endif
""" + lend + """
#ifdef JAOS_NEGCTL
    /* NEGATIVE CONTROL. One variable's loan is dropped on the floor the
     * moment it is lent: the cost keeps the money and the record forgets it,
     * which is exactly what a lost loan is. Both counters must see it. */
    if (v == 7)
        s->shift[v] = 0.0;
#endif""")

# Repayment one: repay_shifts.
rep1 = """        s->cost[v] = s->cost0[v];
        s->shift[v] = 0.0;"""
assert s.count(rep1) == 1
s = s.replace(rep1, """#ifdef JAOS_DIAG
        if (g_repaid) {
            if (g_lent_seg[v] != s->shift[v])
                g_seg_mismatch++;
            g_repaid[v] += s->shift[v];
            g_lent_bysegs[v] += g_lent_seg[v];
            g_lent_seg[v] = 0.0;
            g_nrepay[v]++;
        }
#endif
""" + rep1)

# Repayment two: primal_cleanup.
rep2 = """            const double give_back = s->cost[q] - s->cost0[q];
            s->cost[q] = s->cost0[q];
            s->d[q] -= give_back;
            s->shift[q] = 0.0;"""
assert s.count(rep2) == 1
s = s.replace(rep2, """#ifdef JAOS_DIAG
            if (g_repaid) {
                if (g_lent_seg[q] != s->shift[q])
                    g_seg_mismatch++;
                g_repaid[q] += s->shift[q];
                g_lent_bysegs[q] += g_lent_seg[q];
                g_lent_seg[q] = 0.0;
                g_nrepay[q]++;
            }
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
    double worst = 0.0;             int64_t at = -1;
    int64_t unbal_one = 0;          double worst_one = 0.0;  int64_t at_one = -1;
    int64_t unbal_seg = 0;          double worst_seg = 0.0;  int64_t at_seg = -1;
    int64_t outstanding = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double drift = s->cost[v] - g_cost0[v];
        if (fabs(drift) > fabs(worst)) { worst = drift; at = v; }

        const double one = g_lent[v] - g_repaid[v];
        if (one != 0.0) unbal_one++;
        if (fabs(one) > fabs(worst_one)) { worst_one = one; at_one = v; }

        const double seg = g_lent_bysegs[v] - g_repaid[v];
        if (seg != 0.0) unbal_seg++;
        if (fabs(seg) > fabs(worst_seg)) { worst_seg = seg; at_seg = v; }

        if (s->shift[v] != 0.0) outstanding++;
    }
    fprintf(stderr, "LOAN %s worst_drift=%.17g (v=%lld)\n", tag, worst,
            (long long)at);
    fprintf(stderr, "     one-accumulator: unbalanced=%lld worst=%.17g "
            "(v=%lld)\n", (long long)unbal_one, worst_one, (long long)at_one);
    fprintf(stderr, "     by-segment:      unbalanced=%lld worst=%.17g "
            "(v=%lld)\n", (long long)unbal_seg, worst_seg, (long long)at_seg);
    fprintf(stderr, "     shift written elsewhere: %lld mismatch(es)\n",
            (long long)g_seg_mismatch);
    fprintf(stderr, "     loans still outstanding: %lld\n",
            (long long)outstanding);
    if (at_one >= 0) {
        const double L = fabs(g_lent[at_one]);
        fprintf(stderr, "     worst one-accumulator v=%lld: lent=%.17g "
                "repaid=%.17g lends=%lld repays=%lld ulp(lent)=%.17g\n",
                (long long)at_one, g_lent[at_one], g_repaid[at_one],
                (long long)g_nlend[at_one], (long long)g_nrepay[at_one],
                L > 0.0 ? nextafter(L, HUGE_VAL) - L : 0.0);
    }
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

build_probe() {   # $1 = output binary, $2... = extra flags
    o="$1"; shift
    gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
        "$@" -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
        -o "$o" -lm || { echo "build failed: $o"; exit 2; }
}

# Two binaries, and the md5s are printed: a sweep that measures one binary
# twice reads exactly 1.0000x at every setting and looks like a clean result
# (D82).
build_probe build/diag/probe-head
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
build_probe build/diag/probe-cand
build_probe build/diag/probe-negctl -DJAOS_NEGCTL

{
echo "# TODO.md 5a: are the 186 loans lost, or is the tally re-associating?"
echo "# one-accumulator = 02-29's tally.  by-segment = the same loans added"
echo "# the way the repayments are added.  They disagree only if the tally"
echo "# was the defect."
echo
md5sum build/diag/probe-head build/diag/probe-cand build/diag/probe-negctl
echo

echo "######## NEGATIVE CONTROL: one loan dropped on the floor ########"
echo "# The instrument must find a lost loan before an empty result from it"
echo "# is worth anything. v=7's loan is zeroed the moment it is lent."
echo "---- pilotnov, control in ----"
./build/diag/probe-negctl "bench/instances/pilotnov.mps" -4497.2761882188715 2>&1 \
    | grep -E "LOAN |one-accumulator|by-segment|shift written|outstanding|worst one"
echo

echo "######## CANDIDATE (D118's refused presolve) ########"
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "---- $1 ----"
    ./build/diag/probe-cand "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "LOAN |one-accumulator|by-segment|shift written|outstanding|worst one|objective |reference |checker "
    echo
done

echo "######## HEAD, the shipping configuration ########"
for pair in "pilotnov -4497.2761882188715" "pilot-ja -6113.1364655813431" \
            "dfl001 11266396.046671392"; do
    set -- $pair
    echo "---- $1 ----"
    ./build/diag/probe-head "bench/instances/$1.mps" "$2" 2>&1 \
        | grep -E "LOAN |one-accumulator|by-segment|shift written|outstanding|worst one|objective |reference |checker "
    echo
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
