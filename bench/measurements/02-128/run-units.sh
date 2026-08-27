#!/bin/bash
# Section 0 stage 6: what units does `can_move` belong in?
#
# `can_move`'s last line is
#     wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol
# `wrong_way` is a reduced cost, a RATE. The second factor is a DISTANCE in
# the units of x. Their product is an objective change. `dual_tol` bounds a
# rate: `dual_breach` reads it as one, and so does everything else in the
# file. The comparison is therefore wrong in both directions. A genuine
# breach with a narrow box reads below it; a reduced cost that is pure noise
# with a wide box reads above it.
#
# D184 measured a pure rate test dead on the dual set (94/94 digests) and
# refused the change. The reopen condition was the primal simplex landing.
# It landed (D188) and 02-118 re-ran the same one-line variant: the units are
# LIVE on the forced-primal campaign. That tree was 56be130 and four commits
# to the ratio test have landed since (D207, D209, D212, D213), so the
# reading has to be taken again before anything is decided.
#
# Three arms, because the pure rate test is not obviously safe:
#   base  HEAD, the product against a rate
#   rate  D184's one-liner, the rate against a rate
#   dist  the rate against a rate, AND the flip must actually move the column
#
# The third arm exists because a column with lo == up has a zero distance.
# HEAD's product is then 0 and `can_move` says no. The pure rate test says
# yes, flips the status, moves the point nowhere, and `anything_to_move`
# answers the same way on the next round: SETTLE_ROUNDS rounds spent on a
# column that cannot move. Presolve removes fixed columns, so this needs
# -DJAOS_NO_PRESOLVE to reach, which is a build the project ships.
#
# Two campaigns: the standard gate set (the dual path, D184's population) and
# the forced-primal campaign, whose settling re-entry runs `can_move` on
# 60.5% of all its iterations (D204).
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
    for s in base rate dist; do git worktree remove --force "$D/$s" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

for side in base rate dist; do
    git worktree add --detach "$D/$side" "$ref" > /dev/null 2>&1 || exit 2
    for i in instances instances-infeas instances-kennington; do
        ln -s "$root/bench/$i" "$D/$side/bench/$i"
    done
done

python3 - "$D/rate/src/simplex.c" "$D/dist/src/simplex.c" <<'PY'
import sys
OLD = "    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;\n"
ARMS = [
    # rate: D184's variant, the rate against a rate
    "    return wrong_way > s->dual_tol;\n",
    # dist: the same, and the flip must move the column somewhere
    "    if (wrong_way <= s->dual_tol)\n"
    "        return false;\n"
    "    return other != nonbasic_value(s, v);\n",
]
for path, new in zip(sys.argv[1:], ARMS):
    s = open(path, encoding='utf-8').read()
    assert s.count(OLD) == 1, "can_move's last line did not match exactly once in " + path
    open(path, 'w', encoding='utf-8').write(s.replace(OLD, new))
    print("patched", path.split('/')[-3])
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

run() {   # $1 side
    ( cd "$D/$1" && make netlib J="$J" > "$D/$1-netlib.log" 2>&1
      cd "$D/$1" && make primal J="$J" > "$D/$1-primal.log" 2>&1 )
    # `make primal` exits nonzero on pilot87's guard on every tree; the
    # record is what is compared, never the exit code.
    [ -s "$D/$1/bench/results/netlib.txt" ] && [ -s "$D/$1/bench/results/primal.txt" ]
}

strip() { grep -vE 'elapsed|took|s$' "$1"; }

{
  echo "# can_move's units, three arms, on the tree at $(git rev-parse --short "$ref")"
  echo "#   base = product against a rate (HEAD)"
  echo "#   rate = rate against a rate (D184's variant)"
  echo "#   dist = rate against a rate, and the flip must move the column"
  echo
  for side in base rate dist; do
      run "$side" || { echo "$side RUN FAILED"; exit 2; }
      echo "# $side ran"
  done
  echo
  for rec in netlib primal; do
      echo "===================== $rec ====================="
      a="$D/base/bench/results/$rec.txt"
      for side in rate dist; do
          b="$D/$side/bench/results/$rec.txt"
          if diff <(strip "$a") <(strip "$b") > "$D/$rec-$side.diff"; then
              echo "$side vs base: BYTE-IDENTICAL"
          else
              echo "$side vs base: MOVED, $(grep -c '^[<>]' "$D/$rec-$side.diff") differing lines"
              cat "$D/$rec-$side.diff"
          fi
          echo
      done
      if diff <(strip "$D/rate/bench/results/$rec.txt") \
              <(strip "$D/dist/bench/results/$rec.txt") > /dev/null; then
          echo "rate vs dist: BYTE-IDENTICAL (the zero-distance guard fires on no instance here)"
      else
          echo "rate vs dist: THEY DIFFER, so the guard decides something"
          diff <(strip "$D/rate/bench/results/$rec.txt") \
               <(strip "$D/dist/bench/results/$rec.txt")
      fi
      echo
  done
  echo "===================== verdict counts ====================="
  for side in base rate dist; do
      p="$D/$side/bench/results/primal.txt"
      printf "%-5s ok=%-4s DISAGREE=%-4s overrun=%-4s ERROR=%s\n" "$side" \
          "$(grep -cE '^[a-z0-9._-]+ +ok ' "$p")" \
          "$(grep -cE '^[a-z0-9._-]+ +DISAGREE ' "$p")" \
          "$(grep -c 'overrun' "$p")" \
          "$(grep -c 'ERROR' "$p")"
  done
  echo
  echo "===== done ====="
} 2>&1 | tee "$here/run-units.txt"
