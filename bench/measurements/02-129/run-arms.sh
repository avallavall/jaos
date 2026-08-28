#!/bin/bash
# Section 0 stage 6, second reading: the two sets 02-128 could not take, and
# TWO candidate arms rather than one.
#
# 02-128 measured `can_move`'s units on `netlib` and the forced-primal
# campaign and found no verdict moved. Its own README names what was missing:
# `pds-20` is a Kennington instance, D27's whole cautionary case is about
# `pds-20`, and `make netlib-kennington` was never run. `netlib-infeas` was
# missing for the same reason.
#
# A numerics review of the candidate added a second arm before this ran.
# `wants_a_pivot` (src/simplex.c:2196) is `can_move`'s sibling: same noise
# test, complementary `isfinite(other)` question, and it filters with
# `breached()` -- the union of the scaled and published readings, which D92
# says may not replace one another. A pure scaled-rate test in `can_move`
# therefore leaves a column breached ONLY in published space with no repair
# anywhere: `can_move` rejects it, `arm_reentry`'s else branch is guarded by
# the scaled `dual_breach` so it is not shifted either, and `wants_a_pivot`
# refuses it because its other bound is finite. `settled_dual_violation`
# still counts it.
#
#   rate   D184's one-liner, the scaled space alone: wrong_way > s->dual_tol
#          (identical to `dual_breach(s, v) != 0` past the two gates above)
#   union  the same question in both spaces: breached(s, v)
#          (the filter `wants_a_pivot` already uses)
#
# Base is the committed record in the main tree. A run of clean HEAD on
# 2026-08-28 reproduced all three byte-for-byte, so the 25 src/ commits
# since those records were written were no-ops on these sets and the
# committed record is a valid parent.
#
# Worktrees OUTSIDE the repository: `make clean` is rm -rf build.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
J="${J:-12}"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    for s in rate union; do git worktree remove --force "$D/$s" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

for side in rate union; do
    git worktree add --detach "$D/$side" "$ref" > /dev/null 2>&1 || exit 2
    for i in instances instances-infeas instances-kennington; do
        ln -s "$root/bench/$i" "$D/$side/bench/$i"
    done
done

python3 - "$D/rate/src/simplex.c" "$D/union/src/simplex.c" <<'PY'
import sys
OLD = "    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;\n"
ARMS = [
    "    return wrong_way > s->dual_tol;\n",
    "    return breached(s, v);\n",
]
for path, new in zip(sys.argv[1:], ARMS):
    src = open(path, encoding='utf-8').read()
    assert src.count(OLD) == 1, "can_move's last line did not match once in " + path
    open(path, 'w', encoding='utf-8', newline='').write(src.replace(OLD, new))
    print("patched", path.split('/')[-3])
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

SETS="netlib netlib-infeas netlib-kennington"
RD="$root/.claude/skills/jaos-measure/scripts/record_diff.py"

{
  echo "# can_move's units on all three gate sets, two arms, tree $(git rev-parse --short "$ref")"
  echo "#   rate  = wrong_way > s->dual_tol        (scaled space alone)"
  echo "#   union = breached(s, v)                 (both spaces, D92)"
  echo "# base    = the committed bench/results/, reproduced byte-for-byte by a"
  echo "#           run of clean HEAD on 2026-08-28"
  echo

  # Canary: the two arms must not be the same binary. If the patch did not
  # reach the build, every diff below reads BYTE-IDENTICAL and says nothing.
  echo "===================== canary ====================="
  for side in rate union; do
      echo "$side can_move last line: $(grep -A1 'if (!isfinite(other))' "$D/$side/src/simplex.c" | tail -1)"
      grep -q 'fabs(other - nonbasic_value' "$D/$side/src/simplex.c" &&
          { echo "$side STILL CARRIES THE PRODUCT -- patch did not apply"; exit 2; }
  done
  echo

  for side in rate union; do
      echo "===================== $side: building and running ====================="
      ( cd "$D/$side" && make $SETS J="$J" > "$D/$side.log" 2>&1 )
      echo "$side make exit $?"
      for rec in $SETS; do
          f="$D/$side/bench/results/$rec.txt"
          if [ ! -s "$f" ]; then echo "$side $rec: NO RECORD WRITTEN"; continue; fi
          echo "--- $side $rec: gate line and baseline line ---"
          grep -E '^gate:|^baseline:' "$f"
      done
      grep -E 'gate: FAIL|error|Error' "$D/$side.log" | head -5
      echo
  done

  for rec in $SETS; do
      for side in rate union; do
          echo "===================== record_diff: $side vs base, $rec ====================="
          python3 "$RD" "$D/$side/bench/results/$rec.txt" \
                  --against-file "$root/bench/results/$rec.txt"
          echo "  (record_diff exit $?)"
          echo
      done
  done

  echo "===================== pds-20, the instance D27 warned about ====================="
  echo "--- base ---"
  grep -E '^pds-20 ' "$root/bench/results/netlib-kennington.txt"
  for side in rate union; do
      echo "--- $side ---"
      grep -E '^pds-20 ' "$D/$side/bench/results/netlib-kennington.txt"
  done
  echo
  echo "===== done ====="
} 2>&1 | tee "$here/run-arms.txt"
