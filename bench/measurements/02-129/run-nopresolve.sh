#!/bin/bash
# The one finding the review could not confirm: fixed columns.
#
# `breached(s, v)` accepts a column HEAD's product refused. For a fixed
# column (`lo == up`) the product is exactly zero, so HEAD always called it
# immovable; the union reads its reduced cost and can flip it. The flip moves
# no point, so `digest=` cannot see it -- but `basis_digest` (bench/run.c:188)
# hashes the published statuses, so `basis=` can.
#
# Equality-row logicals have `lo == up` in the default build, and the three
# gate sets came back with `basis=` unchanged everywhere except the two
# `pds` instances that also moved their digest. This runs the build where
# fixed STRUCTURAL columns also survive, which is the one build no reading of
# this change has used.
#
# Three arms, because the product differs from the union in two ways at once
# and only a third arm separates them:
#   np-base   HEAD, the product
#   np-rate   the scaled rate alone -- product vs rate isolates the FIXED
#             COLUMN, whose distance is exactly zero
#   np-union  breached, both spaces -- rate vs union isolates the PUBLISHED
#             SPACE
#
# netlib only, and no baseline comparison: there is no committed baseline for
# a presolve-less build, so both records "REGRESS" against the presolve one.
# The three records are compared against each other.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
J="${J:-12}"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    for s in np-base np-rate np-union; do git worktree remove --force "$D/$s" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

for side in np-base np-rate np-union; do
    git worktree add --detach "$D/$side" "$ref" > /dev/null 2>&1 || exit 2
    ln -s "$root/bench/instances" "$D/$side/bench/instances"
done

python3 - "$D/np-union/src/simplex.c" <<'PY'
import sys
OLD = "    return wrong_way * fabs(other - nonbasic_value(s, v)) > s->dual_tol;\n"
NEW = "    return breached(s, v);\n"
path = sys.argv[1]
src = open(path, encoding='utf-8').read()
assert src.count(OLD) == 1, "no match in " + path
open(path, 'w', encoding='utf-8', newline='').write(src.replace(OLD, NEW))
print("patched np-union")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

{
  echo "# can_move on the -DJAOS_NO_PRESOLVE reference build, netlib, tree $(git rev-parse --short "$ref")"
  echo "#   np-base  = HEAD, the product (a fixed column is always immovable)"
  echo "#   np-rate  = wrong_way > s->dual_tol (the scaled space alone)"
  echo "#   np-union = breached(s, v) (both spaces)"
  echo

  echo "===================== canary ====================="
  grep -q 'fabs(other - nonbasic_value' "$D/np-base/src/simplex.c" ||
      { echo "np-base LOST THE PRODUCT -- wrong worktree"; exit 2; }
  grep -q 'return breached(s, v);' "$D/np-union/src/simplex.c" ||
      { echo "np-union DID NOT GET THE PATCH"; exit 2; }
  echo "np-base carries the product, np-union carries breached: the two trees differ"
  echo

  for side in np-base np-rate np-union; do
      echo "===================== $side ====================="
      ( cd "$D/$side" && make netlib J="$J" EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE \
            > "$D/$side.log" 2>&1 )
      echo "$side make exit $?"
      f="$D/$side/bench/results/netlib.txt"
      [ -s "$f" ] || { echo "$side WROTE NO RECORD"; exit 2; }
      grep -E '^gate:|instances:' "$f"
      echo
  done

  # Field extraction reads only lines that carry a digest, so the instance
  # name and the field always come from the same line. Reading the whole file
  # picked up the summary and REGRESSED lines and misaligned every name --
  # the first version of this script named `pilot4` for a move that was
  # `pilotnov`.
  fields() { grep -E 'digest=[0-9a-f]+' "$1" | awk -v k="$2" '{ n=$1; for (i=1;i<=NF;i++) if ($i ~ "^" k "=") print n, $i }'; }

  base="$D/np-base/bench/results/netlib.txt"
  for side in np-rate np-union; do
      f="$D/$side/bench/results/netlib.txt"
      echo "===================== $side vs np-base ====================="
      if diff "$base" "$f" > "$D/$side.diff"; then
          echo "BYTE-IDENTICAL"
      else
          echo "DIFFER, $(grep -c '^[<>]' "$D/$side.diff") lines"
          for k in digest basis work iters; do
              echo "--- $k moved on ---"
              join <(fields "$base" "$k") <(fields "$f" "$k") |
                  awk '$2 != $3 { print "   ", $1, $2, "->", $3 }'
          done
      fi
      echo
  done

  echo "===================== np-rate vs np-union ====================="
  if diff "$D/np-rate/bench/results/netlib.txt"           "$D/np-union/bench/results/netlib.txt" > /dev/null; then
      echo "BYTE-IDENTICAL: the published space reaches no column on this build"
      echo "either, so the whole move belongs to the units and not to the space"
  else
      echo "THEY DIFFER: the published space decides something here"
      diff "$D/np-rate/bench/results/netlib.txt"            "$D/np-union/bench/results/netlib.txt"
  fi
  echo
  echo "===================== the full diff, np-base vs np-union ====================="
  cat "$D/np-union.diff" 2>/dev/null || echo "(none)"
  echo
  echo "===== done ====="
} 2>&1 | tee "$here/run-nopresolve.txt"
