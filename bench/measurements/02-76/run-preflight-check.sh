#!/usr/bin/env bash
# The new preflight check is worth nothing until it has caught the case it
# exists for. Three states, and the middle one is the case it must reject.
set -u
root=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$root" || exit 9
P=".claude/skills/jaos-measure/scripts/preflight.sh"
line() { bash "$P" 2>&1 | grep -E "worktree|make clean" | sed 's/^/   /'; }

echo "== A. clean tree, nothing registered -- must read ok"
line

echo
echo "== B. a worktree under build/, which is what it exists to catch"
git worktree add --detach "$root/build/diag/wt-probe" HEAD >/dev/null 2>&1 || {
    echo "   could not create the case"; exit 2; }
line

echo
echo "== C. the same worktree outside build/ -- must go quiet again"
git worktree remove --force "$root/build/diag/wt-probe" >/dev/null 2>&1
D=$(mktemp -d) || exit 2
git worktree add --detach "$D/wt" HEAD >/dev/null 2>&1 || {
    echo "   could not create the control"; exit 2; }
line
git worktree remove --force "$D/wt" >/dev/null 2>&1
rm -rf "$D"
git worktree prune
echo
echo "== worktrees left registered:"; git worktree list | sed 's/^/   /'
