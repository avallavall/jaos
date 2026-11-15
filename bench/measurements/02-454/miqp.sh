#!/bin/bash
# QPLIB's 17 convex MIQPs at 1e10 work units, as 02-415 reads them, with
# the tool HEAD names and the tool NEW names, one line each with the
# status, objective, incumbent, bound, nodes and work, and whether the MIP
# presolve mirrored any column. Records land in OUT (default
# ~/miqp454), miqp-head.txt and miqp-new.txt.
#
#   HEAD=jaos NEW=jaos OUT=dir miqp.sh     needs QPLIB in ~/qplib
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
export O=${OUT:-$HOME/miqp454}
export JH=${HEAD:-$HOME/jaos-head/build/cli/jaos}
export JN=${NEW:-$R/build/cli/jaos}
rm -rf $O; mkdir -p $O
one() {
    arm=$1; n=$2; o=$O/$arm-$n.txt
    j=$JH; [ $arm = new ] && j=$JN
    ulimit -v 4000000
    timeout 3000 $j solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 \
        --log summary > $o 2>&1
    get() { sed -n "s/^$1 //p" $o | head -1; }
    mir=$(sed -n 's/.*MIP presolve: \([0-9]*\) columns.*mirrored.*/\1/p' $o | head -1)
    printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%-12s mirrored=%s\n" \
        $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" \
        "$(get nodes)" "$(get work_units)" "${mir:-0}" >> $O/miqp-$arm.txt
}
export -f one
for arm in head new; do
    for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 \
             QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 \
             QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
        echo "$arm $n"
    done
done | xargs -P 3 -L 1 bash -c 'one $0 $1'
for arm in head new; do sort -o $O/miqp-$arm.txt $O/miqp-$arm.txt; done
echo done > $O/done
