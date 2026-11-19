#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 2

REC=${1:?usage: ratio.sh RECORD [BASELINE]}
BASE=${2:-bench/miplib.baseline}
awk -v rec="$REC" '
    FNR == NR {
        if ($0 ~ /^#/ || NF < 11)
            next
        bw[$1] = $9
        bn[$1] = $11
        next
    }
    {
        w = ""; n = ""
        for (i = 1; i <= NF; i++) {
            if ($i ~ /^work=/) w = substr($i, 6)
            if ($i ~ /^nodes=/) n = substr($i, 7)
        }
        if (!($1 in bw) || w == "")
            next
        st = $2
        solved += st == "optimal"
        r = w / bw[$1]
        printf "%-12s %-10s nodes %8d -> %8d work %12d -> %12d %7.3fx\n", $1, st, bn[$1], n, bw[$1], w, r
        lr += log(r)
        k++
        if (w != bw[$1]) {
            lm += log(r)
            km++
        }
        if (r > 2.0)
            over++
    }
    END {
        printf "%d instances, %d optimal, work %.3fx geometric (%.3fx over the %d whose work moved), %d past 2x\n", k, solved, exp(lr / k), (km > 0 ? exp(lm / km) : 1), km, over
    }' "$BASE" "$REC"
