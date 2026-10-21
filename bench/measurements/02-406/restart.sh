#!/bin/bash
# The 34 models of 02-394's mipcmp.txt that JAOS left at 2e9 work units
# and HiGHS proved, solved by default and with --restart at 5e8. Needs
# 02-394's read.sh models in ~/mip394; the lines land in ~/rs406/out.txt.
#
#   restart.sh    needs the tool built
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
J=$R/build/cli/jaos
O=~/rs406; rm -rf $O; mkdir -p $O
grep -E " skip$" $R/bench/measurements/02-394/mipcmp.txt | grep " optimal " | awk '{print $1"-"$2}' > $O/list.txt
while read name; do
  f=$HOME/mip394/$name.mps
  line="$name"
  for o in "" "--restart"; do
    r=$($J solve $f --work-limit 500000000 $o 2>&1)
    get() { echo "$r" | sed -n "s/^$1 //p" | head -1; }
    line="$line | ${o:-default}: inc=$(get incumbent)$(get objective) bound=$(get bound) st=$(get status)"
  done
  echo "$line"
done < $O/list.txt > $O/out.txt
echo done >> $O/out.txt
