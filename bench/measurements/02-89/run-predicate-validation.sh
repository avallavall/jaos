#!/bin/bash
# D177 — does the predicate fire where it could not before?
#
# The case it has to catch: an instance whose suboptimality bound doubles
# while staying far below the old floor of 1e-9. `adlittle` sits at 1.52e-15,
# so halving its baseline value gives a ratio of 2.2x that the old predicate
# cannot see however large it gets.
#
# Three runs, one variable each. Both runners are built from the SAME gcc
# line, so the only thing that differs between them is the source.
#
#   parent    + doctored baseline  -> must stay quiet
#   candidate + doctored baseline  -> must report REGRESSED
#   candidate + committed baseline -> must stay quiet
#
# Usage: run-predicate-validation.sh [ref]      (default: HEAD~1)
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
ref="${1:-HEAD~1}"
inst=adlittle
tmp="$(mktemp -d)" || exit 2
trap 'rm -rf "$tmp"' EXIT

out() { printf '%s\n' "$*"; }

{
out "== D177 predicate validation, tree $(git rev-parse --short HEAD) against $ref =="
out ""

git show "$ref:bench/run.c" > "$tmp/parent.c" || exit 2
cp bench/run.c "$tmp/candidate.c" || exit 2

# The canary. If these two read the same, the experiment has no variable in
# it and every verdict below is about one binary measured twice (D82).
out "-- the one variable:"
out "   $ref        $(grep -m1 'RSUB_FLOOR =' "$tmp/parent.c" | tr -s ' ')"
out "   working tree $(grep -m1 'RSUB_FLOOR =' "$tmp/candidate.c" | tr -s ' ')"
if [ "$(grep -m1 'RSUB_FLOOR =' "$tmp/parent.c")" = \
     "$(grep -m1 'RSUB_FLOOR =' "$tmp/candidate.c")" ]; then
    out "   ABORT: the two sources agree, so there is nothing to compare."
    exit 3
fi
out ""

make -s "build/release/libjaos.a" >/dev/null 2>&1 || make >/dev/null 2>&1 || exit 2
for which in parent candidate; do
    gcc-14 -O2 -std=c23 -Iinclude -Isrc "$tmp/$which.c" build/release/libjaos.a \
           -o "$tmp/run-$which" -lm 2>"$tmp/$which.build" || {
        out "build failed for $which"; cat "$tmp/$which.build"; exit 2; }
done

# The doctored baseline: one field of one line, and nothing else.
sed "s/^\($inst .*\) [0-9.e+-]*\$/\1 7.0e-16/" bench/netlib.baseline \
    > "$tmp/doctored.baseline"
out "-- the doctored baseline, one field of one line:"
out "   committed  $(grep "^$inst " bench/netlib.baseline | tr -s ' ')"
out "   doctored   $(grep "^$inst " "$tmp/doctored.baseline" | tr -s ' ')"
out ""

run() {
    "$1" -j 1 -b "$2" "$inst" 2>&1 \
        | grep -E 'REGRESSED|baseline:|rsub=' \
        | sed -E 's/.*(rsub=[^)]*)\).*/   live \1/'
}
out "-- $ref (floor 1e-9), doctored baseline: must stay quiet"
run "$tmp/run-parent" "$tmp/doctored.baseline" | sed 's/^/   /'
out ""
out "-- working tree (floor 1e-16), same baseline: must report REGRESSED"
run "$tmp/run-candidate" "$tmp/doctored.baseline" | sed 's/^/   /'
out ""
out "-- working tree, committed baseline: must stay quiet"
run "$tmp/run-candidate" bench/netlib.baseline | sed 's/^/   /'
} | tee "$here/predicate-validation.txt"
