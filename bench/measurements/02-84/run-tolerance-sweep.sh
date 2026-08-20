#!/bin/bash
# The two cheapest questions about `pilot`'s 2.31e-05, both through the
# public interface: does the gap survive the reference build, and does either
# tolerance a caller owns close it?
#
# Two binaries, with `make clean` semantics between them — separate output
# directories, so neither can be the other's objects. `make` decides from
# timestamps and does not track a change in EXTRA_CFLAGS, which is how three
# of the five build configurations were broken for a whole session (D154).
#
# Usage: run-tolerance-sweep.sh [instance] [reference]
# Default: pilot, and the Koch optimum bench/netlib.manifest carries for it.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT

name=${1:-pilot}
inst=bench/instances/$name.mps
ref=${2:-}
if [ -z "$ref" ]; then
    ref=$(awk -v n="$name" '$1==n {print $5}' bench/netlib.manifest)
fi
[ -n "$ref" ] || { echo "no reference for $name" >&2; exit 3; }

P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
mkdir -p "$D/ship" "$D/ref"
gcc-14 $P            src/*.c "$here/tolerance-sweep.c" -o "$D/ship/s" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c "$here/tolerance-sweep.c" -o "$D/ref/s" -lm || exit 2

{
    echo "# $name, reference $ref (Koch). gap = published - reference."
    echo "# Positive is worse on a minimization. row/dual/gappos/cert are"
    echo "# jaos_check_solution's, at tol 1e-6."
    "$D/ship/s" "$inst" "$ref" shipping
    "$D/ref/s"  "$inst" "$ref" reference
} | tee "$here/tolerance-sweep-$name.txt"
