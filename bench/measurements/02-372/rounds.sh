#!/bin/bash
# Solves 02-371's 450 QCQPs with the tree's tool and prints, for each model
# the quadratic-program finish touched, where it started and how many
# rounds it took. Run 02-371's qcrun.sh first: it writes the models.
#
#   rounds.sh
#
# SPDX-License-Identifier: Apache-2.0
H=$(cd "$(dirname "$0")" && pwd)
J=$H/../../../build/cli/jaos
for spec in "12 30 1" "12 30 3" "30 80 3"; do
  set -- $spec
  for s in $(seq 1 150); do
    f=~/qcgen/q-$1-$2-$3-$s.mps
    out=$(timeout 60 $J solve $f --check --log summary 2>&1)
    line=$(echo "$out" | grep "quadratic programs")
    [ -n "$line" ] || continue
    from=$(echo "$line" | sed -n 's/.*from the \([a-z]*\).*/\1/p')
    k=$(echo "$line" | sed -n 's/.*, \([0-9]*\) quadratic programs.*/\1/p')
    st=$(echo "$out" | sed -n 's/^status //p')
    ck=$(echo "$out" | sed -n 's/^check_ok //p')
    echo "$1x$2 balls $3 seed $s from $from rounds $k status $st check $ck"
  done
done
