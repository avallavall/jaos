#!/bin/bash
# PDLP_RUIZ_ROUNDS at 0, 5 and 20 against the shipped 10: the first-order
# campaign (bench/barrier -a pdlp, the standard 94 at 10x the dual's work)
# at each count, each built in a worktree of the named commit with only
# that constant changed. 0 rounds is the Pock-Chambolle scaling alone. The
# 10-round reading is bench/results/pdlp.txt at that commit.
#
# The worktree is under `mktemp -d`, OUTSIDE the repository: `make clean` is
# `rm -rf build` and anyone else's `make configs` would delete it mid-run.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
ref="$(cd "$root" && git rev-parse HEAD)"
jobs=${1:-12}

for rounds in 0 5 20; do
    D=$(mktemp -d) || exit 2
    cd "$root" || exit 2
    git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
    cd "$D/wt" || exit 2
    sed -i "s/^constexpr int64_t PDLP_RUIZ_ROUNDS        = 10;/constexpr int64_t PDLP_RUIZ_ROUNDS        = $rounds;/" src/pdlp.c
    grep -q "PDLP_RUIZ_ROUNDS        = $rounds;" src/pdlp.c || { echo "the substitution did not apply"; exit 2; }
    ln -s "$root/bench/instances" bench/instances 2>/dev/null
    make build/bench/barrier >/dev/null 2>&1 || { echo "worktree build failed"; exit 2; }
    {
        echo "# PDLP_RUIZ_ROUNDS $rounds at $ref, the standard 94 at 10x the dual's work"
        ./build/bench/barrier -a pdlp -j "$jobs" -o "$D/out.txt" >/dev/null 2>&1
        cat "$D/out.txt"
    } > "$here/pdlp-ruiz-$rounds.txt"
    cd "$root" || exit 2
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
done
echo "done" >> "$here/pdlp-ruiz-20.txt"
