#!/usr/bin/env bash
# The reading behind 02-239. Builds `point.c` against the library in the
# tree and runs it over six seeds; `point.c` exits non-zero when any of
# its six properties broke. Every 50th model is dumped and run through
# the CLI: `check --point` on the optimum in a foreign shape, then with
# `--duals`, then on a point outside its box.
#
#   point.sh            the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-239
OUT=${1:-/tmp/point-02-239}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/point" \
    "$HERE/point.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in 1 2 3 4 5 6; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/point" 1000 "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 ok=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        cli=$((cli + 1))
        good=1
        out=$("$JAOS" check "$mps" --point "$b.pt" 2>"$b.err"); rc=$?
        { [ "$rc" = 0 ] && printf '%s\n' "$out" | grep -q '^status point$' &&
          printf '%s\n' "$out" | grep -q '^primal_feasible yes$' &&
          printf '%s\n' "$out" | grep -q '^checked_duals no$'; } ||
            { good=0; echo "CLI $b --point: rc=$rc"; printf '%s\n' "$out" | head -3; cat "$b.err"; }
        out=$("$JAOS" check "$mps" --point "$b.pt" --duals "$b.duals" 2>"$b.err"); rc=$?
        { [ "$rc" = 0 ] && printf '%s\n' "$out" | grep -q '^dual_feasible yes$' &&
          printf '%s\n' "$out" | grep -q '^checked_duals yes$'; } ||
            { good=0; echo "CLI $b --duals: rc=$rc"; printf '%s\n' "$out" | head -3; cat "$b.err"; }
        out=$("$JAOS" check "$mps" --point "$b.bad.pt" 2>"$b.err"); rc=$?
        { [ "$rc" = 1 ] && printf '%s\n' "$out" | grep -q '^primal_feasible no$'; } ||
            { good=0; echo "CLI $b bad: rc=$rc"; printf '%s\n' "$out" | head -3; cat "$b.err"; }
        [ "$good" = 1 ] && ok=$((ok + 1))
    done
    echo "cli $cli ok $ok"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
