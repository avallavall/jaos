#!/bin/bash
# The capped repair's own warm campaigns, on both sets.
#
# Run in a worktree with the candidate applied, because a campaign in a dirty
# main tree cannot say which tree it measured. bench/instances* are symlinked
# in; preflight.sh follows symlinks.
#
# What this is for: the sweep in cap-sweep.txt PREDICTS these numbers by
# choosing per instance between two campaigns already measured. The
# prediction is an argument from the code — that a per-instance repair
# decision cannot change another instance — and this project refuses those.
# So the campaign is run and the two are compared line by line. A single
# instance differing means the prediction's premise is wrong and the sweep is
# not evidence.
#
# Predicted, from cap-sweep.txt at cap 4:
#   netlib     work geomean 0.1916, worst 4.65 on scsd1, 20 instances repaired
#   kennington work geomean 0.0070, worst 0.51 on cre-c, 5 instances repaired
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-60-cap"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }

# The candidate is uncommitted, so it is carried across as a patch.
git diff > "$wt/cand.diff" || exit 2
cd "$wt" || exit 2
if [ -s cand.diff ]; then
    git apply cand.diff || { echo "candidate did not apply in the worktree"; exit 2; }
    echo "candidate applied in the worktree"
else
    echo "*** the working tree has no diff — measuring HEAD, not a candidate ***"
fi
for dir in instances instances-kennington; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done

make build/bench/warm > /tmp/cap-build.log 2>&1 \
    || { echo "build failed"; tail -20 /tmp/cap-build.log; exit 2; }
[ -x build/bench/warm ] || { echo "the warm runner was not built"; exit 2; }

./build/bench/warm -j 12 -o "$here/cap-warm.txt" > /tmp/cap-warm.log 2>&1
rc1=$?
./build/bench/warm -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -o "$here/cap-warm-kennington.txt" \
    > /tmp/cap-warm-kn.log 2>&1
rc2=$?

echo "netlib exit=$rc1  kennington exit=$rc2"
[ "$rc1" = 0 ] && [ "$rc2" = 0 ] || { echo "*** a campaign failed — nothing below is evidence ***"; }
echo
echo "===== netlib ====="
tail -10 "$here/cap-warm.txt"
echo
echo "===== kennington ====="
tail -10 "$here/cap-warm-kennington.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo
echo "records written beside this script"
