#!/bin/bash
# The reading behind 02-455, on two sets of generated models written to
# files. mip: 02-224's perm.c with dumpall.patch, seeds 1 to 4 and 2500
# models a seed, each with integer marks and one of an SOS1 or SOS2 set,
# a semi-continuous column, an indicator row or none. cone: 02-431's
# permcone.c with dumpall-cone.patch in its mixed mode, seeds 1 to 4 and
# 2500 models a seed, each with cones or quadratic rows beside the same
# shapes. 02-440's mirrorfile.c with 02-447's modes.patch and sos.patch,
# which mirrors SOS members too, solves each as it is and mirrored, with
# the library given (default: the working tree's), in three modes: rows
# negated and columns mirrored (both), rows only and columns only. Each
# solve may take MEM kilobytes of address space (default 3,000,000), so
# that a model whose tree outgrows it ends with a memory error on its own
# line rather than the kernel killing a batch. Records land in OUT
# (default ~/read455), as SET-MODE.txt, and SET-summary.txt.
#
#   LIB=libjaos.a OUT=dir SETS="mip cone" MEM=kb read.sh   needs gcc-14, python3
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-455
L=${LIB:-$R/build/release/libjaos.a}
O=${OUT:-$HOME/read455}
rm -rf $O; mkdir -p $O
cp $R/bench/measurements/02-224/perm.c $R/bench/measurements/02-431/permcone.c \
    $R/bench/measurements/02-440/mirrorfile.c $O/
(cd $O && patch -s -p1 < $H/dumpall.patch &&
 patch -s -p1 < $H/dumpall-cone.patch &&
 patch -s -p1 < $R/bench/measurements/02-447/modes.patch &&
 patch -s -p1 < $H/sos.patch) || exit 1
for p in perm permcone mirrorfile; do
    gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/$p \
        $O/$p.c $L -lm -pthread || exit 1
done
ulimit -v ${MEM:-3000000}
for set in ${SETS:-mip cone}; do
    gen=perm; [ $set = cone ] && gen=permcone
    M=$O/$set-models
    mkdir -p $M
    for s in 1 2 3 4; do
        mkdir -p $M/s$s
        (cd $M/s$s && DUMPALL=1 $O/$gen 2500 $s > /dev/null 2>&1) &
    done
    wait
    for s in 1 2 3 4; do
        for f in $M/s$s/*.mps; do mv $f $M/s$s-$(basename $f); done
        rmdir $M/s$s
    done
    find $M -name '*.mps' | sort > $O/$set-files.txt
    for mode in both rows cols; do
        if [ $mode = both ]; then
            xargs -P 12 -n 100 $O/mirrorfile 2000000000 1 < $O/$set-files.txt
        else
            MIRROR=$mode xargs -P 12 -n 100 $O/mirrorfile 2000000000 1 < $O/$set-files.txt
        fi | sort > $O/$set-$mode.txt
    done
    python3 $H/summary.py $O $set > $O/$set-summary.txt
done
echo done > $O/done
