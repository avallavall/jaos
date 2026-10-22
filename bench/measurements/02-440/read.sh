#!/bin/bash
# The reading behind 02-440: mirrorfile.c with the working tree's library
# on Netlib and its infeasible set, Kennington, Maros-Meszaros, CBLIB's
# continuous and mixed-integer sets and MIPLIB 3, each file under
# LIMIT work units (default 2e9), with the masks drawn from SEED
# (default 1). Records land in OUT (default
# ~/read440), one file a set.
#
#   LIMIT=n SEED=s OUT=dir read.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
O=${OUT:-$HOME/read440}
L=${LIMIT:-2000000000}
rm -rf $O; mkdir -p $O
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mirrorfile \
    $R/bench/measurements/02-440/mirrorfile.c $R/build/release/libjaos.a -lm -pthread || exit 1
one() {
    set=$1; dir=$2; par=$3
    ls $R/bench/$dir | grep -v -E '\.(csv|txt|set)$' | sed "s#^#$R/bench/$dir/#" |
        xargs -P $par -n 1 $O/mirrorfile $L ${SEED:-1} | sort > $O/$set.txt
}
one netlib instances 6
one infeas instances-infeas 6
one kennington instances-kennington 2
one maros instances-maros-meszaros 6
one cblib instances-cblib 6
one cblib-mip instances-cblib-mip 3
one miplib instances-miplib 2
echo done > $O/done
