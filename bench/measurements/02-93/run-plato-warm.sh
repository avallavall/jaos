#!/bin/bash
# D181 — does the warm repair's doomed trajectory appear on a modern set?
#
# D178 left `degen2` as the only instance in twenty where D148's guard throws a
# repaired warm trajectory away, and one instance cannot supply a threshold.
# TODO.md §3's reopen condition is a second instance, and §4 says a fourth
# instance set is the executable form of that condition. The set is already in
# this repository: `plato-fome` and `plato-pds`, from Mittelmann's LPopt
# (D115, `bench/measurements/02-23/`), and the WARM campaign has never been run
# on either.
#
# Reuses 02-90's probe, which is the directory that owns it: four hooks behind
# #ifdef JAOS_DIAG, reverted however this exits.
#
# -j 1 is load-bearing. The DIAG lines go to stderr and belong to the instance
# named just before them; with -j N the children share one stderr.
#
# Usage: run-plato-warm.sh [fome|pds|nug]        default fome
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
probe="$root/bench/measurements/02-90/patch-warm-probe.py"
cd "$root" || exit 2
which="${1:-fome}"
man="bench/plato-$which.manifest"
dir="bench/instances-plato-$which"
[ -r "$man" ] || { echo "no manifest $man" >&2; exit 2; }
[ -d "$dir" ] || { echo "$dir is not fetched; run make plato-$which first" >&2; exit 2; }

python3 "$probe" apply || exit 2
trap 'python3 "$probe" revert' EXIT

mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/warm.c -o build/diag/warm-plato -lm || exit 2

./build/diag/warm-plato -j 1 -m "$man" -d "$dir" \
    >"$here/plato-$which-stdout.txt" 2>"$here/plato-$which-stderr.txt"
echo "warm exit=$?"

# The canary. A run where the repair never fires reads exactly like a run that
# did not happen, and the summary below would still print.
inst=$(grep -c DIAG-INSTANCE "$here/plato-$which-stderr.txt" || true)
rep=$(grep -c DIAG-REPAIR "$here/plato-$which-stderr.txt" || true)
echo "instances seen: $inst   repairs fired: $rep"
if [ "$inst" -eq 0 ]; then
    echo "CANARY FAILED: no instance ran, so this measured nothing" >&2
    exit 3
fi
echo
echo "-- the guard, per instance --"
grep -E "DIAG-INSTANCE|DIAG-GUARD warm=1|DIAG-REPAIR" "$here/plato-$which-stderr.txt"
echo
echo "-- warm against cold --"
tail -14 "$here/plato-$which-stdout.txt"
