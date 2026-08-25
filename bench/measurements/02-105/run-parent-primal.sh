#!/bin/bash
# The primal campaign at the parent commit, for a per-instance comparison.
#
# Needed because the guard moved pilot-ja from overrun to measured, and the
# campaign's geometric mean is taken over the measured set. A mean over 55
# instances is not comparable to a mean over 54: the population changed. Only
# the instances measured on BOTH sides can be compared, and that needs the
# parent's record beside today's.
#
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
parent="$(git rev-parse HEAD~1)"

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

git worktree add --detach "$D/wt" "$parent" >/dev/null 2>&1 || exit 2
cd "$D/wt" || exit 2
ln -s "$root/bench/instances" bench/instances 2>/dev/null
mkdir -p bench/results

make build/bench/primal >/dev/null 2>&1 || { echo "parent build failed"; exit 2; }
./build/bench/primal -j 12 -o "$here/primal-parent.txt" >/dev/null 2>&1
echo "parent $parent written to primal-parent.txt"
grep -c . "$here/primal-parent.txt"
