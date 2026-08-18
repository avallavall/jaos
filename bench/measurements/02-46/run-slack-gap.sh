#!/bin/bash
# Is the singleton row's basis status decidable from its own dual?
#
# D134 counted SINGLETON_ROW's four combinations and found two wrong. Reading
# the case again, the rule is not a choice to be invented — it falls out of
# complementary slackness, and the code already computes both halves of it:
#
#   `y_i = 0.0`            when `zero_works || !this_row_owns`, and the column
#                          is left alone
#   `y_i = d0 / rec->coef` otherwise, and the column is set BASIC
#
# A basic logical requires a zero dual. So:
#
#   y_i == 0  ->  the row's logical MAY be basic, and no column was taken, so
#                 marking the row BASIC balances the restored row
#   y_i != 0  ->  the row's logical MUST be nonbasic, and the column was taken
#                 instead, so the row must rest on a bound
#
# Checked against D134's four combinations, that rule agrees with both correct
# ones and disagrees with both wrong ones. It is derived, not fitted.
#
# **One case is not obviously implementable.** The `y_i != 0` rule wants the
# row published at a bound, and the +1 combination is exactly the firings
# where the activity is NOT on one — 526 on netlib, 29058 on Kennington.
# Publishing AT_LOWER there claims a bound the row is not on.
#
# So: how far off is it? If the gap is a few ulps of the row's own traffic,
# the exact comparison is the whole difficulty and a relative test settles it.
# If the gap is real, the published point is inconsistent with its own dual
# and that is a different and larger finding.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-46-gap"
out="$here/slack-gap.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#include <math.h>
static int dg_ynz;              /* did this firing take the y_i != 0 branch? */
/* Recorded per row during the replay, CLASSIFIED after it. A row's activity
 * is not final until every record touching it has replayed (`ps_row_add`
 * accumulates), so reading it inside the replay compares a partial sum
 * against a bound. That trap cost 02-45 a pass and this one another. */
static signed char *dg_branch;  /* -1 no singleton row, 0 y==0, 1 y!=0 */
static double *dg_traffic;
/* Buckets on log10(gap / traffic), one decade each, -16 to +2. The range
 * has to reach 1 or the top bucket lumps rounding in with a real gap. */
#define DG_NB 20
static int64_t dg_gap[DG_NB];   /* decade of gap/traffic, -16 up to +2 */
static int64_t dg_agree, dg_disagree_low, dg_disagree_high, dg_noboundatall;
#endif
""" + head)

a = """            y_i = 0.0;"""
assert s.count(a) == 1
s = s.replace(a, """#ifdef JAOS_DIAG
            dg_ynz = 0;
#endif
""" + a)

b = """            y_i = d0 / rec->coef;"""
assert s.count(b) == 1
s = s.replace(b, """#ifdef JAOS_DIAG
            dg_ynz = 1;
#endif
""" + b)

c = """        if (act == orig->row_lower[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        else if (act == orig->row_upper[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_UPPER;
        else
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;"""
assert s.count(c) == 1
s = s.replace(c, c + """
#ifdef JAOS_DIAG
    if (dg_branch != nullptr) {
        dg_branch[i] = (signed char)dg_ynz;
        dg_traffic[i] = fabs(rec->coef * xv);
    }
#endif""")

loop = """    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);"""
assert s.count(loop) == 2
at = s.index(loop)
s = s[:at] + """#ifdef JAOS_DIAG
    dg_agree = dg_disagree_low = dg_disagree_high = dg_noboundatall = 0;
    for (int dgb = 0; dgb < DG_NB; dgb++) dg_gap[dgb] = 0;
    dg_branch = malloc((size_t)orig->num_row * sizeof *dg_branch);
    dg_traffic = malloc((size_t)orig->num_row * sizeof *dg_traffic);
    if (dg_branch != nullptr)
        for (int64_t dgi = 0; dgi < orig->num_row; dgi++) dg_branch[dgi] = -1;
#endif
""" + loop + """
#ifdef JAOS_DIAG
    {
        /* Judged now, with every activity final. */
        for (int64_t i2 = 0; dg_branch != nullptr && i2 < orig->num_row; i2++) {
            if (dg_branch[i2] < 0) continue;
            const bool on_bound =
                orig->sol_row_status[i2] != JAOS_BASIS_BASIC;
            /* The rule: y_i == 0 -> row basic; y_i != 0 -> row on a bound. */
            if ((dg_branch[i2] != 0) == on_bound) {
                dg_agree++;
                continue;
            }
            if (dg_branch[i2] == 0) {
                /* Wants BASIC, published on a bound. A basic variable is
                 * allowed to sit exactly on a bound, so this is free to fix. */
                dg_disagree_low++;
                continue;
            }
            /* Wants a bound and is not on one. How far, against the row's own
             * traffic — its one live term. */
            dg_disagree_high++;
            const double act2 = orig->sol_row[i2];
            double gap = HUGE_VAL;
            if (isfinite(orig->row_lower[i2]))
                gap = fabs(act2 - orig->row_lower[i2]);
            if (isfinite(orig->row_upper[i2])) {
                const double g2 = fabs(act2 - orig->row_upper[i2]);
                if (g2 < gap) gap = g2;
            }
            if (!isfinite(gap)) { dg_noboundatall++; continue; }
            const double rel =
                dg_traffic[i2] > 0.0 ? gap / dg_traffic[i2] : gap;
            int bkt = 0;
            if (rel != 0.0) {
                bkt = (int)floor(log10(rel)) + 17;
                if (bkt < 1) bkt = 1;
                if (bkt > DG_NB - 1) bkt = DG_NB - 1;
            }
            dg_gap[bkt]++;
        }
        free(dg_branch); free(dg_traffic);
        dg_branch = nullptr; dg_traffic = nullptr;
        char buf[1024];
        int k = snprintf(buf, sizeof buf,
                "GAP agree=%lld wants_basic=%lld wants_bound=%lld nobound=%lld hist=",
                (long long)dg_agree, (long long)dg_disagree_low,
                (long long)dg_disagree_high, (long long)dg_noboundatall);
        for (int dgb = 0; dgb < DG_NB && k > 0 && k < (int)sizeof buf; dgb++)
            k += snprintf(buf + k, sizeof buf - (size_t)k, "%lld%s",
                          (long long)dg_gap[dgb], dgb + 1 < DG_NB ? "," : "\\n");
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
#endif
""" + s[at + len(loop):]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-gap -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-gap -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-gap -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Does the singleton row's own dual decide its basis status?"
echo "#"
echo "# rule: y_i == 0 -> the row may be basic; y_i != 0 -> it must be on a"
echo "# bound. 'wants BASIC' is free to fix — a basic variable may sit on a"
echo "# bound. 'wants a bound' is the hard one: how far from one is it?"
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "GAP " "$d/$f.log"); ok=$(grep -c "^GAP " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^GAP /{
        for (i = 2; i <= NF; i++) {
            split($i, kv, "=")
            if (kv[1] == "hist") {
                m = split(kv[2], H, ",")
                for (q = 1; q <= m; q++) B[q] += H[q]
            } else v[kv[1]] += kv[2]
        }
        solves++
    }
    END {
        printf "%d solves\n", solves
        printf "  %-44s %8d\n", "rule already agrees with what is published", v["agree"]
        printf "  %-44s %8d\n", "rule wants BASIC, published on a bound", v["wants_basic"]
        printf "  %-44s %8d\n", "rule wants a bound, row is not on one", v["wants_bound"]
        printf "  %-44s %8d\n", "  of those, the row has no finite bound", v["nobound"]
        printf "\n  gap to the nearest bound, relative to the row traffic\n"
        printf "  %-14s %8d\n", "exactly 0", B[1]
        for (q = 2; q <= m; q++)
            # C bucket b holds floor(log10(rel)) = b - 17, and awk index q is
            # b + 1, so the decade is q - 18. The first pass printed q - 17
            # and was one decade optimistic.
            if (B[q] > 0) printf "  10^%-11d %8d\n", q - 18, B[q]
    }' "$d/$f.log"
    echo
done
echo "# canary: 'wants a bound' must be 526 on netlib and 29058 on Kennington"
echo "# (D134's +1 combination) or this is not the same set of firings."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
