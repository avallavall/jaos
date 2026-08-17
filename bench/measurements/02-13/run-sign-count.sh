#!/bin/bash
# Build the sign-classifying implied-free counter, refuse to report until it
# reproduces a hand-worked model and two known 02-10 values, then count both
# feasible sets. Read-only: nothing in src/ and nothing the shipping build
# reads is touched; the binary goes to build/diag/.
#
# Usage (from anywhere, inside WSL): bash run-sign-count.sh
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9

mkdir -p build/diag "$here/counts"

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -Iinclude -Isrc \
    "$here/implied_free_sign.c" src/*.c -o build/diag/ifsign -lm \
    || { echo "build failed"; exit 2; }

# The hand model: nine singleton columns, every one implied free (all FR, so
# the containment test passes whatever the row implies), one per row.
#   S1  L row, y = +1  -> needs rl, rl = -inf   DECLINED
#   S2  L row, y = -1  -> needs ru, finite      OK
#   S3  G row, y = +1  -> needs rl, finite      OK
#   S4  G row, y = -1  -> needs ru, ru = +inf   DECLINED
#   S5  range row [0,4], y = +1                 OK, rng
#   S6  E row                                   EQ
#   S7  L row, cost 0  -> y = 0                 OK, c0
#   S8  L row, a = -1, y = -1 -> needs ru       OK   (negative-coefficient case)
#   S9  G row, a = -1, y = -1 -> needs ru       DECLINED
# Expected: rows 9 cols 9 hits 9 rows_hit 9 eq 1 in 8 ok 5 dec 3
#           ok_rows 5 ok_nz 5 c0 1 rng 1
cat > build/diag/ifsign-hand.mps <<'EOF'
NAME          IFSIGN
ROWS
 N  COST
 L  R1
 L  R2
 G  R3
 G  R4
 L  R5
 E  R6
 L  R7
 L  R8
 G  R9
COLUMNS
    S1        COST      1.0        R1        1.0
    S2        COST      -1.0       R2        1.0
    S3        COST      1.0        R3        1.0
    S4        COST      -1.0       R4        1.0
    S5        COST      1.0        R5        1.0
    S6        COST      1.0        R6        1.0
    S7        R7        1.0
    S8        COST      1.0        R8        -1.0
    S9        COST      1.0        R9        -1.0
RHS
    RHS       R1        4.0        R2        4.0
    RHS       R3        1.0        R4        1.0
    RHS       R5        4.0        R6        2.0
    RHS       R7        4.0        R8        4.0
    RHS       R9        1.0
RANGES
    RNG       R5        4.0
BOUNDS
 FR BND       S1
 FR BND       S2
 FR BND       S3
 FR BND       S4
 FR BND       S5
 FR BND       S6
 FR BND       S7
 FR BND       S8
 FR BND       S9
ENDATA
EOF

hand=$(./build/diag/ifsign build/diag/ifsign-hand.mps | tail -1)
want="9 9 9 9 1 8 5 3 5 5 1 1"
got=$(echo "$hand" | awk '{$1=""; print}' | tr -s ' ' | sed 's/^ //;s/ $//')
if [ "$got" != "$want" ]; then
    echo "CALIBRATION FAILED on the hand model."
    echo "  want: $want"
    echo "  got:  $got"
    exit 1
fi
echo "calibration: hand model reproduced ($want)"

./build/diag/ifsign bench/instances/*.mps            > "$here/counts/netlib.txt"     || exit 2
./build/diag/ifsign bench/instances-kennington/*.mps > "$here/counts/kennington.txt" || exit 2

# Detection must reproduce 02-10 exactly: same models, same logic.
mr7=$(awk '$1=="maros-r7.mps"{print $4}' "$here/counts/netlib.txt")
trs=$(awk '$1=="truss.mps"{print $4}'    "$here/counts/netlib.txt")
tot=$(awk 'NR>1{h+=$4; r+=$5}END{print h, r}' "$here/counts/netlib.txt")
if [ "$mr7" != "984" ] || [ "$trs" != "0" ] || [ "$tot" != "3321 3315" ]; then
    echo "CALIBRATION FAILED against 02-10's committed values."
    echo "  maros-r7 hits: want 984, got $mr7"
    echo "  truss hits:    want 0, got $trs"
    echo "  netlib hits rows_hit: want '3321 3315', got '$tot'"
    exit 1
fi
echo "calibration: 02-10 reproduced (maros-r7 984, truss 0, netlib 3321/3315)"

{
    echo "# Totals over each set. Detection reproduces 02-10; the split is new."
    echo "# set hits rows_hit eq in ok dec ok_rows ok_nz c0 rng inst_with_ok"
    awk 'NR>1{h+=$4;r+=$5;e+=$6;i+=$7;o+=$8;d+=$9;kr+=$10;kn+=$11;c+=$12;g+=$13;
              if($10>0)n++}
         END{print "netlib", h, r, e, i, o, d, kr, kn, c, g, n+0}' \
        "$here/counts/netlib.txt"
    awk 'NR>1{h+=$4;r+=$5;e+=$6;i+=$7;o+=$8;d+=$9;kr+=$10;kn+=$11;c+=$12;g+=$13;
              if($10>0)n++}
         END{print "kennington", h, r, e, i, o, d, kr, kn, c, g, n+0}' \
        "$here/counts/kennington.txt"
} | tee "$here/counts/totals.txt"
