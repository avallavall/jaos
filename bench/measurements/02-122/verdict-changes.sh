#!/bin/bash
# Per-instance verdict changes against the C=0 control, for each setting, with
# the primal-side iteration count beside them so a verdict that held while the
# cost moved is still visible. Self-contained: /tmp does not survive between
# `wsl` invocations, so the filtered records are rebuilt here every time.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2
w=$(mktemp -d) || exit 2
trap 'rm -rf "$w"' EXIT

only_records() {
    awk '$2=="ok" || $2=="DISAGREE" || $2=="ERROR" || $2=="overrun" ||
         $2=="REJECTED" || $2=="unbounded?" || $2=="skipped" ||
         $2=="unreached" {print}' "$1"
}
# name verdict primal-iterations ("-" when the primal never reported one)
key() {
    only_records "$1" | awk '{
        n = "-"
        if ($4 ~ /^primal=/) { n = $4; sub("primal=", "", n); sub("/.*", "", n) }
        print $1, $2, n
    }' | sort
}

key sweep-0.txt > "$w/0"
for C in 3e-6 1e-5 1e-3 1e-1 1 5; do
    [ -f "sweep-$C.txt" ] || { echo "=== C=$C : MISSING ==="; continue; }
    key "sweep-$C.txt" > "$w/c"
    echo "=== C=$C ==="
    join -j1 "$w/0" "$w/c" |
        awk '$2 != $4 || $3 != $5 {
            printf "  %-12s %-9s -> %-9s   primal iters %s -> %s\n",
                   $1, $2, $4, $3, $5 }'
    echo
done
