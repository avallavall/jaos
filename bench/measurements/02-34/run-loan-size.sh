#!/bin/bash
# TODO.md 5a, the last item: nothing bounds a loan relative to the cost it
# lands on.
#
# `shift_to_feasible` does three things and only the first is argued for:
#
#     s->cost[v] += need;      /* however large `need` is */
#     s->shift[v] += need;
#     s->d[v] = 0.0;           /* asserts the cost moved by exactly `need` */
#
# Two separate defects, and this measures both before anything is designed:
#
#   A. THE OVERWRITE. A `need` of 1e32 landing on a cost of one does not
#      repair a sign condition, it replaces the model's cost with the loan.
#      Measured as the ratio |need| / |cost| at the moment of the lend.
#   B. THE FABRICATION. `d[v] = 0.0` is a claim that the cost moved by
#      exactly `need`. It is false whenever `need` is below the ulp of the
#      cost: the cost does not move at all and `d` is set to zero anyway, so
#      dual feasibility is asserted rather than repaired. Measured on the
#      outcome (`cost + need == cost`) and on the inputs (`|need| < ulp`).
#
# A bound needs evidence on both sides, so this reports the whole
# distribution rather than a maximum. **Nothing here proposes a number.**
#
# One line per solve on stderr, over all three gate sets. src/ is read and
# never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-34-size"
out="$here/loan-size.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-infeas instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/simplex.c"
s = open(p, encoding="utf-8").read()

head = "constexpr double ARTIFICIAL_BOUND = 1e10;"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
/* Buckets on log10(|need| / |cost|), one decade each, clamped at both ends.
 * Bucket 0 is a cost of exact zero, where the ratio has no value. */
#define NB 24
static int64_t g_bucket[NB];
static int64_t g_lends, g_cost_unmoved, g_below_ulp, g_cost_zero;
static double g_max_ratio, g_max_need, g_max_need_cost;
/* At the worst ratio, both magnitudes. A ratio is large either because the
 * loan is large or because the cost is near zero, and only these two say
 * which — the maxima above are tracked independently and cannot. */
static double g_wr_need, g_wr_cost;
/* The D121 shape: a loan that is a number in its own right landing on a cost
 * that is also a number, and swamping it. The largest such loan, and the
 * cost it landed on. */
static double g_ovr_need, g_ovr_cost;
static int64_t g_ovr_count;
#endif""")

# Reset where cost0 is written, so the counters describe one solve.
snap = """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
#ifdef JAOS_DIAG
    memset(g_bucket, 0, sizeof g_bucket);
    g_lends = g_cost_unmoved = g_below_ulp = g_cost_zero = g_ovr_count = 0;
    g_max_ratio = g_max_need = g_max_need_cost = 0.0;
    g_wr_need = g_wr_cost = g_ovr_need = g_ovr_cost = 0.0;
#endif
    return JAOS_OK;""")

lend = """    s->cost[v] += need;
    s->shift[v] += need;
    s->d[v] = 0.0;"""
assert s.count(lend) == 1, s.count(lend)
s = s.replace(lend, """#ifdef JAOS_DIAG
    {
        const double a = fabs(s->cost[v]), n = fabs(need);
        g_lends++;
        /* Asked on the outcome, not on the inputs: the question is whether
         * the cost moves, and `d[v] = 0.0` three lines down is a fabrication
         * exactly when it does not. */
        if (s->cost[v] + need == s->cost[v])
            g_cost_unmoved++;
        if (n > g_max_need) { g_max_need = n; g_max_need_cost = a; }
        if (a == 0.0) {
            g_cost_zero++;
            g_bucket[0]++;
        } else {
            const double r = n / a;
            if (r > g_max_ratio) {
                g_max_ratio = r;
                g_wr_need = n;
                g_wr_cost = a;
            }
            /* Both sides a number, and the loan swamping the cost. The
             * thresholds are not a proposal: 1e-6 is where this solver's own
             * DUAL_TOL sits by orders of magnitude, and 1e6 is six decades of
             * swamping. They select a shape to look at, and decide nothing. */
            if (n > 1e-6 && a > 1e-6 && r > 1e6) {
                g_ovr_count++;
                if (n > g_ovr_need) { g_ovr_need = n; g_ovr_cost = a; }
            }
            if (n < nextafter(a, HUGE_VAL) - a) g_below_ulp++;
            /* log10 is not correctly rounded and is not pinned across C
             * libraries, so it decides nothing: it chooses which decade a
             * reading is printed in, and a last-ulp difference moves one
             * count between two adjacent buckets of a histogram nobody
             * compares bit for bit. */
            int b = (int)floor(log10(r)) + 12;
            if (b < 1) b = 1;
            if (b > NB - 1) b = NB - 1;
            g_bucket[b]++;
        }
    }
#endif
""" + lend)

anchor = """    sx_free(&s);
    jm_presolve_free(&p);
    return st;"""
assert s.count(anchor) == 1
s = s.replace(anchor, """#ifdef JAOS_DIAG
    /* ONE write, not fourteen fprintf calls. The acceptance runner forks a
     * child per instance and twelve of them share this stderr, so every
     * separate write is a chance for two lines to interleave — and a mangled
     * line is silently dropped by the reader, which is how the first pass of
     * this probe read 188 solves and the second read 187. A single write()
     * on the shared file description is atomic against the offset. */
    {
        char buf[1024];
        int k = snprintf(buf, sizeof buf,
                "LOANSIZE lends=%lld unmoved=%lld below_ulp=%lld "
                "cost_zero=%lld max_ratio=%.6g max_need=%.6g at_cost=%.6g "
                "wr_need=%.6g wr_cost=%.6g ovr=%lld ovr_need=%.6g "
                "ovr_cost=%.6g hist=",
                (long long)g_lends, (long long)g_cost_unmoved,
                (long long)g_below_ulp, (long long)g_cost_zero,
                g_max_ratio, g_max_need, g_max_need_cost,
                g_wr_need, g_wr_cost, (long long)g_ovr_count,
                g_ovr_need, g_ovr_cost);
        for (int b = 0; b < NB && k > 0 && k < (int)sizeof buf; b++)
            k += snprintf(buf + k, sizeof buf - (size_t)k, "%lld%s",
                          (long long)g_bucket[b], b + 1 < NB ? "," : "\\n");
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + anchor)

open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-size -lm \
    || { echo "build failed"; exit 2; }

# The canary: a probe with the hooks compiled out and a solve that never lends
# produce the same empty result.
./build/diag/run-size -j 1 -o /dev/null 25fv47 2>&1 | grep -q "LOANSIZE" \
    || { echo "CANARY FAILED: no LOANSIZE line from a solve"; exit 3; }

d=$(mktemp -d)
./build/diag/run-size -j 12 -o "$d/netlib.txt" > "$d/netlib.log" 2>&1
# Repeated once, because the first version of this probe read 188 solves and
# then 187 from the same tree: twelve children shared one stderr and a line
# was lost to interleaving. Every figure below is quoted from a set that
# repeats, and the check is the two logs agreeing rather than a claim.
./build/diag/run-size -j 12 -o "$d/netlib2.txt" > "$d/netlib2.log" 2>&1
./build/diag/run-size -j 12 -m bench/netlib-infeas.manifest -e infeasible \
    -d bench/instances-infeas -o "$d/infeas.txt" > "$d/infeas.log" 2>&1
./build/diag/run-size -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kennington.txt" \
    > "$d/kennington.log" 2>&1

{
echo "# TODO.md 5a: how large is a loan against the cost it lands on, and"
echo "# how often does the cost not move at all?"
echo "#"
echo "# unmoved   = the cost does not change; d[v]=0 is then a fabrication"
echo "# below_ulp = |need| < ulp(|cost|), the same question on the inputs"
echo "# decade k  = lends with 10^k <= |need|/|cost| < 10^(k+1), clamped to"
echo "#             [-11, 10] at both ends"
echo
echo "-- no line was mangled by twelve children sharing one stderr --"
for f in netlib netlib2 infeas kennington; do
    all=$(grep -c "LOANSIZE" "$d/$f.log")
    ok=$(grep -c "^LOANSIZE" "$d/$f.log")
    printf "%-12s %d line(s), %d intact%s\n" "$f" "$all" "$ok" \
        "$([ "$all" = "$ok" ] && echo "" || echo "   *** MANGLED ***")"
done
echo
for f in netlib netlib2 infeas kennington; do
    [ "$f" = "netlib2" ] || echo "######## $f ########"
    awk '/^LOANSIZE/ {
        n = split($0, F, " ")
        delete v
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        lends += v["lends"]; unmoved += v["unmoved"]
        below += v["below_ulp"]; zero += v["cost_zero"]
        if (v["max_ratio"] + 0 > mr) {
            mr = v["max_ratio"] + 0
            wrn = v["wr_need"]; wrc = v["wr_cost"]
        }
        if (v["max_need"] + 0 > mn) { mn = v["max_need"] + 0; mc = v["at_cost"] }
        ovr += v["ovr"]
        if (v["ovr_need"] + 0 > on) { on = v["ovr_need"] + 0; oc = v["ovr_cost"] }
        m = split(v["hist"], H, ",")
        for (i = 1; i <= m; i++) B[i] += H[i]
        solves++
        if (v["unmoved"] + 0 > 0) with_unmoved++
    }
    END {
        printf "solves=%d  lends=%d  unmoved=%d  below_ulp=%d  cost_zero=%d\n",
               solves, lends, unmoved, below, zero
        printf "solves with at least one unmoved cost: %d\n", with_unmoved
        printf "worst |need|/|cost| = %.6g   (|need|=%s on |cost|=%s)\n",
               mr, wrn, wrc
        printf "largest |need| = %.6g  on a cost of %.6g\n", mn, mc
        printf "both sides above 1e-6 and swamped by 1e6: %d lend(s)", ovr
        if (ovr + 0 > 0)
            printf "   worst |need|=%.6g on |cost|=%.6g", on, oc
        printf "\n"
        printf "decade      count\n"
        for (i = 2; i <= m; i++)
            if (B[i] > 0) printf "%6d  %10d\n", i - 13, B[i]
        if (B[1] > 0) printf "cost==0 %10d\n", B[1]
    }' "$d/$f.log" > "$d/$f.sum"
    [ "$f" = "netlib2" ] || { cat "$d/$f.sum"; echo; }
done

echo "-- netlib run twice from the same tree --"
if cmp -s "$d/netlib.sum" "$d/netlib2.sum"; then
    echo "the two aggregates are byte-identical"
else
    echo "*** THE TWO RUNS DISAGREE, every figure above is unquotable ***"
    diff "$d/netlib.sum" "$d/netlib2.sum"
fi
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
