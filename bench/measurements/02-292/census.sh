#!/bin/bash
D=/home/vall/qplib
C6=/mnt/c/Users/vall-/Desktop/projectes/jaos-c6/build/cli/jaos
for f in $(ls -Sr $D/*.qplib); do
  n=$(basename $f .qplib)
  t=$(sed -n 2p $f | tr -d " " | cut -c1-3)
  case "$t" in CBL|CML|DML) ;; *) continue ;; esac
  line=$(timeout 120 $C6 solve $f --node-limit 1 --opt mip_symmetry=true --log summary 2>&1 | grep -i -m1 "symmetr")
  echo "$n $t: $line" | cut -c1-220
done
echo sym-done
