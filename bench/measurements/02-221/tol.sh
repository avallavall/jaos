#!/bin/bash
# PDLP_TOL at 1e-4 against the shipped 1e-6: the first-order campaign
# (bench/barrier -a pdlp, the standard 94 at 10x the dual's work) at each
# setting. The 1e-6 reading is bench/results/pdlp.txt at the tree this
# script names; the 1e-4 one is built in a worktree of that tree with the
# constant changed, so the two differ in nothing else.
#
# The worktree is under `mktemp -d`, OUTSIDE the repository: `make clean` is
# `rm -rf build` and anyone else's `make configs` would delete it mid-run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
ref="$(cd "$root" && git rev-parse HEAD)"
jobs=${1:-12}

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

sed -i 's/^constexpr double  PDLP_TOL                = 1e-6;/constexpr double  PDLP_TOL                = 1e-4;/' src/pdlp.c
grep -q 'PDLP_TOL                = 1e-4;' src/pdlp.c || { echo "the substitution did not apply"; exit 2; }

ln -s "$root/bench/instances" bench/instances 2>/dev/null
make build/bench/barrier >/dev/null 2>&1 || { echo "worktree build failed"; exit 2; }
{
    echo "# PDLP_TOL 1e-4 at $ref, the standard 94 at 10x the dual's work"
    ./build/bench/barrier -a pdlp -j "$jobs" -o "$D/out.txt" >/dev/null 2>&1
    cat "$D/out.txt"
} > "$here/pdlp-tol-1e-4.txt"
echo "done" >> "$here/pdlp-tol-1e-4.txt"
