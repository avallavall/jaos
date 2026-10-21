#!/bin/bash
# The reading behind 02-394: mipcmp.py on seeds 1 to 700 with the tree's
# tool and HiGHS 1.15.1 from bench/compare/solvers/. Models land in
# ~/mip394, the lines in ~/mip394.txt.
#
#   read.sh       needs python3 and `make cli`
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/mip394
rm -rf $O; mkdir -p $O
cd $R/bench/measurements/02-394 && python3 -I mipcmp.py $R/build/cli/jaos \
    $R/bench/compare/solvers/highs-1.15.1 $O 1 700 > $HOME/mip394.txt 2>&1
echo done >> $HOME/mip394.txt
