#!/bin/bash
# The reading behind 02-325: TODO J11's MIP switches, each turned on alone
# on the tree of that day, on MIPLIB 3 and on the 2017 set at 1e10 work
# units, against the default. One arm at a time at J=2, 4 GB a solve.
#
#   arms.sh          records land next to this script
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 2
H=bench/measurements/02-325
make build/bench/run > /dev/null 2>&1 || exit 1
arm() {
    local name=$1; shift
    ( ulimit -v 4000000; build/bench/run -j 2 -m bench/miplib.manifest -e mip \
        -d bench/instances-miplib "$@" -o $H/m3-$name.txt > /dev/null 2>&1 )
    ( ulimit -v 4000000; build/bench/run -j 2 -m bench/miplib2017.manifest -e mip \
        -d bench/instances-miplib2017 -L 10000000000 "$@" \
        -o $H/m17-$name.txt > /dev/null 2>&1 )
    echo "$name $(date +%H:%M)"
}
arm default
arm zerohalf -O mip_zero_half_rounds=2
arm coverlift -O mip_cover_lift=true
arm rins -O mip_rins=50
arm localbranch -O mip_local_branching=10
arm propagate -O mip_propagate=4
arm rcfix -O mip_rcfix=true
arm probing -O mip_probing=true
arm cliquefix -O mip_clique_fix=true
python3 bench/measurements/02-298/gapsum.py default=$H/m17-default.txt \
    zerohalf=$H/m17-zerohalf.txt coverlift=$H/m17-coverlift.txt \
    rins=$H/m17-rins.txt localbranch=$H/m17-localbranch.txt \
    propagate=$H/m17-propagate.txt rcfix=$H/m17-rcfix.txt \
    probing=$H/m17-probing.txt cliquefix=$H/m17-cliquefix.txt
echo done
