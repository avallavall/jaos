#!/bin/bash
# Does an abandoned solve publish the PREVIOUS solve's iteration total?
#
# `m->solve_iters` has one writer inside `publish()`, and `publish()` runs only
# when the solve returned JAOS_OK. So a refused solve left the field holding
# whatever the model was solved with last time. `bench/primal.c` reads it and
# subtracts the primal counts from it, which on a refused instance is a
# difference between two different solves.
#
# `pilot87` is the one instance of the standard 94 that takes the hard refusal
# path today, so it is the whole population and it is named rather than
# sampled. Its dual reference solve costs 38000 iterations and its primal
# phase 1 costs 17165 before refusing. 38000 - 17165 = 20835, and that is what
# the `dual:` column printed for a re-entry that never ran.
#
# Two trees, one machine, one session. The parent carries the defect; the
# working tree carries the fix. The column must read 20835 on one and 0 on the
# other, and any other pair of numbers means this control measured something
# else.
#
# Worktree under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
parent="${1:-HEAD}"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

run_one() {   # $1 = directory to build in, $2 = label
    ( cd "$1" || exit 2
      make build/bench/primal > /dev/null 2>&1 || { echo "$2: BUILD FAILED"; exit 2; }
      ./build/bench/primal -j 1 -o "$D/$2.txt" pilot87 > /dev/null 2>&1
      line=$(grep '^pilot87' "$D/$2.txt")
      echo "$2: $line"
    )
}

{
  echo "# does an abandoned solve publish the previous solve's total?"
  echo "# parent ref: $(git rev-parse --short "$parent"), working tree: the fix"
  echo "# the dual: column must be 20835 on the parent and 0 on the fix"
  echo

  git worktree add --detach "$D/wt" "$parent" > /dev/null 2>&1 || exit 2
  ln -s "$root/bench/instances" "$D/wt/bench/instances"
  run_one "$D/wt" "parent"

  run_one "$root" "working-tree"
} 2>&1 | tee "$here/run-carried-total.txt"
