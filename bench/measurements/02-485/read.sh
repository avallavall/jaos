#!/bin/bash
# The reading behind 02-485: `solve --exact --exact-decimal` on the 94
# Netlib models, once with each tool in TOOLS (default: `head`, the tool
# of 1235543, and `new`, the tool of the change), both built by head.sh in
# ~/j485. The runs of a model go back to back in one job, so they see the
# same load; four jobs at a time, LIMIT seconds a run (default 600). Each
# record line holds the model, the tool, the status, the exact verdict, the
# first 12 hex digits of the md5 of the objective_exact line, the work
# units, the seconds and the peak resident kilobytes. Records land in
# ~/j485/read/OUT (default read.txt).
#
#   D=instance-dir [TOOLS="head new"] [LIMIT=600] read.sh [OUT] [MODEL...]
#
# SPDX-License-Identifier: Apache-2.0
D=${D:?set D to the Netlib instance directory}
TOOLS=${TOOLS:-head new}
LIMIT=${LIMIT:-600}
OUT=${1:-read.txt}; shift
O=~/j485/read
mkdir -p $O
rm -f $O/$OUT $O/$OUT.done
MODELS="$*"
[ -n "$MODELS" ] || MODELS=$(ls $D | sed 's/\..*//' | sort -u)
one() {
    name=$1
    f=$(ls $D | grep "^$name\." | head -1)
    for t in $TOOLS; do
        log=$O/$t-$name.log
        t0=$(date +%s.%N)
        /usr/bin/time -f "rss %M" -o $O/$t-$name.rss timeout $LIMIT \
            ~/j485/$t/build/cli/jaos solve $D/$f --exact --exact-decimal \
            --log summary > $log 2>&1
        t1=$(date +%s.%N)
        st=$(grep -m1 '^status ' $log | cut -d' ' -f2)
        [ -n "$st" ] || st=timeout
        why=$(grep -m1 -E 'exact:|exact solving' $log | sed -E 's/^jaos: .*ends numerical_error: //; s/^exact: //; s/ /_/g')
        obj=$(grep -m1 '^objective_exact ' $log | md5sum | cut -c1-12)
        grep -q '^objective_exact ' $log || obj=-
        wu=$(grep -m1 '^work_units ' $log | cut -d' ' -f2)
        rss=$(grep -m1 '^rss ' $O/$t-$name.rss | cut -d' ' -f2)
        printf '%s %s %s %s %s %s %.2f %s\n' "$name" $t $st "${why:--}" $obj \
            "${wu:--}" "$(echo "$t1 - $t0" | bc)" "${rss:--}" >> $O/$OUT
    done
}
export -f one
export D O OUT TOOLS LIMIT
for n in $MODELS; do echo $n; done | xargs -P 4 -I{} bash -c 'one {}'
sort -o $O/$OUT $O/$OUT
echo done > $O/$OUT.done
