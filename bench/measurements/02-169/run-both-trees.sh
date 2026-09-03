#!/usr/bin/env bash
# The exact-objective oracle over the whole standard set, on the parent tree
# and on the working tree, so every moved instance names itself and nothing
# is credited to the wrong commit. Writes exact-parent.txt and
# exact-candidate.txt beside this script, plus the difference.
set -u
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
R="${1:-/mnt/c/Users/vall-/Desktop/projectes/jaos-ref-02168}"
M="$(cd "$here/../../.." && pwd)"
"$here/run-exact-recheck.sh" "$R" > "$here/exact-parent.txt" 2>&1
echo "PARENT_EXIT=$?"
"$here/run-exact-recheck.sh" "$M" > "$here/exact-candidate.txt" 2>&1
echo "CANDIDATE_EXIT=$?"
{
  echo "# instances whose exact objective, gap to the reference or row"
  echo "# residual moved between the parent tree and the candidate."
  echo "# parent   < ... ,  candidate  > ..."
  diff <(grep -v '^#' "$here/exact-parent.txt") \
       <(grep -v '^#' "$here/exact-candidate.txt")
} | tee "$here/exact-diff.txt"
