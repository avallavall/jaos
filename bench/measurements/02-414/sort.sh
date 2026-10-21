#!/bin/bash
# 02-414: where each circulation of TODO row J24 stops. Solves the 31
# models of 02-365's netqp.py at 12 nodes and the 2 at 40 nodes that end
# numerical_error in 02-413's reading, with the summary log, and prints the
# lines that say where the barrier, its pushes and the conic walk stop.
# Models and logs land in ~/sort414.
#
#   sort.sh       needs python3 and build/cli/jaos
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
M=$R/bench/measurements
O=$HOME/sort414
mkdir -p $O
J=$R/build/cli/jaos
run() {
    n=$1; c=$2; s=$3
    f=$O/q$n-$s.mps
    [ -f $f ] || python3 -I $M/02-365/netqp.py $n $c $s $f
    $J solve --log summary $f > $O/q$n-$s.log 2>&1
    sig=$(grep -o 'barrier stopped after [0-9]* iterations[^:]*: [^;]*\|push that pins only[^,]*\|numerical error after [0-9]* barrier\|ends numerical_error: .\{0,90\}' $O/q$n-$s.log | tr '\n' '|')
    echo "$n $s $sig"
}
for s in 18 25 64 110 127 195 199 201 287 288 1104 1156 1163 1282 1340 1357 1411 1450 1458 1504 1514 1535 1537 1551 1596 1607 1611 1624 1710 1735 1746; do
    run 12 30 $s
done
run 40 100 95
run 40 100 1076
