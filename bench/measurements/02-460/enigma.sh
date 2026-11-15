#!/bin/bash
# MIPLIB 3's enigma with the shipped CLI under the node-cut switches, one
# line a run: status, nodes and work units (enigma.txt).
#
#   enigma.sh       needs build/cli/jaos
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
cd $R
F=bench/instances-miplib/enigma.mps
run() {
    label=$1; shift
    out=$(./build/cli/jaos solve $F "$@" 2>&1)
    printf '%-34s %-10s nodes %7s work %s\n' "$label" "$(echo "$out" | sed -n 's/^status //p')" \
        "$(echo "$out" | sed -n 's/^nodes //p')" "$(echo "$out" | sed -n 's/^work_units //p')"
}
run base
run "depth10 stall1e-3" --cut-depth 10 --node-cut-stall 1e-3
run "nodemir stall1e-3" --node-mir --node-cut-stall 1e-3
run "d10 mir stall1e-3" --cut-depth 10 --node-mir --node-cut-stall 1e-3
run "d10 mir stall3e-3" --cut-depth 10 --node-mir --node-cut-stall 3e-3
run "d10 mir stall1e-2" --cut-depth 10 --node-mir --node-cut-stall 1e-2
run "d5 mir stall1e-3" --cut-depth 5 --node-mir --node-cut-stall 1e-3
run "d3 mir stall1e-3" --node-mir --node-cut-stall 1e-3 --cut-depth 3
run "stall1e-3 only, depth 3" --node-cut-stall 1e-3
