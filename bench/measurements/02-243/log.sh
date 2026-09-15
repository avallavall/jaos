#!/usr/bin/env bash
# The reading behind 02-243. Builds `log.c` against the library in the
# tree and runs it over six seeds; `log.c` exits non-zero when any of its
# six properties broke. Every 50th model is written out and the tool runs
# on it: `--log off` leaves stderr empty, `--log summary` fills it and
# leaves stdout as without the flag above the `time` line, `--log
# progress` gives at least as many lines as `summary`, and `--quiet`
# prints the `status` line alone.
#
#   log.sh              the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-243
OUT=${1:-/tmp/log-02-243}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/log" \
    "$HERE/log.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/log" "${RUNS:-1000}" "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 ok=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        cli=$((cli + 1))
        good=1
        "$JAOS" solve "$mps" > "$b.plain" 2>"$b.plain.err"
        "$JAOS" solve "$mps" --log off > "$b.off" 2>"$b.off.err"
        "$JAOS" solve "$mps" --log summary > "$b.sum" 2>"$b.sum.err"
        "$JAOS" solve "$mps" --log progress > "$b.prog" 2>"$b.prog.err"
        "$JAOS" solve "$mps" --quiet > "$b.quiet" 2>"$b.quiet.err"
        [ -s "$b.off.err" ] && { good=0; echo "CLI $b: --log off wrote to stderr"; }
        [ -s "$b.sum.err" ] || { good=0; echo "CLI $b: --log summary wrote nothing"; }
        cmp -s <(grep -v '^time ' "$b.plain") <(grep -v '^time ' "$b.sum") ||
            { good=0; echo "CLI $b: stdout differs with --log summary"; }
        [ "$(wc -l < "$b.prog.err")" -ge "$(wc -l < "$b.sum.err")" ] ||
            { good=0; echo "CLI $b: progress has fewer lines than summary"; }
        [ "$(wc -l < "$b.quiet")" = 1 ] && grep -q '^status ' "$b.quiet" ||
            { good=0; echo "CLI $b: --quiet is not one status line"; head -3 "$b.quiet"; }
        [ "$good" = 1 ] && ok=$((ok + 1))
    done
    echo "cli $cli ok $ok"
    [ "$cli" = "$ok" ] || fail=1
done
exit $fail
