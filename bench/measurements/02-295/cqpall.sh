#!/bin/bash
S=$(cd "$(dirname "$0")" && pwd)
Q=${QPLIB:-$HOME/qplib}
arm=$1; lim=${2:-100000000000}
for f in $Q/*.qplib; do
  t=$(sed -n 2p $f | tr -d ' ' | cut -c1-3)
  case "$t" in CCB|CCL|DCL) echo $f ;; esac
done | xargs -P 3 -I{} bash $S/cqp1.sh {} $arm $lim | sort > $S/cqp-$arm.txt
echo "$arm done: $(wc -l < $S/cqp-$arm.txt) lines"
