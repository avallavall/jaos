#!/bin/bash
# The reading behind 02-454: 02-440's mirrorfile.c with 02-447's
# modes.patch on QPLIB's 17 convex mixed-integer QPs (02-258's list), with
# the library given (default: the working tree's). Each file is solved as
# it is and mirrored under LIMIT work units (default 2e9), masks from seed
# 1, in five modes: rows negated and columns mirrored (both), rows only,
# columns only, integer columns only and continuous columns only. Records
# land in OUT (default ~/read454), one file a mode.
#
#   LIB=libjaos.a QPLIB=dir LIMIT=n OUT=dir read.sh     needs gcc-14
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
L=${LIB:-$R/build/release/libjaos.a}
Q=${QPLIB:-$HOME/qplib}
O=${OUT:-$HOME/read454}
W=${LIMIT:-2000000000}
rm -rf $O; mkdir -p $O
cp $R/bench/measurements/02-440/mirrorfile.c $O/mirrorfile.c
(cd $O && patch -s -p1 < $R/bench/measurements/02-447/modes.patch) || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Wall -Wextra -I$R/include -o $O/mirrorfile \
    $O/mirrorfile.c $L -lm -pthread || exit 1
for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 \
         QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 \
         QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
    for mode in both rows cols int cont; do
        echo "$mode $Q/$n.qplib"
    done
done | xargs -P 4 -L 1 sh -c '
    if [ "$0" = both ]; then '"$O"'/mirrorfile '"$W"' 1 "$1"
    else MIRROR="$0" '"$O"'/mirrorfile '"$W"' 1 "$1"; fi > '"$O"'/"$0"-$(basename "$1").txt 2>&1'
for mode in both rows cols int cont; do
    cat $O/$mode-*.txt | sort > $O/$mode.txt
    rm -f $O/$mode-*.txt
done
echo done > $O/done
