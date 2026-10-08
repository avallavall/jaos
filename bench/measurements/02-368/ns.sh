#!/bin/bash
# Solves six MIPLIB 3 models with and without JAOS_NODESCALE, the switch of
# node-scale.diff, and prints status, nodes, work and seconds for each.
#
#   ns.sh        from a tree with node-scale.diff applied
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 2
make -j12 cli 2>&1 | grep -E ' error:| warning:'
D=bench/instances-miplib
for M in bell5 flugpl lseu enigma p0201 misc07; do
  f=$(ls $D/$M.* | head -1)
  for e in 0 1; do
    if [ $e = 1 ]; then export JAOS_NODESCALE=1; else unset JAOS_NODESCALE; fi
    s=$(date +%s.%N)
    ./build/cli/jaos solve $f > ~/ns-$M-$e.log 2>&1
    t=$(echo "$(date +%s.%N) - $s" | bc)
    printf '%-8s %s %s %s %s time %.2f\n' $M $e "$(grep '^status' ~/ns-$M-$e.log)" "$(grep '^nodes' ~/ns-$M-$e.log)" "$(grep '^work_units' ~/ns-$M-$e.log)" $t
  done
done
