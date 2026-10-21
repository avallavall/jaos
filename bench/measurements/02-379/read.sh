#!/bin/bash
# The readings behind 02-379, the working tree against HEAD: 02-255's 3000
# generated mixed-integer conic models with 02-376's unsure.patch, CBLIB's
# 80 mixed-integer instances at 1e10 work units (02-255's cblib.sh line
# format), and QPLIB's 14 mixed-integer QCQPs at 1e10 (02-376's
# miqcqp.sh). HEAD is built in ~/jaos-head from git archive; records land
# in ~/read379.
#
#   read.sh       needs gcc-14, CBLIB's mixed-integer set in
#                 bench/instances-cblib-mip and QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
OUT=$HOME/read379
DIR=$R/bench/instances-cblib-mip
STAT=$DIR/stat.set-cblib2014.csv
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make all cli > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make all cli > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
mkdir -p $OUT/new-bin
cp $R/build/cli/jaos $R/build/release/libjaos.a $OUT/new-bin/
for arm in head new; do
  T=$([ $arm = head ] && echo $HOME/jaos-head/build || echo $OUT/new-bin)
  JB=$([ $arm = head ] && echo $T/cli/jaos || echo $T/jaos)
  LB=$([ $arm = head ] && echo $T/release/libjaos.a || echo $T/libjaos.a)
  (
    mkdir -p $OUT/mis-$arm
    git -C $R show HEAD:bench/measurements/02-255/misocp.c > $OUT/mis-$arm/misocp.c
    (cd $OUT/mis-$arm && patch -s -p4 < $R/bench/measurements/02-376/unsure.patch)
    gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include \
        -o $OUT/mis-$arm/misocp $OUT/mis-$arm/misocp.c $LB -lm -pthread 2>/dev/null
    for seed in 1 2 3; do
      echo "== seed $seed"
      mkdir -p $HOME/uns
      $OUT/mis-$arm/misocp 1000 $seed $OUT/mis-$arm | grep -vE '^UNSURE file'
    done > $OUT/mip-$arm.txt 2>&1
    mkdir -p $OUT/cb-$arm
    for f in $(ls -Sr $DIR/*.cbf.gz); do
      n=$(basename $f .cbf.gz)
      timeout 1500 $JB solve $f --check --work-limit 10000000000 > $OUT/cb-$arm/$n.txt 2>&1
      get() { sed -n "s/^$1 //p" $OUT/cb-$arm/$n.txt; }
      ref=$(awk -F';' -v n="$n" '$2 == n {print $6}' $STAT)
      claim=$(awk -F';' -v n="$n" '$2 == n {print $10}' $STAT)
      printf '%-24s %-12s obj=%-22s inc=%-22s bound=%-22s ref=%-22s %-20s check=%-3s nodes=%-7s work=%s\n' \
          "$n" "$(get status)" "$(get objective)" "$(get incumbent)" \
          "$(get bound)" "$ref" "$claim" "$(get check_ok)" "$(get nodes)" \
          "$(get work_units)"
    done > $OUT/cblib-mip-$arm.txt
    JB=$JB OUT=$OUT/miqcqp-$arm.txt bash $R/bench/measurements/02-376/miqcqp.sh
  ) &
done
wait
echo done > $OUT/done
