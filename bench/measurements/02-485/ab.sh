#!/bin/bash
# Times two tools built by head.sh on the same models, alternating A and B
# so the two see the same load, ROUNDS rounds each. Prints one line per run:
# model, tool, status, md5 of the objective_exact line, seconds.
#
#   D=instance-dir ab.sh A B ROUNDS MODEL...
#
# SPDX-License-Identifier: Apache-2.0
D=${D:?set D to the Netlib instance directory}
A=$1; B=$2; N=$3; shift 3
for name in "$@"; do
    f=$(ls $D | grep "^$name\." | head -1)
    for k in $(seq $N); do
        for t in $A $B; do
            t0=$(date +%s.%N)
            out=$(timeout 900 ~/j485/$t/build/cli/jaos solve $D/$f --exact \
                  --exact-decimal --log summary 2>&1)
            t1=$(date +%s.%N)
            st=$(echo "$out" | grep -m1 '^status ' | cut -d' ' -f2)
            obj=$(echo "$out" | grep -m1 '^objective_exact ' | md5sum | cut -c1-12)
            printf '%s %s %s %s %.2f\n' $name $t "${st:-timeout}" $obj \
                "$(echo "$t1 - $t0" | bc)"
        done
    done
done
