#!/bin/bash
# Is pilotnov's wrong answer NUMERICAL or STRUCTURAL? (TODO.md 4c / D119)
#
# If it is numerical, refactorizing more often should recover the right
# objective on the same reduced model. If it is structural -- presolve having
# cut off the optimum -- no refactorization interval can reach it.
#
# The candidate (D118's refused diff, source half only) is applied in a
# worktree; src/ is read and never written. REFACTOR_EVERY is patched in a
# COPY of the tree for each setting and each setting gets its own binary, so
# nothing can measure one build twice; the md5 beside each row says so.
#
# Produces sweep-refactor.txt. Usage (inside WSL): bash run-sweep.sh
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-28"
out="$here/sweep-refactor.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

{
echo "# TODO.md 4c: pilotnov under D118's candidate, swept over REFACTOR_EVERY."
echo "# Each row is its own binary. The md5 is the canary."
echo "# candidate applied:"
git diff --stat | tail -2

echo
echo "######## CONTROL: HEAD, no candidate, REFACTOR_EVERY=64 ########"
gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
    -Iinclude -I"$root/src" "$root"/src/*.c "$here/trace.c" \
    -o build/diag/trace-parent -lm || { echo "control build failed"; exit 2; }
./build/diag/trace-parent bench/instances/pilotnov.mps -4497.2761882188715 \
    | grep -E "log: optimal|objective |reference |checker |basic "

for n in 64 16 8 4; do
    src=build/diag/src-$n
    rm -rf "$src"; mkdir -p "$src"
    cp src/*.c src/*.h "$src/"
    sed -i "s/^constexpr int64_t REFACTOR_EVERY = 64;/constexpr int64_t REFACTOR_EVERY = $n;/" \
        "$src/simplex.c"
    grep -q "REFACTOR_EVERY = $n;" "$src/simplex.c" || { echo "patch failed at $n"; exit 2; }
    gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
        -Iinclude -I"$src" "$src"/*.c "$here/trace.c" \
        -o "build/diag/trace-$n" -lm || { echo "build failed at $n"; exit 2; }
    echo
    echo "######## CANDIDATE, REFACTOR_EVERY=$n  (md5 $(md5sum build/diag/trace-$n | cut -c1-12)) ########"
    ./build/diag/trace-$n bench/instances/pilotnov.mps -4497.2761882188715 \
        | grep -E "log: optimal|objective |reference |checker |basic "
done
} 2>&1 | tee "$out"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo
echo "saved to $out"
