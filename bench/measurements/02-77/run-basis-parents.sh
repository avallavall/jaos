#!/usr/bin/env bash
# The published-basis count at three trees, so the move is attributable.
#
# 02-48's script always reads the MAIN tree's src/presolve.c, so the only way
# to point it at a parent is to put that parent's file there and put it back
# afterwards. The trap does the putting back on every exit path, because
# leaving the tree dirty is D127's warning and this script would otherwise be
# the thing that trips it.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 9

restore() { cd "$root" && git checkout -- src/presolve.c; }
trap restore EXIT

# Refuse to start if the tree is already dirty: the restore would then throw
# away someone's work rather than this script's.
if ! git diff --quiet -- src/presolve.c; then
    echo "src/presolve.c is already modified -- refusing to overwrite it"; exit 2
fi

for ref in 4c5f58f cd68630 HEAD; do
    git show "$ref:src/presolve.c" > src/presolve.c || exit 2
    echo "==== $ref ($(git log --oneline -1 --format=%s "$ref" | cut -c1-60))"
    bash bench/measurements/02-48/run-verify-count.sh 2>&1 | grep -E "^netlib|^kennington" | sed 's/^/   /'
done
