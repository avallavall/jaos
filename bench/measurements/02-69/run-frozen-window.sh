#!/bin/bash
# The frozen-row test's window: what it covers, and what it should.
#
# Two questions, and the second is the one that decides it:
#
#   1. Over the three sets, how far is the shipped window from the error the
#      comparison actually carries, and would either candidate shape flip a
#      verdict? Both shapes are swept together, because "the wider one is
#      correct" is an argument and the alternative shape is what refutes an
#      argument here.
#   2. Is there a model the shipped window gets WRONG? The campaign cannot
#      answer this: the shipped window flips no verdict on any of the 139, so
#      only a constructed pair can separate the two.
#
# The probe applies to the tree BEFORE the change, so it runs against a
# detached worktree at the parent rather than at HEAD.
#
# Usage: run-frozen-window.sh [git-ref]        default: 0ac44fd, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-0ac44fd}
wt="$root/build/diag/wt-02-69"

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
python3 "$here/probe-frozen.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
       -Iinclude -Isrc src/*.c bench/run.c -o build/diag/frozen -lm || exit 2

report () {
    local label=$1; shift
    ./build/diag/frozen "$@" -j 12 2>/tmp/fz.err >/dev/null
    echo "---- $label"
    grep 'DIAG-FROZEN' /tmp/fz.err | awk '
      # A field is either a COUNT to sum or an EXTREME to take the max of, and
      # getting that wrong reads zero rather than reading wrong loudly: the
      # max_* fields were summed into v[] and printed from m[], which is empty.
      { for (i=2;i<=NF;i++) { split($i,kv,"=");
          if (kv[1] ~ /^worst/ || kv[1] ~ /^max_/) {
              if (kv[2]+0 > m[kv[1]]) m[kv[1]] = kv[2]+0
          } else v[kv[1]] += kv[2] } }
      END {
        printf "  frozen rows tested                     %d\n", v["rows"]
        printf "    residue above zero                   %d\n", v["res_pos"]
        printf "  fires with the SHIPPED window          %d\n", v["fires_now"]
        printf "  worst residue / shipped window         %g\n", m["worst_res_over_now"]
        printf "  -- shape A: max(bound scale, row_traffic) --\n"
        printf "     rows where it exceeds the shipped   %d\n", v["A_is_max"]
        printf "     worst A/shipped ratio               %g\n", m["worst_ratioA"]
        printf "     verdicts A would flip               %d\n", v["flipA"]
        printf "  -- shape B: A, plus the activity traffic --\n"
        printf "     rows where B exceeds A              %d\n", v["B_is_max"]
        printf "     worst B/shipped ratio               %g\n", m["worst_ratioB"]
        printf "     verdicts B would flip               %d\n", v["flipB"]
        printf "  -- the ABSOLUTE window, which no ratio answers --\n"
        printf "     widest shipped                      %.4g\n", m["max_rtol_now"]
        printf "     widest A                            %.4g\n", m["max_rtolA"]
        printf "     widest B                            %.4g\n", m["max_rtolB"]
        printf "     largest rg.traffic                  %.4g\n", m["max_rg_traffic"]
      }'
}

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  echo "######## 1. the three sets ########"
  report "netlib standard (94)" -d bench/instances            -m bench/netlib.manifest
  report "netlib-infeas (29)"   -d bench/instances-infeas     -m bench/netlib-infeas.manifest -e infeasible
  report "kennington (16)"      -d bench/instances-kennington -m bench/netlib-kennington.manifest
} | tee "$here/window.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune

# ---- 2. the constructed pair, on three builds -------------------------------
D=$(mktemp -d) || exit 2
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
mkdir -p "$D/before"
git show "$ref:src/presolve.c" > "$D/before/presolve.c"
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/before/$b"
done
gcc-14 $P                    "$D/before"/*.c "$here/frozen-scale.c" -o "$D/b" -lm || exit 2
gcc-14 $P                    src/*.c         "$here/frozen-scale.c" -o "$D/a" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c         "$here/frozen-scale.c" -o "$D/r" -lm || exit 2
gcc-14 $P                    "$D/before"/*.c "$here/activity-pass.c" -o "$D/ab" -lm || exit 2
gcc-14 $P                    src/*.c         "$here/activity-pass.c" -o "$D/aa" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c         "$here/activity-pass.c" -o "$D/ar" -lm || exit 2
gcc-14 $P -DJAOS_PRESOLVE_ROUND_ULPS_VALUE=1e12 src/*.c "$here/activity-pass.c" -o "$D/aw" -lm || exit 2
{
  echo
  echo "######## 2. the constructed pair ########"
  echo "The campaign flips no verdict either way, so only this separates them."
  echo "-- BEFORE: the window scaled by the row's own bounds"
  "$D/b" | sed 's/^/   /'
  echo "-- AFTER: the window covers the traffic"
  "$D/a" | sed 's/^/   /'
  echo "-- REFERENCE BUILD (-DJAOS_NO_PRESOLVE), the oracle"
  "$D/r" | sed 's/^/   /'
  echo
  echo "######## 3. the two models the review turned up ########"
  echo "A is the SAME defect at the activity pass, which this change does not"
  echo "touch: it must read 2 on all three of the first columns and 1 on the"
  echo "oracle, which is a live feasible-model-refused. B is the guard on the"
  echo "widening: infeasible everywhere until the constant is raised."
  echo "-- BEFORE";           "$D/ab" | sed 's/^/   /'
  echo "-- AFTER";            "$D/aa" | sed 's/^/   /'
  echo "-- REFERENCE, oracle"; "$D/ar" | sed 's/^/   /'
  echo "-- ROUND_ULPS = 1e12"; "$D/aw" | sed 's/^/   /'
} | tee -a "$here/window.txt"
rm -rf "$D"

echo
echo "readings in $here/window.txt"
