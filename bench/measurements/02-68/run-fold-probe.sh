#!/bin/bash
# Where does the collapsed fold's midpoint land, and does the branch ever run?
#
# Two questions, in the order that decides the item:
#
#   1. Can the instrument see a collapse at all? A model that collapses and
#      two that do not.
#   2. How often does the branch fire on the three sets, and how far outside
#      the column's box does the midpoint land when it does?
#
# The probe matches the text of the UNCLAMPED midpoint, so it applies at the
# parent of the commit that landed the clamp, not at HEAD. It builds a
# detached worktree at that ref and never touches the main tree.
#
# Usage: run-fold-probe.sh [git-ref]        default: 78e7084, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-78e7084}
wt="$root/build/diag/wt-02-68"

cd "$root" || exit 9
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || {
    echo "worktree add failed for $ref"; exit 2; }
for d in instances instances-infeas instances-kennington; do
    [ -d "$root/bench/$d" ] && { rm -rf "$wt/bench/$d"; \
        ln -s "$root/bench/$d" "$wt/bench/$d"; }
done

cd "$wt" || exit 2
python3 "$here/probe-fold.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
DIAG="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG -Iinclude -Isrc"
gcc-14 $DIAG src/*.c bench/run.c        -o build/diag/fold     -lm || exit 2
gcc-14 $DIAG src/*.c "$here/fold-case.c" -o build/diag/foldcase -lm || exit 2

report () {
    local label=$1; shift
    ./build/diag/fold "$@" -j 12 2>/tmp/f.err >/dev/null
    echo "---- $label"
    grep 'DIAG-FOLD' /tmp/f.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1] ~ /worst/) { if (kv[2]+0>m[kv[1]]) m[kv[1]]=kv[2]+0 }
          else v[kv[1]]+=kv[2] } }
      END {
        printf "  singleton-row folds                 %d\n", v["folds"]
        printf "    intersection COLLAPSED            %d\n", v["collapse"]
        printf "    midpoint outside the current box  %d\n", v["out_cur"]
        printf "    midpoint outside the CALLERs box  %d\n", v["out_orig"]
        printf "    worst outside, callers box        %g\n", m["worst_out_orig"]
        printf "    worst window btol                 %g\n", m["worst_btol"]
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  echo "######## 1. the instrument, before believing its zero ########"
  echo "A must read collapse=1 out_orig=1; B and C must read collapse=0."
  ./build/diag/foldcase 2>&1 | sed 's/^/  /'
  echo
  echo "######## 2. the three sets ########"
  report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/fold.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
echo
echo "readings in $here/fold.txt; worktree removed"
