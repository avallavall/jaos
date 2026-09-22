#!/bin/bash
f=$1; arm=$2; lim=$3
J=${JB:-build/cli/jaos}
Q=${QPLIB:-$HOME/qplib}
n=$(basename $f .qplib)
out=$HOME/cqp-$arm/$n.txt
mkdir -p $HOME/cqp-$arm
ulimit -v 4000000; timeout 1500 $J solve $f --work-limit $lim --check > $out 2>&1
get() { sed -n "s/^$1 //p" $out | head -1; }
ref=$(awk -v n="$n" '$2 == n {print $3}' $Q/qplib.solu | head -1)
printf '%-12s %-16s obj=%-24s ref=%-24s check=%-4s dual=%-10s rowrel=%-10s work=%s\n' "$n" "$(get status)" "$(get objective)" "$ref" "$(get check_ok)" "$(get max_dual_violation | cut -c1-9)" "$(get max_row_violation_relative | cut -c1-9)" "$(get work_units)"
