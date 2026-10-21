#!/bin/bash
# For each LP file lpsweep.sh left in ~/lpr/SET that JAOS reads, solve it
# and the original under the same work limit and compare status and
# objective (1e-7 relative). One line per model: same, DIFF, STATUS or
# UNREAD with the reader's message.
#
#   lpsolve.sh netlib|mm|miplib|infeas [WORK_LIMIT]     default 1e9
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
J=$R/build/cli/jaos
SET=$1
LIM=${2:-1000000000}
OUT=$HOME/lpr/$SET
case $SET in
  netlib) FILES="$R/bench/instances/*.mps" ;;
  mm) FILES="$R/bench/instances-maros-meszaros/*" ;;
  miplib) FILES="$R/bench/instances-miplib/*" ;;
  infeas) FILES="$R/bench/instances-infeas/*" ;;
esac
for f in $FILES; do
  n=$(basename $f); n=${n%%.*}
  [ -s $OUT/$n.lp ] || continue
  if ! $J stats $OUT/$n.lp > /dev/null 2>&1; then echo "$n UNREAD $($J stats $OUT/$n.lp 2>&1 | head -1 | cut -c1-100)"; continue; fi
  a=$(timeout 300 $J solve $f --work-limit $LIM 2>&1)
  b=$(timeout 300 $J solve $OUT/$n.lp --work-limit $LIM 2>&1)
  sa=$(echo "$a" | sed -n 's/^status //p'); sb=$(echo "$b" | sed -n 's/^status //p')
  oa=$(echo "$a" | sed -n 's/^objective //p'); ob=$(echo "$b" | sed -n 's/^objective //p')
  ia=$(echo "$a" | sed -n 's/^incumbent //p'); ib=$(echo "$b" | sed -n 's/^incumbent //p')
  v=$(awk -v x="$oa" -v y="$ob" 'BEGIN { if (x == "" && y == "") { print "same"; exit } d = x - y; if (d < 0) d = -d; s = (x < 0 ? -x : x); if (s < 1) s = 1; print (d <= 1e-7 * s ? "same" : "DIFF") }')
  [ "$sa" = "$sb" ] || v="STATUS"
  echo "$n $v $sa $sb obj $oa $ob inc $ia $ib"
done
