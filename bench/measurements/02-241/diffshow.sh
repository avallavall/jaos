#!/usr/bin/env bash
# The reading behind 02-241. Builds `diffshow.c` against the library in
# the tree, which writes the files, and runs the tool over them:
#
#   P1 `diff A.mps A.mps` and `diff A.mps A.lp` print `differences 0`
#      and exit 0
#   P2 `diff A.mps B.mps`, B being A with one edit, exits 1, prints the
#      edit's line first and `differences` at least 1
#   P3 `show --row` and `show --col` print exactly the text the harness
#      formatted from the model, and a name the model has not got exits 5
#
#   diffshow.sh         the reading, six seeds of 300
#
# Needs `make all cli` first. Run from anywhere; it finds the repository from
# its own path.
#
# SPDX-License-Identifier: Apache-2.0
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-241
OUT=${1:-/tmp/diffshow-02-241}
JAOS=build/cli/jaos
rm -rf "$OUT"
mkdir -p "$OUT"

gcc-14 -std=c23 -O2 -ffp-contract=off -Iinclude -o "$OUT/diffshow" \
    "$HERE/diffshow.c" build/release/libjaos.a -lm || exit 1
fail=0
for seed in ${SEEDS:-1 2 3 4 5 6}; do
    echo "== seed $seed"
    d="$OUT/s$seed"
    mkdir -p "$d"
    "$OUT/diffshow" "${RUNS:-300}" "$seed" "$d" | tail -3 || fail=1
    same=0 same_ok=0 lp=0 lp_ok=0 edit=0 edit_ok=0 show=0 show_ok=0
    for a in "$d"/m*.a.mps; do
        b=${a%.a.mps}
        same=$((same + 1))
        out=$("$JAOS" diff "$a" "$a" 2>&1); rc=$?
        [ "$rc" = 0 ] && [ "$out" = "differences 0" ] && same_ok=$((same_ok + 1)) ||
            { echo "DIFF $a self: rc=$rc"; printf '%s\n' "$out" | head -3; }
        if [ -e "$b.a.lp" ]; then
            lp=$((lp + 1))
            out=$("$JAOS" diff "$a" "$b.a.lp" 2>&1); rc=$?
            [ "$rc" = 0 ] && [ "$out" = "differences 0" ] && lp_ok=$((lp_ok + 1)) ||
                { echo "DIFF $a lp: rc=$rc"; printf '%s\n' "$out" | head -3; }
        fi
        edit=$((edit + 1))
        want=$(cat "$b.expect")
        out=$("$JAOS" diff "$a" "$b.b.mps" 2>&1); rc=$?
        first=$(printf '%s\n' "$out" | head -1)
        count=$(printf '%s\n' "$out" | sed -n 's/^differences //p')
        if [ "$rc" = 1 ] && [ -n "$count" ] && [ "$count" -ge 1 ] &&
           [ "${first:0:${#want}}" = "$want" ]; then
            edit_ok=$((edit_ok + 1))
        else
            echo "DIFF $a edit: rc=$rc wanted '$want'"; printf '%s\n' "$out" | head -3
        fi
        show=$((show + 1))
        good=1
        name=$(head -1 "$b.showrow")
        "$JAOS" show "$a" --row "$name" > "$b.showrow.got" 2>&1; rc=$?
        { [ "$rc" = 0 ] && cmp -s "$b.showrow.got" <(tail -n +2 "$b.showrow"); } ||
            { good=0; echo "SHOW $a --row $name: rc=$rc"; diff "$b.showrow.got" <(tail -n +2 "$b.showrow") | head -4; }
        name=$(head -1 "$b.showcol")
        "$JAOS" show "$a" --col "$name" > "$b.showcol.got" 2>&1; rc=$?
        { [ "$rc" = 0 ] && cmp -s "$b.showcol.got" <(tail -n +2 "$b.showcol"); } ||
            { good=0; echo "SHOW $a --col $name: rc=$rc"; diff "$b.showcol.got" <(tail -n +2 "$b.showcol") | head -4; }
        "$JAOS" show "$a" --row NOSUCH >/dev/null 2>&1; rc=$?
        [ "$rc" = 5 ] || { good=0; echo "SHOW $a --row NOSUCH: rc=$rc"; }
        [ "$good" = 1 ] && show_ok=$((show_ok + 1))
    done
    echo "same $same ok $same_ok lp $lp ok $lp_ok edit $edit ok $edit_ok show $show ok $show_ok"
    [ "$same" = "$same_ok" ] && [ "$lp" = "$lp_ok" ] && [ "$edit" = "$edit_ok" ] && [ "$show" = "$show_ok" ] || fail=1
    rm -f "$d"/m*
done
exit $fail
