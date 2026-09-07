#!/usr/bin/env bash
# D346's evidence: how many gate instances reach an LP file with their own
# names, and how many reach one when the names are dropped for positional
# ones. The second number is what `convert --positional` buys.
#
# A conversion counts only when the file reads back and solves to the same
# status and objective line, because a file that is written and not read
# is not a conversion.
#
# Its working files go under this directory rather than under build/,
# because `make clean` removes build/ and this run takes long enough to
# overlap one. Run from anywhere; it finds the repository from its own
# path. Needs
# `make cli` and the three instance sets.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
J=build/cli/jaos
OUT=${1:-bench/measurements/02-219/work}
mkdir -p "$OUT"
: > "$OUT/rows.txt"
n=0
for f in bench/instances/*.mps bench/instances-infeas/*.mps \
         bench/instances-kennington/*.mps; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .mps)
    want=$($J solve "$f" 2>/dev/null | grep -E '^(status|objective) ')
    plain=refused
    pos=refused
    if $J convert "$f" "$OUT/p.lp" >/dev/null 2>&1; then
        got=$($J solve "$OUT/p.lp" 2>/dev/null | grep -E '^(status|objective) ')
        [ "$got" = "$want" ] && plain=ok || plain=DIFFERS
    fi
    if $J convert "$f" "$OUT/q.lp" --positional >/dev/null 2>&1; then
        got=$($J solve "$OUT/q.lp" 2>/dev/null | grep -E '^(status|objective) ')
        [ "$got" = "$want" ] && pos=ok || pos=DIFFERS
    fi
    printf '%s %s %s\n' "$b" "$plain" "$pos" >> "$OUT/rows.txt"
    n=$((n + 1))
done
rm -f "$OUT/p.lp" "$OUT/q.lp"
echo "instances=$n"
awk '{p[$2]++; q[$3]++}
     END { printf "plain:      ok=%d refused=%d differs=%d\n", p["ok"], p["refused"], p["DIFFERS"];
           printf "positional: ok=%d refused=%d differs=%d\n", q["ok"], q["refused"], q["DIFFERS"] }' \
    "$OUT/rows.txt"
