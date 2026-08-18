#!/bin/bash
# Two-point read: the singleton column's status at its OWN replay write, and
# the same status when ps_singleton_col_swap's guard reads it.
#
# run-reduced-status.sh failed its canary: 6132 records read BASIC at the
# second pass against D134/D135's 5902 at the replay. The writer is
# src/presolve.c:2132 — JM_PS_SINGLETON_ROW's replay sets its own column
# BASIC, and that column can be one a SINGLETON_COL record restored earlier
# in the same LIFO walk. The swap's guard then reads the rewrite, not the
# recovery.
#
# So every SINGLETON_COL record is classified twice:
#   replay status (dg_colst, stored at the write)  -> the true population
#   second-pass status (what the guard reads)      -> what the swap acts on
#
# The true population (replay BASIC) gets the v1 classification, plus:
#   redact -> the REDUCED solve's activity against the REDUCED (widened) row
#             bounds, for surviving rows: was the nonbasic logical resting on
#             a widened bound (degenerate, legitimate) or off it?
# PHANTOM records (replay nonbasic, guard reads BASIC) get their own line;
# any phantom with swapped=1 means the swap took a logical out to pay for a
# column whose basis slot SINGLETON_ROW already paid with its own row.
#
# Predictions, stated before the run:
#   netlib true fired=5902 (the canary), phantom=230, X(basic->nonbasic)=0;
#   kennington true fired=482, phantom=0 or phantom-swapped=0 (under=0 there).
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-49-twopoint"
out="$here/two-point.txt"
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
static signed char *dg_redst;   /* reduced row status, -1 = did not survive */
static signed char *dg_colst;   /* singleton col status AT ITS REPLAY WRITE */
static unsigned char *dg_prevsw;
static long long dg_nofire, dg_fired, dg_phantom, dg_lost;
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
    dg_colst = malloc((size_t)orig->num_col);
    dg_prevsw = calloc((size_t)orig->num_row, 1);
    dg_nofire = 0; dg_fired = 0; dg_phantom = 0; dg_lost = 0;
    if (dg_colst != nullptr)
        memset(dg_colst, 0xFF, (size_t)orig->num_col);
    if (dg_redst != nullptr)
        for (int64_t dgi = 0; dgi < orig->num_row; dgi++)
            dg_redst[dgi] = (p->row_map[dgi] < 0)
                ? (signed char)-1
                : (signed char)red->sol_row_status[p->row_map[dgi]];
#endif""")

# The status the SINGLETON_COL replay itself decided, before anything later
# in the walk can rewrite it.
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
            const int replayst = (dg_colst != nullptr) ? dg_colst[dj] : -2;
            const int precol = (int)orig->sol_col_status[dj];
            const int prerow = (int)orig->sol_row_status[di];
#endif
            ps_singleton_col_swap(orig, rec);
#ifdef JAOS_DIAG
            if (dg_redst != nullptr && dg_colst != nullptr &&
                dg_prevsw != nullptr) {
                const int postrow = (int)orig->sol_row_status[di];
                const int swapped =
                    (prerow == JAOS_BASIS_BASIC && postrow != JAOS_BASIS_BASIC);
                const double dact = orig->sol_row[di];
                const char dtight = (dact == orig->row_lower[di]) ? 'L' :
                                    (dact == orig->row_upper[di]) ? 'U' : 'I';
                const int64_t dri = p->row_map[di];
                char redact = '-';
                if (dri >= 0) {
                    const double ra = red->sol_row[dri];
                    redact = (ra == red->row_lower[dri]) ? 'l' :
                             (ra == red->row_upper[dri]) ? 'u' : 'i';
                }
                if (replayst == JAOS_BASIS_BASIC && precol == JAOS_BASIS_BASIC) {
                    dg_fired++;
                    char dbuf[224];
                    int dk = snprintf(dbuf, sizeof dbuf,
                        "SC2 prerow=%d surv=%d redst=%d prevsw=%d swapped=%d "
                        "tight=%c redact=%c\\n",
                        prerow, dg_redst[di] >= 0 ? 1 : 0, (int)dg_redst[di],
                        (int)dg_prevsw[di], swapped, dtight, redact);
                    if (dk > 0 && dk < (int)sizeof dbuf) {
                        ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
                    }
                } else if (replayst != JAOS_BASIS_BASIC &&
                           precol == JAOS_BASIS_BASIC) {
                    dg_phantom++;
                    char dbuf[224];
                    int dk = snprintf(dbuf, sizeof dbuf,
                        "PHANTOM replayst=%d prerow=%d surv=%d redst=%d "
                        "swapped=%d tight=%c redact=%c\\n",
                        replayst, prerow, dg_redst[di] >= 0 ? 1 : 0,
                        (int)dg_redst[di], swapped, dtight, redact);
                    if (dk > 0 && dk < (int)sizeof dbuf) {
                        ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
                    }
                } else if (replayst == JAOS_BASIS_BASIC) {
                    dg_lost++;    /* replay BASIC, guard read nonbasic */
                } else {
                    dg_nofire++;
                }
                if (swapped)
                    dg_prevsw[di] = 1;
            }
#endif
        }
    }
#ifdef JAOS_DIAG
    {
        char sbuf[160];
        int sk = snprintf(sbuf, sizeof sbuf,
            "SC2TOT fired=%lld phantom=%lld lost=%lld nofire=%lld\\n",
            dg_fired, dg_phantom, dg_lost, dg_nofire);
        if (sk > 0 && sk < (int)sizeof sbuf) {
            ssize_t sw2 = write(2, sbuf, (size_t)sk); (void)sw2;
        }
        free(dg_redst); dg_redst = nullptr;
        free(dg_colst); dg_colst = nullptr;
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
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-2pt -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-2pt -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-2pt -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Two-point read. SC2 = replay-BASIC (the true population, canary 5902"
echo "# netlib / 482 kennington). PHANTOM = replay-nonbasic, guard read BASIC"
echo "# (rewritten by SINGLETON_ROW's replay, presolve.c:2132). lost = replay"
echo "# BASIC, guard read nonbasic (no known writer)."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    for tag in SC2 PHANTOM; do
        all=$(grep -c "$tag " "$d/$f.log"); ok=$(grep -c "^$tag " "$d/$f.log")
        [ "$all" = "$ok" ] || echo "*** $L $tag: $all lines, $ok intact — MANGLED ***"
    done
    echo "######## $L ########"
    awk '/^SC2 /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        if (v["swapped"] == 1) { SW++ }
        else if (v["prerow"] == 0) {
            if (v["tight"] == "I") LOOSE++; else ODD++
        } else if (v["surv"] == 0) { A++ }
        else if (v["redst"] == 0 && v["prevsw"] == 1) { C++ }
        else if (v["redst"] == 0) { D++ }
        else {
            B++; BRED[v["redst"]]++; BT[v["tight"]]++; BA[v["redact"]]++
        }
    }
    /^PHANTOM /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        P++; if (v["swapped"] == 1) PSW++
        PR[v["replayst"]]++; PT[v["tight"]]++
    }
    /^SC2TOT/{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        F += v["fired"]; PH += v["phantom"]; LO += v["lost"]; N += v["nofire"]
        solves++
    }
    END {
        printf "%d solves; true fired=%d (canary 5902/482) phantom=%d lost=%d nofire=%d\n\n",
               solves, F, PH, LO, N
        printf "TRUE population (column interior at its own replay):\n"
        printf "  %-52s %6d\n", "swapped — the exchange fired", SW
        printf "  %-52s %6d\n", "declined: partner in, row interior", LOOSE
        printf "  %-52s %6d\n", "declined: partner in, row tight, NOT swapped (odd)", ODD
        printf "  %-52s %6d\n", "declined: row did not survive (A)", A
        printf "  %-52s %6d\n", "declined: this family already swapped the row (C)", C
        printf "  %-52s %6d\n", "declined: survived, reduced BASIC, overwritten (D)", D
        printf "  %-52s %6d\n", "declined: reduced solve left it nonbasic (B)", B
        for (t in BRED) printf "      %-48s %6d\n", "reduced status " t, BRED[t]
        for (t in BT)   printf "      %-48s %6d\n", "final activity vs ORIGINAL bounds: " t, BT[t]
        for (t in BA)   printf "      %-48s %6d\n", "REDUCED activity vs WIDENED bounds: " t, BA[t]
        printf "\nPHANTOM (rewritten to BASIC by another family):\n"
        printf "  %-52s %6d\n", "records", P
        printf "  %-52s %6d\n", "of those, the swap FIRED — a logical removed", PSW
        for (t in PR) printf "      %-48s %6d\n", "column status at its own replay " t, PR[t]
        for (t in PT) printf "      %-48s %6d\n", "final row activity class " t, PT[t]
    }' "$d/$f.log"
    echo
done
echo "# predictions: nl true=5902, phantom=230, lost=0; any PHANTOM swapped>0"
echo "# removes a basis member SINGLETON_ROW already paid for (an under-count"
echo "# the published 48-solve over-count can mask)."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
