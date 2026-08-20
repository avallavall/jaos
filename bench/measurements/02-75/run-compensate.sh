#!/bin/bash
# What would compensating `cur_rl`/`cur_ru` change, before anything is built?
#
# They are the only running sums in src/presolve.c without compensation.
# D162, D163 and D164 are all consequences of that. Compensating removes the
# error rather than covering it and would subsume all three -- and unlike all
# three it cannot be a no-op, because it changes the reduced model.
#
# The number that decides the size of the job is `folds moved`: a folded value
# is written into a column box and every later reduction reads it, so if many
# move the gate moves with them.
#
# Usage: run-compensate.sh [git-ref]       default: HEAD
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-HEAD}
wt="$root/build/diag/wt-02-75"

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
python3 "$here/probe-compensate.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/run.c -o build/diag/comp -lm || exit 2

report () {
    local label=$1; shift
    ./build/diag/comp "$@" -j 12 2>/tmp/cp.err >/dev/null
    echo "---- $label"
    # Every EXTREME is matched by its own name, not by a suffix. A suffix rule
    # silently reclassified the next field added in 02-74 and read zero.
    grep 'DIAG-COMP' /tmp/cp.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1]=="max_abs_corr" || kv[1]=="max_rel_corr" ||
              kv[1]=="max_fold_delta" || kv[1]=="max_fold_rel") {
              if (kv[2]+0 > m[kv[1]]) m[kv[1]] = kv[2]+0
          } else v[kv[1]] += kv[2] } }
      END {
        printf "  window reads                         %d\n", v["reads"]
        printf "    with a non-zero correction         %d\n", v["reads_corr"]
        printf "  worst correction, absolute           %.4g\n", m["max_abs_corr"]
        printf "  worst correction / row traffic       %.4g\n", m["max_rel_corr"]
        printf "  -- verdicts, BOTH directions --\n"
        printf "     rows the compensated value spares %d\n", v["spared"]
        printf "     rows it would NEWLY refuse        %d\n", v["newly_refused"]
        printf "  -- folded values, which decide the size of the job --\n"
        printf "     folds                             %d\n", v["folds"]
        printf "     folds whose value MOVES           %d\n", v["folds_moved"]
        printf "     worst move, absolute              %.4g\n", m["max_fold_delta"]
        printf "     worst move / the box it lands in  %.4g\n", m["max_fold_rel"]
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/compensate.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
echo "readings in $here/compensate.txt"
