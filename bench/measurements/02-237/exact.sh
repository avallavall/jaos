#!/usr/bin/env bash
# The reading behind 02-237. Builds `exact.c` against the library in the
# tree and runs it over six seeds; `exact.c` exits non-zero when any of
# its six properties broke. Its control is inside the harness: every proof
# file it accepts is also checked with one column value replaced, and
# that file has to be refused.
#
#   exact.sh            the reading, six seeds of 1000
#
# Needs `make all` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-237
OUT=${1:-/tmp/exact-02-237}
mkdir -p "$OUT/proofs"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/exact" \
    "$HERE/exact.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in 1 2 3 4 5 6; do
    echo "== seed $seed"
    "$OUT/exact" 1000 "$seed" "$OUT/proofs" | tail -8 || fail=1
done
exit $fail
