#!/usr/bin/env bash
# The reading behind 02-235. Builds `certs.c` against the library in the
# tree and runs it over six seeds; `certs.c` exits non-zero when any of
# its eight properties broke. Its control is inside the harness: every
# certificate it accepts is also read with its sign flipped and with its
# entries zeroed, and both have to be refused.
#
#   certs.sh            the reading, six seeds of 2000
#
# Needs `make all` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-235
OUT=${1:-/tmp/certs-02-235}
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/certs" \
    "$HERE/certs.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in 1 2 3 4 5 6; do
    echo "== seed $seed"
    "$OUT/certs" 2000 "$seed" | tail -8 || fail=1
done
exit $fail
