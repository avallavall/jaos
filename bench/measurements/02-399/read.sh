#!/bin/bash
# The reading behind 02-399: fj.py at 20000 moves on the 34 models of
# 02-394 that JAOS left at 2e9 work units and HiGHS proved, and on the
# first ten general files of ~/mip394 by name. Needs 02-394's read.sh
# models in ~/mip394; the lines land in ~/read399.
#
#   read.sh       needs python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/measurements/02-399
O=$HOME/read399
rm -rf $O; mkdir -p $O
cd $HOME/mip394 || exit 1
grep -E " skip$" $R/bench/measurements/02-394/mipcmp.txt | grep " optimal " |
    awk '{print $1"-"$2".mps"}' > $O/list.txt
python3 $D/fj.py 20000 $(cat $O/list.txt) > $O/hard.txt
python3 $D/fj.py 20000 $(ls general-*.mps | head -10) > $O/easy.txt
echo done > $O/done
