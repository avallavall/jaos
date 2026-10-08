#!/bin/bash
OUT=~/npt.txt
echo "start $(date +%T)" > $OUT
DST=~/jaos-x bash ~/j7/tools/sync.sh >> $OUT 2>&1
for arm in "npt10 JAOS_NPT=10" "npt25 JAOS_NPT=25"; do
  set -- $arm
  name=$1; shift
  echo "== $name $* $(date +%T)" >> $OUT
  bash ~/j7/tools/arm.sh $name "$@" --
  grep -E "x  nodes|geomean|solved|^npt|^base" ~/j7/logs/arm-$name.log >> $OUT
done
echo "NPT-END $(date +%T)" >> $OUT
