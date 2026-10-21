#!/bin/bash
# The push on circulation seed 1282 once the walk reaches it: the tree with
# 02-398's knobs.py, the growth stop at 1e30 and a push at the stall, logged
# round by round. Needs 02-398's read.sh models in ~/read398; the lines
# land in ~/push400/push1282.txt.
#
#   push1282.sh   needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=$HOME/push400
rm -rf $O; mkdir -p $O/tree
rsync -a $R/src $R/include $R/cli $R/Makefile $O/tree/
(cd $O/tree && python3 $R/bench/measurements/02-398/knobs.py && make -s build/cli/jaos > $O/build.log 2>&1) || { echo "build failed" > $O/done; exit 1; }
J24FAR=1e30 J24NEAR=1e-3 $O/tree/build/cli/jaos solve $HOME/read398/n1282.mps --log detail 2>&1 |
    grep -E 'push round [0-9]+:|J24 stalled|the push|^push|^status' > $O/push1282.txt
echo done > $O/done
