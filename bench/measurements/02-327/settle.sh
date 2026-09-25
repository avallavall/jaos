#!/bin/bash
# The reading behind 02-327: settle.diff applied to a copy of the tree in
# ~/jaos-settle, one QPLIB QCQP solved under each switch it adds, the
# settle's lines of the detail log kept.
#
#   settle.sh MODEL ARM...     ARM: plain, keep, force, both, or a batch
#                              fraction such as 0.25
#
# Needs QPLIB in ~/qplib; logs land in ~/settle-MODEL-ARM.log.
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
W=$HOME/jaos-settle
rm -rf $W; mkdir -p $W
(cd $R && git archive HEAD) | tar -x -C $W
(cd $W && patch -p1 < $R/bench/measurements/02-327/settle.diff > /dev/null) || exit 1
(cd $W && make build/cli/jaos > /dev/null 2>&1) || exit 1
n=$1; shift
for arm in "$@"; do
  unset JAOS_KEEPINACT JAOS_FORCE JAOS_BATCH
  case $arm in
    plain) ;;
    keep) export JAOS_KEEPINACT=1 ;;
    force) export JAOS_FORCE=1 ;;
    both) export JAOS_KEEPINACT=1 JAOS_FORCE=1 ;;
    *) export JAOS_BATCH=$arm ;;
  esac
  echo "== $n $arm"
  ( ulimit -v 6000000; timeout 1500 $W/build/cli/jaos solve $HOME/qplib/$n.qplib --check --work-limit 100000000000 --log detail 2>&1 ) > $HOME/settle-$n-$arm.log
  grep -E 'settle (batch|round|check)|projection (taken|refused)|refit step|dual refit (taken|refused)|gives a point it takes' $HOME/settle-$n-$arm.log
  grep -E '^status|^check_ok|^max_dual_violation|ends numerical' $HOME/settle-$n-$arm.log | cut -c1-200
done
