#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
J=build/cli/jaos
OUT=${1:-bench/measurements/02-275/rows.txt}
: > "$OUT"
proved=0
broken=0
refused=0
other=0
for f in bench/instances/*.mps bench/instances-kennington/*.mps; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .mps)
    out=$("$J" verify "$f" 2>/dev/null)
    rc=$?
    word=$(printf '%s\n' "$out" | sed -n 's/^proof //p' | head -1)
    case "$word" in
        optimal) proved=$((proved + 1)) ;;
        broken) broken=$((broken + 1)) ;;
        refused) refused=$((refused + 1)) ;;
        *) other=$((other + 1)); word="none" ;;
    esac
    echo "$b $word $rc" >> "$OUT"
done
echo "instances=$((proved + broken + refused + other))"
echo "proved=$proved broken=$broken refused=$refused none=$other"
