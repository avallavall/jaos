#!/bin/bash
OUT=~/relarms.txt
echo "wait $(date +%T)" > $OUT
until grep -q 'J11-END' ~/j11arms.txt; do sleep 30; done
for arm in "r4c1 mip_reliability=4 mip_probe_cap=1" "r2c1 mip_reliability=2 mip_probe_cap=1"; do
  set -- $arm
  name=j11$1
  echo "== $name $2 $3 $(date +%T)" >> $OUT
  bash ~/j7/tools/arm.sh $name -- -O $2 -O $3
  grep -E "x  nodes|geomean|solved|^j11|^base" ~/j7/logs/arm-$name.log >> $OUT
done
echo "REL-END $(date +%T)" >> $OUT
