#!/usr/bin/env bash
# The reading behind 02-248. Builds `push.c` against the library in the
# tree and runs it over six seeds; `push.c` exits non-zero when any of its
# five properties broke. Then the tool solves every 50th model, written
# as LP and as MPS, with --check, and the runner counts the check_ok lines.
#
#   push.sh           the reading, six seeds of 1000
#   RUNS=200 SEEDS=1 push.sh   one short seed
#
# Rebuilds the library and the tool first. Run from anywhere; it finds
# the repository from its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-248
OUT=${1:-/tmp/push-02-248}
rm -rf "$OUT"
mkdir -p "$OUT"

make all cli >/dev/null || exit 1
gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/push" \
    "$HERE/push.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    "$OUT/push" "${RUNS:-1000}" "$seed" "$OUT" | tail -8 || fail=1
done
cli_ok=0
cli_bad=0
for f in "$OUT"/s*-m*.lp "$OUT"/s*-m*.mps; do
    [ -e "$f" ] || continue
    line=$(build/cli/jaos solve --check "$f" 2>/dev/null | grep '^check_ok')
    if [ "$line" = "check_ok yes" ]; then
        cli_ok=$((cli_ok + 1))
    else
        cli_bad=$((cli_bad + 1))
        echo "cli: $f: $line"
    fi
done
echo "cli: check_ok yes on $cli_ok, not on $cli_bad"
[ "$cli_bad" -eq 0 ] || fail=1
exit $fail
