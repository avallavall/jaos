#!/usr/bin/env bash
# The reading behind 02-244. Builds `tol.c` against the library in the
# tree and runs it over six seeds; `tol.c` exits non-zero when any of its
# four properties broke. Every 50th model is written out and the tool
# runs on it: `--primal-tol 1e-5` and `--opt primal_tolerance=1e-5` give
# the same output above the `time` line, `--dual-tol 1e-9` gives the
# output of no flag, and `--primal-tol -1` exits 5.
#
#   tol.sh              the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-244
OUT=${1:-/tmp/tol-02-244}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/tol" \
    "$HERE/tol.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/tol" "${RUNS:-1000}" "$seed" "$d" 50 | tail -12 || fail=1
    cli=0 ok=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        cli=$((cli + 1))
        good=1
        "$JAOS" solve "$mps" > "$b.plain" 2>/dev/null
        "$JAOS" solve "$mps" --primal-tol 1e-5 > "$b.flag" 2>/dev/null
        "$JAOS" solve "$mps" --opt primal_tolerance=1e-5 > "$b.opt" 2>/dev/null
        "$JAOS" solve "$mps" --dual-tol 1e-9 > "$b.dual" 2>/dev/null
        cmp -s <(grep -v '^time ' "$b.flag") <(grep -v '^time ' "$b.opt") ||
            { good=0; echo "CLI $b: --primal-tol and --opt differ"; }
        cmp -s <(grep -v '^time ' "$b.plain") <(grep -v '^time ' "$b.dual") ||
            { good=0; echo "CLI $b: --dual-tol 1e-9 differs from the default"; }
        "$JAOS" solve "$mps" --primal-tol -1 >/dev/null 2>&1; rc=$?
        [ "$rc" = 5 ] || { good=0; echo "CLI $b: --primal-tol -1 rc=$rc"; }
        [ "$good" = 1 ] && ok=$((ok + 1))
    done
    echo "cli $cli ok $ok"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
