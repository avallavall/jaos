#!/usr/bin/env bash
# The reading behind 02-240. Builds `refuse.c` against the library in the
# tree and runs it over six seeds; `refuse.c` exits non-zero when any of
# its three properties broke. Every 50th model leaves one corrupted MPS
# and one corrupted LP behind, and the tool has to refuse each with exit
# 5 and `line N:` on stderr, plain and gzipped.
#
#   refuse.sh           the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-240
OUT=${1:-/tmp/refuse-02-240}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/refuse" \
    "$HERE/refuse.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/refuse" "${RUNS:-1000}" "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 ok=0
    for keep in "$d"/m*.keep.mps "$d"/m*.keep.lp; do
        [ -e "$keep" ] || continue
        want=$(cat "${keep/.keep./.bad.}.expect")
        cli=$((cli + 1))
        good=1
        "$JAOS" stats "$keep" >/dev/null 2>"$keep.err"; rc=$?
        { [ "$rc" = 5 ] && grep -q "line $want:" "$keep.err"; } ||
            { good=0; echo "CLI $keep: rc=$rc wanted line $want"; head -2 "$keep.err"; }
        gzip -c "$keep" > "$keep.gz"
        "$JAOS" stats "$keep.gz" >/dev/null 2>"$keep.gz.err"; rc=$?
        { [ "$rc" = 5 ] && grep -q "line $want:" "$keep.gz.err"; } ||
            { good=0; echo "CLI $keep.gz: rc=$rc wanted line $want"; head -2 "$keep.gz.err"; }
        [ "$good" = 1 ] && ok=$((ok + 1))
    done
    echo "cli $cli ok $ok"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
