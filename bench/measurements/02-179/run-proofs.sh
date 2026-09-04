#!/usr/bin/env bash
# D274. What jaos_verify does on every gate basis. Writes proofs.txt beside
# this script. Not a gate tool.
#
#   bash bench/measurements/02-179/run-proofs.sh          # all three sets
#   bash bench/measurements/02-179/run-proofs.sh netlib   # the first only
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_verify >/dev/null 2>&1 || { echo "library build failed"; exit 1; }
objs=$(ls build/dev/*.o | grep -v unity)

$CC -std=c23 -Wall -Wextra -Wpedantic -ffp-contract=off -g -O2 -DNDEBUG \
    -Iinclude -Isrc "$here/proofs.c" $objs -o build/dev/proofs -lm || exit 1

case "${1:-all}" in
    netlib) sets="bench/instances" ;;
    *)      sets="bench/instances bench/instances-infeas bench/instances-kennington" ;;
esac
files=""
for d in $sets; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
    files="$files $d/*.mps"
done

out="$here/proofs.txt"
[ "${1:-all}" = "netlib" ] && out="$here/proofs-netlib.txt"

{
    echo "# instrument: bench/measurements/02-179/proofs.c"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    # shellcheck disable=SC2086
    ./build/dev/proofs $files
} > "$out"
rc=$?

tail -5 "$out"
echo "proofs exit=$rc  ->  $out"
exit $rc
