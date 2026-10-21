#!/bin/bash
# The sweep behind 02-412: the brute-force generators of
# 02-235 to 02-248 on seeds 13 to 18, then 02-365's and 02-366's
# circulations on seeds 1401 to 1800 (1401 to 1500 at 40 nodes),
# 02-371's QCQPs on seeds 1151 to 1300, and the harnesses of 02-396 to
# 02-411 on new seeds. Records land in ~/sweep412.
#
#   sweep.sh      needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/sweep412
rm -rf $O; mkdir -p $O
cd $R
make -s all cli > $O/build.log 2>&1 || { echo "build failed" > $O/done; exit 1; }
cp build/release/libjaos.a build/cli/jaos $O/
L=$O/libjaos.a
J=$O/jaos
M=$R/bench/measurements
cc() { gcc-14 -std=c23 -O2 -ffp-contract=off -I$R/include -o $O/$1 $2 $L -lm -pthread 2>> $O/build.log; }
cc certs $M/02-235/certs.c
cc iissub $M/02-236/iissub.c
cc exact $M/02-237/exact.c
cc basis $M/02-238/basis.c
cc point $M/02-239/point.c
cc tol $M/02-244/tol.c
cc push $M/02-248/push.c
SEEDS="13 14 15 16 17 18"
(
  for s in $SEEDS; do echo "== certs $s"; $O/certs 2000 $s | tail -8; done
  for s in $SEEDS; do echo "== iissub $s"; $O/iissub 1000 $s | tail -8; done
  for s in $SEEDS; do echo "== exact $s"; mkdir -p $O/proofs; $O/exact 1000 $s $O/proofs | tail -8; done
) > $O/a.txt 2>&1 &
(
  for s in $SEEDS; do echo "== basis $s"; mkdir -p $O/b$s; $O/basis 1000 $s $O/b$s 50 | tail -8; done
  for s in $SEEDS; do echo "== point $s"; mkdir -p $O/p$s; $O/point 1000 $s $O/p$s 50 | tail -8; done
) > $O/b.txt 2>&1 &
(
  for s in $SEEDS; do echo "== tol $s"; mkdir -p $O/t$s; $O/tol 1000 $s $O/t$s 50 | tail -12; done
  for s in $SEEDS; do echo "== push $s"; $O/push 1000 $s $O | tail -8; done
) > $O/c.txt 2>&1 &
wait
mkdir -p $O/nq $O/nqb $O/qc
(
  cd $M/02-365 && python3 -I netqp_read.py $J $J $O/nq 12 30 1401 1800 | tail -6
  cd $M/02-365 && python3 -I netqp_read.py $J $J $O/nq 40 100 1401 1500 | tail -6
) > $O/nq.txt 2>&1 &
(
  cd $M/02-366 && python3 -I netqpb_read.py $J $J $O/nqb 12 30 1401 1800 | tail -6
) > $O/nqb.txt 2>&1 &
(
  for spec in "12 30 1" "12 30 3" "30 80 3"; do
    set -- $spec
    ok=0; bad=0
    for s in $(seq 1151 1300); do
      f=$O/qc/q-$1-$2-$3-$s.mps
      python3 -I $M/02-371/qcgen.py $1 $2 $3 $s $f
      out=$(timeout 60 $J solve $f --check 2>&1)
      st=$(echo "$out" | sed -n 's/^status //p')
      ck=$(echo "$out" | sed -n 's/^check_ok //p')
      if [ "$st" = optimal ] && [ "$ck" = yes ]; then ok=$((ok+1)); else bad=$((bad+1)); echo "  $1x$2 balls $3 seed $s: status $st check $ck"; fi
    done
    echo "$1x$2 balls $3: ok $ok bad $bad"
  done
) > $O/qc.txt 2>&1 &
wait
cc mipopts $M/02-397/mipopts.c
cc algos $M/02-396/algos.c
cc warmmod $M/02-403/warmmod.c
cc warmqp $M/02-404/warmqp.c
cc structedit $M/02-405/structedit.c
cc answers $M/02-408/answers.c
cc iis $M/02-409/iis.c
cc edge $M/02-410/edge.c
cc scaled $M/02-411/scaled.c
NEW="4 5 6"
(
  for s in $NEW; do echo "== mipopts $s"; $O/mipopts 1000 $s | tail -3; done
  for s in $NEW; do echo "== algos $s"; $O/algos 1000 $s | tail -6; done
) > $O/d.txt 2>&1 &
(
  for s in $NEW; do for a in "lp dual" "lp primal" "mip dual"; do echo "== warmmod $s $a"; $O/warmmod 1000 $s $a 8 | tail -1; done; done
  for s in $NEW; do for md in qp miqp; do echo "== warmqp $s $md"; $O/warmqp 1000 $s $md 8 | tail -1; done; done
) > $O/e.txt 2>&1 &
(
  mkdir -p $O/se $O/an
  for s in $NEW; do echo "== structedit $s"; $O/structedit 2000 $s 40 $O/se | tail -8; done
  for s in $NEW; do echo "== answers $s"; $O/answers 3000 $s $O/an | tail -2; done
  for s in $NEW; do echo "== iis $s"; $O/iis 3000 $s | tail -2; done
  for s in $NEW; do echo "== edge $s"; $O/edge 3000 $s | tail -7; done
  for s in $NEW; do echo "== scaled $s"; $O/scaled 1000 $s 2 | tail -5; done
) > $O/f.txt 2>&1 &
wait
echo done > $O/done
