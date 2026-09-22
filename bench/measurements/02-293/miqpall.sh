#!/bin/bash
S=$(cd "$(dirname "$0")" && pwd)
Q=${QPLIB:-$HOME/qplib}
arm=$1; lim=${2:-10000000000}
for f in $Q/*.qplib; do
  t=$(sed -n 2p $f | tr -d ' ' | cut -c1-3)
  case "$t" in CBL|CML|DML) echo $f ;; esac
done | xargs -P 3 -I{} bash $S/miqp1.sh {} $arm $lim | sort > $S/miqp-$arm.txt
echo "$arm done: $(wc -l < $S/miqp-$arm.txt) lines"
