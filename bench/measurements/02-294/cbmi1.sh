#!/bin/bash
f=$1; arm=$2; lim=$3
J=${JB:-build/cli/jaos}
STAT=${STAT:-$HOME/cblib-mip/stat.set-cblib2014.csv}
n=$(basename $f .cbf.gz)
out=$HOME/cbmi-$arm/$n.txt
mkdir -p $HOME/cbmi-$arm
case "$arm" in
  *bound) opt="--node-select bound" ;;
  *) opt="" ;;
esac
ulimit -v 3000000; timeout 1200 $J solve $f --check --work-limit $lim $opt > $out 2>&1
get() { sed -n "s/^$1 //p" $out | head -1; }
ref=$(awk -F';' -v n="$n" '$2 == n {print $6}' $STAT)
claim=$(awk -F';' -v n="$n" '$2 == n {print $10}' $STAT)
printf '%-24s %-12s obj=%-22s inc=%-22s bound=%-22s ref=%-22s %-20s check=%-3s nodes=%-7s work=%s\n' "$n" "$(get status)" "$(get objective)" "$(get incumbent)" "$(get bound)" "$ref" "$claim" "$(get check_ok)" "$(get nodes)" "$(get work_units)"
