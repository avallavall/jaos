#!/bin/bash
# Reads a family of campaign records -- `<prefix>-<C>.txt` -- and reports the
# verdict tally per setting and, per instance, what changed against C=0.
# Self-contained in one invocation: /tmp does not survive between `wsl` calls,
# and a comparison whose inputs are missing reports "identical".
#
#   read-sweep.sh cheap 0 1e-5 1e-3 1e-1 3e-1 1 2 5
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here" || exit 2
pfx=${1:?prefix}
shift
w=$(mktemp -d) || exit 2
trap 'rm -rf "$w"' EXIT

only_records() {
    awk '$2=="ok" || $2=="DISAGREE" || $2=="ERROR" || $2=="overrun" ||
         $2=="REJECTED" || $2=="unbounded?" || $2=="skipped" ||
         $2=="unreached" {print}' "$1" | sort
}
key() {
    only_records "$1" | awk '{
        n = "-"
        if ($4 ~ /^primal=/) { n = $4; sub("primal=", "", n); sub("/.*", "", n) }
        print $1, $2, n
    }'
}

echo "== verdict tally =="
printf "%-8s %-5s %-9s %-8s %-6s %s\n" C ok DISAGREE overrun ERROR total
for C in "$@"; do
    f="$pfx-$C.txt"
    [ -s "$f" ] || { printf "%-8s MISSING OR EMPTY\n" "$C"; continue; }
    only_records "$f" > "$w/r-$C"
    n() { awk -v v="$1" '$2==v' "$w/r-$C" | wc -l; }
    printf "%-8s %-5s %-9s %-8s %-6s %s\n" "$C" "$(n ok)" "$(n DISAGREE)" \
        "$(n overrun)" "$(n ERROR)" "$(wc -l < "$w/r-$C")"
done

[ -s "$w/r-0" ] || { echo; echo "no C=0 control -- nothing to compare against"; exit 2; }
key "$pfx-0.txt" > "$w/k0"

echo
echo "== against C=0: verdict, and primal iterations =="
for C in "$@"; do
    [ "$C" = 0 ] && continue
    [ -s "$w/r-$C" ] || continue
    key "$pfx-$C.txt" > "$w/kc"
    echo "--- C=$C"
    join -j1 "$w/k0" "$w/kc" |
        awk '$2 != $4 || $3 != $5 {
            printf "    %-12s %-9s -> %-9s   iters %s -> %s\n",
                   $1, $2, $4, $3, $5 }'
done

echo
echo "== pilot87, every setting =="
for C in "$@"; do
    [ -s "$w/r-$C" ] || continue
    printf "  C=%-6s " "$C"
    awk '$1=="pilot87"' "$w/r-$C" | cut -c1-170
done
