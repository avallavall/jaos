#!/bin/bash
# icmp.sh OLDREF MODEL... : total instructions of a full MIP solve, the tree at OLDREF against the working tree (callgrind)
set -u
R=/mnt/c/Users/vall-/Desktop/projectes/jaos
OLD=$1; shift
rm -rf ~/jaos-old ~/jaos-new; mkdir -p ~/jaos-old ~/jaos-new
git -C $R archive $OLD | tar -x -C ~/jaos-old
rsync -a --exclude build --exclude .git --exclude 'bench/instances*' --exclude 'bench/measurements' --exclude python --exclude bindings $R/ ~/jaos-new/
for t in jaos-old jaos-new; do (cd ~/$t && make -j12 cli > ~/icmp-build-$t.log 2>&1; echo "$t build rc=$?"); done
for m in "$@"; do
  F=$R/bench/instances-miplib/$m.mps
  for t in jaos-old jaos-new; do
    valgrind --tool=callgrind --callgrind-out-file=$HOME/cg-$t-$m ~/$t/build/cli/jaos solve $F --log off > ~/icmp-$t-$m.out 2>/dev/null
    eval "ir_${t//-/_}=$(grep -E '^summary:' ~/cg-$t-$m | awk '{print $2}')"
  done
  echo "$m old $ir_jaos_old new $ir_jaos_new ratio $(awk -v a=$ir_jaos_old -v b=$ir_jaos_new 'BEGIN {printf "%.3f", b/a}') work old $(grep -E '^work_units' ~/icmp-jaos-old-$m.out | awk '{print $2}') new $(grep -E '^work_units' ~/icmp-jaos-new-$m.out | awk '{print $2}')"
done
echo done
