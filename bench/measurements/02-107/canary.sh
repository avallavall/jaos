#!/bin/bash
# Can the probe see a flip that damages feasibility?
#
# The census reported 0 flips growing either phase's own measure, and a 0 from
# a probe that has never fired is not a measurement. canary-patch.py makes every
# flip damage the point on purpose. The totals below MUST be non-zero.
#
# An earlier version of this file raised PIVOT_MIN to 1e-3 instead, meaning to
# make D191's hazard reachable. It reported nothing and proved nothing: it also
# took the flip count from 10604 to 221, so it changed the trajectory rather
# than isolating the hazard. That reading is in canary-pivotmin.txt and is kept
# because it is a real negative result about that approach.
#
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2
python3 "$here/patch.py" src/simplex.c || exit 2
python3 "$here/canary-patch.py" src/simplex.c || exit 2
ln -s "$root/bench/instances" bench/instances 2>/dev/null
make all >/dev/null 2>&1 || { echo "library build failed"; exit 2; }
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" build/release/libjaos.a \
    -o "$D/probe" -lm || { echo "probe build failed"; exit 2; }
{
  echo "# canary: every PHASE-2 flip followed by a forced 1e6 push on every basic"
  echo "# run on the 8 instances whose phase 1 takes zero iterations, the only"
  echo "# ones that reach a phase-2 flip at all"
  echo "# tree: $ref plus patch.py plus canary-patch.py, in a worktree"
  echo "# 'the phase's own measure GREW' MUST be non-zero or the probe is blind"
  echo
  "$D/probe" bench/instances/{d6cube,degen3,dfl001,maros-r7,pilot87,scrs8,scsd8,wood1p}.mps
} 2>&1 | tee "$here/canary.txt" | tail -14
