#!/bin/bash
# The reading behind 02-321: MIR aggregation kept only when it pays on a
# copy of the root LP. MIPLIB 3 and the 2017 set, the default against
# `mip_mir_aggregate=6` with the probe, on the working tree. Records land
# next to this script.
#
#   arms.sh [m3|m17|all]      default all
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 2
H=bench/measurements/02-321
what=${1:-all}
arm() {
    local set=$1 name=$2; shift 2
    if [ "$set" = m3 ]; then
        ( ulimit -v 4000000; build/bench/run -j 2 -m bench/miplib.manifest -e mip \
            -d bench/instances-miplib "$@" -o $H/m3-$name.txt > /dev/null 2>&1 )
    else
        ( ulimit -v ${M17_MEM:-5000000}; build/bench/run -j ${J:-2} \
            -m bench/miplib2017.manifest -e mip \
            -d bench/instances-miplib2017 -L 10000000000 "$@" \
            -o $H/m17-$name.txt > /dev/null 2>&1 )
    fi
    echo "$set $name rc=$? $(date +%H:%M)"
}
if [ "$what" = m3 ] || [ "$what" = all ]; then
    arm m3 default
    arm m3 probe -O mip_mir_aggregate=6
fi
if [ "$what" = m17 ] || [ "$what" = all ]; then
    arm m17 default
    arm m17 probe -O mip_mir_aggregate=6
    python3 bench/measurements/02-298/gapsum.py default=$H/m17-default.txt \
        probe=$H/m17-probe.txt
fi
echo done
