#!/bin/bash
# Prints every MIP instance that holds parity rows, with the rank of its
# system mod 2 and the binaries elimination fixes. ALLROWS=1 counts every
# equality row over binaries, not only those with an even coefficient.
#
# SPDX-License-Identifier: Apache-2.0
H=$(cd "$(dirname "$0")" && pwd)
B=$H/../..
T=$(mktemp -d)
for f in "$B"/instances-miplib/* "$B"/instances-miplib2017/*; do
    n=$(basename "$f")
    case $n in
        *.gz) zcat "$f" > "$T/x.mps"; g=$T/x.mps ;;
        *) g=$f ;;
    esac
    r=$(python3 "$H/parity.py" "$g" 2>&1 | tr '\n' ' ')
    case "$r" in
        *"parity rows 0 "*) ;;
        *) echo "$n $r" ;;
    esac
done
rm -rf "$T"
