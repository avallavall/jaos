#!/bin/bash
# What a compensated sum could not reach, and what recovers it.
#
# D169 made the published objective a compensated sum, which removed the
# ACCUMULATION error and left the other one: each `c_j * x_j` is rounded once
# before it is added, and no accumulator reaches an error already inside a
# term. This measures the residue and the repair, in three readings.
#
#   1. the minimum model -- two columns where every accumulator gives 0 and
#      the answer is 1, so only recovering the product's residue can pass
#   2. `finnis` and `scagr7` split four ways, which separates the accumulation
#      error from the product error on the worst cancellation in the set
#   3. |published objective - the checker's long double objective of the same
#      point|, over every instance that solves, before and after
#
# Usage: run-two-product.sh [git-ref]      default: 39a49f6, D172's parent
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-39a49f6}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
for f in src/*.c src/*.h; do
    git show "$ref:$f" > "$D/before/$(basename "$f")" || exit 2
done

{
  echo "### 1. the minimum model"
  for cfg in "" "-DJAOS_NO_PRESOLVE"; do
      gcc-14 $P $cfg "$D/before"/*.c "$here/minimal-model.c" -o "$D/b" -lm || exit 2
      gcc-14 $P $cfg src/*.c         "$here/minimal-model.c" -o "$D/a" -lm || exit 2
      echo "-- ${cfg:-shipping build}, the parent ($ref)"; "$D/b"
      echo "-- ${cfg:-shipping build}, the two-product";   "$D/a"
  done
  echo
  echo "### 2. where the error is, on the worst cancellation in the set"
  gcc-14 $P "$D/before"/*.c "$here/../02-79/split-the-error.c" -o "$D/sb" -lm || exit 2
  gcc-14 $P src/*.c "$here/../02-79/split-the-error.c" -o "$D/sa" -lm || exit 2
  echo "-- the parent"; "$D/sb" bench/instances/finnis.mps bench/instances/scagr7.mps
  echo "-- the two-product (the first two lines are the probe's own sums and"
  echo "   do not move; what moves is what jaos_objective publishes, in 3)"
  "$D/sa" bench/instances/finnis.mps bench/instances/scagr7.mps
  echo
  echo "### 3. against the checker, every instance that solves"
  gcc-14 $P "$D/before"/*.c "$here/../02-79/objective-vs-checker.c" -o "$D/ob" -lm 2>/dev/null || exit 2
  gcc-14 $P src/*.c         "$here/../02-79/objective-vs-checker.c" -o "$D/oa" -lm 2>/dev/null || exit 2
  for dir in bench/instances bench/instances-kennington; do
      "$D/ob" "$dir" > "$D/b.txt"; "$D/oa" "$dir" > "$D/a.txt"
      python3 - "$dir" "$D/b.txt" "$D/a.txt" <<'PY'
import sys
def load(p):
    d = {}
    for L in open(p):
        f = L.split()
        if len(f) == 3:
            d[f[0]] = (float(f[1]), float(f[2]))
    return d
tag, a, b = sys.argv[1], load(sys.argv[2]), load(sys.argv[3])
closer = further = same = 0
ea = eb = 0
worst = []
for k in a:
    if k not in b:
        continue
    x, y = abs(a[k][0] - a[k][1]), abs(b[k][0] - b[k][1])
    ea += (x == 0)
    eb += (y == 0)
    if y < x:
        closer += 1
    elif y > x:
        further += 1
        worst.append((y - x, k, x, y))
    else:
        same += 1
worst.sort(reverse=True)
print(f"{tag}: {len(a)} instances -- closer={closer} further={further} "
      f"unchanged={same}")
print(f"   EXACT agreement with the checker: "
      f"compensated sum only={ea}   two-product={eb}")
for d, k, x, y in worst[:5]:
    print(f"   further {k}: {x:.6g} -> {y:.6g}")
PY
  done
} | tee "$here/two-product.txt"
