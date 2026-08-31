#!/usr/bin/env bash
# Measures how many digits `wr_num` prints and whether every value it prints
# reads back as the same double. Writes digits.txt beside this script.
#
# `digits.c` includes `src/write.c` because `wr_num` is static, so write.o is
# left out of the link.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-138/run-digits.sh [count] [seed]
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_write >/dev/null 2>&1 || { echo "library build failed"; exit 2; }

# Every library object except write.o, which digits.c includes as source.
objs=$(ls build/dev/*.o | grep -v unity | grep -v '/write\.o$')

$CC -std=c23 -Wall -Wextra -ffp-contract=off -g -O1 \
    -Iinclude -Isrc "$here/digits.c" $objs -o build/dev/digits -lm || exit 2

{
    echo "# wr_num digit split and round trip"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
    ./build/dev/digits "$@"
} > "$here/digits.txt"
rc=$?

tail -8 "$here/digits.txt"
echo "digits exit=$rc"
exit $rc
