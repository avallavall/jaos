#!/usr/bin/env bash
# The reading behind 02-258: QPLIB's 17 convex mixed-integer QPs (the CBL,
# CML and DML files 02-256's qpfetch.sh fetches) under a work limit, one
# line each with the status, the objective or the incumbent, the bound,
# qplib.solu's value, the checker, the nodes and the first incumbent's node.
#
#   miqp.sh [LIMIT]      default 1e11 work units; needs `make cli`
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
DIR=${DIR:-bench/instances-qplib}
OUT=${OUT:-$HOME/qplib-02-258}
LIM=${1:-100000000000}
mkdir -p "$OUT"
for n in QPLIB_10050 QPLIB_10056 QPLIB_10069 QPLIB_3980 QPLIB_3913 \
         QPLIB_4270 QPLIB_3871 QPLIB_3547 QPLIB_3698 QPLIB_3792 QPLIB_3694 \
         QPLIB_3861 QPLIB_3708 QPLIB_5577 QPLIB_5924 QPLIB_5527 QPLIB_5543; do
    build/cli/jaos solve "$DIR/$n.qplib" --check --work-limit "$LIM" \
        > "$OUT/$n.txt" 2>&1
    get() { sed -n "s/^$1 //p" "$OUT/$n.txt"; }
    ref=$(awk -v n="$n" '$2 == n {print $3}' "$DIR/qplib.solu" | head -1)
    printf '%-12s %-15s obj=%-22s inc=%-22s bound=%-22s ref=%-22s check=%-3s nodes=%-7s first=%s\n' \
        "$n" "$(get status)" "$(get objective)" "$(get incumbent)" \
        "$(get bound)" "$ref" "$(get check_ok)" "$(get nodes)" \
        "$(get first_incumbent)"
    [ -z "$(get status)" ] && tail -1 "$OUT/$n.txt" | sed 's/^/    /'
done
