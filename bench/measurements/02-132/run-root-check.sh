#!/bin/bash
# Every measurement script now derives the repository root from its own
# location. This proves it, and it is the check that would have caught the
# defect D215 found: a script that hardcodes the root measures the MAIN tree
# whatever worktree it is run from, and returns one number at every ref.
#
# Three things are checked, and the third is the only one that matters:
#   1. no script still carries the literal path
#   2. every script parses
#   3. the SAME script reads the main tree when it lives in the main tree and
#      the worktree when it lives in a worktree, from any working directory
#
# This script derives its own root the same way, so it is its own first case.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
LIT=/mnt/c/Users/vall-/Desktop/projectes/jaos

{
echo "# every measurement script derives its root, checked at $(git rev-parse --short HEAD)"
echo

echo "===================== 1. the literal is gone ====================="
n=$(git grep -l -- "$LIT" -- '*.sh' | wc -l)
echo "  .sh files still carrying the literal path: $n"
[ "$n" -eq 0 ] || git grep -l -- "$LIT" -- '*.sh'
echo

echo "===================== 2. every script parses ====================="
bad=0
tot=0
while read -r f; do
    tot=$((tot+1))
    bash -n "$f" 2>/tmp/syn.txt || { echo "  SYNTAX ERROR $f"; cat /tmp/syn.txt; bad=1; }
done < <(git grep -l 'JAOS_ROOT=' -- '*.sh')
echo "  $tot scripts declare JAOS_ROOT, syntax errors: $bad"
echo

echo "===================== 3. it reads the tree it lives in ====================="
D=$(mktemp -d) || exit 2
trap 'cd "$root"; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"' EXIT
git worktree add --detach "$D/wt" HEAD > /dev/null 2>&1 || { echo "worktree failed"; exit 2; }
# The worktree comes from HEAD, so it carries HEAD's scripts. This check is
# meant to run BEFORE a commit as well as after, so the working tree's copies
# are put in: what is verified is the tree you are about to commit, and once
# it is committed the two are the same file. Without this the probe reads an
# empty line from a script that does not declare JAOS_ROOT yet, and the
# verdict says STOP for the wrong reason -- which is what it did the first
# time this ran.
while read -r f; do
    [ -n "$f" ] || continue
    mkdir -p "$D/wt/$(dirname "$f")"
    cp "$root/$f" "$D/wt/$f"
done < <(git grep -l 'JAOS_ROOT=' -- '*.sh'; git ls-files -o --exclude-standard -- '*.sh')

probe() {   # $1 tree, $2 script, $3 cwd
    local line dir
    line=$(grep -m1 '^JAOS_ROOT=' "$1/$2") || return 1
    dir=$(dirname "$1/$2")
    printf '%s\necho "$JAOS_ROOT"\n' "$line" > "$dir/.probe.sh"
    ( cd "$3" && bash "$dir/.probe.sh" )
    rm -f "$dir/.probe.sh"
}

pass=1
for s in bench/measurements/02-125/unbounded-census.sh \
         bench/measurements/02-122/gate.sh \
         bench/measurements/02-21/excavate.sh \
         bench/measurements/02-132/run-root-check.sh; do
    echo "--- $s ---"
    for cwd in / "$root" /tmp; do
        a=$(probe "$root"  "$s" "$cwd")
        b=$(probe "$D/wt"  "$s" "$cwd")
        printf '  cwd=%-40s main -> %s\n' "$cwd" "$a"
        printf '  %-44s wtre -> %s\n' "" "$b"
        [ "$a" = "$root" ]   || { echo "    WRONG: main should be $root"; pass=0; }
        [ "$b" = "$D/wt" ]   || { echo "    WRONG: worktree should be $D/wt"; pass=0; }
    done
done
echo

echo "===================== verdict ====================="
if [ "$n" -eq 0 ] && [ "$bad" -eq 0 ] && [ "$pass" -eq 1 ]; then
    echo "PASS: no literal left, every script parses, and each reads the tree it lives in"
else
    echo "STOP: literal_files=$n syntax_bad=$bad resolution_pass=$pass"
fi
echo "===== done ====="
} 2>&1 | tee "$here/run-root-check.txt"
