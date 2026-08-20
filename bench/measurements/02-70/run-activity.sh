#!/bin/bash
# The activity pass's INFEASIBLE clause, and the window it did not have.
#
#   1. Over the three sets: how much wider is a clause-1-only window, does it
#      flip a verdict, and does it raise the ABSOLUTE window at all?
#   2. The model, on three builds. The campaign flips nothing either way, so
#      only a constructed case separates them.
#
# The probe applies to the tree BEFORE the change, so it builds a detached
# worktree at the parent.
#
# Usage: run-activity.sh [git-ref]        default: 36320ad, the parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-36320ad}
wt="$root/build/diag/wt-02-70"

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
python3 "$here/probe-activity.py" || { echo "probe did not apply at $ref"; exit 2; }
mkdir -p build/diag
gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -DJAOS_DIAG -Iinclude -Isrc \
       src/*.c bench/run.c -o build/diag/act -lm 2>/dev/null || exit 2

{
  echo "probe at $ref ($(git rev-parse --short HEAD))"
  echo
  echo "######## 1. the three sets ########"
  for spec in "instances netlib.manifest" \
              "instances-infeas netlib-infeas.manifest -e infeasible" \
              "instances-kennington netlib-kennington.manifest"; do
      set -- $spec; d=$1; m=$2; shift 2
      ./build/diag/act -d "bench/$d" -m "bench/$m" "$@" -j 12 2>/tmp/a.err >/dev/null
      awk -v S="$d" '/DIAG-ACT/{for(i=2;i<=NF;i++){split($i,k,"=");
          if(k[1] ~ /^worst|^max_/){ if(k[2]+0>m[k[1]]) m[k[1]]=k[2]+0 }
          else v[k[1]]+=k[2]}}
        END{printf "---- %s\n", S
            printf "  rows at the activity pass    %d\n", v["rows"]
            printf "  clause 1 fires, shipped      %d\n", v["c1_now"]
            printf "  clause 1 fires, candidate    %d\n", v["c1_cand"]
            printf "  verdicts it would FLIP       %d\n", v["flip"]
            printf "  rows where it is wider       %d\n", v["wider"]
            printf "  worst candidate/shipped      %g\n", m["worst_ratio"]
            printf "  widest ABSOLUTE, shipped     %.4g\n", m["max_now"]
            printf "  widest ABSOLUTE, candidate   %.4g\n", m["max_cand"]}' /tmp/a.err
  done
} | tee "$here/activity.txt"

cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
git worktree prune

# ---- 2. the model, on three builds -----------------------------------------
D=$(mktemp -d) || exit 2
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"
CASE=$root/bench/measurements/02-69/activity-pass.c
mkdir -p "$D/before"
git show "$ref:src/presolve.c" > "$D/before/presolve.c"
for f in src/*.c src/*.h; do
    b=$(basename "$f"); [ "$b" = presolve.c ] || cp "$f" "$D/before/$b"
done
gcc-14 $P                    "$D/before"/*.c "$CASE" -o "$D/b" -lm || exit 2
gcc-14 $P                    src/*.c         "$CASE" -o "$D/a" -lm || exit 2
gcc-14 $P -DJAOS_NO_PRESOLVE src/*.c         "$CASE" -o "$D/r" -lm || exit 2
{
  echo
  echo "######## 2. the model ########"
  echo "A is this change's; B is D159's guard and must not move."
  echo "-- BEFORE";            "$D/b" | sed 's/^/   /'
  echo "-- AFTER";             "$D/a" | sed 's/^/   /'
  echo "-- REFERENCE, oracle"; "$D/r" | sed 's/^/   /'
} | tee -a "$here/activity.txt"
rm -rf "$D"

echo
echo "readings in $here/activity.txt"
