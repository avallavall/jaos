#!/bin/bash
# The reading behind 02-320: why bell5's tree grows under MIR aggregation.
# The root with and without aggregation, the trees with and without a
# cutoff at the optimum, and capped trees with MIR at the root only, with
# fewer aggregation steps and with most-fractional branching.
#
#   bell5.sh         needs `make cli`; takes about 20 minutes
#
# Every run that can grow is capped at 200000 nodes and 2.5 GB, because
# bell5 with node cuts off grew past 3.5 GB in 17 minutes.
#
# SPDX-License-Identifier: Apache-2.0
cd "$(dirname "$0")/../../.." || exit 1
f=bench/instances-miplib/bell5.mps
J=build/cli/jaos
agg="--opt mip_mir_aggregate=6"
echo "== the root"
for arm in "" "$agg"; do
    printf '%-28s: ' "${arm:-default}"
    $J solve $f --node-limit 1 --log summary $arm 2>&1 >/dev/null |
        sed -n 's/^root: //p'
done
echo "== the trees, with and without a cutoff at the optimum"
for arm in "" "$agg"; do
    for cut in "" "--cutoff 8966407"; do
        printf '%-28s %-18s: ' "${arm:-default}" "$cut"
        $J solve $f $arm $cut | grep -E '^status|^nodes|^work_units' |
            tr '\n' ' '
        echo
    done
done
echo "== the first incumbents"
for arm in "" "$agg"; do
    printf '%-28s: ' "${arm:-default}"
    $J solve $f --log progress $arm 2>&1 >/dev/null |
        grep -m1 'incumbent [0-9]'
done
echo "== capped at 200000 nodes"
ulimit -v 2500000
for arm in "--opt mip_node_mir=off" "--opt mip_node_mir=off $agg" \
           "--opt mip_mir_aggregate=1" "--opt mip_mir_aggregate=2" \
           "--branching most-fractional" \
           "--branching most-fractional $agg"; do
    printf '%-60s: ' "$arm"
    timeout 900 $J solve $f $arm --node-limit 200000 |
        grep -E '^status|^nodes|^work_units|^bound' | tr '\n' ' '
    echo
done
