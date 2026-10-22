#!/bin/bash
# The 45 mixed disagreements 02-431 left where one order ends
# numerical_error (02-431's mixed-new.txt), dumped by 02-431's permcone.c
# in dump mode, then each order solved by 02-431's coneout.c as it is and
# moved out, and by the CLI. LIB is the library permcone.c and coneout.c
# are built with; the CLI is the working tree's. Records land in
# ~/dump432.
#
#   LIB=libjaos.a dump.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/measurements/02-431
L=${LIB:-$R/build/release/libjaos.a}
O=$HOME/dump432
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/perm $D/permcone.c $L -lm -pthread || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/coneout $D/coneout.c $L -lm -pthread || exit 1
grep -E '^(==|STATUS)' $D/mixed-new.txt | awk '/^==/{s=$NF; next} {sub("t=","",$2); print s, $2}' > $O/list.txt
for s in $(seq 1 10); do
  last=$(awk -v s=$s '$1==s{t=$2} END{print t}' $O/list.txt)
  mkdir -p $O/s$s
  (cd $O/s$s && $O/perm $((last + 1)) $s dump > dump.txt 2>&1) &
done
wait
while read s t; do
  echo "== seed $s t$t"
  (cd $O/s$s && $O/coneout t${t}a.mps t${t}b.mps)
done < $O/list.txt > $O/coneout.txt 2>&1
J=$R/build/cli/jaos
while read s t; do
  a=$($J solve $O/s$s/t${t}a.mps 2>&1 | grep -E '^(status|objective)' | tr '\n' ' ')
  b=$($J solve $O/s$s/t${t}b.mps 2>&1 | grep -E '^(status|objective)' | tr '\n' ' ')
  echo "seed $s t$t | a: $a| b: $b"
done < $O/list.txt > $O/cli.txt
