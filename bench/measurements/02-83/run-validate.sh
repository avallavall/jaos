#!/bin/bash
# The accumulator against an implementation that shares no code with it.
#
# `run-exact-objective.sh` validates it against cases this session wrote down.
# That catches a broken accumulator and cannot catch a wrong expectation. This
# adds up the same terms with Python's `fractions`, which is exact by
# construction, and compares the digits.
#
# Usage: run-validate.sh [instance ...]   (default: the four that decide the
# record — the worst cancellation, the two off-optimum points, and a small one)
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
gcc-14 $P src/*.c "$here/exact-objective.c" -o "$D/s" -lm || exit 2

if [ $# -eq 0 ]; then
    set -- finnis pilot pilot87 afiro
fi

{
    bad=0
    for name in "$@"; do
        inst="bench/instances/$name.mps"
        "$D/s" --terms "$inst" > "$D/terms" || { echo "$name: no terms"; continue; }
        want=$("$D/s" bench/netlib.manifest "$inst" | grep -v '^#' | awk '{print $2}')
        echo "== $name"
        python3 "$here/validate-against-fractions.py" "$D/terms" "$want" || bad=$((bad + 1))
    done
    echo "instances disagreeing: $bad"
} | tee "$here/validate.txt"
