#!/bin/bash
# What the D146 guard sees on one instance, at two refs, side by side.
#
# Both trees are worktrees under mktemp -d, OUTSIDE the repository, and both
# are patched by patch.py. The working tree is never touched: this compares two
# committed refs and must not depend on what is checked out.
#
# Usage: run-tail.sh <instance> [ref-a] [ref-b]
#   defaults: pilot4, HEAD (the guard) and HEAD~2 (its parent)
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
inst="${1:-pilot4}"
refa="$(git rev-parse "${2:-HEAD}")"
refb="$(git rev-parse "${3:-HEAD~2}")"

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/a" 2>/dev/null
    git worktree remove --force "$D/b" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

one() {                                  # $1 = ref, $2 = dir, $3 = out file
    git worktree add --detach "$2" "$1" >/dev/null 2>&1 || return 2
    ( cd "$2" || exit 2
      python3 "$here/patch.py" src/simplex.c || exit 2
      ln -s "$root/bench/instances" bench/instances 2>/dev/null
      make all >/dev/null 2>&1 || { echo "library build failed at $1"; exit 2; }
      gcc-14 -std=c23 -O2 -Iinclude -Isrc "$here/probe.c" \
          build/release/libjaos.a -o "$D/probe" -lm || exit 2
      "$D/probe" "bench/instances/$inst.mps" ) > "$3" 2>&1
}

cd "$root" || exit 2
one "$refa" "$D/a" "$here/$inst-head.txt"   || { echo "ref A failed"; exit 2; }
one "$refb" "$D/b" "$here/$inst-parent.txt" || { echo "ref B failed"; exit 2; }

for f in "$here/$inst-head.txt:$refa" "$here/$inst-parent.txt:$refb"; do
    p="${f%%:*}"; r="${f##*:}"
    echo "== $inst at $r"
    grep -E "DIAG|phase 1 reached|^== |numerical error after|optimal after" "$p"
    echo
done
