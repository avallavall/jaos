#!/bin/bash
# highs_sum.sh LOG...: one line per HiGHS log: the restarts, the root's
# bound after its cuts and the bound of the last root after a restart,
# the status, the nodes and the last bound, and the switches
#
# SPDX-License-Identifier: Apache-2.0
for f in "$@"; do
    n=$(basename $f .txt)
    opts=$(grep '^Set option' $f | grep -v -E 'threads|time_limit|mip_rel_gap' | sed 's/Set option //; s/ to /=/' | tr '\n' ' ')
    rs=$(grep -c '^Restarting' $f)
    # progress lines, read from their end: Proc InQueue Leaves Expl BestBound BestSol Gap Cuts InLp Confl LpIters Time
    prog() { grep -E '^ [A-Z ] +[0-9]+ +[0-9]+ +[0-9]+ +[0-9.]+% ' $f; }
    roots=$(prog | awk '$(NF-10) == 0 && $(NF-9) == 0 {print $(NF-7)}')
    first=$(echo "$roots" | grep -v -- '-inf' | sed -n 2p)
    lastroot=$(echo "$roots" | tail -n 1)
    last=$(prog | tail -n 1)
    st=$(grep -E '^  Status ' $f | awk '{print $2}')
    nodes=$(grep -E '^  Nodes ' $f | awk '{print $2}')
    [ -n "$nodes" ] || nodes="$(echo "$last" | awk '{print $(NF-11)}')+"
    printf '%-10s %-8s nodes %-9s restarts %s root %-11s last root %-11s last bound %-11s %s\n' \
        $n "${st:-limit}" "$nodes" $rs "$first" "$lastroot" "$(echo "$last" | awk '{print $(NF-7)}')" "$opts"
done
