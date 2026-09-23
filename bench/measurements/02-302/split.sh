#!/bin/bash
R=$(cd "$(dirname "$0")/../../.." && pwd)
P=$R/bench/measurements/02-302/doubleton-moves.patch
O=${OUT:-$HOME}
F=$O/moves-status.txt; : > $F
for v in "$@"; do
  H=$HOME/jaos-rf-a$v
  rm -rf $H; mkdir -p $H
  (cd $R && tar cf - --exclude=./build --exclude='./bench/instances*' --exclude=./bench/measurements --exclude=./.git --exclude='./dotnet/*/bin' --exclude='./dotnet/*/obj' .) | (cd $H && tar xf -)
  for d in instances instances-infeas instances-kennington; do ln -s $R/bench/$d $H/bench/$d; done
  cd $H && patch -p1 < $P > /dev/null
  if [ $v = moves ]; then sed -i 's/                    !ag_implied_free_any(&a, j, rl, ru, clo, cup, w))/                    !(false \&\& ag_implied_free_any(\&a, j, rl, ru, clo, cup, w)))/' src/aggregate.c; fi
  if [ $v = anyrow ]; then sed -i 's/!m->cfg.barrier \&\& !m->cfg.pdlp, \&pre_work);/false, \&pre_work);/' src/simplex.c; fi
  make netlib J=8 > $O/split-$v-netlib.log 2>&1; echo "$v netlib exit=$?" >> $F
  make netlib-infeas J=8 > $O/split-$v-inf.log 2>&1; echo "$v infeas exit=$?" >> $F
  make netlib-kennington J=2 > $O/split-$v-kenn.log 2>&1; echo "$v kennington exit=$?" >> $F
done
echo done >> $F
