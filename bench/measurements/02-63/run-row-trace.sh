#!/bin/bash
# Who writes pilotnov's row 931, in what order, and with what?
#
# D153 localised the defect to a quantity: the published activity of an
# equality row at zero reads 0.0 while its three columns make -1.93e-07. That
# is a symptom. This asks which producer put the 0.0 there, which is the
# defect.
#
# Every write to sol_row[TARGET] is logged with its producer and value — a
# trajectory rather than a snapshot, per jaos-debug. The row's own columns and
# their published values are dumped at the end, so the -1.93e-07 can be
# attributed to a term rather than inferred.
#
# JAOS_DIAG guards every hook; the release build cannot change. src/ is read
# and never written — the patch lives in a throwaway worktree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-63"
out="$here/row-trace.txt"
TARGET=${1:-931}
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

python3 - "$wt" "$TARGET" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
target = sys.argv[2]
s = open(p, encoding="utf-8").read()

head = "static double ps_published(double v)"
assert s.count(head) == 1
s = s.replace(head, """#ifdef JAOS_DIAG
#include <stdio.h>
#include <unistd.h>
#define DG_ROW """ + target + """
static void dg_say(const char *who, long long i, double v, double extra)
{
    if (i != DG_ROW) return;
    char b[256];
    int k = snprintf(b, sizeof b, "TRACE %-22s row=%lld value=%.17g extra=%.17g\\n",
                     who, i, v, extra);
    if (k > 0 && k < (int)sizeof b) { ssize_t w = write(2, b, (size_t)k); (void)w; }
}
#endif
""" + head)

# 1. the compensated accumulator
add = """static void ps_row_add(jaos_model *orig, double *rowc, int64_t i, double t)
{"""
assert s.count(add) == 1
s = s.replace(add, add + """
#ifdef JAOS_DIAG
    dg_say("ps_row_add", (long long)i, orig->sol_row[i] + t, t);
#endif""")

# 2. the empty-row assignment
er = "        orig->sol_row[i] = 0.0;"
assert s.count(er) == 1, "empty-row assignment not found"
s = s.replace(er, """#ifdef JAOS_DIAG
        dg_say("EMPTY_ROW assign", (long long)i, 0.0, orig->sol_row[i]);
#endif
""" + er)

# 3. the singleton-row assignment
sr = "        orig->sol_row[i] = ps_published(rec->coef * xv);"
assert s.count(sr) == 1, "singleton-row assignment not found"
s = s.replace(sr, """#ifdef JAOS_DIAG
        dg_say("SINGLETON_ROW assign", (long long)i, rec->coef * xv,
               orig->sol_row[i]);
#endif
""" + sr)

# 4. the copy from the reduced solve
cp = "        orig->sol_row[i] = red->sol_row[ri];"
assert s.count(cp) == 1, "reduced-solve copy not found"
s = s.replace(cp, """#ifdef JAOS_DIAG
        dg_say("copy from reduced", (long long)i, red->sol_row[ri], (double)ri);
#endif
""" + cp)

# 5. the final fold. The loop body is a single statement with no braces, so
#    the whole loop is replaced rather than prefixed — prefixing puts the
#    assignment outside the loop and does not compile.
fold = """    for (int64_t i = 0; i < orig->num_row; i++)
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);"""
n = s.count(fold)
assert n == 2, f"expected 2 fold sites, found {n}"
s = s.replace(fold, """    for (int64_t i = 0; i < orig->num_row; i++) {
#ifdef JAOS_DIAG
        dg_say("fold sum+carry", (long long)i, orig->sol_row[i] + rowc[i],
               rowc[i]);
#endif
        orig->sol_row[i] = ps_published(orig->sol_row[i] + rowc[i]);
    }""")

# 6. at the very end, the row's columns and their published values
anchor = "    (void)jm_model_remember_basis(orig);\n    return JAOS_OK;\n}"
n = s.count(anchor)
assert n >= 1, "end-of-postsolve anchor not found"
s = s.replace(anchor, """#ifdef JAOS_DIAG
    {
        double sum = 0.0;
        for (int64_t j = 0; j < orig->num_col; j++)
            for (int64_t k = orig->a_start[j]; k < orig->a_start[j+1]; k++)
                if (orig->a_index[k] == DG_ROW) {
                    const double t = orig->a_value[k] * orig->sol_col[j];
                    sum += t;
                    char b[300];
                    int kk = snprintf(b, sizeof b,
                        "TERM col=%lld a=%.17g x=%.17g term=%.17g "
                        "colstat=%d collo=%.17g colhi=%.17g\\n",
                        (long long)j, orig->a_value[k], orig->sol_col[j], t,
                        (int)orig->sol_col_status[j],
                        orig->col_lower[j], orig->col_upper[j]);
                    if (kk > 0 && kk < (int)sizeof b) {
                        ssize_t w = write(2, b, (size_t)kk); (void)w;
                    }
                }
        char b2[220];
        int k2 = snprintf(b2, sizeof b2,
            "FINAL row=%d published=%.17g recomputed=%.17g rowlo=%.17g "
            "rowhi=%.17g dual=%.17g\\n",
            DG_ROW, orig->sol_row[DG_ROW], sum,
            orig->row_lower[DG_ROW], orig->row_upper[DG_ROW],
            orig->sol_dual != NULL ? orig->sol_dual[DG_ROW] : 0.0);
        if (k2 > 0 && k2 < (int)sizeof b2) {
            ssize_t w = write(2, b2, (size_t)k2); (void)w;
        }
    }
#endif
""" + anchor)
open(p, "w", encoding="utf-8").write(s)
print(f"instrumented for row {target}")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -g -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-trace -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
{
echo "# Every write to pilotnov's sol_row[$TARGET], in order, with its producer."
echo "# Then the row's own columns and the sum they make."
echo
./build/diag/run-trace -j 1 pilotnov > "$d/pn.log" 2>&1
echo "---- the writes, in order ----"
grep "^TRACE " "$d/pn.log"
echo
echo "---- how many of each producer ----"
grep "^TRACE " "$d/pn.log" | awk '{print $2, $3}' | sort | uniq -c | sort -rn
echo
echo "---- the row's terms ----"
grep "^TERM " "$d/pn.log"
echo
grep "^FINAL " "$d/pn.log"
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
