#!/bin/bash
# The probe reported 0 phase-1 arms on all 94. That is what a working probe
# on a set that never stalls looks like, and it is also what a probe that
# cannot see a phase-1 line looks like. This forces the effect on.
#
# STALL_FACTOR goes from 10 to 0 in a worktree, so the detector arms on the
# first iteration that does not improve. If `phase1` stays 0 after that, the
# probe is broken and its 0 means nothing.
#
# The worktree is under `mktemp -d`, OUTSIDE the repository: `make clean` is
# `rm -rf build` and anyone else's `make configs` would delete it mid-run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
ref="$(cd "$root" && git rev-parse HEAD)"

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

cd "$root" || exit 2
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2

sed -i 's/^constexpr int64_t STALL_FACTOR = 10;/constexpr int64_t STALL_FACTOR = 0;/' src/simplex.c
grep -q 'STALL_FACTOR = 0;' src/simplex.c || { echo "the substitution did not apply"; exit 2; }

ln -s "$root/bench/instances" bench/instances 2>/dev/null
make all >/dev/null 2>&1 || { echo "worktree library build failed"; exit 2; }
gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" build/release/libjaos.a \
    -o "$D/probe" -lm || { echo "probe build failed"; exit 2; }

{
  echo "# canary: STALL_FACTOR forced to 0 at $ref, five instances"
  echo "# every line must show phase1 > 0, or the probe cannot see a phase-1 arm"
  echo
  for n in afiro adlittle share2b sc50a stocfor1; do
      "$D/probe" "bench/instances/$n.mps"
  done
} | tee "$here/canary.txt"
