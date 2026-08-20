#!/bin/bash
# Is what `finnis` has left a summation error or a different point?
#
# TODO.md, from D172: the published objective now agrees with the checker on
# 109 of 110 instances, and `finnis` is the one that does not, 7.62e-05 from
# Koch's optimum. Both numbers that could settle it round: the published one
# sums doubles, and the checker multiplies in `long double`, whose 64-bit
# mantissa cannot hold a binary64 product's 106 bits.
#
# This computes c'x + c0 with no rounding at all, over the same published
# point, and the exact row activities beside it. Offline: it reads the
# library through its public interface plus `obj_offset`, changes nothing,
# and is not a campaign.
#
# Usage: run-exact-objective.sh [something.manifest] [instance.mps ...]
# Default: bench/netlib.manifest and every instance of the standard set. The
# output file is named after the manifest, so a Kennington run does not
# overwrite a netlib one.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/exact-objective.c" -o "$D/s" -lm || exit 2
v=
case "${1:-}" in
    -v) v=-v; shift ;;
esac
manifest=bench/netlib.manifest
case "${1:-}" in
    *.manifest) manifest=$1; shift ;;
esac
# The record is rewritten only by a run over a whole set. A run naming its
# own instances prints and keeps its hands off the file, so a one-instance
# check cannot leave a record that looks like a campaign.
whole=no
if [ $# -eq 0 ]; then
    whole=yes
    case "$manifest" in
        *kennington*) set -- bench/instances-kennington/*.mps ;;
        *)            set -- bench/instances/*.mps ;;
    esac
fi
# The instrument is validated before every reading, and the readings are
# refused if it fails. An accumulator nobody checked would produce a table of
# confident wrong numbers and nothing in the output would say so.
# Written to the file first, because the exit status of a pipeline is tee's
# and not the program's — the trap this repository has hit before.
"$D/s" --selftest > "$here/selftest.txt" 2>&1
rc=$?
cat "$here/selftest.txt"
if [ "$rc" -ne 0 ]; then
    echo "the accumulator failed its own cases; no reading taken" >&2
    exit 4
fi

tag=$(basename "$manifest" .manifest)
if [ "$whole" = yes ]; then
    "$D/s" $v "$manifest" "$@" | tee "$here/exact-objective-$tag.txt"
else
    "$D/s" $v "$manifest" "$@"
fi
