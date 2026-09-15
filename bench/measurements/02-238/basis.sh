#!/usr/bin/env bash
# The reading behind 02-238. Builds `basis.c` against the library in the
# tree and runs it over six seeds; `basis.c` exits non-zero when any of
# its six properties broke. Every 50th model is dumped and run through
# the CLI chain: `verify --basis` on a foreign-spelled basis file, the
# proof file it writes judged by `check --proof`, and a broken basis
# refused with the stage the harness expects.
#
#   basis.sh            the reading, six seeds of 1000
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-238
OUT=${1:-/tmp/basis-02-238}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/basis" \
    "$HERE/basis.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in 1 2 3 4 5 6; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/basis" 1000 "$seed" "$d" 50 | tail -8 || fail=1
    cli=0 cli_ok=0 bad=0 bad_ok=0
    for mps in "$d"/m*.mps; do
        [ -e "$mps" ] || continue
        b=${mps%.mps}
        want=$(head -1 "$b.expect")
        cli=$((cli + 1))
        got=$("$JAOS" verify "$mps" --basis "$b.bas" --proof "$b.proof" 2>"$b.err" | head -1)
        rc=${PIPESTATUS[0]}
        case "$want" in
            optimal) if [ "$got" = "proof optimal" ] && [ "$rc" = 0 ] &&
                        "$JAOS" check "$mps" --proof "$b.proof" 2>/dev/null | grep -q '^proof holds$'; then
                        cli_ok=$((cli_ok + 1)); else echo "CLI $b: wanted optimal, got '$got' rc=$rc"; fi ;;
            refused) if [ "$got" = "proof refused" ] && [ "$rc" = 3 ]; then cli_ok=$((cli_ok + 1));
                     else echo "CLI $b: wanted refused, got '$got' rc=$rc"; fi ;;
            *) echo "CLI $b: harness expected '$want' from a solver's basis" ;;
        esac
        if [ -e "$b.bad.bas" ]; then
            bad=$((bad + 1))
            stage=$(sed -n '2p' "$b.expect" | cut -d' ' -f2)
            out=$("$JAOS" verify "$mps" --basis "$b.bad.bas" 2>/dev/null)
            rc=$?
            if [ "$rc" = 1 ] && printf '%s\n' "$out" | grep -q '^proof broken$' &&
               printf '%s\n' "$out" | grep -q "^stage $stage$"; then
                bad_ok=$((bad_ok + 1))
            else
                echo "CLI $b.bad: wanted broken $stage rc=1, got rc=$rc"; printf '%s\n' "$out" | head -3
            fi
        fi
    done
    echo "cli $cli ok $cli_ok broken $bad ok $bad_ok"
    [ "$cli" = "$cli_ok" ] && [ "$bad" = "$bad_ok" ] || fail=1
done
exit $fail
