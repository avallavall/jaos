#!/bin/bash
# The reading behind 02-418: 02-358's exact solve of the 94 Netlib models,
# once on the doubles and once with --exact-decimal, from a copy of the
# working tree's tool, and once on the doubles with the tool of 6642d88,
# the tree before the change. Each model gets 300 seconds. Records land in
# ~/read418 (binary.txt, decimal.txt, binary-head.txt).
#
#   read.sh       needs git and the tool built in build/cli/jaos
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
D=$R/bench/instances
O=$HOME/read418
rm -rf $O; mkdir -p $O/head
cp $R/build/cli/jaos $O/jaos
(cd $R && git archive 6642d88 src include cli Makefile) | tar -x -C $O/head
(cd $O/head && make -s build/cli/jaos > $O/head-build.log 2>&1) || exit 1
one() {
    tool=$1; out=$2; shift 2
    for f in $(ls $D | sort); do
        name=${f%%.*}
        res=$(timeout 300 $tool solve $D/$f --exact "$@" --log summary 2>&1 | grep -E '^status |^objective_exact |exact:|exact solving' | sed -E 's/^jaos: .*ends numerical_error: //' | tr '\n' ' ')
        printf '%s %s\n' "$name" "$res" >> $out
    done
}
one $O/jaos $O/binary.txt &
one $O/jaos $O/decimal.txt --exact-decimal &
one $O/head/build/cli/jaos $O/binary-head.txt &
wait
echo done > $O/done
