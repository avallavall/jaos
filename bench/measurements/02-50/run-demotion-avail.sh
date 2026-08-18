#!/bin/bash
# Is a demotion partner available for the 232 declines the swap leaves?
#
# D140 split the netlib residue into 80 firings whose row logical the reduced
# solve left nonbasic (an exact degenerate tie) and 152 whose row is interior
# with its logical basic. Both leave the published basis one member too many.
#
# The snap D140 sketched for the 80 does not survive the arithmetic: 02-49
# measured 74 of the 80 rows landing EXACTLY on their original bound with the
# interior xv, and moving xv to the column bound perturbs that activity by
# ulps — it would trade a column-status defect for a row-status one.
#
# The alternative both classes share is the one D135 asked about the logical:
# demote some OTHER basic variable of the same row that rests exactly on its
# own bound (a degenerate basic member; making it nonbasic claims nothing
# false and moves no value). This probe measures availability only — whether
# such a member exists per declined firing — exactly as D135 measured the
# logical's availability before D139 built the exchange. Rank is the design
# question that follows, not this one.
#
# For every true-population declined firing (column interior at its own
# replay, no swap fired), after the full replay and fold:
#   class B -> row logical nonbasic (the 80)
#   class L -> row logical basic, row interior (the 152)
#   cands   -> columns j' != j of row i with status BASIC and value exactly
#              on col_lower or col_upper
#
# Prediction, stated before the run: unknown; the question is whether cands=0
# dominates (no demotion rule can exist) or cands>=1 (one can).
# Kennington is the control: zero declines, so zero AVAIL lines.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-50-avail"
out="$here/demotion-avail.txt"
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
static signed char *dg_colst;   /* singleton col status AT ITS REPLAY WRITE */
static long long dg_fired;
#endif
""" + head)

alloc_anchor = """    /* Strictly LIFO (D-07). */
    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);"""
assert s.count(alloc_anchor) == 1
s = s.replace(alloc_anchor, """#ifdef JAOS_DIAG
    dg_colst = malloc((size_t)orig->num_col);
    dg_fired = 0;
    if (dg_colst != nullptr)
        memset(dg_colst, 0xFF, (size_t)orig->num_col);
#endif
""" + alloc_anchor)

wr = """        orig->sol_col_status[j] =
            (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
            (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;"""
assert s.count(wr) == 1
s = s.replace(wr, wr + """
#ifdef JAOS_DIAG
        if (dg_colst != nullptr)
            dg_colst[j] = (signed char)orig->sol_col_status[j];
#endif""")

branch = """        } else if (rec->tag == JM_PS_SINGLETON_COL) {
            ps_singleton_col_swap(orig, rec);
        }
    }"""
assert s.count(branch) == 2
diag = """        } else if (rec->tag == JM_PS_SINGLETON_COL) {
#ifdef JAOS_DIAG
            const int64_t dj = ps_restore_index(rec->index2, orig->num_col);
            const int64_t di = rec->index;
            const int prerow = (int)orig->sol_row_status[di];
#endif
            ps_singleton_col_swap(orig, rec);
#ifdef JAOS_DIAG
            if (dg_colst != nullptr && dg_colst[dj] == JAOS_BASIS_BASIC) {
                dg_fired++;
                const int postrow = (int)orig->sol_row_status[di];
                const int swapped =
                    (prerow == JAOS_BASIS_BASIC && postrow != JAOS_BASIS_BASIC);
                if (!swapped) {
                    const char cls =
                        (prerow != JAOS_BASIS_BASIC) ? 'B' : 'L';
                    long long cands = 0, basics = 0;
                    for (int64_t j2 = 0; j2 < orig->num_col; j2++) {
                        if (j2 == dj) continue;
                        bool inrow = false;
                        for (int64_t k = orig->a_start[j2];
                             k < orig->a_start[j2 + 1]; k++)
                            if (orig->a_index[k] == di) { inrow = true; break; }
                        if (!inrow) continue;
                        if (orig->sol_col_status[j2] != JAOS_BASIS_BASIC)
                            continue;
                        basics++;
                        const double v2 = orig->sol_col[j2];
                        if (v2 == orig->col_lower[j2] ||
                            v2 == orig->col_upper[j2])
                            cands++;
                    }
                    char dbuf[160];
                    int dk = snprintf(dbuf, sizeof dbuf,
                        "AVAIL cls=%c cands=%lld rowbasics=%lld\\n",
                        cls, cands, basics);
                    if (dk > 0 && dk < (int)sizeof dbuf) {
                        ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
                    }
                }
            }
#endif
        }
    }
#ifdef JAOS_DIAG
    {
        char sbuf[96];
        int sk = snprintf(sbuf, sizeof sbuf, "AVTOT fired=%lld\\n", dg_fired);
        if (sk > 0 && sk < (int)sizeof sbuf) {
            ssize_t sw2 = write(2, sbuf, (size_t)sk); (void)sw2;
        }
        free(dg_colst); dg_colst = nullptr;
    }
#endif"""
at = s.index(branch)
s = s[:at] + diag + s[at + len(branch):]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-avail -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-avail -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-avail -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# For every declined true firing: how many OTHER basic columns of the"
echo "# row rest exactly on one of their own bounds (demotion candidates)?"
echo "# cls=B: the 80 (logical nonbasic). cls=L: the 152 (row interior)."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "AVAIL " "$d/$f.log"); ok=$(grep -c "^AVAIL " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^AVAIL /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        c = v["cls"]; n[c]++
        if (v["cands"] == 0) z[c]++
        else if (v["cands"] == 1) one[c]++
        else more[c]++
        CS[c] += v["cands"]; BS[c] += v["rowbasics"]
        if (v["rowbasics"] == 0) nb[c]++
    }
    /^AVTOT/{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        F += v["fired"]; solves++
    }
    END {
        printf "%d solves; true fired=%d (canary 5902 nl / 482 kn)\n\n", solves, F
        for (c in n) {
            printf "class %s: %d declined firings\n", c, n[c]
            printf "  %-44s %6d\n", "cands = 0  — no demotion partner", z[c]
            printf "  %-44s %6d\n", "cands = 1  — forced partner", one[c]
            printf "  %-44s %6d\n", "cands >= 2 — a choice, needs a rule", more[c]
            printf "  %-44s %6d\n", "rows with NO other basic column at all", nb[c]
            printf "  %-44s %6.2f\n", "mean basic columns in the row", n[c] ? BS[c]/n[c] : 0
        }
        if (!("B" in n) && !("L" in n)) print "no declined firings (control holds)"
    }' "$d/$f.log"
    echo
done
echo "# expected totals: nl B=80, L=152 (D140); kn none. cands=0 dominating"
echo "# means no demotion rule can exist and both classes need another design."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
