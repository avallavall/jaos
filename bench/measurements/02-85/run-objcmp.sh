#!/bin/bash
# Can the naive sum in `settled_objective` pick the wrong round?
#
# TODO.md, from D169's review: `settled_objective` is a naive sum and it does
# not decide a trajectory — it decides which point gets PUBLISHED, through
# `better_point` and `take_best_if_better`. The repair is the one D169 and
# D172 already applied to `jm_model_publish_objective`. What is missing is
# evidence that it changes an answer.
#
# The comparison flips only when the two rounds' objectives are separated by
# no more than the sums' own error. So the decisive quantity is not the error
# but the error BESIDE the separation, at every comparison the settling loop
# makes. This records both, on all three sets.
#
# Instrumented in a COPY of the tree. src/ is read and never written.
# The copy lives outside build/, because `make clean` deletes build/ and
# `make configs` runs it five times (preflight.sh warns about this).
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
d=$(mktemp -d) || exit 2
trap 'rm -rf "$d"' EXIT
cd "$root" || exit 9

# WHICH TREE IS MEASURED, and it is the whole point of the probe.
#
# The question is what the NAIVE sum did, so the copy has to come from a tree
# where `settled_objective` is still naive. Copying the working tree after the
# repair is in it compares the compensated sum against itself and every error
# column reads exactly zero — which is what the first corrected run did, and
# it looks like a clean result rather than a mistake.
#
# Default is the parent commit for that reason. Pass `--from working` to
# measure the tree as it stands.
from=${1:-HEAD}
mkdir -p "$d/src" "$d/include"
if [ "$from" = working ]; then
    cp "$root"/src/*.c "$root"/src/*.h "$d/src/" || exit 2
    cp "$root"/include/*.h "$d/include/" || exit 2
else
    git -C "$root" rev-parse --verify "$from" > /dev/null 2>&1 || {
        echo "no such ref: $from" >&2; exit 3; }
    git -C "$root" archive "$from" src include | tar -x -C "$d" || exit 2
fi
tree=$(git -C "$root" rev-parse --short "$from" 2>/dev/null || echo working)
echo "measuring tree: $from ($tree)" >&2

python3 "$here/patch-objcmp.py" "$d" || exit 2

gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
    -I"$d/include" -I"$d/src" "$d"/src/*.c bench/run.c -o "$d/run" -lm || exit 2

# -j 1: the records go to one stderr and the sets are small enough that the
# wall clock is not what this costs.
{
echo "tree: $from ($tree)" >&2
for set in netlib netlib-infeas netlib-kennington; do
    case $set in
        netlib)            dir=bench/instances;            man=bench/netlib.manifest ;;
        netlib-infeas)     dir=bench/instances-infeas;     man=bench/netlib-infeas.manifest ;;
        netlib-kennington) dir=bench/instances-kennington; man=bench/netlib-kennington.manifest ;;
    esac
    echo "== $set" >&2
    # -d as well as -m. `bench/run` defaults the directory to bench/instances
    # and takes the manifest separately, so passing only -m runs the STANDARD
    # set three times: the first version of this script did exactly that, and
    # Kennington recorded nothing while the output still said its name
    # (`numerics-reviewer`).
    "$d/run" -d "$dir" -m "$man" -j 1 > /dev/null
    echo "== $set instances: $(ls "$dir"/*.mps | wc -l)" >&2
done
} 2> "$here/objcmp-raw.txt"

grep -c OBJCMP "$here/objcmp-raw.txt" > /dev/null 2>&1 || true
python3 "$here/read-objcmp.py" "$here/objcmp-raw.txt" | tee "$here/objcmp.txt"
