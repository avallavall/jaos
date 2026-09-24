#!/bin/bash
cd "$(dirname "$0")/../../.." || exit 2

out=bench/measurements/02-317/arms-log.txt
H=$HOME/j7
mkdir -p $H
cp build/bench/run $H/run
{
echo "start $(date '+%F %H:%M') tree $(git rev-parse --short HEAD)"
arm() {
  local name=$1; shift
  ( ulimit -v 5000000; $H/run -j 2 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 -L 10000000000 "$@" -o $H/m17-$name.txt > $H/m17-$name.log 2>&1 )
  echo "2017 $name rc=$? $(date +%H:%M)"
}
arm default
arm fc5 -O mip_flow_cover_rounds=5
arm agg6 -O mip_mir_aggregate=6
arm both -O mip_flow_cover_rounds=5 -O mip_mir_aggregate=6
python3 bench/measurements/02-298/gapsum.py default=$H/m17-default.txt fc5=$H/m17-fc5.txt agg6=$H/m17-agg6.txt both=$H/m17-both.txt
mip() {
  local name=$1; shift
  ( ulimit -v 5000000; $H/run -j 2 -m bench/miplib.manifest -e mip -d bench/instances-miplib "$@" -o $H/m3-$name.txt > $H/m3-$name.log 2>&1 )
  echo "miplib3 $name rc=$? $(date +%H:%M)"
}
mip default
mip fc5 -O mip_flow_cover_rounds=5
mip agg6 -O mip_mir_aggregate=6
mip both -O mip_flow_cover_rounds=5 -O mip_mir_aggregate=6
echo done
} > "$out" 2>&1
