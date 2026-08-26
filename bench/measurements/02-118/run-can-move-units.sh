#!/bin/bash
# D184's reopen condition is met. Does it change the answer?
#
# `can_move` compares a rate-times-distance PRODUCT against `dual_tol`, which is
# a bound on a RATE everywhere else. D184 measured that correcting the units
# moves 0 of 94 digests, and gave the reason: `can_move` feeds
# `anything_to_move` on the settling re-entry, and what those columns need is a
# primal pivot that did not exist. Its stated reopen condition was the primal
# simplex landing. It landed 2026-08-25 (D188) and nothing re-measured this.
#
# The re-test is the same one-line variant D184 ran, on the same two campaigns
# that can see it now: the standard gate (the dual, D184's original population)
# and the forced-primal campaign, whose settling re-entry runs `can_move` on
# 60.5% of all its iterations (D204). If every digest and every verdict is
# byte-identical on both, the refusal still holds. If anything moves, the
# units are live and the question goes back to TODO.md as open.
#
# Exit 0 = refusal holds (identical), 1 = something moved, 2 = could not run.
# `make refusals` reads that code.
#
# Both sides in worktrees of HEAD, OUTSIDE the repository (make clean is
# rm -rf build). The candidate is HEAD plus the one-line patch.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
J="${J:-6}"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/base" 2>/dev/null
    git worktree remove --force "$D/cand" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

for side in base cand; do
    git worktree add --detach "$D/$side" "$ref" > /dev/null 2>&1 || exit 2
    for i in instances instances-infeas instances-kennington; do
        ln -s "$root/bench/$i" "$D/$side/bench/$i"
    done
done

python3 - "$D/cand/src/simplex.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
OLD = "    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;\n"
NEW = "    return wrong_way > s->dual_tol;   /* D184's variant: rate against rate */\n"
assert s.count(OLD) == 1, "can_move's last line did not match exactly once"
open(p, 'w', encoding='utf-8').write(s.replace(OLD, NEW))
print("candidate patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }
grep -q "D184's variant" "$D/cand/src/simplex.c" || { echo "patch did not apply"; exit 2; }

run() {   # $1 side
    ( cd "$D/$1" && make netlib J="$J" > "$D/$1-netlib.log" 2>&1 \
                 && make primal J="$J" > "$D/$1-primal.log" 2>&1 )
    # `make primal` exits 1 on pilot87's guard on every tree; the record is
    # what is compared, not the exit code.
    [ -s "$D/$1/bench/results/netlib.txt" ] && [ -s "$D/$1/bench/results/primal.txt" ]
}

{
  echo "# can_move: rate-times-distance (HEAD) against rate (D184's variant), on the tree at $(git rev-parse --short "$ref")"
  echo "# two campaigns: the gate's standard set (the dual) and the forced-primal campaign"
  echo
  run base || { echo "BASE RUN FAILED"; exit 2; }
  run cand || { echo "CANDIDATE RUN FAILED"; exit 2; }
  moved=0
  for rec in netlib primal; do
      a="$D/base/bench/results/$rec.txt"; b="$D/cand/bench/results/$rec.txt"
      # strip the elapsed-seconds lines, which are never comparable
      if diff <(grep -vE 'elapsed|took|s$' "$a") <(grep -vE 'elapsed|took|s$' "$b") > "$D/$rec.diff"; then
          echo "$rec: BYTE-IDENTICAL, $(grep -c digest= "$a" 2>/dev/null || grep -cE '^[a-z0-9-]+ ' "$a") record lines"
      else
          moved=1
          echo "$rec: MOVED, $(grep -c '^[<>]' "$D/$rec.diff") differing lines:"
          head -20 "$D/$rec.diff"
      fi
  done
  echo
  if [ $moved -eq 0 ]; then
      echo "VERDICT: the refusal holds. can_move's units move nothing on the dual and nothing on the primal."
  else
      echo "VERDICT: the units are LIVE. D184's premise has expired; the question is open."
  fi
} 2>&1 | tee "$here/run-can-move-units.txt"
grep -q 'refusal holds' "$here/run-can-move-units.txt" && exit 0
grep -q 'units are LIVE' "$here/run-can-move-units.txt" && exit 1
exit 2
