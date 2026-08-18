#!/bin/bash
# WHY do six instances publish more basic variables than rows?
#
# D130 named them: 80bau3b over by 21, finnis 12, standmps 11, standata 10,
# vtp-base 2, boeing1 1. `src/presolve.c` maps a starting basis into reduced
# indices at line 1641, and its comment contemplates only one direction:
#
#     "A removed row or column recorded basic has no reduced counterpart to
#      carry that status; dropping it UNDERCOUNTS the basic total, and
#      build_warm_basis already falls back ... safe, never wrong, only colder"
#
# Over-counting is not contemplated anywhere. The arithmetic says how it
# happens. A valid basis on the original has `basic == nr`, so after mapping:
#
#     nbasic - rrow = rows_removed - dropped_basic - singleton_adjustments
#
# where `dropped_basic` counts entities whose status was BASIC and whose map
# is negative, and `singleton_adjustments` counts the JM_PS_SINGLETON_ROW pass
# turning a BASIC column into a bound. **Removing a row whose logical was
# NONBASIC costs a basis position and no basic variable**, so every such row
# pushes the count over.
#
# This measures each term and checks the identity holds, which is what turns
# the reasoning above into a measurement.
#
# src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-40-ledger"
out="$here/overcount-ledger.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

python3 - "$wt" << 'PY'
import sys
root = sys.argv[1]

p = root + "/src/presolve.c"
s = open(p, encoding="utf-8").read()

# The ledger, counted where the map is applied.
a = """        for (int64_t j = 0; j < nc; j++) {
            const int64_t rjj = p->col_map[j];
            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];
        }
        for (int64_t i = 0; i < nr; i++) {
            const int64_t rii = p->row_map[i];
            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];
        }"""
assert s.count(a) == 1
s = s.replace(a, """#ifdef JAOS_DIAG
        dg_basic_in = dg_drop_col = dg_drop_row = dg_adj = 0;
        for (int64_t j = 0; j < nc; j++)
            if (m->start_col_status[j] == JAOS_BASIS_BASIC) dg_basic_in++;
        for (int64_t i = 0; i < nr; i++)
            if (m->start_row_status[i] == JAOS_BASIS_BASIC) dg_basic_in++;
#endif
""" + a.replace("""            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];""",
"""            if (rjj >= 0)
                p->reduced.start_col_status[rjj] = m->start_col_status[j];
#ifdef JAOS_DIAG
            else if (m->start_col_status[j] == JAOS_BASIS_BASIC) dg_drop_col++;
#endif""").replace("""            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];""",
"""            if (rii >= 0)
                p->reduced.start_row_status[rii] = m->start_row_status[i];
#ifdef JAOS_DIAG
            else if (m->start_row_status[i] == JAOS_BASIS_BASIC) dg_drop_row++;
#endif"""))

# The singleton-row pass turns a BASIC column into a bound; count only the
# times it actually changed a BASIC one, which is what moves the total.
b = """                p->reduced.start_col_status[rjj] =
                    x_at_lo ? JAOS_BASIS_AT_LOWER : JAOS_BASIS_AT_UPPER;"""
assert s.count(b) == 1
s = s.replace(b, """#ifdef JAOS_DIAG
                if (p->reduced.start_col_status[rjj] == JAOS_BASIS_BASIC)
                    dg_adj++;
#endif
""" + b)

c = """                if (m->sol_col[j] == cur_cl[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_LOWER;
                else if (m->sol_col[j] == cur_cu[j])
                    p->reduced.start_col_status[rjj] = JAOS_BASIS_AT_UPPER;"""
assert s.count(c) == 1
s = s.replace(c, """#ifdef JAOS_DIAG
                if (p->reduced.start_col_status[rjj] == JAOS_BASIS_BASIC &&
                    (m->sol_col[j] == cur_cl[j] || m->sol_col[j] == cur_cu[j]))
                    dg_adj++;
#endif
""" + c)

# Report where the mapping block ends.
d = """    p->outcome = (rcol == 0) ? JM_PRESOLVE_SOLVED : JM_PRESOLVE_REDUCED;"""
assert s.count(d) == 1
s = s.replace(d, """#ifdef JAOS_DIAG
    if (m->start_col_status != nullptr && m->start_row_status != nullptr) {
        int64_t dg_out = 0;
        for (int64_t j = 0; j < rcol; j++)
            if (p->reduced.start_col_status[j] == JAOS_BASIS_BASIC) dg_out++;
        for (int64_t i = 0; i < rrow; i++)
            if (p->reduced.start_row_status[i] == JAOS_BASIS_BASIC) dg_out++;
        fprintf(stderr, "LEDGER nr=%lld rrow=%lld rows_removed=%lld "
                "basic_in=%lld drop_row=%lld drop_col=%lld adj=%lld "
                "nbasic=%lld over=%lld\\n",
                (long long)nr, (long long)rrow, (long long)(nr - rrow),
                (long long)dg_basic_in, (long long)dg_drop_row,
                (long long)dg_drop_col, (long long)dg_adj,
                (long long)dg_out, (long long)(dg_out - rrow));
    }
#endif
""" + d)
open(p, "w", encoding="utf-8").write(s)

# The counters live at file scope: the mapping block that fills them and the
# report that reads them are two different scopes.
w = open(p, encoding="utf-8").read()
# Inserted before the file's own first #include, so the two headers this
# needs come with it rather than being assumed.
w = w.replace("#include", """#ifdef JAOS_DIAG
#include <stdio.h>
#include <stdint.h>
static int64_t dg_basic_in, dg_drop_col, dg_drop_row, dg_adj;
#endif
#include""", 1)
open(p, "w", encoding="utf-8").write(w)

# The driver names the instance immediately before the WARM solve — the FIRST
# `st = jaos_solve(m);`; the second is the cold one (D130).
r = root + "/bench/warm.c"
w = open(r, encoding="utf-8").read()
anchor = "    st = jaos_solve(m);\n"
assert w.count(anchor) == 2, w.count(anchor)
first = w.index(anchor)
w = w[:first] + '#ifdef JAOS_DIAG\n    fprintf(stderr, "WARMINST %s\\n", r->name);\n#endif\n' + w[first:]
open(r, "w", encoding="utf-8").write(w)
print("instrumented")
PY
[ $? -eq 0 ] || exit 2

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG -DJAOS_DIAG \
    -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-ledger -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/warm-ledger -j 1 -o "$d/nl.txt" > "$d/nl.log" 2>&1

{
echo "# Where does an over-count come from?"
echo "#"
echo "# The identity, if the reasoning is right:"
echo "#   over = rows_removed - drop_row - drop_col - adj"
echo "# A row removed whose logical was NONBASIC costs a basis position and"
echo "# no basic variable, so it pushes the count over."
echo
awk '/^WARMINST/ { inst = $2; armed = 1; next }
     /^LEDGER/ {
        if (!armed) next
        armed = 0
        delete v
        n = split($0, F, " ")
        for (i = 2; i <= n; i++) { split(F[i], kv, "="); v[kv[1]] = kv[2] }
        seen++
        # The mapping identity, stated on what the mapping actually reads:
        #   nbasic_out = basic_in - drop_row - drop_col - adj
        pred = v["basic_in"] - v["drop_row"] - v["drop_col"] - v["adj"]
        ok = (pred == v["nbasic"] + 0) ? "" : "   *** MAPPING IDENTITY FAILS ***"
        if (ok != "") bad++
        # And the question this relocates: was the count already wrong on the
        # callers own model, before presolve mapped anything? A valid basis
        # there has exactly nr basic variables.
        pre = v["basic_in"] - v["nr"]
        if (pre != 0) prebad++
        if (v["over"] + 0 > 0) {
            printf "OVER  %-11s over=%-4s  nbasic=%-6s rrow=%-6s\n",
                   inst, v["over"], v["nbasic"], v["rrow"]
            printf "      %-11s mapping: basic_in %-6s - drop_row %-4s - drop_col %-4s - adj %-4s = %s%s\n",
                   "", v["basic_in"], v["drop_row"], v["drop_col"], v["adj"], pred, ok
            printf "      %-11s ON THE CALLERS OWN MODEL: basic_in %s vs nr %s  -> off by %d\n",
                   "", v["basic_in"], v["nr"], pre
        }
        else if (v["over"] + 0 < 0) short++
        else acc++
     }
     END {
        printf "\nsolves with a ledger=%d  over=%d  short=%d  exact=%d\n",
               seen, seen - short - acc, short, acc
        printf "mapping identity failures: %d of %d\n", bad, seen
        printf "SOLVES WHERE THE CALLERS OWN BASIS COUNT IS ALREADY WRONG: %d of %d\n",
               prebad, seen
     }' "$d/nl.log"
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
