#!/bin/bash
# How much error arrives inside a fixed column's VALUE, and would carrying it
# change a verdict on the three sets?
#
# D162 and D163 put a shift COUNT on the four windows. A count bounds the
# roundings this row made. It cannot see an error that came from another row
# through a folded column's value, and 02-73's CHAIN model is where that
# refuses a feasible model.
#
# This computes what a carried error weight would be, beside the shipped
# windows, and decides nothing. Three questions:
#
#   1. How often does a fold write a value carrying error, and how often is
#      such a column then subtracted from another row?
#   2. Would adding the inheritance to a window spare a row the shipped one
#      refuses -- on any of the 139?
#   3. How large does it get in ABSOLUTE terms, against PRIMAL_TOL 1e-7 and
#      CHECK_TOL 1e-6? No ratio answers that.
#
# Usage: run-inherited.sh [git-ref]        default: HEAD
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-HEAD}
wt="$root/build/diag/wt-02-74"

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
python3 "$here/probe-inherited.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/run.c -o build/diag/inh -lm || exit 2

report () {
    local label=$1; shift
    ./build/diag/inh "$@" -j 12 2>/tmp/ih.err >/dev/null
    echo "---- $label"
    # COUNT fields are summed, EXTREME fields are maxed. Getting that backwards
    # reads zero rather than reading wrong loudly (02-69) -- and it happened
    # again here: `_w$` does not match `D_wnow`, so the fold's shipped window
    # was summed into v[] and printed from m[], which is empty. It read
    # `0 -> 5.536e-08`, a window of zero next to a non-zero sum, which is the
    # only reason it was caught. Anchor every extreme, not a suffix.
    grep 'DIAG-INH' /tmp/ih.err | awk '
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1] ~ /^max_/ || kv[1] ~ /_w$/ || kv[1] ~ /_wnow$/ ||
              kv[1] ~ /^worst_/) {
              if (kv[2]+0 > m[kv[1]]) m[kv[1]] = kv[2]+0
          } else v[kv[1]] += kv[2] } }
      END {
        printf "  folds writing a derived end          %d\n", v["folds"]
        printf "    of those, carrying an error        %d\n", v["folds_err"]
        printf "  subtractions of such a column        %d\n", v["carried"]
        printf "  window reads with inheritance > 0    %d\n", v["rows_inh"]
        printf "  worst error in a fixed VALUE         %.4g\n", m["max_colerr"]
        printf "  worst inheritance on one row         %.4g\n", m["max_inh"]
        printf "  worst inheritance / shipped window   %.4g\n", m["worst_inh_over_win"]
        printf "  -- would carrying it spare a refusal? --\n"
        printf "     emptied row       fires %4d   spared %d\n", v["E_fires"], v["E_flip"]
        printf "     singleton fold    fires %4d   spared %d\n", v["D_fires"], v["D_flip"]
        printf "     activity clause 1 fires %4d   spared %d\n", v["I_fires"], v["I_flip"]
        printf "     frozen row        fires %4d   spared %d\n", v["F_fires"], v["F_flip"]
        printf "  -- widest ABSOLUTE window with it added --\n"
        printf "     emptied row  %.4g   fold %.4g\n", m["E_w"], m["D_w"]
        printf "     clause 1     %.4g   frozen %.4g\n", m["I_w"], m["F_w"]
        printf "     the fold SHIPPED, for the move: %.4g -> %.4g\n", m["D_wnow"], m["D_w"]
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  echo "PRIMAL_TOL is 1e-7 and CHECK_TOL is 1e-6."
  echo
  report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/inherited.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune
echo "readings in $here/inherited.txt"
