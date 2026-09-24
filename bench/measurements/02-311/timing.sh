#!/bin/bash
cd "$(dirname "$0")/../../.." || exit 2
out=bench/measurements/02-311/timing-raw.txt
H=$HOME/h7
mkdir -p $H
{
uptime; free -g | head -2
for v in 999999999999 0 10000 100000 1000000; do
  rm -f build/release/barrier.o build/release/libjaos.a build/cli/jaos
  make build/cli/jaos EXTRA_CFLAGS=-DJAOS_BARRIER_NORMAL_LANE_MIN_VALUE=$v > $H/b-$v.log 2>&1 || echo "build $v failed"
  cp build/cli/jaos $H/jaos-$v
done
rm -f build/release/barrier.o build/release/libjaos.a build/cli/jaos
make build/cli/jaos > /dev/null 2>&1
for f in bench/instances/maros-r7.mps bench/instances/dfl001.mps bench/instances/d2q06c.mps bench/instances/pilot87.mps bench/instances-kennington/pds-06.mps; do
  for v in 999999999999 0 10000 100000 1000000; do
    for t in 1 4; do
      [ "$v" != 999999999999 ] && [ $t = 1 ] && continue
      r=$( { /usr/bin/time -f "wall=%e" $H/jaos-$v solve --algorithm barrier --threads $t "$f" 2>&1; } | grep -E "^work_units|^objective|wall=" | tr '\n' ' ')
      echo "$(basename $f) lane_min=$v threads=$t $r"
    done
  done
done
echo done
} > "$out" 2>&1
