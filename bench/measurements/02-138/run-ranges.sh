#!/usr/bin/env bash
# Measures the MPS writer's third refusal: a ranged row whose bounds no
# RANGES entry reproduces exactly. Writes ranges.txt beside this script.
#
# `ranges.c` includes `src/write.c` because `range_form` is static, so
# write.o is left out of the link.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-138/run-ranges.sh [pairs per shape] [seed]
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2

CC=${CC:-gcc-14}
mkdir -p build/dev
make build/dev/test_write >/dev/null 2>&1 || { echo "library build failed"; exit 2; }

objs=$(ls build/dev/*.o | grep -v unity | grep -v '/write\.o$')

$CC -std=c23 -Wall -Wextra -ffp-contract=off -g -O1 \
    -Iinclude -Isrc "$here/ranges.c" $objs -o build/dev/ranges -lm || exit 2

{
    echo "# range_form: which MPS form is picked, and when neither is exact"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
    ./build/dev/ranges "$@"
} > "$here/ranges.txt"
rc=$?

tail -8 "$here/ranges.txt"
echo "ranges exit=$rc"
exit $rc
