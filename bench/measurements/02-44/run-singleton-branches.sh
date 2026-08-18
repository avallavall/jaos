#!/bin/bash
# Which branch of each singleton family costs the basis position?
#
# D133 closed the attribution: `SINGLETON_COL` and `SINGLETON_ROW` are the
# whole of the published basic-count error, +5902/-1998 on netlib and
# +482/+25172 on Kennington, and the sum equals the published error exactly.
# `SINGLETON_ROW` changes sign between the sets, so a repair has to handle
# both directions and the branches have to be counted first.
#
# SINGLETON_ROW restores one row and writes two statuses by two INDEPENDENT
# tests (`src/presolve.c`, the JM_PS_SINGLETON_ROW case):
#
#   the column  BASIC only when `!zero_works && this_row_owns`
#   the row     AT_LOWER / AT_UPPER when the activity lands exactly on a
#               bound, BASIC otherwise
#
# Nothing makes the pair sum to one, and one is what a restored row is worth:
#
#   drift per firing = (row BASIC) + (column set BASIC) - 1
#
# so the four combinations are -1, 0, 0 and +1. SINGLETON_COL is simpler: it
# restores a column, its row survives, and it writes
# nonbasic when the value is exactly at rec->lo or rec->hi and BASIC
# otherwise — every BASIC is +1 with nothing to pay
# for it.
#
# This counts each combination. src/ is read and never written.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-44-br"
out="$here/singleton-branches.txt"
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
/* SINGLETON_ROW, indexed [row basic][column set basic]. */
static int64_t dg_sr[2][2];
/* SINGLETON_COL, indexed [column set basic]. */
static int64_t dg_sc[2];
static int dg_col_basic;   /* did this firing set its column BASIC? */
#endif
""" + head)

# SINGLETON_ROW: the column branch.
a = """            y_i = d0 / rec->coef;
            orig->sol_redcost[j] = 0.0;
            orig->sol_col_status[j] = JAOS_BASIS_BASIC;"""
assert s.count(a) == 1
s = s.replace(a, a + """
#ifdef JAOS_DIAG
            dg_col_basic = 1;
#endif""")

b = """        double y_i;
        if (zero_works || !this_row_owns) {"""
assert s.count(b) == 1
s = s.replace(b, """#ifdef JAOS_DIAG
        dg_col_basic = 0;
#endif
""" + b)

# SINGLETON_ROW: the row branch, counted where the pair is finally known.
c = """        if (act == orig->row_lower[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_LOWER;
        else if (act == orig->row_upper[i])
            orig->sol_row_status[i] = JAOS_BASIS_AT_UPPER;
        else
            orig->sol_row_status[i] = JAOS_BASIS_BASIC;"""
assert s.count(c) == 1
s = s.replace(c, c + """
#ifdef JAOS_DIAG
        dg_sr[orig->sol_row_status[i] == JAOS_BASIS_BASIC][dg_col_basic]++;
#endif""")

# SINGLETON_COL: its one write.
d = """        orig->sol_col_status[j] =
            (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
            (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;"""
assert s.count(d) == 1, s.count(d)
s = s.replace(d, d + """
#ifdef JAOS_DIAG
        dg_sc[orig->sol_col_status[j] == JAOS_BASIS_BASIC]++;
#endif""")

# Reported once per solve, after the replay.
loop = """    for (int64_t r = p->arena_len - 1; r >= 0; r--)
        ps_replay_one(orig, p, r, rowc);"""
assert s.count(loop) == 2
at = s.index(loop)
s = s[:at] + """#ifdef JAOS_DIAG
    dg_sr[0][0] = dg_sr[0][1] = dg_sr[1][0] = dg_sr[1][1] = 0;
    dg_sc[0] = dg_sc[1] = 0;
#endif
""" + loop + """
#ifdef JAOS_DIAG
    {
        char buf[512];
        int k = snprintf(buf, sizeof buf,
                "BR sr_bound_nocol=%lld sr_bound_col=%lld "
                "sr_basic_nocol=%lld sr_basic_col=%lld "
                "sc_nonbasic=%lld sc_basic=%lld\\n",
                (long long)dg_sr[0][0], (long long)dg_sr[0][1],
                (long long)dg_sr[1][0], (long long)dg_sr[1][1],
                (long long)dg_sc[0], (long long)dg_sc[1]);
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
    -Iinclude -Isrc src/*.c bench/run.c -o build/diag/run-br -lm \
    || { echo "build failed"; exit 2; }

d=$(mktemp -d)
./build/diag/run-br -j 12 -o "$d/nl.txt" > "$d/nl.log" 2>&1
./build/diag/run-br -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$d/kn.txt" > "$d/kn.log" 2>&1

{
echo "# Which branch costs the basis position?"
echo "#"
echo "# SINGLETON_ROW restores one row and is worth one basic. Its column and"
echo "# its row are decided by two independent tests, so the pair can sum to"
echo "# 0, 1 or 2 and only 1 is right."
echo "#"
echo "# SINGLETON_COL restores a column and its row survives, so every BASIC"
echo "# it writes is +1 with nothing to pay for it."
echo
for f in nl kn; do
    case $f in nl) L=netlib;; kn) L=kennington;; esac
    all=$(grep -c "BR " "$d/$f.log"); ok=$(grep -c "^BR " "$d/$f.log")
    [ "$all" = "$ok" ] || echo "*** $L: $all lines, $ok intact — MANGLED ***"
    echo "######## $L ########"
    awk '/^BR /{
        for (i = 2; i <= NF; i++) { split($i, kv, "="); v[kv[1]] += kv[2] }
        solves++
    }
    END {
        bn = v["sr_bound_nocol"]; bc = v["sr_bound_col"]
        an = v["sr_basic_nocol"]; ac = v["sr_basic_col"]
        printf "%d solves\n\n", solves
        printf "SINGLETON_ROW, %d firings\n", bn + bc + an + ac
        printf "  %-34s %10d   drift %+d each  = %+d\n",
               "row at a bound, column not set", bn, -1, -bn
        printf "  %-34s %10d   drift  %d each  = %+d\n",
               "row at a bound, column set BASIC", bc, 0, 0
        printf "  %-34s %10d   drift  %d each  = %+d\n",
               "row BASIC, column not set", an, 0, 0
        printf "  %-34s %10d   drift %+d each  = %+d\n",
               "row BASIC, column set BASIC", ac, 1, ac
        printf "  %-34s %10s   %s   = %+d\n", "", "", "net", ac - bn
        printf "\nSINGLETON_COL, %d firings\n", v["sc_nonbasic"] + v["sc_basic"]
        printf "  %-34s %10d   drift  0 each  = %+d\n",
               "value at lo or hi, published nonbasic", v["sc_nonbasic"], 0
        printf "  %-34s %10d   drift %+d each  = %+d\n",
               "otherwise, published BASIC", v["sc_basic"], 1, v["sc_basic"]
        printf "\nTOTAL from both families: %+d\n", ac - bn + v["sc_basic"]
    }' "$d/$f.log"
    echo
done
echo "# canary: the total must equal D133's per-set sum — +3904 on netlib and"
echo "# +25654 on Kennington — or a branch is being counted in the wrong place."
} 2>&1 | tee "$out"
rm -rf "$d"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
