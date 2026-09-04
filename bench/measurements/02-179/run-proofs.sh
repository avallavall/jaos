#!/usr/bin/env bash
# D274. What jaos_verify does on every gate basis. Writes proofs.txt beside
# this script. Not a gate tool.
#
#   bash bench/measurements/02-179/run-proofs.sh          # all three sets
#   bash bench/measurements/02-179/run-proofs.sh netlib   # the first only
#
# Links the release objects. The committed proofs.txt was taken against the
# dev objects, before `objs.sh` existed, and its header says so; to reproduce
# it exactly, run with JAOS_OBJS=dev. The verdicts are the same either way --
# only the seconds move, and by about 7x.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

CC=${CC:-gcc-14}
. bench/measurements/objs.sh
jaos_objs || exit 1

$CC $JAOS_OBJS_FLAGS -Iinclude -Isrc "$here/proofs.c" $JAOS_OBJS_LIST \
    -o "build/proofs-$JAOS_OBJS_KIND" -lm || exit 1

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
    echo "# objects: $JAOS_OBJS_KIND -- the seconds below mean nothing without this"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    # shellcheck disable=SC2086
    "./build/proofs-$JAOS_OBJS_KIND" $files
} > "$out"
rc=$?

tail -5 "$out"
echo "proofs exit=$rc  ->  $out"
exit $rc
