#!/bin/bash
# How many removals does a row's bound carry, and would counting them matter?
#
# `cur_rl[i]` and `cur_ru[i]` are a plain running difference. Three windows
# judge one of those numbers and all three counted a fixed eight ulps of a
# scale, which covers about three removals. This adds the counter, computes the
# candidate window beside the shipped one at all three sites, and decides
# nothing: the shipped window still runs the solve.
#
# Two questions, and the second is the one that could refuse the change:
#
#   1. Does the wider window flip a verdict on any of the 139? The twelve
#      genuine infeasibilities in netlib-infeas are the ones that must survive.
#   2. What is the WIDEST ABSOLUTE window either produces? Every other figure
#      here is a ratio, and no ratio says whether the widening can reach
#      PRIMAL_TOL 1e-7 or CHECK_TOL 1e-6.
#
# The probe applies to the tree BEFORE the change, so it runs against a
# detached worktree rather than the working tree.
#
# Usage: run-shifts.sh [git-ref]           default: 4c5f58f, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-4c5f58f}
wt="$root/build/diag/wt-02-72"

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
python3 "$here/probe-shifts.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/run.c -o build/diag/shifts -lm || exit 2

report () {
    local label=$1; shift
    ./build/diag/shifts "$@" -j 12 2>/tmp/sh.err >/dev/null
    echo "---- $label"
    # A field is either a COUNT to sum or an EXTREME to take the max of, and
    # getting that wrong reads zero rather than reading wrong loudly (02-69).
    grep 'DIAG-SHIFTS' /tmp/sh.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1] ~ /_k$/ || kv[1] ~ /_w/ || kv[1] ~ /_near$/ ||
              kv[1] ~ /_BoA$/ || kv[1] ~ /_CoB$/) {
              if (kv[2]+0 > m[kv[1]]) m[kv[1]] = kv[2]+0
          } else v[kv[1]] += kv[2] } }
      END {
        split("E I F", S, " ")
        name["E"] = "emptied-row test"
        name["I"] = "activity pass, clause 1"
        name["F"] = "frozen-row test"
        for (q = 1; q <= 3; q++) {
          p = S[q]
          printf "  %s\n", name[p]
          printf "     rows tested                        %d\n", v[p"_rows"]
          printf "     rows the shipped window refuses    %d\n", v[p"_fires"]
          printf "     of those, A (traffic) spares       %d\n", v[p"_fA"]
          printf "     of those, B (this end)  spares     %d\n", v[p"_fB"]
          printf "     of those, C (either end) spares    %d\n", v[p"_fC"]
          printf "     largest shift count on one row     %d\n", m[p"_k"]
          printf "     rows with more than 8 shifts       %d\n", v[p"_k8"]
          printf "     widest ABSOLUTE window, shipped    %.4g\n", m[p"_wnow"]
          printf "     widest ABSOLUTE window, A          %.4g\n", m[p"_wA"]
          printf "     widest ABSOLUTE window, B (ships)  %.4g\n", m[p"_wB"]
          printf "     widest ABSOLUTE window, C          %.4g\n", m[p"_wC"]
          printf "     worst B/A on one row               %.4g\n", m[p"_BoA"]
          printf "     worst C/B on one row               %.4g\n", m[p"_CoB"]
          printf "     worst residue that PASSED / window %.4g\n", m[p"_near"]
        }
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  echo "PRIMAL_TOL is 1e-7 and CHECK_TOL is 1e-6. A window above either is the"
  echo "reopen condition, not a ratio."
  echo
  report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/shifts.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
echo "readings in $here/shifts.txt"
