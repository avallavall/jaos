#!/usr/bin/env bash
# D343's evidence: the subsystem `jaos iis --write` builds is a model, and
# a model is only right if it answers. Every reference infeasibility gets
# its IIS written out and solved again, and the written model has to read
# INFEASIBLE.
#
# It also records the two sizes, which is what says the subsystem is a
# subsystem: rows and columns of the original against rows and columns of
# the file.
#
# Run from anywhere; it finds the repository from its own path. Needs
# `make cli` first and the infeasible set fetched.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
J=build/cli/jaos
OUT=${1:-/tmp/iissub}
mkdir -p "$OUT"
: > "$OUT/sizes.txt"
bad=0
n=0
for f in bench/instances-infeas/*.mps; do
    [ -e "$f" ] || continue
    b=$(basename "$f" .mps)
    sub=$OUT/$b.sub.mps
    if ! $J iis "$f" --write "$sub" > "$OUT/$b.iis" 2>&1; then
        echo "IIS-FAILED $b"; bad=1; continue
    fi
    st=$($J solve "$sub" 2>/dev/null | sed -n 's/^status //p')
    if [ "$st" != "infeasible" ]; then
        echo "NOT-INFEASIBLE $b -> $st"; bad=1; continue
    fi
    printf '%s %s %s %s %s %s\n' "$b" \
        "$($J stats "$f" | sed -n 's/^rows //p')" \
        "$($J stats "$f" | sed -n 's/^columns //p')" \
        "$(sed -n 's/^subsystem_rows //p' "$OUT/$b.iis")" \
        "$(sed -n 's/^subsystem_columns //p' "$OUT/$b.iis")" \
        "$(sed -n 's/^members //p' "$OUT/$b.iis")" >> "$OUT/sizes.txt"
    rm -f "$sub" "$OUT/$b.iis"
    n=$((n + 1))
done
echo "instances=$n bad=$bad"
awk '{r+=$2; c+=$3; sr+=$4; sc+=$5; mb+=$6}
     END { printf "rows=%d cols=%d sub_rows=%d sub_cols=%d members=%d row_share=%.4f col_share=%.4f\n",
                  r, c, sr, sc, mb, sr/r, sc/c }' "$OUT/sizes.txt"
exit $bad
