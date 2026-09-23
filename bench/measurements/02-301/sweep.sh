#!/bin/bash
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-/tmp}
F=$O/agg-status.txt; : > $F
for rf in "$@"; do
  r=${rf%/*}; f=${rf#*/}; H=$HOME/jaos-rf-a$r-$f
  rm -rf $H; mkdir -p $H
  (cd $R && tar cf - --exclude=./build --exclude='./bench/instances*' --exclude=./bench/measurements --exclude=./.git --exclude='./dotnet/*/bin' --exclude='./dotnet/*/obj' .) | (cd $H && tar xf -)
  for d in instances instances-infeas instances-kennington; do ln -s $R/bench/$d $H/bench/$d; done
  cd $H
  X="EXTRA_CFLAGS=-DJAOS_AGG_ROW_MAX_VALUE=$r -DJAOS_AGG_FILL_MAX_VALUE=$f"
  make netlib J=8 "$X" > $O/agg-$r-$f-netlib.log 2>&1; echo "$r/$f netlib exit=$?" >> $F
  make netlib-infeas J=8 "$X" > $O/agg-$r-$f-inf.log 2>&1; echo "$r/$f infeas exit=$?" >> $F
  make netlib-kennington J=2 "$X" > $O/agg-$r-$f-kenn.log 2>&1; echo "$r/$f kennington exit=$?" >> $F
done
echo done >> $F
