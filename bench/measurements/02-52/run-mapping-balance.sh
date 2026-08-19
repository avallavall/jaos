#!/bin/bash
# The mapping's balance, per family, with a closing sum — D143 ordered this
# derivation before any repair is built.
#
# For every warm solve that maps a stored basis, decompose the mapped basic
# count exactly:
#
#   redbasic = origbasic - dropped_basic_cols - dropped_basic_rows
#              - demoted_by_correction + promoted_by_correction
#
# where dropped members are stored-BASIC columns/rows presolve removes this
# run (attributed to the removing family via one arena pass), and the
# correction terms are measured by snapshotting the naive per-index copy and
# diffing it against the corrected reduced start. A solve where the identity
# does not close exactly is counted as BROKEN and the probe's own numbers
# are then not to be believed.
#
# Also counted: the un-swap candidates the D143 repair would promote — a
# SINGLETON_COL record whose stored column is BASIC and removed this run,
# whose row survives with a stored-nonbasic logical mapped through. If
# cands per solve == the solve's shortfall (rrow - redbasic) on the 35+5
# cold fallbacks, the single-family repair closes the whole gap; any
# remainder names the other families' share.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-52-bal"
out="$here/mapping-balance.txt"
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
static signed char *dg_precorr_col;
static signed char *dg_precorr_row;
#endif
""" + head)

# Snapshot the naive per-index copy, before the correction passes run.
naive = """        for (int64_t i = 0; i < nr; i++) {
            const int64_t rii = p->row_map[i];
            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];
        }"""
assert s.count(naive) == 1
s = s.replace(naive, naive + """
#ifdef JAOS_DIAG
        free(dg_precorr_col); free(dg_precorr_row);
        dg_precorr_col = malloc((size_t)(rcol > 0 ? rcol : 1));
        dg_precorr_row = malloc((size_t)(rrow > 0 ? rrow : 1));
        if (dg_precorr_col != nullptr && dg_precorr_row != nullptr) {
            for (int64_t dgi = 0; dgi < rcol; dgi++)
                dg_precorr_col[dgi] =
                    (signed char)p->reduced.start_col_status[dgi];
            for (int64_t dgi = 0; dgi < rrow; dgi++)
                dg_precorr_row[dgi] =
                    (signed char)p->reduced.start_row_status[dgi];
        }
#endif""")

anchor = """    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;"""
assert s.count(anchor) == 1
s = s.replace(anchor, """#ifdef JAOS_DIAG
    if (m->start_col_status != nullptr && m->start_row_status != nullptr &&
        dg_precorr_col != nullptr && dg_precorr_row != nullptr) {
        /* One arena pass: who removed each column and row this run. */
        signed char *crm = malloc((size_t)(nc > 0 ? nc : 1));
        signed char *rrm = malloc((size_t)(nr > 0 ? nr : 1));
        if (crm != nullptr && rrm != nullptr) {
            for (int64_t dgi = 0; dgi < nc; dgi++) crm[dgi] = -1;
            for (int64_t dgi = 0; dgi < nr; dgi++) rrm[dgi] = -1;
            for (int64_t r2 = 0; r2 < p->arena_len; r2++) {
                const jm_presolve_rec *rq = &p->arena[r2];
                switch (rq->tag) {
                case JM_PS_FIXED_COL: case JM_PS_EMPTY_COL:
                    crm[rq->index] = (signed char)rq->tag; break;
                case JM_PS_SINGLETON_COL:
                    crm[rq->index2] = (signed char)rq->tag; break;
                case JM_PS_EMPTY_ROW: case JM_PS_SINGLETON_ROW:
                case JM_PS_REDUNDANT_ROW: case JM_PS_FORCING_ROW:
                    rrm[rq->index] = (signed char)rq->tag; break;
                case JM_PS_FREE_COL_SINGLETON: case JM_PS_IMPLIED_FREE_COL:
                    rrm[rq->index] = (signed char)rq->tag;
                    crm[rq->index2] = (signed char)rq->tag; break;
                }
            }
            long long ob = 0, rb = 0, dc[9] = {0}, drB[9] = {0}, drN[9] = {0};
            long long dem = 0, pro = 0, cand = 0, orph = 0;
            for (int64_t j2 = 0; j2 < nc; j2++) {
                ob += m->start_col_status[j2] == JAOS_BASIS_BASIC;
                if (p->col_map[j2] < 0 &&
                    m->start_col_status[j2] == JAOS_BASIS_BASIC) {
                    if (crm[j2] >= 0) dc[(int)crm[j2]]++; else orph++;
                }
            }
            for (int64_t i2 = 0; i2 < nr; i2++) {
                ob += m->start_row_status[i2] == JAOS_BASIS_BASIC;
                if (p->row_map[i2] < 0) {
                    const int t = crm != nullptr && rrm[i2] >= 0
                                      ? (int)rrm[i2] : -1;
                    if (m->start_row_status[i2] == JAOS_BASIS_BASIC) {
                        if (t >= 0) drB[t]++; else orph++;
                    } else {
                        if (t >= 0) drN[t]++; else orph++;
                    }
                }
            }
            for (int64_t dgi = 0; dgi < rcol; dgi++) {
                const int was = dg_precorr_col[dgi];
                const int now = (int)p->reduced.start_col_status[dgi];
                if (was == JAOS_BASIS_BASIC && now != JAOS_BASIS_BASIC) dem++;
                if (was != JAOS_BASIS_BASIC && now == JAOS_BASIS_BASIC) pro++;
                rb += now == JAOS_BASIS_BASIC;
            }
            for (int64_t dgi = 0; dgi < rrow; dgi++) {
                const int was = dg_precorr_row[dgi];
                const int now = (int)p->reduced.start_row_status[dgi];
                if (was == JAOS_BASIS_BASIC && now != JAOS_BASIS_BASIC) dem++;
                if (was != JAOS_BASIS_BASIC && now == JAOS_BASIS_BASIC) pro++;
                rb += now == JAOS_BASIS_BASIC;
            }
            for (int64_t r2 = 0; r2 < p->arena_len; r2++) {
                const jm_presolve_rec *rq = &p->arena[r2];
                if (rq->tag != JM_PS_SINGLETON_COL) continue;
                const int64_t j2 = rq->index2;
                if (p->col_map[j2] >= 0) continue;
                if (m->start_col_status[j2] != JAOS_BASIS_BASIC) continue;
                const int64_t ri2 = p->row_map[rq->index];
                if (ri2 >= 0 &&
                    p->reduced.start_row_status[ri2] != JAOS_BASIS_BASIC)
                    cand++;
            }
            long long dctot = 0, drBtot = 0;
            for (int t2 = 0; t2 < 9; t2++) { dctot += dc[t2]; drBtot += drB[t2]; }
            const long long pred = ob - dctot - drBtot - dem + pro;
            char dbuf[420];
            int dk = snprintf(dbuf, sizeof dbuf,
                "WBAL ob=%lld nr=%lld rrow=%lld rb=%lld pred=%lld "
                "dcFIX=%lld dcEMP=%lld dcSC=%lld dcFCS=%lld dcIFC=%lld "
                "drB3=%lld drB6=%lld drB7=%lld drBo=%lld "
                "drN3=%lld drN6=%lld drN7=%lld drNo=%lld "
                "dem=%lld pro=%lld cand=%lld orph=%lld\\n",
                ob, (long long)nr, (long long)rrow, rb, pred,
                dc[0], dc[2], dc[4], dc[5], dc[8],
                drB[3], drB[6], drB[7], drB[1] + drB[5] + drB[8],
                drN[3], drN[6], drN[7], drN[1] + drN[5] + drN[8],
                dem, pro, cand, orph);
            if (dk > 0 && dk < (int)sizeof dbuf) {
                ssize_t dw = write(2, dbuf, (size_t)dk); (void)dw;
            }
        }
        free(crm); free(rrm);
        free(dg_precorr_col); dg_precorr_col = nullptr;
        free(dg_precorr_row); dg_precorr_row = nullptr;
    }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-bal -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-bal -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/warm-bal -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Exact decomposition of every mapped stored basis. pred must equal rb"
echo "# on every line or the decomposition itself is wrong (BROKEN)."
echo "# Shortfall S = rrow - rb. cand = un-swap promotions available."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "WBAL " "$d/$f.log"); ok=$(grep -c "^WBAL " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^WBAL /{
        delete v; for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] = kv[2] }
        n++
        if (v["pred"] != v["rb"]) broken++
        S = v["rrow"] - v["rb"]
        if (S > 0) { short++; Stot += S; cands += v["cand"]
                     if (v["cand"] == S) closed++
                     else if (v["cand"] > S) over++
                     else under++ }
        for (k in v) if (k ~ /^(dc|dr)/) T[k] += v[k]
        DEM += v["dem"]; PRO += v["pro"]; ORPH += v["orph"]
    }
    END {
        printf "%d mapped solves; DECOMPOSITION BROKEN on %d (must be 0)\n\n", n, broken
        printf "  %-46s %6d\n", "solves mapped short (S > 0)", short
        printf "  %-46s %6d\n", "  total shortfall", Stot
        printf "  %-46s %6d\n", "  un-swap candidates on those solves", cands
        printf "  %-46s %6d\n", "  solves where cand == S (repair closes)", closed
        printf "  %-46s %6d\n", "  solves where cand < S (remainder)", under
        printf "  %-46s %6d\n", "  solves where cand > S (would overshoot)", over
        printf "\n  dropped stored-BASIC columns by remover:\n"
        printf "    FIXED=%d EMPTY=%d SINGLETON_COL=%d FREE_CS=%d IMPLIED_FREE=%d\n",
               T["dcFIX"], T["dcEMP"], T["dcSC"], T["dcFCS"], T["dcIFC"]
        printf "  dropped stored-BASIC rows by remover (net 0 each):\n"
        printf "    SROW=%d REDUND=%d FORCING=%d other=%d\n",
               T["drB3"], T["drB6"], T["drB7"], T["drBo"]
        printf "  dropped stored-NONBASIC rows (each makes the map LONGER):\n"
        printf "    SROW=%d REDUND=%d FORCING=%d other=%d\n",
               T["drN3"], T["drN6"], T["drN7"], T["drNo"]
        printf "  correction pass: demoted=%d promoted=%d, orphans=%d\n",
               DEM, PRO, ORPH
    }' "$d/$f.log"
    echo
done
echo "# prediction: BROKEN=0 (else nothing here is evidence); the shortfall"
echo "# is dominated by dcSC (dropped basic singleton columns); cand == S on"
echo "# most short solves, and the remainder names the next family."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
