#!/usr/bin/env bash
# Every dual-side figure the checker publishes, over all three gate sets.
# Writes dual-cover.txt beside this script.
#
# Links the release objects, which is what the gate links (D274, objs.sh).
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-182/run-dual-cover.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
. bench/measurements/objs.sh
jaos_objs || exit 1

$CC $JAOS_OBJS_FLAGS -Iinclude -Isrc "$here/dual-cover.c" \
    $JAOS_OBJS_LIST -o build/dual-cover -lm || exit 1

for d in bench/instances bench/instances-infeas bench/instances-kennington; do
    [ -d "$d" ] || { echo "missing $d -- run bench/fetch.sh"; exit 1; }
done

out="${1:-$here/dual-cover.txt}"
{
    echo "# every figure jaos_check_solution's dual side publishes, plus"
    echo "# jaos_check_certificate and jaos_check_ray, at 17 digits"
    echo "# instrument: ${here#"$PWD"/}/dual-cover.c"
    echo "# objects: $JAOS_OBJS_KIND"
    echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' WITH UNCOMMITTED CHANGES')"
    echo "# check.c long double uses: $(grep -c 'long double' src/check.c)"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo
    ./build/dual-cover bench/instances/*.mps bench/instances-infeas/*.mps \
        bench/instances-kennington/*.mps
} > "$out"
rc=$?

tail -2 "$out"
echo "dual-cover exit=$rc  ->  $out"
exit $rc
