#!/bin/bash
# MIPLIB 2017 at 1e10 work units with node1000.sh's copy (~/j7prop3) and
# J7PROP=4, against bench/results/miplib2017.txt. The record lands in
# ~/m17prop.txt.
#
#   m17.sh        needs node1000.sh run first
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../../.." && pwd)
cd $R
J7PROP=4 ~/j7prop3/tree/build/bench/run -j 2 -m bench/miplib2017.manifest -e mip -d bench/instances-miplib2017 -L 1e10 -o ~/m17prop.txt > ~/m17prop.log 2>&1
echo done > ~/m17prop.done
