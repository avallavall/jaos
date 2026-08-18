#!/bin/bash
# Does the ratio test compute a dual step from a reduced cost on the wrong
# side of zero, and how big does the division make it?
#
# `admit_candidate` clamps the NUMERATOR — `rnum[k] = dist > 0.0 ? dist : 0.0`
# — so a candidate already past zero blocks at once. `bfrt_walk`,
# `jm_harris_pick` and `jm_bland_pick` read only `rnum`, `rden` and `rrange`,
# so the clamp covers the whole choice.
#
# It does not cover the STEP. Both exits of `dual_ratio_test` compute
#
#     *theta_out = s->d[best] / s->alpha[best];
#
# from the raw `d`, and `|alpha|` is only required to be above
# PIVOT_MIN = 1e-9. So a `d` half an ulp on the wrong side of zero — 1.1e-16
# on a cost of order one — can come out as a dual step of 1.1e-07 with the
# wrong sign. `update_dual` then carries it into every other reduced cost.
#
# Two questions, and the first is about HEAD rather than about any candidate:
#
#   1. Does this happen at HEAD? The clamp's own comment says a drifted cost
#      is expected, so the answer is not obviously no.
#   2. Does the candidate in the working tree make it worse? It stops
#      `shift_to_feasible` writing a fabricated zero into `d` when the cost
#      cannot move, which is 16.7% of netlib's lends (D125), so more
#      wrong-signed `d` reaches this line.
#
# Both binaries are built in one run with distinct md5s printed. src/ is read
# and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-35-theta"
out="$here/theta-sign.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
# The candidate is uncommitted, so it is copied in rather than checked out.
cp "$root/src/simplex.c" "$wt/src/simplex.c.candidate" || exit 2
cd "$wt" || exit 2
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

patch_one() {   # $1 = the simplex.c to instrument, in place
python3 - "$1" << 'PY'
import sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()

head = "constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */"
assert s.count(head) == 1
s = s.replace(head, head + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static int64_t g_theta_total, g_theta_wrong;
static double g_worst_dist, g_worst_theta, g_worst_theta_alpha;
#endif""")

# Reset per solve, beside the one write to cost0.
snap = """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
    return JAOS_OK;"""
assert s.count(snap) == 1
s = s.replace(snap, """    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);
#ifdef JAOS_DIAG
    g_theta_total = g_theta_wrong = 0;
    g_worst_dist = g_worst_theta = g_worst_theta_alpha = 0.0;
#endif
    return JAOS_OK;""")

# The observer. Reads only; bills no work, so the counters cannot move.
probe = r'''
#ifdef JAOS_DIAG
/* The winner's distance to infeasibility, computed exactly as
 * admit_candidate computes it, and what the division makes of it. */
static void diag_theta(const sx *s, int64_t w)
{
    double dist;
    if (s->status[w] == JM_AT_LOWER)
        dist = s->d[w];
    else if (s->status[w] == JM_AT_UPPER)
        dist = -s->d[w];
    else
        return;                 /* free: admit_candidate calls its dist zero */
    g_theta_total++;
    if (dist >= 0.0)
        return;
    g_theta_wrong++;
    if (-dist > g_worst_dist)
        g_worst_dist = -dist;
    const double th = fabs(s->d[w] / s->alpha[w]);
    if (th > g_worst_theta) {
        g_worst_theta = th;
        g_worst_theta_alpha = fabs(s->alpha[w]);
    }
}
#endif
'''
anchor = "static int64_t dual_ratio_test(sx *s, bool below, double violation,"
assert s.count(anchor) == 1
s = s.replace(anchor, probe + anchor)

b1 = """        int64_t bv = s->cand[b];
        *theta_out = s->d[bv] / s->alpha[bv];"""
assert s.count(b1) == 1
s = s.replace(b1, """        int64_t bv = s->cand[b];
#ifdef JAOS_DIAG
        diag_theta(s, bv);
#endif
        *theta_out = s->d[bv] / s->alpha[bv];""")

b2 = """    *theta_out = s->d[best] / s->alpha[best];
    return best;"""
assert s.count(b2) == 1
s = s.replace(b2, """#ifdef JAOS_DIAG
    diag_theta(s, best);
#endif
    *theta_out = s->d[best] / s->alpha[best];
    return best;""")

# One write per record: twelve forked children share this stderr and several
# fprintf calls interleave (D125).
tail = """    sx_free(&s);
    jm_presolve_free(&p);
    return st;"""
assert s.count(tail) == 1
s = s.replace(tail, """#ifdef JAOS_DIAG
    {
        char buf[512];
        int k = snprintf(buf, sizeof buf,
                "THETA picks=%lld wrong=%lld worst_dist=%.6g "
                "worst_theta=%.6g at_alpha=%.6g\\n",
                (long long)g_theta_total, (long long)g_theta_wrong,
                g_worst_dist, g_worst_theta, g_worst_theta_alpha);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + tail)

open(p, "w", encoding="utf-8").write(s)
print("instrumented " + p)
PY
}

build_one() {   # $1 = binary name
    gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
        -Iinclude -Isrc src/*.c bench/run.c -o "build/diag/$1" -lm \
        || { echo "build failed: $1"; exit 2; }
}

cp src/simplex.c src/simplex.c.head
patch_one src/simplex.c || exit 2
build_one run-head

cp src/simplex.c.candidate src/simplex.c
patch_one src/simplex.c || exit 2
build_one run-cand
rm -f src/simplex.c.head src/simplex.c.candidate

report() {      # $1 = label, $2 = log
    awk -v L="$1" '/^THETA/ {
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        picks += v["picks"]; wrong += v["wrong"]; solves++
        if (v["wrong"] + 0 > 0) badsolves++
        if (v["worst_dist"] + 0 > wd) wd = v["worst_dist"] + 0
        if (v["worst_theta"] + 0 > wt) { wt = v["worst_theta"] + 0; wa = v["at_alpha"] }
    }
    END {
        printf "%-10s solves=%d picks=%d wrong=%d (%.4f%%) solves_with_any=%d\n",
               L, solves, picks, wrong, picks ? 100.0 * wrong / picks : 0, badsolves
        printf "%-10s worst |dist| past zero = %.6g\n", "", wd
        printf "%-10s worst |theta| from one = %.6g   at |alpha|=%.6g\n",
               "", wt, wa
    }' "$2"
}

d=$(mktemp -d)
for b in head cand; do
    ./build/diag/run-$b -j 12 -o "$d/nl-$b.txt" > "$d/nl-$b.log" 2>&1
    ./build/diag/run-$b -j 12 -m bench/netlib-kennington.manifest \
        -d bench/instances-kennington -o "$d/kn-$b.txt" > "$d/kn-$b.log" 2>&1
done

{
echo "# Does the dual step come out of a reduced cost on the wrong side of"
echo "# zero?  admit_candidate clamps the numerator; the step is computed"
echo "# from the raw d, and |alpha| may be as small as PIVOT_MIN = 1e-9."
echo
md5sum build/diag/run-head build/diag/run-cand
echo
for f in nl kn; do
    [ "$f" = nl ] && echo "######## netlib ########" || echo "######## kennington ########"
    for b in head cand; do
        all=$(grep -c "THETA" "$d/$f-$b.log")
        ok=$(grep -c "^THETA" "$d/$f-$b.log")
        [ "$all" = "$ok" ] || echo "*** $f-$b: $all lines, $ok intact — MANGLED ***"
        report "$b" "$d/$f-$b.log"
    done
    echo
done
echo "# canary: a probe that never ran and one that found nothing look alike."
echo "# picks>0 above is what says the observer executed."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
