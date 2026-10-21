#!/bin/bash
# The readings behind 02-393, the working tree against HEAD: miqp.c on
# seeds 1 to 6, and on seeds 1 to 3 with its equality sides rounded to
# 2^-20 as its first version had them; 02-328's fcnet on seeds 1 to 3;
# 02-235's certs, 02-326's discrete and 02-255's misocp on seeds 1 to 3;
# then `make miplib J=2` on the working tree. HEAD is built in ~/jaos-head
# from git archive; records land in ~/read393.
#
#   read.sh       needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-393
M=$R/bench/measurements
OUT=$HOME/read393
rm -rf $OUT; mkdir -p $OUT
rm -rf $HOME/jaos-head; mkdir -p $HOME/jaos-head
(cd $R && git archive HEAD) | tar -x -C $HOME/jaos-head
(cd $HOME/jaos-head && make all cli > $OUT/head-build.log 2>&1) || { echo "head build failed" > $OUT/done; exit 1; }
(cd $R && make all cli > $OUT/new-build.log 2>&1) || { echo "new build failed" > $OUT/done; exit 1; }
for arm in head new; do
  T=$([ $arm = head ] && echo $HOME/jaos-head || echo $R)
  mkdir -p $OUT/$arm
  cp $T/build/release/libjaos.a $OUT/$arm/
  (cd $T && make -s B=$OUT/$arm/net EXTRA_CFLAGS=-DJAOS_MIP_NET_MIN_COLS_VALUE=1 $OUT/$arm/net/release/libjaos.a > $OUT/$arm/net.log 2>&1)
done
python3 - "$H/miqp.c" "$OUT/miqp-round.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
old = "                if (add_row(m, act, act, nn, idx, val) != JAOS_OK)\n"
new = ("                const double eq = ldexp(round(ldexp(act, 20)), -20);\n"
       "                if (add_row(m, eq, eq, nn, idx, val) != JAOS_OK)\n")
assert old in s
open(sys.argv[2], 'w').write(s.replace(old, new, 1))
PY
for arm in head new; do
  A=$OUT/$arm
  L=$A/libjaos.a
  (
    cc() { gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $A/$1 $2 $3 -lm -pthread 2>> $A/cc.log; }
    cc miqp $H/miqp.c $L
    cc miqpr $OUT/miqp-round.c $L
    cc certs $M/02-235/certs.c $L
    cc discrete $M/02-326/discrete.c $L
    cc misocp $M/02-255/misocp.c $L
    cc fcnet $M/02-328/fcnet.c $A/net/release/libjaos.a
    mkdir -p $A/d
    for s in 1 2 3 4 5 6; do echo "== miqp $s"; $A/miqp 1000 $s $A/d | tail -14; done > $A/miqp.txt 2>&1
    for s in 1 2 3; do echo "== miqp-round $s"; $A/miqpr 1000 $s $A/d | tail -14; done > $A/miqpr.txt 2>&1
    for s in 1 2 3; do echo "== fcnet $s"; $A/fcnet 1000 $s $A/d; done > $A/fcnet.txt 2>&1
    for s in 1 2 3; do echo "== certs $s"; $A/certs 2000 $s | tail -8; done > $A/certs.txt 2>&1
    for s in 1 2 3; do echo "== discrete $s"; $A/discrete 1000 $s $A/d | tail -14; done > $A/discrete.txt 2>&1
    for s in 1 2 3; do echo "== misocp $s"; $A/misocp 1000 $s $A/d | tail -14; done > $A/misocp.txt 2>&1
  ) &
done
wait
(cd $R && make miplib J=2 > $OUT/miplib.log 2>&1)
echo done > $OUT/done
