#!/bin/bash
# The 80 declines whose row logical is "already nonbasic": already nonbasic
# according to WHOM?
#
# D135 counted them from the PUBLISHED status and assumed in its own comment
# that "that row survives into the reduced model" — it never checked. TODO.md
# orders this re-measure before believing the 80 are a second defect: a row a
# SINGLETON_COL relaxed can still be removed later by another family, and then
# the published status is that family's replay, not the reduced solve's.
#
# So, at the exact moment ps_singleton_col_swap reads the row status (the
# second pass, every activity final), every firing whose column published
# BASIC is classified:
#
#   swapped            -> the exchange fired; nothing owed
#   prerow BASIC, row interior -> partner available, no bound to rest on (the 108)
#   prerow nonbasic:
#     surv=0           -> the row DID NOT survive; rmtag says which family
#                         removed it (D135's unchecked assumption, refuted)
#     surv=1 redst=BASIC prevsw=1 -> an earlier firing of THIS family already
#                         took the logical (two interior columns, one row)
#     surv=1 redst=BASIC prevsw=0 -> someone else overwrote a surviving row's
#                         status (no known writer; a finding if nonzero)
#     surv=1 redst!=BASIC -> the reduced solve itself left the logical
#                         nonbasic (the real contradiction, if it exists)
#
# Predictions, stated before the run (D135 + 02-48's SUM=+272):
#   netlib: fired=5902 nofire=11332; swapped=5630, loose=108, C=84, A+B+D=80
#   kennington: fired=482 nofire=120; swapped=482, everything else 0
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-49-redst"
out="$here/reduced-status.txt"
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
/* Reduced-solve row statuses, snapshotted at the surviving-row copy so the
 * second pass can compare what it reads against what the reduced solve said.
 * -1 = the row did not survive. dg_prevsw marks rows whose logical THIS
 * family's swap already took out, so a second firing on the same row is
 * distinguishable from a pre-existing nonbasic. */
static signed char *dg_redst;
static unsigned char *dg_prevsw;
static long long dg_nofire;
static long long dg_fired;
#endif
""" + head)

copy = """    for (int64_t i = 0; i < orig->num_row; i++) {
        const int64_t ri = p->row_map[i];
        if (ri < 0)
            continue;
        orig->sol_dual[i] = red->sol_dual[ri];
        orig->sol_row_status[i] = red->sol_row_status[ri];
        orig->sol_row[i] = red->sol_row[ri];
    }"""
assert s.count(copy) == 1
s = s.replace(copy, copy + """
#ifdef JAOS_DIAG
    dg_redst = malloc((size_t)orig->num_row);
    dg_prevsw = calloc((size_t)orig->num_row, 1);
    dg_nofire = 0; dg_fired = 0;
    if (dg_redst != nullptr)
        for (int64_t dgi = 0; dgi < orig->num_row; dgi++)
            dg_redst[dgi] = (p->row_map[dgi] < 0)
                ? (signed char)-1
                : (signed char)red->sol_row_status[p->row_map[dgi]];
#endif""")

# The second pass appears twice (expand and solved paths, identical text).
# Only the expand one runs on these sets; instrument the FIRST occurrence.
branch = """        } else if (rec->tag == JM_PS_SINGLETON_COL) {
            ps_singleton_col_swap(orig, rec);
        }
    }"""
assert s.count(branch) == 2
diag = """        } else if (rec->tag == JM_PS_SINGLETON_COL) {
#ifdef JAOS_DIAG
            const int64_t dj = ps_restore_index(rec->index2, orig->num_col);
            const int64_t di = rec->index;
            const int precol = (int)orig->sol_col_status[dj];
            const int prerow = (int)orig->sol_row_status[di];
#endif
            ps_singleton_col_swap(orig, rec);
#ifdef JAOS_DIAG
            if (precol != JAOS_BASIS_BASIC) {
                dg_nofire++;
            } else if (dg_redst != nullptr && dg_prevsw != nullptr) {
                dg_fired++;
                const int postrow = (int)orig->sol_row_status[di];
                const int swapped =
                    (prerow == JAOS_BASIS_BASIC && postrow != JAOS_BASIS_BASIC);
                int rmtag = -1, rmafter = 0;
                if (dg_redst[di] < 0) {
                    for (int64_t q = 0; q < p->arena_len; q++) {
                        const jm_presolve_rec *rq = &p->arena[q];
                        if (rq->index != di) continue;
                        if (rq->tag == JM_PS_EMPTY_ROW ||
                            rq->tag == JM_PS_SINGLETON_ROW ||
                            rq->tag == JM_PS_FREE_COL_SINGLETON ||
                            rq->tag == JM_PS_REDUNDANT_ROW ||
                            rq->tag == JM_PS_FORCING_ROW ||
                            rq->tag == JM_PS_IMPLIED_FREE_COL) {
                            rmtag = (int)rq->tag; rmafter = q > r; break;
                        }
                    }
                }
                const double dact = orig->sol_row[di];
                const char dtight = (dact == orig->row_lower[di]) ? 'L' :
                                    (dact == orig->row_upper[di]) ? 'U' : 'I';
                char dbuf[224];
                int dk = snprintf(dbuf, sizeof dbuf,
                    "SC2 prerow=%d surv=%d redst=%d prevsw=%d swapped=%d "
                    "rmtag=%d rmafter=%d tight=%c\\n",
                    prerow, dg_redst[di] >= 0 ? 1 : 0, (int)dg_redst[di],
                    (int)dg_prevsw[di], swapped, rmtag, rmafter, dtight);
                if (dk > 0 && dk < (int)sizeof dbuf) {
                    ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
                }
                if (swapped)
                    dg_prevsw[di] = 1;
            }
#endif
        }
    }
#ifdef JAOS_DIAG
    {
        char sbuf[128];
        int sk = snprintf(sbuf, sizeof sbuf,
            "SC2TOT fired=%lld nofire=%lld\\n", dg_fired, dg_nofire);
        if (sk > 0 && sk < (int)sizeof sbuf) {
            ssize_t sw2 = write(2, sbuf, (size_t)sk); (void)sw2;
        }
        free(dg_redst); dg_redst = nullptr;
        free(dg_prevsw); dg_prevsw = nullptr;
    }
#endif"""
at = s.index(branch)
s = s[:at] + diag + s[at + len(branch):]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-redst -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-redst -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-redst -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# For every SINGLETON_COL firing publishing its column BASIC, read at"
echo "# the second pass: did the row survive, what did the reduced solve say,"
echo "# and who wrote what the swap reads."
echo "# tags: 1=EMPTY_ROW 3=SINGLETON_ROW 5=FREE_COL_SINGLETON"
echo "#       6=REDUNDANT_ROW 7=FORCING_ROW 8=IMPLIED_FREE_COL"
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "SC2 " "$d/$f.log"); ok=$(grep -c "^SC2 " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^SC2 /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        if (v["swapped"] == 1) { SW++ }
        else if (v["prerow"] == 0) {
            if (v["tight"] == "I") LOOSE++; else ODD++
        } else if (v["surv"] == 0) {
            A++; RM[v["rmtag"]]++
            if (v["rmafter"] == 1) RMA++
            AT[v["tight"]]++; AP[v["prerow"]]++
        } else if (v["redst"] == 0 && v["prevsw"] == 1) { C++ }
        else if (v["redst"] == 0) { D++ }
        else { B++; BRED[v["redst"]]++; BT[v["tight"]]++ }
    }
    /^SC2TOT/{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        F += v["fired"]; N += v["nofire"]; solves++
    }
    END {
        printf "%d solves;  fired=%d nofire=%d (canary: 5902/11332 nl, 482/120 kn)\n\n", solves, F, N
        printf "  %-52s %6d\n", "swapped — the exchange fired", SW
        printf "  %-52s %6d\n", "declined: partner in, row interior (the 108)", LOOSE
        printf "  %-52s %6d\n", "declined: partner in, row tight, NOT swapped (odd)", ODD
        printf "  %-52s %6d\n", "declined: row DID NOT SURVIVE (A)", A
        for (t in RM) printf "      %-48s %6d\n", "removed by tag " t, RM[t]
        if (A) printf "      %-48s %6d\n", "removal fired AFTER the col (arena order)", RMA
        for (t in AT) printf "      %-48s %6d\n", "final activity class " t, AT[t]
        for (t in AP) printf "      %-48s %6d\n", "published status at read " t, AP[t]
        printf "  %-52s %6d\n", "declined: this family already swapped the row (C)", C
        printf "  %-52s %6d\n", "declined: survived, reduced BASIC, overwritten (D)", D
        printf "  %-52s %6d\n", "declined: REDUCED SOLVE left it nonbasic (B)", B
        for (t in BRED) printf "      %-48s %6d\n", "reduced status " t, BRED[t]
        for (t in BT) printf "      %-48s %6d\n", "final activity class " t, BT[t]
        printf "\n  unpaid total (fired - swapped) = %d; 02-48 says netlib SUM=+272, kn 0\n", F - SW
    }' "$d/$f.log"
    echo
done
echo "# prediction (before the run): nl swapped=5630 loose=108 C=84 A+B+D=80;"
echo "# kn all 482 swapped. A refutes D135's survival assumption; B is a real"
echo "# contradiction; D has no known writer and is a finding on its own."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
