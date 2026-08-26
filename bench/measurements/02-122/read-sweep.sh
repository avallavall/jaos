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
# name, verdict, primal iterations, and the phase split.
#
# The split is in the key because an `overrun` line carries no `primal=` field
# at all -- `bench/primal.c` drops it when the solve never finished -- so a key
# of verdict plus primal iterations reads every overrun instance as unchanged
# however far its phase 1 moved. That is how this script reported twelve
# instances moving at C=1 when fifteen did: `scsd8`, `d6cube` and `dfl001` all
# overrun on both sides with different phase-1 counts. `jaos-measurer` caught
# it; a full line diff is the check this key now matches.
key() {
    only_records "$1" | awk '{
        n = "-"; sp = "-"
        for (i = 3; i <= NF; i++) {
            if ($i ~ /^primal=/) { n = $i; sub("primal=", "", n); sub("/.*", "", n) }
            if ($i ~ /^split=/)  { sp = $i }
        }
        print $1, $2, n, sp
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
    # The COUNT comes from a full line diff and never from the key. Any key is
    # lossy: verdict plus iterations misses an `overrun` line, which carries no
    # `primal=` field at all, and adding the split still misses an instance
    # whose only change is its work figure -- `stair` at C=3e-1 is exactly
    # that. Both were reported as "did not move" before `jaos-measurer` diffed
    # the lines. The key below is for reading, not for counting.
    moved=$(diff "$w/r-0" "$w/r-$C" | grep -c '^>')
    echo "--- C=$C  ($moved instances moved, from a full line diff)"
    diff "$w/r-0" "$w/r-$C" | awk '/^>/{print $2}' | sort | tr '\n' ' '
    echo
    join -j1 "$w/k0" "$w/kc" |
        awk '$2 != $5 || $3 != $6 || $4 != $7 {
            printf "    %-12s %-9s -> %-9s   iters %s -> %s\n",
                   $1, $2, $5, $3, $6
            if ($4 != $7) printf "    %-12s   %s -> %s\n", "", $4, $7 }'
done

echo
echo "== pilot87, every setting =="
for C in "$@"; do
    [ -s "$w/r-$C" ] || continue
    printf "  C=%-6s " "$C"
    awk '$1=="pilot87"' "$w/r-$C" | cut -c1-170
done
