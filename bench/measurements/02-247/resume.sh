#!/usr/bin/env bash
# The reading behind 02-247. Builds `resume.c` against the library in the
# tree and runs it over six seeds; `resume.c` exits non-zero when any of
# its five properties broke.
#
#   resume.sh           the reading, six seeds of 1000
#
# Needs `make all` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-247
OUT=${1:-/tmp/resume-02-247}
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/resume" \
    "$HERE/resume.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    "$OUT/resume" "${RUNS:-1000}" "$seed" "$OUT" | tail -8 || fail=1
done
exit $fail
