#!/bin/bash
# The three gate sets on the capped candidate.
#
# The claim being tested is that this change is a NO-OP on the gate: the gate
# solves each instance once from a fresh load with no stored basis, so
# build_warm_basis is never reached and every digest, basis hash, work count
# and verdict must be bit-identical to the committed baselines.
#
# Since D150 the record carries basis= per optimal line and det covers it, so
# a basis-only change is visible here where last week it would not have been.
# That makes this a stronger no-op check than the same run used to be.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-60-gate-own"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
git diff > "$wt/cand.diff" || exit 2
cd "$wt" || exit 2
[ -s cand.diff ] || { echo "*** no candidate diff — this would measure HEAD ***"; exit 2; }
git apply cand.diff || { echo "the candidate did not apply"; exit 2; }
echo "candidate applied"
for dir in instances instances-infeas instances-kennington; do
    [ -d "$root/bench/$dir" ] && { rm -rf "bench/$dir"; ln -s "$root/bench/$dir" "bench/$dir"; }
done

echo "=== netlib ==="
make netlib J=12 > /tmp/g1.log 2>&1; r1=$?
tail -6 /tmp/g1.log
echo "=== netlib-infeas ==="
make netlib-infeas J=12 > /tmp/g2.log 2>&1; r2=$?
tail -6 /tmp/g2.log
echo "=== netlib-kennington ==="
make netlib-kennington J=12 > /tmp/g3.log 2>&1; r3=$?
tail -6 /tmp/g3.log

echo
echo "exits: netlib=$r1 infeas=$r2 kennington=$r3"

# The summary line is not the result: diff every instance against the
# committed record with the skill's own script.
echo
echo "=== per-instance diff against the committed records ==="
for f in netlib netlib-infeas netlib-kennington; do
    echo "--- $f ---"
    python3 "$root/.claude/skills/jaos-measure/scripts/record_diff.py" \
        --against-file "$root/bench/results/$f.txt" \
        "bench/results/$f.txt" 2>&1 | tail -20
done

# The three records are NOT copied beside this script. They come back
# bit-identical to the committed ones, so a copy would be 50 KB restating a
# file the repository already has. What decided the verdict is the
# record_diff output above, and that is saved as gate-run.txt.

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "done — save this output as gate-run.txt"
