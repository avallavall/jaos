#!/bin/bash
# WHICH record writes each published status last?
#
# D132 attributed every restored entity to the record that RESTORED it, read
# off the arena, and said plainly that this is not the record that WROTE its
# status last: `JM_PS_SINGLETON_ROW` assigns a status to a column a different
# record restored. Its row numbers stand for that reason — no family writes a
# row another family restored — and its per-family column split does not.
#
# This settles the split. Every write to `sol_col_status` and `sol_row_status`
# inside `ps_replay_one` is rewritten to go through a helper that records the
# arena index doing the writing, at the moment it writes. Conditional branches
# stop mattering: a case that does not take its branch does not record.
#
# The tally is then exact:
#
#     drift(tag) = basics whose LAST writer carries that tag
#                - rows whose last writer carries that tag
#
# 14 write sites, all with a simple `[i]` or `[j]` index, so the rewrite is a
# regex over the function body and nothing else in the file is touched.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-43-lw"
out="$here/last-writer.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done
mkdir -p build/diag

python3 - "$wt" << 'PY'
import re, sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

# The helper, after jaos_internal.h so the tag enum exists.
head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
static int64_t dg_rec = -1;          /* arena index currently replaying */
static int64_t *dg_wrow, *dg_wcol_a; /* last writer per row / per column */
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
/* Returns the slot so the call site stays an assignment, and stamps the
 * writer on the way. */
static jaos_basis_status *dg_wcol(jaos_model *o, int64_t j)
{
    if (dg_wcol_a != nullptr) dg_wcol_a[j] = dg_rec;
    return &o->sol_col_status[j];
}
static jaos_basis_status *dg_wrowf(jaos_model *o, int64_t i)
{
    if (dg_wrow != nullptr) dg_wrow[i] = dg_rec;
    return &o->sol_row_status[i];
}
#endif
""" + head)

# Rewrite the writes, inside ps_replay_one's body only.
a = s.index("static void ps_replay_one(")
b = s.index("jm_postsolve_expand(jm_presolve *p)")
body = s[a:b]
n1 = len(re.findall(r"orig->sol_col_status\[([a-z]+)\]", body))
n2 = len(re.findall(r"orig->sol_row_status\[([a-z]+)\]", body))
assert n1 == 6 and n2 == 8, (n1, n2)
body = re.sub(r"orig->sol_col_status\[([a-z]+)\]",
              r"(*dg_wcol(orig, \1))", body)
body = re.sub(r"orig->sol_row_status\[([a-z]+)\]",
              r"(*dg_wrowf(orig, \1))", body)
# Guarded so a non-diag build is byte-identical to HEAD.
body = body.replace("(*dg_wcol(orig, ", "(*DG_WCOL(orig, ")
body = body.replace("(*dg_wrowf(orig, ", "(*DG_WROW(orig, ")
s = s[:a] + body + s[b:]
s = s.replace("static void ps_replay_one(", """#ifdef JAOS_DIAG
#define DG_WCOL(o, x) dg_wcol((o), (x))
#define DG_WROW(o, x) dg_wrowf((o), (x))
#else
#define DG_WCOL(o, x) (&(o)->sol_col_status[(x)])
#define DG_WROW(o, x) (&(o)->sol_row_status[(x)])
#endif
static void ps_replay_one(""", 1)

# Stamp the record index before each replay, and report after the loop.
loop = """    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);"""
assert s.count(loop) == 2, s.count(loop)
at = s.index(loop)
s = s[:at] + """#ifdef JAOS_DIAG
    dg_wrow = malloc((size_t)orig->num_row * sizeof *dg_wrow);
    dg_wcol_a = malloc((size_t)orig->num_col * sizeof *dg_wcol_a);
    if (dg_wrow != nullptr)
        for (int64_t i = 0; i < orig->num_row; i++) dg_wrow[i] = -1;
    if (dg_wcol_a != nullptr)
        for (int64_t j = 0; j < orig->num_col; j++) dg_wcol_a[j] = -1;
    for (int64_t r = p->arena_len - 1; r >= 0; r--) {
        dg_rec = r;
        ps_replay_one(orig, p, r, rowc);
    }
    dg_rec = -1;
    {
        char buf[4096];
        int k = snprintf(buf, sizeof buf, "LW");
        static const jm_presolve_tag tags[] = {
            JM_PS_FIXED_COL, JM_PS_EMPTY_ROW, JM_PS_EMPTY_COL,
            JM_PS_SINGLETON_ROW, JM_PS_SINGLETON_COL,
            JM_PS_FREE_COL_SINGLETON, JM_PS_REDUNDANT_ROW,
            JM_PS_FORCING_ROW, JM_PS_IMPLIED_FREE_COL };
        for (size_t t = 0; t < sizeof tags / sizeof *tags; t++) {
            int64_t nb = 0, nr = 0, fired = 0;
            for (int64_t r = 0; r < p->arena_len; r++)
                if (p->arena[r].tag == tags[t]) fired++;
            if (fired == 0) continue;
            for (int64_t i = 0; i < orig->num_row; i++)
                if (dg_wrow != nullptr && dg_wrow[i] >= 0 &&
                    p->arena[dg_wrow[i]].tag == tags[t]) {
                    nr++;
                    nb += orig->sol_row_status[i] == JAOS_BASIS_BASIC;
                }
            for (int64_t j = 0; j < orig->num_col; j++)
                if (dg_wcol_a != nullptr && dg_wcol_a[j] >= 0 &&
                    p->arena[dg_wcol_a[j]].tag == tags[t])
                    nb += orig->sol_col_status[j] == JAOS_BASIS_BASIC;
            k += snprintf(buf + k, sizeof buf - (size_t)k, " %s=%lld/%lld/%lld",
                          dg_tag(tags[t]), (long long)nb, (long long)nr,
                          (long long)(nb - nr));
            if (k <= 0 || k >= (int)sizeof buf) break;
        }
        /* Never written by any record: the survivors, whose status the
         * reduced solve set. Rows only for the row count. */
        int64_t ub = 0, ur = 0;
        for (int64_t i = 0; i < orig->num_row; i++)
            if (dg_wrow == nullptr || dg_wrow[i] < 0) {
                ur++;
                ub += orig->sol_row_status[i] == JAOS_BASIS_BASIC;
            }
        for (int64_t j = 0; j < orig->num_col; j++)
            if (dg_wcol_a == nullptr || dg_wcol_a[j] < 0)
                ub += orig->sol_col_status[j] == JAOS_BASIS_BASIC;
        int64_t tb = 0;
        for (int64_t i = 0; i < orig->num_row; i++)
            tb += orig->sol_row_status[i] == JAOS_BASIS_BASIC;
        for (int64_t j = 0; j < orig->num_col; j++)
            tb += orig->sol_col_status[j] == JAOS_BASIS_BASIC;
        if (k > 0 && k < (int)sizeof buf)
            k += snprintf(buf + k, sizeof buf - (size_t)k,
                    " UNWRITTEN=%lld/%lld TOTAL=%lld num_row=%lld\\n",
                    (long long)ub, (long long)ur, (long long)tb,
                    (long long)orig->num_row);
        if (k > 0 && k < (int)sizeof buf) {
            fflush(stderr);
            ssize_t w = write(2, buf, (size_t)k);
            (void)w;
        }
    }
    free(dg_wrow); free(dg_wcol_a); dg_wrow = nullptr; dg_wcol_a = nullptr;
#else
""" + loop + """
#endif
""" + s[at + len(loop):]
open(p, "w", encoding="utf-8").write(s)
print("presolve.c instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-lw -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-lw -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-lw -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Which record wrote each published status LAST?"
echo "#"
echo "# TAG=basics/rows/drift, attributed at the moment of the write, so a"
echo "# branch not taken records nothing. UNWRITTEN is what no record touched"
echo "# — the survivors, whose status the reduced solve set."
echo "#"
echo "# A correct replay adds one basic per row it writes, so drift 0."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "LW " "$d/$f.log"); ok=$(grep -c "^LW " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk -v L="$L" '/^LW /{
        solves++
        for (i = 2; i <= NF; i++) {
            split($i, kv, "=")
            if (kv[1] == "num_row") { NR_ += kv[2]; continue }
            if (kv[1] == "TOTAL")   { TB += kv[2]; continue }
            split(kv[2], q, "/")
            if (kv[1] == "UNWRITTEN") { ub += q[1]; ur += q[2]; continue }
            B[kv[1]] += q[1]; R[kv[1]] += q[2]; D[kv[1]] += q[3]
            if (q[3] + 0 != 0) S[kv[1]]++
        }
    }
    END {
        printf "%d solves\n", solves
        printf "%-20s %10s %10s %12s %10s\n", "last writer", "basics", "rows", "DRIFT", "solves off"
        for (t in D)
            printf "%-20s %10d %10d %12d %10d\n", t, B[t], R[t], D[t], S[t]
        printf "%-20s %10d %10d %12d\n", "UNWRITTEN (survivors)", ub, ur, ub - ur
        printf "\npublished basics %d against num_row %d  ->  off by %d\n",
               TB, NR_, TB - NR_
    }' "$d/$f.log"
    echo
done
echo "# canary: solves > 0 says the observer ran, and the off-by line must"
echo "# equal the sum of the drifts above it or the attribution is incomplete."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
