#!/bin/bash
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-/tmp}
F=$O/rf-status.txt; : > $F
for v in "$@"; do
  H=$HOME/jaos-rf-$v
  rm -rf $H; mkdir -p $H
  (cd $R && tar cf - --exclude=./build --exclude='./bench/instances*' --exclude=./bench/measurements --exclude=./.git --exclude='./dotnet/*/bin' --exclude='./dotnet/*/obj' .) | (cd $H && tar xf -)
  for d in instances instances-infeas instances-kennington; do ln -s $R/bench/$d $H/bench/$d; done
  sed -i "s/^constexpr int64_t REFACTOR_EVERY = [0-9]*;/constexpr int64_t REFACTOR_EVERY = $v;/" $H/src/simplex.c
  grep -q "REFACTOR_EVERY = $v;" $H/src/simplex.c || { echo "$v sed failed" >> $F; continue; }
  cd $H
  make netlib J=12 > $O/rf-$v-netlib.log 2>&1; echo "$v netlib exit=$?" >> $F
  make netlib-infeas J=12 > $O/rf-$v-inf.log 2>&1; echo "$v infeas exit=$?" >> $F
  make netlib-kennington J=2 > $O/rf-$v-kenn.log 2>&1; echo "$v kennington exit=$?" >> $F
done
echo done >> $F
