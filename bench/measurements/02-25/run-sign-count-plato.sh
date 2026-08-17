#!/bin/bash
# D107's reopen condition, asked of the fourth set.
#
# D107 refused the inequality implied-free column singleton because the
# sign-respecting count is 341 rows of 3315 on netlib and 0 on Kennington, with
# 304 of the 341 on `ship*` instances below the comparison's floor. Its reopen
# condition is "a model population where bench/measurements/02-13's counter
# reports a non-trivial sign-ok share", and its entry names section 4's fourth
# set as the standing candidate. That set now exists (D115), so this asks.
#
# **The instrument is 02-13's, unmodified.** This script compiles
# ../02-13/implied_free_sign.c and re-runs 02-13's own two calibrations before
# reporting anything, so a number here is comparable to a number there by
# construction rather than by assertion. 02-13's script is left alone: it is
# the record of what was measured then, and editing it would rewrite that.
#
# Usage (inside WSL, from anywhere): bash run-sign-count-plato.sh
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
src="$root/bench/measurements/02-13"
cd "$root" || exit 9

mkdir -p build/diag "$here/counts"

gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -Iinclude -Isrc \
    "$src/implied_free_sign.c" src/*.c -o build/diag/ifsign -lm \
    || { echo "build failed"; exit 2; }

# Calibration 1: 02-13's hand model, nine singleton columns with a known split.
sed -n '/^cat > build\/diag\/ifsign-hand.mps/,/^EOF$/p' "$src/run-sign-count.sh" \
    | sed '1d;$d' > build/diag/ifsign-hand.mps
hand=$(./build/diag/ifsign build/diag/ifsign-hand.mps | tail -1)
want="9 9 9 9 1 8 5 3 5 5 1 1"
got=$(echo "$hand" | awk '{$1=""; print}' | tr -s ' ' | sed 's/^ //;s/ $//')
if [ "$got" != "$want" ]; then
    echo "CALIBRATION FAILED on 02-13's hand model."
    echo "  want: $want"
    echo "  got:  $got"
    exit 1
fi
echo "calibration 1: 02-13's hand model reproduced ($want)"

# Calibration 2: 02-10's committed netlib values, through this same binary.
./build/diag/ifsign bench/instances/*.mps > build/diag/ifsign-netlib.txt || exit 2
mr7=$(awk '$1=="maros-r7.mps"{print $4}' build/diag/ifsign-netlib.txt)
tot=$(awk 'NR>1{h+=$4; r+=$5}END{print h, r}' build/diag/ifsign-netlib.txt)
if [ "$mr7" != "984" ] || [ "$tot" != "3321 3315" ]; then
    echo "CALIBRATION FAILED against 02-10's committed values."
    echo "  maros-r7 hits: want 984, got $mr7"
    echo "  netlib hits rows_hit: want '3321 3315', got '$tot'"
    exit 1
fi
echo "calibration 2: 02-10 reproduced (maros-r7 984, netlib 3321/3315)"
echo

for fam in pds fome nug; do
    dir="bench/instances-plato-$fam"
    n=$(ls "$dir"/*.mps 2>/dev/null | wc -l)
    if [ "$n" -eq 0 ]; then
        echo "$fam: NOT FETCHED, skipped -- it is absent from every total below"
        continue
    fi
    ./build/diag/ifsign "$dir"/*.mps > "$here/counts/$fam.txt" || exit 2
done

{
    echo "# D107's counter over the fourth set. Instrument and calibration are"
    echo "# 02-13's; only the population is new. A family with no line below was"
    echo "# not fetched rather than empty."
    echo "# set hits rows_hit eq in ok dec ok_rows ok_nz c0 rng inst_with_ok"
    for fam in pds fome nug; do
        f="$here/counts/$fam.txt"
        [ -s "$f" ] || continue
        awk -v s="plato-$fam" \
            'NR>1{h+=$4;r+=$5;e+=$6;i+=$7;o+=$8;d+=$9;kr+=$10;kn+=$11;c+=$12;g+=$13;
                  if($10>0)n++}
             END{print s, h+0, r+0, e+0, i+0, o+0, d+0, kr+0, kn+0, c+0, g+0, n+0}' "$f"
    done
} | tee "$here/counts/totals.txt"
