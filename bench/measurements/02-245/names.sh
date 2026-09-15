#!/usr/bin/env bash
# The reading behind 02-245. Builds `names.c` against the library in the
# tree and runs it over six seeds; `names.c` exits non-zero when any of
# its eight properties broke. Every 50th model is written out as MPS with
# one 255-byte column name beside it, and the tool has to find that
# column by `show --col`, print it under that name, and `convert` the
# file to LP and back with `diff` reporting only name lines.
#
#   names.sh            the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-245
OUT=${1:-/tmp/names-02-245}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/names" \
    "$HERE/names.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/names" "${RUNS:-1000}" "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 ok=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        name=$(cat "$b.long")
        cli=$((cli + 1))
        good=1
        out=$("$JAOS" show "$mps" --col "$name" 2>&1); rc=$?
        { [ "$rc" = 0 ] && [ "$(printf '%s\n' "$out" | head -1)" = "col $name" ]; } ||
            { good=0; echo "CLI $b: show --col rc=$rc"; printf '%s\n' "$out" | head -2; }
        "$JAOS" convert "$mps" "$b.lp" >/dev/null 2>&1 && "$JAOS" convert "$b.lp" "$b.back.mps" >/dev/null 2>&1 ||
            { good=0; echo "CLI $b: convert failed"; }
        out=$("$JAOS" diff "$mps" "$b.back.mps" 2>&1)
        printf '%s\n' "$out" | grep -v '^col_name \|^row_name \|^differences ' | grep -q . &&
            { good=0; echo "CLI $b: diff after LP round trip reports more than names"; printf '%s\n' "$out" | head -3; }
        [ "$good" = 1 ] && ok=$((ok + 1))
    done
    echo "cli $cli ok $ok"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
