#!/bin/bash
# binkar10_1 (or INST) under 1e10 work units with the shipped CLI, once
# with the defaults and once with each node-cut and restart switch, one line
# a run: status, nodes, bound and incumbent (binkar10_1.txt).
#
#   INST=binkar10_1 binkar10_1.sh       needs build/cli/jaos
#
# SPDX-License-Identifier: Apache-2.0
R=$(cd "$(dirname "$0")/../../.." && pwd)
cd $R
F=bench/instances-miplib2017/${INST:-binkar10_1}.mps; [ -f $F ] || F=$F.gz
run() {
    label=$1; shift
    out=$(./build/cli/jaos solve $F --work-limit 10000000000 "$@" 2>&1)
    printf '%-22s %-12s nodes %7s bound %-20s inc %s\n' "$label" \
        "$(echo "$out" | sed -n 's/^status //p')" "$(echo "$out" | sed -n 's/^nodes //p')" \
        "$(echo "$out" | sed -n 's/^bound //p')" "$(echo "$out" | sed -n 's/^incumbent //p')"
}
run base
run restart --restart
run cutdepth10 --cut-depth 10
run nodemir --node-mir
run cutdepth10-nodemir --cut-depth 10 --node-mir
run propagate4 --propagate 4
run nodecuts-nocap --cut-depth 10 --node-cut-cap 0
