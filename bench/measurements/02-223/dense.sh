#!/bin/bash
# BARRIER_DENSE_FACTOR at 5 and 20 against the shipped 10: the barrier
# campaign (bench/barrier, the standard 94 at 10x the dual's work) at each
# factor, each built in a worktree of the named commit with only that
# constant changed. The 10 reading is bench/results/barrier.txt at that
# commit. A column is dense when its count exceeds the factor times the
# average column count and BARRIER_DENSE_MIN.
#
# The worktree is under `mktemp -d`, OUTSIDE the repository: `make clean` is
# `rm -rf build` and anyone else's `make configs` would delete it mid-run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
ref="$(cd "$root" && git rev-parse HEAD)"
jobs=${1:-12}

for factor in 5 20; do
    D=$(mktemp -d) || exit 2
    cd "$root" || exit 2
    git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
    cd "$D/wt" || exit 2
    sed -i "s/^constexpr double  BARRIER_DENSE_FACTOR = 10.0;/constexpr double  BARRIER_DENSE_FACTOR = $factor.0;/" src/barrier.c
    grep -q "BARRIER_DENSE_FACTOR = $factor.0;" src/barrier.c || { echo "the substitution did not apply"; exit 2; }
    ln -s "$root/bench/instances" bench/instances 2>/dev/null
    make build/bench/barrier >/dev/null 2>&1 || { echo "worktree build failed"; exit 2; }
    {
        echo "# BARRIER_DENSE_FACTOR $factor at $ref, the standard 94 at 10x the dual's work"
        ./build/bench/barrier -j "$jobs" -o "$D/out.txt" >/dev/null 2>&1
        cat "$D/out.txt"
    } > "$here/barrier-dense-$factor.txt"
    cd "$root" || exit 2
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
done
echo "done" >> "$here/barrier-dense-20.txt"
