#!/bin/bash
J=${J:-/mnt/c/Users/vall-/Desktop/projectes/jaos/build/cli/jaos}
D=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances
OUT=${OUT:-~/exnet.txt}
echo "start $(date +%T)" > $OUT
for f in $(ls $D | sort); do
  name=${f%%.*}
  t0=$(date +%s.%N)
  res=$(timeout 300 $J solve $D/$f --exact --log summary 2>&1 | grep -E '^status |exact:|exact solving' | sed -E 's/^jaos: .*ends numerical_error: //' | tr '\n' ' ')
  t1=$(date +%s.%N)
  printf '%s %s secs %.1f\n' "$name" "$res" "$(echo "$t1 - $t0" | bc)" >> $OUT
done
echo "EXNET-END $(date +%T)" >> $OUT
