#!/bin/bash
# HiGHS writes each model of a set as MPS (bench/compare/fetch-solvers.sh
# builds it); JAOS reads the file and `jaos diff` sets it against the
# original. One line per model: the name, diff's exit code and its first
# lines. The MPS files land in ~/mpsr/SET.
#
#   mpssweep.sh netlib|mm|miplib|infeas
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/compare/solvers/highs-1.15.1
J=$R/build/cli/jaos
SET=$1
OUT=$HOME/mpsr/$SET
mkdir -p $OUT
case $SET in
  netlib) FILES="$R/bench/instances/*.mps" ;;
  mm) FILES="$R/bench/instances-maros-meszaros/*" ;;
  miplib) FILES="$R/bench/instances-miplib/*" ;;
  infeas) FILES="$R/bench/instances-infeas/*" ;;
esac
for f in $FILES; do
  n=$(basename $f); n=${n%%.*}
  printf 'write_model_to_file = true\nwrite_model_file = %s/%s.mps\ntime_limit = 0\n' $OUT $n > $OUT/o.txt
  timeout 120 $H --model_file $f --options_file $OUT/o.txt > $OUT/$n.hlog 2>&1
  if [ ! -s $OUT/$n.mps ]; then echo "$n NOLP"; continue; fi
  d=$(timeout 120 $J diff $f $OUT/$n.mps 2>&1)
  rc=$?
  first=$(echo "$d" | head -2 | tr '\n' ' ' | cut -c1-160)
  echo "$n rc=$rc $first"
done
