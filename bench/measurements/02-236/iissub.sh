#!/usr/bin/env bash
# The reading behind 02-236. Builds `iissub.c` against the library in the
# tree and runs it over six seeds; `iissub.c` exits non-zero when any of
# its seven properties broke. The irreducibility property is its own
# control: every member side is dropped in turn and the model written
# without it has to solve feasible, so a subsystem carrying one side too
# many is caught on that side.
#
#   iissub.sh            the reading, six seeds of 1000
#
# Needs `make all` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-236
OUT=${1:-/tmp/iissub-02-236}
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/iissub" \
    "$HERE/iissub.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in 1 2 3 4 5 6; do
    echo "== seed $seed"
    "$OUT/iissub" 1000 "$seed" | tail -8 || fail=1
done
exit $fail
