#!/bin/bash
OUT=~/j11arms.txt
echo "build $(date +%T)" > $OUT
DST=~/jaos-x bash ~/j7/tools/sync.sh >> $OUT 2>&1
grep -q '^rc=0' $OUT || { echo "J11-END build failed" >> $OUT; exit 1; }
for arm in "zh mip_zero_half_rounds=2" "cl mip_cover_lift=true" "pr mip_propagate=4" \
           "cf mip_conflicts=true" "rs mip_restart=true" "pb mip_probing=true"; do
  set -- $arm
  name=j11b$1; opt=$2
  echo "== $name $opt $(date +%T)" >> $OUT
  bash ~/j7/tools/arm.sh $name -- -O $opt
  grep -E "x  nodes|geomean|solved|^j11|^base" ~/j7/logs/arm-$name.log >> $OUT
done
echo "J11-END $(date +%T)" >> $OUT
