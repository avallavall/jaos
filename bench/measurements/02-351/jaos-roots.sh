#!/bin/bash
# jall.sh OUT : JAOS's root bound and root work on MIPLIB 3 and the 2017 set (node limit 1, work limit 2e10)
cd ~/jaos-x || exit 2
OUT=$1; : > $OUT
for F in $(sed -E 's/^([^ #]+).*/\1/' bench/miplib.manifest | grep -v '^#' | sed 's#^#bench/instances-miplib/#; s#$#.mps#') \
         $(sed -E 's/^([^ #]+).*/\1/' bench/miplib2017.manifest | grep -v '^#' | sed 's#^#bench/instances-miplib2017/#; s#$#.mps#'); do
  [ -e "$F" ] || continue
  n=$(basename $F .mps)
  r=$( ( ulimit -v 4000000; build/cli/jaos solve $F --node-limit 1 --work-limit 20000000000 --no-heuristics --log summary 2>&1 ) | grep -E '^root: relaxation|^work_units|^status' | sed -E 's/^root: relaxation ([^ ]+).*/\1/; s/^work_units //; s/^status //' | tr '\n' ' ')
  echo "$n $r" >> $OUT
done
echo done >> $OUT
