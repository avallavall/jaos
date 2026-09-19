#!/usr/bin/env bash
# The QPLIB reading behind 02-256: every instance qpfetch.sh fetched, under
# a work limit, one line each with the class, the status, the objective or
# the incumbent, the bound, qplib.solu's value and the checker.
#
#   qpall.sh [LIMIT]      default 1e11 work units; needs `make cli`
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
DIR=${DIR:-bench/instances-qplib}
OUT=${OUT:-$HOME/qplib-02-256}
LIM=${1:-100000000000}
mkdir -p "$OUT"
for f in $(ls -Sr "$DIR"/*.qplib); do
    n=$(basename "$f" .qplib)
    t=$(awk -F'\t' -v n="$n" '$1 == n {print $3 $4 $5}' "$DIR/table.tsv")
    build/cli/jaos solve "$f" --check --work-limit "$LIM" > "$OUT/$n.txt" 2>&1
    get() { sed -n "s/^$1 //p" "$OUT/$n.txt"; }
    ref=$(awk -v n="$n" '$2 == n {print $3}' "$DIR/qplib.solu" | head -1)
    printf '%-12s %-4s %-15s obj=%-22s inc=%-22s bound=%-22s ref=%-22s check=%-3s nodes=%s\n' \
        "$n" "$t" "$(get status)" "$(get objective)" "$(get incumbent)" \
        "$(get bound)" "$ref" "$(get check_ok)" "$(get nodes)"
    [ -z "$(get status)" ] && tail -1 "$OUT/$n.txt" | sed 's/^/    /'
done
