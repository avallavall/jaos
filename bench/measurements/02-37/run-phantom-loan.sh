#!/bin/bash
# TODO.md 5a, the last item: `shift[v] += need` records a loan that was never
# made, on 16.7% of netlib's lends (D125).
#
# Recording `moved` instead is not free, and the reason is one line in
# `settle_shifts`:
#
#     if (!repay_shifts(s))
#         return;
#
# `repay_shifts` returns whether anything was outstanding, and `settle_shifts`
# skips `compute_duals` and `repair_dual_infeasibility` when nothing was. A
# phantom loan makes it return true, so it currently forces a re-pricing that
# would otherwise be skipped.
#
# So the question is not whether the record is honest. It is **how often the
# honest record would flip that return from true to false**, because that is
# the whole of the behaviour change. Measure before repairing.
#
# The two answers differ only when every outstanding record is phantom: a
# column whose cost really moved has `cost != cost0`, which both readings see.
#
#   flips = 0  -> the candidate is a pure no-op and only the record changes
#   flips > 0  -> it skips re-pricings, and the gate has to price that
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-37-phantom"
out="$here/phantom-loan.txt"
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

head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
/* The record the candidate would keep: `moved` rather than `need`. */
static double *g_shift_real;
static int64_t g_repay_calls, g_flips, g_phantom_lends, g_real_lends;
#endif""")

snap = """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
#ifdef JAOS_DIAG
    free(g_shift_real);
    g_shift_real = calloc((size_t)s->nvar, sizeof *g_shift_real);
    g_repay_calls = g_flips = g_phantom_lends = g_real_lends = 0;
#endif
    return JAOS_OK;""")

lend = """    s->cost[v] += need;
    s->shift[v] += need;
    s->d[v] = 0.0;"""
assert s.count(lend) == 1
s = s.replace(lend, """#ifdef JAOS_DIAG
    const double diag_before = s->cost[v];
#endif
""" + lend + """
#ifdef JAOS_DIAG
    if (g_shift_real) {
        const double diag_moved = s->cost[v] - diag_before;
        g_shift_real[v] += diag_moved;
        if (diag_moved == 0.0) g_phantom_lends++; else g_real_lends++;
    }
#endif""")

# Both readings of "was anything outstanding", taken before the loop that
# clears the record.
rep = """static bool repay_shifts(sx *s)
{
    bool any = false;"""
assert s.count(rep) == 1
s = s.replace(rep, """static bool repay_shifts(sx *s)
{
#ifdef JAOS_DIAG
    if (g_shift_real) {
        bool today = false, cand = false;
        for (int64_t v = 0; v < s->nvar; v++) {
            const bool moved_cost = s->cost[v] != s->cost0[v];
            if (moved_cost || s->shift[v] != 0.0)       today = true;
            if (moved_cost || g_shift_real[v] != 0.0)   cand  = true;
        }
        g_repay_calls++;
        if (today != cand) g_flips++;
        memset(g_shift_real, 0, (size_t)s->nvar * sizeof *g_shift_real);
    }
#endif
    bool any = false;""")

# The other repayment clears one column's record, so the shadow must too.
rep2 = """            const double give_back = s->cost[q] - s->cost0[q];
            s->cost[q] = s->cost0[q];"""
assert s.count(rep2) == 1
s = s.replace(rep2, """#ifdef JAOS_DIAG
            if (g_shift_real) g_shift_real[q] = 0.0;
#endif
            const double give_back = s->cost[q] - s->cost0[q];
            s->cost[q] = s->cost0[q];""")

tail = """    sx_free(&s);
    jm_presolve_free(&p);
    return st;"""
assert s.count(tail) == 1
s = s.replace(tail, """#ifdef JAOS_DIAG
    {
        char buf[512];
        int k = snprintf(buf, sizeof buf,
                "PHANTOM repay_calls=%lld flips=%lld phantom_lends=%lld "
                "real_lends=%lld\\n",
                (long long)g_repay_calls, (long long)g_flips,
                (long long)g_phantom_lends, (long long)g_real_lends);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + tail)

open(p, "w", encoding="utf-8").write(s)
print("simplex.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-phantom -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-phantom -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-phantom -j 12 -m bench/netlib-infeas.manifest -e infeasible \
    -d bench/instances-infeas -o "$d/inf.txt" > "$d/inf.log" 2>&1
./build/diag/run-phantom -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Would an honest shift record skip a re-pricing?"
echo "#"
echo "# flips = calls to repay_shifts where 'was anything outstanding'"
echo "#         answers true today and false with the honest record instead."
echo "#         That is the whole of the behaviour change."
echo
for f in nl inf kn; do
    case $f in nl) L=netlib;; inf) L=infeasible;; kn) L=kennington;; esac
    all=$(grep -c "PHANTOM" "$d/$f.log")
    ok=$(grep -c "^PHANTOM" "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    awk -v L="$L" '/^PHANTOM/ {
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        calls += v["repay_calls"]; flips += v["flips"]
        ph += v["phantom_lends"]; re += v["real_lends"]; solves++
        if (v["flips"] + 0 > 0) badsolves++
    }
    END {
        printf "%-12s solves=%d  repay_calls=%d  FLIPS=%d  solves_with_a_flip=%d\n",
               L, solves, calls, flips, badsolves
        printf "%-12s lends: %d phantom, %d real (%.1f%% phantom)\n",
               "", ph, re, (ph + re) ? 100.0 * ph / (ph + re) : 0
    }' "$d/$f.log"
done
echo
echo "# canary: repay_calls > 0 is what says the observer executed; a probe"
echo "# that never ran and one that found no flips look alike otherwise."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
