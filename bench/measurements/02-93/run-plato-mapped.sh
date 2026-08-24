#!/bin/bash
# D181 — why do three of four plato-fome instances lose their warm start?
#
# The published basic count is wrong on exactly those three (02-91's probe,
# `plato-fome-basis.txt`), but the published basis and the MAPPED basis are
# different objects: `build_warm_basis` judges what presolve's mapping hands
# it, not what postsolve publishes. Without a line on the refusing path there
# is no way to tell a long map from a short one over the cap, and both would
# print nothing at all.
#
# `fome11 -> fome12 -> fome13` doubles exactly in both dimensions, which is the
# one family in this repository that can say whether a cost grows linearly or
# worse with nothing else about the model changing.
#
# Every exit from build_warm_basis names itself now: DIAG-REFUSE says WHY,
# because a map short past the cap and a map that arrives long both reach the
# same line and both printed nothing.
#
# The control is `degen2`: a netlib instance whose map arrives short by 1, so
# the repair fires and the guard rejects it. If the control prints nothing the
# harness is broken and every reading below is meaningless.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
probe="$root/bench/measurements/02-90/patch-warm-probe.py"
cd "$root" || exit 2
python3 "$probe" apply || exit 2
trap 'python3 "$probe" revert' EXIT
mkdir -p build/diag
gcc-14 -std=c23 -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-mapped -lm 2>/dev/null || exit 2
{
echo "# D181 — the MAPPED basic count at the moment build_warm_basis judges it."
echo "# delta = nbasic - nrow. Negative is short. WARM_REPAIR_MAX_SHORT is 4."
echo
echo "## the control first: degen2, whose map arrives short by 1"
./build/diag/warm-mapped -j 1 degen2 2>&1 >/dev/null \
    | grep -E "DIAG-MAPPED|DIAG-REFUSE|DIAG-REPAIR|DIAG-GUARD warm=1"
echo
echo "## plato-fome, the family that doubles exactly"
for i in fome11 fome12 fome13 fome21; do
    echo "-- $i"
    ./build/diag/warm-mapped -j 1 -m bench/plato-fome.manifest \
        -d bench/instances-plato-fome "$i" 2>&1 >/dev/null \
        | grep -E "DIAG-MAPPED|DIAG-REFUSE|DIAG-REPAIR|DIAG-GUARD warm=1"
done
} | tee "$here/plato-fome-mapped.txt"
