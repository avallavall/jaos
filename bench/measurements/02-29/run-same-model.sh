#!/bin/bash
# Is the reduced model identical at REFACTOR_EVERY 64 and 16?
# Presolve runs before the simplex and cannot depend on a simplex constant,
# but the sweep's conclusion rests on it, so it is read rather than assumed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
root="$JAOS_ROOT"
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-same
out="$root/bench/measurements/02-29/same-model.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag "$(dirname "$out")"

{
echo "# The presolve line and the answer, at two refactorization intervals."
echo "# Same candidate, same instance. If the two presolve lines agree, the"
echo "# reduced model is the same and only the solve differs."
for n in 64 16; do
    src=build/diag/src-$n
    rm -rf "$src"; mkdir -p "$src"
    cp src/*.c src/*.h "$src/"
    sed -i "s/^constexpr int64_t REFACTOR_EVERY = 64;/constexpr int64_t REFACTOR_EVERY = $n;/" \
        "$src/simplex.c"
    grep -q "REFACTOR_EVERY = $n;" "$src/simplex.c" || { echo "patch failed at $n"; exit 2; }
    gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
        -Iinclude -I"$src" "$src"/*.c "$root/bench/measurements/02-28/trace.c" \
        -o "build/diag/t-$n" -lm || { echo "build failed at $n"; exit 2; }
    echo
    echo "######## REFACTOR_EVERY=$n  (md5 $(md5sum build/diag/t-$n | cut -c1-12)) ########"
    ./build/diag/t-$n bench/instances/pilotnov.mps -4497.2761882188715 \
        | grep -E "log: presolve|log: optimal|objective |reference |checker "
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo
echo "saved to $out"
