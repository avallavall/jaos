#!/bin/bash
# The generated reading behind 02-439: flipfile.c with the working tree's
# library on COUNT (default 2000) models of gensym.py, each under 2e8
# work units. The record lands in OUT/sym.txt (default ~/sym439).
#
#   COUNT=n OUT=dir sym.sh     needs gcc-14 and python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/sym439}
rm -rf $O; mkdir -p $O/gen
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/flipfile \
    $R/bench/measurements/02-439/flipfile.c $R/build/release/libjaos.a -lm -pthread || exit 1
python3 -I $R/bench/measurements/02-439/gensym.py $O/gen ${COUNT:-2000} || exit 1
ls $O/gen | sed "s#^#$O/gen/#" | xargs -P 8 -n 20 $O/flipfile 200000000 | sort > $O/sym.txt
echo done > $O/done
