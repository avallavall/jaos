#!/usr/bin/env bash
# D273. The Hadamard bound once the basis is made integral, over every gate
# instance. Writes integral.txt beside this script. Not a gate tool.
#
#   bash bench/measurements/02-178/run-integral.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_exact >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O2 -DNDEBUG \
    -Iinclude -Isrc "$here/integral.c" $objs -o build/dev/integral -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# instrument: bench/measurements/02-178/integral.c"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    ./build/dev/integral bench/instances/*.mps bench/instances-infeas/*.mps \
                         bench/instances-kennington/*.mps
} > "$here/integral.txt"
rc=$?

tail -5 "$here/integral.txt"
echo "integral exit=$rc"
exit $rc
