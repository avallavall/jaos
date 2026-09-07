#!/usr/bin/env bash
# D340's evidence: every gate instance written twice, plain and compressed,
# and the compressed one read back by the REAL gzip. Three things per
# instance:
#   1. `gzip -t` accepts the file JAOS wrote.
#   2. `gzip -dc` of it is byte-identical to the plain file JAOS wrote.
#   3. its size is recorded beside `gzip -9` of the same bytes, which is
#      what says how much a dynamic-Huffman encoder would have bought.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make cli` first, and the three instance sets fetched.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
J=build/cli/jaos
OUT=${1:-/tmp/gzcheck}
mkdir -p "$OUT"
: > "$OUT/sizes.txt"
bad=0
n=0
for f in bench/instances/*.mps bench/instances-infeas/*.mps \
         bench/instances-kennington/*.mps; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .mps)
    plain=$OUT/$b.mps
    packed=$OUT/$b.mps.gz
    $J convert "$f" "$plain" >/dev/null 2>&1 || { echo "CONVERT-PLAIN $b"; bad=1; continue; }
    $J convert "$f" "$packed" >/dev/null 2>&1 || { echo "CONVERT-GZ $b"; bad=1; continue; }
    gzip -t "$packed" 2>/dev/null || { echo "GZIP-T $b"; bad=1; continue; }
    gzip -dc "$packed" > "$OUT/$b.back" 2>/dev/null || { echo "GUNZIP $b"; bad=1; continue; }
    cmp -s "$plain" "$OUT/$b.back" || { echo "DIFFER $b"; bad=1; continue; }
    gzip -9 -c "$plain" > "$OUT/$b.ref.gz" 2>/dev/null
    printf '%s %s %s %s\n' "$b" "$(stat -c %s "$plain")" \
        "$(stat -c %s "$packed")" "$(stat -c %s "$OUT/$b.ref.gz")" \
        >> "$OUT/sizes.txt"
    rm -f "$OUT/$b.back" "$OUT/$b.ref.gz" "$plain" "$packed"
    n=$((n + 1))
done
echo "instances=$n bad=$bad"
awk '{p+=$2; j+=$3; g+=$4} END {
    printf "plain=%d jaos_gz=%d gzip9=%d ratio_jaos=%.4f ratio_gzip9=%.4f jaos_over_gzip=%.4f\n",
        p, j, g, j/p, g/p, j/g }' "$OUT/sizes.txt"
exit $bad
