#!/usr/bin/env bash
# Round-trips every gate instance through jaos_write_mps and compares the
# model field by field. Writes roundtrip.txt beside this script.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-138/run-roundtrip.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$(dirname "$0")/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev

# The library objects the dev build already produces; -Isrc because the
# comparison reads the model struct directly.
make build/dev/test_write >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O1 \
    -Iinclude -Isrc "$here/roundtrip.c" $objs -o build/dev/roundtrip -lm || exit 1

sets="bench/instances bench/instances-infeas bench/instances-kennington"
files=""
for d in $sets; do
    [ -d "$d" ] || { echo "missing $d — run bench/fetch.sh"; exit 1; }
    files="$files $(ls "$d"/*.mps)"
done

{
    echo "# jaos_write_mps round trip over every gate instance"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# read -> write MPS -> read back -> compare every field with =="
    echo
    ./build/dev/roundtrip $files
} > "$here/roundtrip.txt"
rc=$?

tail -3 "$here/roundtrip.txt"
echo "roundtrip exit=$rc"
exit $rc
