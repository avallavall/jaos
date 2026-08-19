#!/bin/bash
# What row_traffic[i] carries, and what it would carry repaired.
#
# The probe patches the site that saturates and carries a SECOND accumulation
# beside the shipped one, so one run reports both and changes nothing. It
# matches the text of the UNREPAIRED accumulation, so it applies to the parent
# of the commit that landed the repair, not to HEAD.
#
# Usage: run-traffic-probe.sh [git-ref]        default: f0ffec8, the parent
#
# It builds a detached worktree at that ref, symlinks the gitignored instance
# directories in, patches there and never touches the main tree.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-f0ffec8}
wt="$root/build/diag/wt-02-66"

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
python3 "$here/probe-traffic.py" || { echo "probe did not apply at $ref"; exit 2; }

mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG \
       -DJAOS_DIAG -Iinclude -Isrc src/*.c bench/run.c \
       -o build/diag/traffic -lm || { echo "build failed"; exit 2; }

# The canary: a probe that prints nothing has measured nothing.
n=$(./build/diag/traffic -d bench/instances -m bench/netlib.manifest -j 1 \
      2>&1 >/dev/null | grep -c 'DIAG-TRAFFIC')
[ "$n" -gt 0 ] || { echo "CANARY FAILED: the probe printed nothing"; exit 2; }
echo "canary: $n presolve runs reported on the standard set"
echo

# -j 1 so the per-run lines cannot interleave. The counters are summed over
# every presolve run in the set; the two "worst" fields are maxima.
report () {
    local label=$1; shift
    ./build/diag/traffic "$@" -j 1 2>/tmp/dg.err >/dev/null
    echo "---- $label"
    grep 'DIAG-TRAFFIC' /tmp/dg.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1]=="worst_repaired_over_bscale") { if (kv[2]+0>w) w=kv[2]+0 }
          else if (kv[1]=="zero_worst_repaired_traffic") { if (kv[2]+0>z) z=kv[2]+0 }
          else if (kv[1]=="max_repaired_traffic") { if (kv[2]+0>t) t=kv[2]+0 }
          else v[kv[1]]+=kv[2] } }
      END {
        printf "  saturating sites              %d\n", v["sat_sites"]
        printf "  INF read by empty-row test    %d\n", v["read_empty_inf"]
        printf "  INF read by singleton fold    %d\n", v["read_fold_inf"]
        printf "  rows at frozen-row test       %d\n", v["frozen"]
        printf "    traffic == inf              %d\n", v["frozen_inf"]
        printf "  rows at EXACTLY zero margin   %d\n", v["zero_margin"]
        printf "    of those, traffic == inf    %d\n", v["frozen_zero_inf"]
        printf "    worst repaired traffic      %g\n", z
        printf "  worst repaired/bound-scale    %g\n", w
        printf "  MAX repaired traffic, any row %g\n", t
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo "Counters are summed over every presolve run in the set, and the gate"
  echo "solves each instance twice, so a per-solve figure is half the count."
  echo
  report "netlib standard (94)"   -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"     -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"        -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/traffic.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
echo
echo "readings in $here/traffic.txt; worktree removed"
