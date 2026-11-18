#!/bin/bash
# The shape of the 20 bases 02-418 could not prove: `jaos verify
# --exact-decimal` with the tool of 1235543 (~/j485/head, head.sh) prints
# each basis's Hadamard bound, its block count and its largest block.
# Four at a time, 200 seconds each; records land in ~/j485/blocks.txt.
#
#   D=instance-dir blocks.sh
#
# SPDX-License-Identifier: Apache-2.0
D=${D:?set D to the Netlib instance directory}
M="25fv47 d2q06c dfl001 greenbea greenbeb grow15 grow22 grow7 maros-r7 modszk1 nesm perold pilot pilot-ja pilot-we pilot4 pilot87 pilotnov scsd8 stair"
rm -f ~/j485/blocks.txt
one() {
    t0=$(date +%s.%N)
    r=$(timeout 200 ~/j485/head/build/cli/jaos verify $D/$1.mps --exact-decimal 2>&1 |
        grep -E '^(proof|stage|bound_bits|blocks|largest_block|bytes_held|terms|status) ' |
        tr '\n' ' ')
    echo "$1 $r secs $(echo "$(date +%s.%N) - $t0" | bc)" >> ~/j485/blocks.txt
}
export -f one
export D
for n in $M; do echo $n; done | xargs -P 4 -I{} bash -c 'one {}'
sort -o ~/j485/blocks.txt ~/j485/blocks.txt
