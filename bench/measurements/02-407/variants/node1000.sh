#!/bin/bash
# Builds a copy with node1000.py and reads it: the 34 models of 02-394 that JAOS
# left at 2e9 work units, at 5e8 without and with J7PROP=4 (node1000-34.txt);
# MIPLIB 3 with J7PROP=4 (node1000-miplib.txt), against bench/results/miplib.txt.
# Needs 02-394's read.sh models in ~/mip394.
#
#   node1000.sh       needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
S=$(cd "$(dirname "$0")" && pwd)
R=$(cd "$(dirname "$0")/../../../.." && pwd)
O=~/j7prop3; rm -rf $O; mkdir -p $O/tree/bench
rsync -a $R/src $R/include $R/cli $R/Makefile $O/tree/
cp $R/bench/*.c $O/tree/bench/
(cd $O/tree && python3 $S/node1000.py && make -s build/cli/jaos build/bench/run > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
J=$O/tree/build/cli/jaos
grep -E " skip$" $R/bench/measurements/02-394/mipcmp.txt | grep " optimal " | awk '{print $1"-"$2" "$8}' > $O/list.txt
while read name ref; do
  f=$HOME/mip394/$name.mps
  line="$name ref=$ref"
  for v in 0 4; do
    if [ $v = 0 ]; then r=$($J solve $f --work-limit 500000000 2>&1); else r=$(J7PROP=$v $J solve $f --work-limit 500000000 2>&1); fi
    get() { echo "$r" | sed -n "s/^$1 //p" | head -1; }
    line="$line | prop$v: inc=$(get incumbent)$(get objective) first=$(get first_incumbent)"
  done
  echo "$line"
done < $O/list.txt > $O/out.txt
cd $R
J7PROP=4 $O/tree/build/bench/run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib -o $O/miplib-prop4.txt > $O/miplib.log 2>&1
echo done > $O/done
