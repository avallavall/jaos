#!/bin/bash
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
J=$R/build/cli/jaos
D=$R/bench/instances
OUT=~/vnet.txt
echo "start $(date +%T)" > $OUT
for f in $(ls $D | sort); do
  name=${f%%.*}
  res=$(timeout 60 $J verify $D/$f 2>&1 | grep -E '^(proof|status|stage|at_row|at_col|violation|bound_bits)' | tr '\n' ' ')
  rc=$?
  echo "$name $res" >> $OUT
done
echo "VNET-END $(date +%T)" >> $OUT
