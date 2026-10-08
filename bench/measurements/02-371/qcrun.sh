#!/bin/bash
# Solves qcgen.py's QCQPs at three sizes with the tree's tool and counts
# the answers that are optimal and pass the checker.
#
#   qcrun.sh [COUNT]     the models go to ~/qcgen
#
# SPDX-License-Identifier: Apache-2.0
H=$(cd "$(dirname "$0")" && pwd)
J=$H/../../../build/cli/jaos
COUNT=${1:-150}
mkdir -p ~/qcgen
for spec in "12 30 1" "12 30 3" "30 80 3"; do
  set -- $spec
  ok=0; bad=0
  for s in $(seq 1 $COUNT); do
    f=~/qcgen/q-$1-$2-$3-$s.mps
    python3 $H/qcgen.py $1 $2 $3 $s $f
    out=$(timeout 60 $J solve $f --check 2>&1)
    st=$(echo "$out" | sed -n 's/^status //p')
    ck=$(echo "$out" | sed -n 's/^check_ok //p')
    if [ "$st" = optimal ] && [ "$ck" = yes ]; then
      ok=$((ok+1))
    else
      bad=$((bad+1))
      echo "  $1x$2 balls $3 seed $s: status $st"
    fi
  done
  echo "$1 nodes $2 arcs $3 balls: $ok optimal and checked, $bad other"
done
