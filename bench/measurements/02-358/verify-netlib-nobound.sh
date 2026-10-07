#!/bin/bash
# vnet2.sh BUILD TAG : exact verification over Netlib with the up-front bound skipped
S=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/f617864d-1882-4bfe-b5ed-8996f9c3535b/scratchpad
J=$1/build/cli/jaos
D=/mnt/c/Users/vall-/Desktop/projectes/jaos/bench/instances
OUT=~/vnet-$2.txt
echo "start $(date +%T)" > $OUT
for f in $(ls $D | sort); do
  name=${f%%.*}
  t0=$(date +%s.%N)
  res=$(JAOS_NOBOUND=1 timeout 120 $J verify $D/$f 2>&1 | grep -E '^(proof|stage|at_row|at_col|bound_bits|largest_block|terms)' | tr '\n' ' ')
  t1=$(date +%s.%N)
  printf '%s %s secs %.1f\n' "$name" "$res" "$(echo "$t1 - $t0" | bc)" >> $OUT
done
echo "VNET-END $(date +%T)" >> $OUT
