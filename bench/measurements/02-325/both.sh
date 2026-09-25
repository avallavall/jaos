#!/bin/bash
# The second reading behind 02-325: clique fixing and RINS on together,
# the two arms of arms.sh that gain on one set each, on MIPLIB 3 and on
# the 2017 set at 1e10 work units. One arm at J=2, 4 GB a solve.
#
#   both.sh          records land next to this script
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 2
H=bench/measurements/02-325
make build/bench/run > /dev/null 2>&1 || exit 1
( ulimit -v 4000000; build/bench/run -j 2 -m bench/miplib.manifest -e mip \
    -d bench/instances-miplib -O mip_clique_fix=true -O mip_rins=50 \
    -o $H/m3-both.txt > /dev/null 2>&1 )
( ulimit -v 4000000; build/bench/run -j 2 -m bench/miplib2017.manifest -e mip \
    -d bench/instances-miplib2017 -L 10000000000 -O mip_clique_fix=true \
    -O mip_rins=50 -o $H/m17-both.txt > /dev/null 2>&1 )
python3 bench/measurements/02-298/gapsum.py default=$H/m17-default.txt \
    cliquefix=$H/m17-cliquefix.txt rins=$H/m17-rins.txt both=$H/m17-both.txt
echo done
