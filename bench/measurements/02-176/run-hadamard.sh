#!/usr/bin/env bash
# D271. The a-priori budget for an exact verifier, over every gate instance.
# Writes hadamard.txt beside this script. Not a gate tool.
#
#   bash bench/measurements/02-176/run-hadamard.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_exact >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O2 -DNDEBUG \
    -Iinclude -Isrc "$here/hadamard.c" $objs -o build/dev/hadamard -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

{
    echo "# instrument: bench/measurements/02-176/hadamard.c"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    ./build/dev/hadamard bench/instances/*.mps bench/instances-infeas/*.mps \
                         bench/instances-kennington/*.mps
} > "$here/hadamard.txt"
rc=$?

tail -5 "$here/hadamard.txt"
echo "hadamard exit=$rc"
exit $rc
