#!/bin/bash
# PINNED: 856ba2b -- this script is evidence for that commit's tree; its anchors do not
# match later trees. Re-run it in a worktree of that commit, not against HEAD.
# WHICH presolve family unbalances the published basic count?
#
# D131 measured the count wrong on 132 of netlib's 188 optimal solves and 24
# of Kennington's 32, worst error 12104, and showed presolve's basis MAPPING
# is exact. This asks the other end: postsolve.
#
# The arithmetic a correct postsolve satisfies. The reduced solve leaves
# exactly `rrow` basic variables, and `jm_postsolve_expand` copies those onto
# the surviving rows and columns. So the replay must add exactly `nr - rrow`
# basic variables among the entities it restores — one per row it brings back,
# no more and no less.
#
# Per record tag, then:
#
#     drift(tag) = basics it owns - rows it owns
#
# and the sum over tags is the published error. A tag that restores a row and
# marks both that row and its folded column BASIC contributes +1 every time it
# fires; a tag that restores a row and marks nothing basic contributes -1.
#
# Ownership is read off the arena rather than inferred: each record names the
# row and column it removed. Anything named by no record survived, and its
# status came from the reduced solve.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-42-attr"
out="$here/basis-attribution.txt"
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

# Placed after jaos_internal.h rather than before the first #include: the tag
# enum is declared there, and a helper inserted above it does not compile.
head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static const char *dg_tag(jm_presolve_tag t)
{
    switch (t) {
    case JM_PS_FIXED_COL:          return "FIXED_COL";
    case JM_PS_EMPTY_ROW:          return "EMPTY_ROW";
    case JM_PS_EMPTY_COL:          return "EMPTY_COL";
    case JM_PS_SINGLETON_ROW:      return "SINGLETON_ROW";
    case JM_PS_SINGLETON_COL:      return "SINGLETON_COL";
    case JM_PS_FREE_COL_SINGLETON: return "FREE_COL_SINGLETON";
    case JM_PS_REDUNDANT_ROW:      return "REDUNDANT_ROW";
    case JM_PS_FORCING_ROW:        return "FORCING_ROW";
    case JM_PS_IMPLIED_FREE_COL:   return "IMPLIED_FREE_COL";
    }
    return "UNKNOWN";
}
#endif
""" + head)

# After the whole replay, attribute every original row and column to the
# record that restored it, or to "survived".
anchor = """    for (int64_t i = 0; i < orig->num_row; i++)
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);"""
# The same two lines end both postsolve paths. The FIRST is
# jm_postsolve_expand, which every gate instance takes; the second is
# jm_postsolve_solved, which the comment above it says nothing in the three
# sets reaches.
assert s.count(anchor) == 2, s.count(anchor)
at = s.index(anchor)
s = s[:at] + """#ifdef JAOS_DIAG
    {
        /* owner[x] = arena index that restored x, or -1 for a survivor. */
        int64_t *orow = malloc((size_t)orig->num_row * sizeof *orow);
        int64_t *ocol = malloc((size_t)orig->num_col * sizeof *ocol);
        if (orow != nullptr && ocol != nullptr) {
            for (int64_t i = 0; i < orig->num_row; i++) orow[i] = -1;
            for (int64_t j = 0; j < orig->num_col; j++) ocol[j] = -1;
            /* Backwards, because the replay is strictly LIFO (D-07): it
             * walks r from arena_len-1 down to 0, so the LOWEST arena index
             * touching an entity is the one that writes its status last and
             * therefore owns what is published. Walking forwards records the
             * first writer instead, which is the opposite record. */
            for (int64_t r = p->arena_len - 1; r >= 0; r--) {
                const jm_presolve_rec *rec = &p->arena[r];
                switch (rec->tag) {
                case JM_PS_FIXED_COL:
                case JM_PS_EMPTY_COL:
                    if (rec->index >= 0 && rec->index < orig->num_col &&
                        p->col_map[rec->index] < 0) ocol[rec->index] = r;
                    break;
                case JM_PS_EMPTY_ROW:
                case JM_PS_REDUNDANT_ROW:
                case JM_PS_FORCING_ROW:
                    if (rec->index >= 0 && rec->index < orig->num_row &&
                        p->row_map[rec->index] < 0) orow[rec->index] = r;
                    break;
                case JM_PS_SINGLETON_ROW:
                    if (rec->index >= 0 && rec->index < orig->num_row &&
                        p->row_map[rec->index] < 0) orow[rec->index] = r;
                    break;
                case JM_PS_SINGLETON_COL:
                    if (rec->index2 >= 0 && rec->index2 < orig->num_col &&
                        p->col_map[rec->index2] < 0) ocol[rec->index2] = r;
                    break;
                case JM_PS_FREE_COL_SINGLETON:
                case JM_PS_IMPLIED_FREE_COL:
                    if (rec->index >= 0 && rec->index < orig->num_row &&
                        p->row_map[rec->index] < 0) orow[rec->index] = r;
                    if (rec->index2 >= 0 && rec->index2 < orig->num_col &&
                        p->col_map[rec->index2] < 0) ocol[rec->index2] = r;
                    break;
                }
            }
            /* Tally per tag, plus a bucket for anything no record claimed. */
            char buf[4096];
            int k = snprintf(buf, sizeof buf, "ATTR");
            int64_t surv_b = 0, surv_r = 0, orph_b = 0, orph_r = 0;
            for (int64_t i = 0; i < orig->num_row; i++) {
                const bool b = orig->sol_row_status[i] == JAOS_BASIS_BASIC;
                if (p->row_map[i] >= 0) { surv_r++; surv_b += b; }
                else if (orow[i] < 0)   { orph_r++; orph_b += b; }
            }
            for (int64_t j = 0; j < orig->num_col; j++) {
                const bool b = orig->sol_col_status[j] == JAOS_BASIS_BASIC;
                if (p->col_map[j] >= 0)      surv_b += b;
                else if (ocol[j] < 0)      { orph_b += b; }
            }
            static const jm_presolve_tag tags[] = {
                JM_PS_FIXED_COL, JM_PS_EMPTY_ROW, JM_PS_EMPTY_COL,
                JM_PS_SINGLETON_ROW, JM_PS_SINGLETON_COL,
                JM_PS_FREE_COL_SINGLETON, JM_PS_REDUNDANT_ROW,
                JM_PS_FORCING_ROW, JM_PS_IMPLIED_FREE_COL };
            for (size_t t = 0; t < sizeof tags / sizeof *tags; t++) {
                int64_t nb = 0, nr = 0, fired = 0, owned = 0, cbasic = 0;
                for (int64_t r = 0; r < p->arena_len; r++)
                    if (p->arena[r].tag == tags[t]) fired++;
                for (int64_t i = 0; i < orig->num_row; i++)
                    if (orow[i] >= 0 && p->arena[orow[i]].tag == tags[t]) {
                        nr++; owned++;
                        nb += orig->sol_row_status[i] == JAOS_BASIS_BASIC;
                    }
                for (int64_t j = 0; j < orig->num_col; j++)
                    if (ocol[j] >= 0 && p->arena[ocol[j]].tag == tags[t]) {
                        owned++;
                        cbasic += orig->sol_col_status[j] == JAOS_BASIS_BASIC;
                        nb += orig->sol_col_status[j] == JAOS_BASIS_BASIC;
                    }
                if (fired == 0) continue;
                /* basics / rows / drift / entities owned / of which columns
                 * that came out BASIC — the last one separates "this family
                 * marks its column basic" from "this family restores rows". */
                k += snprintf(buf + k, sizeof buf - (size_t)k,
                              " %s=%lld/%lld/%lld/%lld/%lld", dg_tag(tags[t]),
                              (long long)nb, (long long)nr,
                              (long long)(nb - nr), (long long)owned,
                              (long long)cbasic);
                if (k <= 0 || k >= (int)sizeof buf) break;
            }
            if (k > 0 && k < (int)sizeof buf)
                k += snprintf(buf + k, sizeof buf - (size_t)k,
                        " SURV=%lld/%lld ORPHAN=%lld/%lld num_row=%lld\\n",
                        (long long)surv_b, (long long)surv_r,
                        (long long)orph_b, (long long)orph_r,
                        (long long)orig->num_row);
            if (k > 0 && k < (int)sizeof buf) {
                fflush(stderr);
                ssize_t w = write(2, buf, (size_t)k);
                (void)w;
            }
        }
        free(orow); free(ocol);
    }
#endif
""" + s[at:]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-attr -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-attr -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-attr -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Which family unbalances the published basic count?"
echo "#"
echo "# Each entry is TAG=basics/rows/drift over the entities that tag owns."
echo "# A correct replay adds one basic per row it restores, so drift 0."
echo "# SURV is what the reduced solve contributed; ORPHAN is anything no"
echo "# record claimed, which should be nothing."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "ATTR" "$d/$f.log"); ok=$(grep -c "^ATTR" "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk -v L="$L" '/^ATTR/ {
        solves++
        for (i = 2; i <= NF; i++) {
            split($i, kv, "=")
            if (kv[1] == "num_row") { nrow = kv[2]; continue }
            split(kv[2], q, "/")
            if (kv[1] == "SURV")   { sb += q[1]; sr += q[2]; continue }
            if (kv[1] == "ORPHAN") { ob += q[1]; orr += q[2]; if (q[2] + 0 > 0) osolves++; continue }
            B[kv[1]] += q[1]; R[kv[1]] += q[2]; D[kv[1]] += q[3]
            O[kv[1]] += q[4]; C[kv[1]] += q[5]
            if (q[3] + 0 != 0) S[kv[1]]++
            F[kv[1]]++
        }
    }
    END {
        printf "%d solves\n", solves
        printf "%-20s %10s %10s %12s %10s %10s %10s\n", "family", "basics",
               "rows", "DRIFT", "owned", "cols BASIC", "solves off"
        for (t in D)
            printf "%-20s %10d %10d %12d %10d %10d %10d\n",
                   t, B[t], R[t], D[t], O[t], C[t], S[t]
        printf "%-20s %10d %10d %12s %10s\n", "survivors", sb, sr, "-", "-"
        printf "%-20s %10d %10d %12d %10d\n", "ORPHANED", ob, orr, ob - orr, osolves
    }' "$d/$f.log"
    echo
done
echo "# canary: solves > 0 says the observer ran; a probe that never ran and"
echo "# one where every family balances look alike otherwise."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
