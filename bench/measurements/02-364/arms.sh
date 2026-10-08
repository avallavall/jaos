#!/bin/bash
OUT=~/garms.txt
echo "start $(date +%T)" > $OUT
DST=~/jaos-x bash ~/j7/tools/sync.sh >> $OUT 2>&1
for arm in "g0s3 JAOS_GISL=0 JAOS_GSTALL=1e-3" "g0s3c1 JAOS_GISL=0 JAOS_GSTALL=1e-3 JAOS_GCAP=1" "g0s4c1 JAOS_GISL=0 JAOS_GSTALL=1e-4 JAOS_GCAP=1"; do
  set -- $arm
  name=$1; shift
  echo "== $name $* $(date +%T)" >> $OUT
  bash ~/j7/tools/arm.sh $name "$@" --
  grep -E "x  nodes|geomean|solved|^g0|^base" ~/j7/logs/arm-$name.log >> $OUT
done
echo "GARMS-END $(date +%T)" >> $OUT
