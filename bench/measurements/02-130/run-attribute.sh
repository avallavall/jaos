#!/bin/bash
# Attribute D211's flip, correctly this time.
#
# relrise.sh hardcodes `root=/mnt/c/.../jaos` and cds there before reading
# HEAD, so running it from inside a worktree measures the MAIN tree anyway.
# The first attempt did exactly that and got 9.36752e-10 at all three refs,
# which is one binary read three times (D82's canary).
#
# The fix: give each ref its own root. Copy the script into the worktree with
# `root=` rewritten to that worktree, so `git rev-parse HEAD` there returns
# the ref being measured.
#
# The canary: the three readings must NOT all be equal. If they are, this
# script is measuring one tree again and its output means nothing.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    for r in a b c; do git worktree remove --force "$D/$r" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

i=0
for ref in 3221397 2ee580f e2daf9c; do
    i=$((i+1))
    case $i in 1) w=a;; 2) w=b;; 3) w=c;; esac
    git worktree add --detach "$D/$w" "$ref" > /dev/null 2>&1 || { echo "$ref WORKTREE FAILED"; continue; }
    ln -s "$root/bench/instances" "$D/$w/bench/instances"
    sed "s|^root="$JAOS_ROOT"$|root=$D/$w|" \
        "$D/$w/bench/measurements/02-126/relrise.sh" > "$D/$w/rr.sh"
    grep -q "^root=$D/$w\$" "$D/$w/rr.sh" || { echo "$ref ROOT NOT REWRITTEN"; continue; }
    echo "===================== $ref ====================="
    ( cd "$D/$w" && bash rr.sh > "$D/$w.log" 2>&1 )
    echo "exit $?"
    echo "tree it actually measured: $(cd "$D/$w" && git rev-parse --short HEAD)"
    grep -E "largest rise|REOPEN|HOLDS|control:" "$D/$w.log" | tail -4
    grep -E "^RR (wood1p|pilot-ja|pilot87) " "$D/$w.log"
    echo
done

echo "===================== canary ====================="
a=$(grep -h "largest rise" "$D/a.log" 2>/dev/null)
b=$(grep -h "largest rise" "$D/b.log" 2>/dev/null)
c=$(grep -h "largest rise" "$D/c.log" 2>/dev/null)
if [ "$a" = "$b" ] && [ "$b" = "$c" ]; then
    echo "ALL THREE EQUAL -- this is one tree measured three times, the reading means nothing"
else
    echo "the three refs differ, so each worktree really was built and run"
fi
echo "===== done ====="
