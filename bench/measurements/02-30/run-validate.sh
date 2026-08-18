#!/bin/bash
# Does the exact repayment actually repair what it targets?
#
# The failing case is pilotnov under D118's refused presolve candidate: 67
# costs permanently wrong, objective 29% off, checker rejecting at 0.89. The
# repair is in the main tree; this applies the presolve candidate on top of it
# in a worktree and asks the same question again.
#
# The negative control is HEAD without the repair, which is what D121
# measured. Both are built here so the pair is read from one run.
set -u
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
wt=/mnt/c/Users/vall-/AppData/Local/Temp/claude/C--Users-vall--Desktop-projectes-jaos/dbf2e500-9288-4cc8-b7f1-c859a31990ff/scratchpad/jaos-val
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
cd "$wt" || exit 2
git apply --include=src/presolve.c "$root/bench/measurements/02-27/candidate.diff" \
    || { echo "candidate.diff did not apply"; exit 2; }
rm -rf bench/instances && ln -s "$root/bench/instances" bench/instances
mkdir -p build/diag

echo "=== WITHOUT the repair (HEAD's simplex.c + the presolve candidate) ==="
gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/before -lm || { echo "build failed"; exit 2; }
./build/diag/before bench/instances/pilotnov.mps -4497.2761882188715 \
    | grep -E "log: optimal|objective |reference |checker "

echo
echo "=== WITH the repair (the working tree's simplex.c + the same candidate) ==="
cp "$root/src/simplex.c" src/simplex.c
gcc-14 -std=c23 -O2 -Wall -Wextra -ffp-contract=off -DNDEBUG \
    -Iinclude -Isrc src/*.c "$root/bench/measurements/02-28/trace.c" \
    -o build/diag/after -lm || { echo "build failed"; exit 2; }
./build/diag/after bench/instances/pilotnov.mps -4497.2761882188715 \
    | grep -E "log: optimal|objective |reference |checker "

echo
echo "md5 before=$(md5sum build/diag/before | cut -c1-12) after=$(md5sum build/diag/after | cut -c1-12)"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
