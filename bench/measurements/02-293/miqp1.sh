#!/bin/bash
f=$1; arm=$2; lim=$3
J=${JB:-build/cli/jaos}
Q=${QPLIB:-$HOME/qplib}
n=$(basename $f .qplib)
out=$HOME/miqp-$arm/$n.txt
mkdir -p $HOME/miqp-$arm
case "$arm" in
  bound) opt="--node-select bound" ;;
  *) opt="" ;;
esac
ulimit -v 3000000; timeout 900 $J solve $f --work-limit $lim $opt > $out 2>&1
get() { sed -n "s/^$1 //p" $out; }
ref=$(awk -v n="$n" '$2 == n {print $3}' $Q/qplib.solu | head -1)
printf '%-12s %-16s obj=%-22s inc=%-22s bound=%-22s ref=%-22s nodes=%s\n' "$n" "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$ref" "$(get nodes)"
