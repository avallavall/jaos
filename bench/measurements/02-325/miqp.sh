#!/bin/bash
# The third reading behind 02-325: QPLIB's 17 convex MIQPs at 1e10 work
# units, the default against clique fixing and RINS on together and
# clique fixing alone, three at a time at 4 GB each. The CLI is copied
# first so a rebuild cannot change it midway.
#
#   miqp.sh [ARM...]     arms: default, both, cliquefix (all three by default)
#
# Needs QPLIB in ~/qplib; records land next to this script.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
H=$R/bench/measurements/02-325
(cd $R && make build/cli/jaos > /dev/null 2>&1) || exit 1
cp $R/build/cli/jaos $HOME/j11-jaos
arms=${*:-default both cliquefix}
for arm in $arms; do
  case $arm in
    default) opts="" ;;
    both) opts="--opt mip_clique_fix=true --opt mip_rins=50" ;;
    cliquefix) opts="--opt mip_clique_fix=true" ;;
    *) echo "unknown arm $arm"; exit 2 ;;
  esac
  for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do echo $n; done | \
  xargs -P 3 -I{} bash -c 'n={}; o=$HOME/j11-miqp-'$arm'-$n.txt; ulimit -v 4000000; timeout 1500 $HOME/j11-jaos solve $HOME/qplib/$n.qplib --check --work-limit 10000000000 '"$opts"' > $o 2>&1; get() { sed -n "s/^$1 //p" $o | head -1; }; printf "%-12s %-15s obj=%-22s inc=%-22s bound=%-22s nodes=%-7s work=%s\n" $n "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$(get nodes)" "$(get work_units)"' | sort > $H/miqp-$arm.txt
done
echo done > $HOME/j11-miqp.done
