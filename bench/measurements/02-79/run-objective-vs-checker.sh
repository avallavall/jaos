#!/bin/bash
# How far is the published objective from the objective of the point it is
# published with?
#
# `jaos_check_solution` answers the same question about the same point in
# `long double`, independently of every solver bookkeeping, so it is the
# oracle here. `jaos.h` promises "objective value of the solution held by the
# model", and this is the measurement of that promise.
#
# The gate cannot see this: bench/run.c's digest covers x and y and not the
# objective, and its `objective=ok` verdict compares against a published
# reference with a tolerance, which both trees pass on all 94.
#
# Usage: run-objective-vs-checker.sh [git-ref] [dir]
#        default: ba69a88, D169's parent; bench/instances
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
ref=${1:-ba69a88}
dir=${2:-bench/instances}
cd "$root" || exit 9
D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
P="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc"

mkdir -p "$D/before"
for f in src/*.c src/*.h; do
    git show "$ref:$f" > "$D/before/$(basename "$f")" || exit 2
done
gcc-14 $P "$D/before"/*.c "$here/objective-vs-checker.c" -o "$D/b" -lm 2>/dev/null || exit 2
gcc-14 $P src/*.c         "$here/objective-vs-checker.c" -o "$D/a" -lm 2>/dev/null || exit 2
"$D/b" "$dir" > "$D/b.txt"
"$D/a" "$dir" > "$D/a.txt"

{
  echo "$dir, the parent $ref against D169."
  echo
  python3 - "$D/b.txt" "$D/a.txt" <<'PY'
import sys
def load(p):
    d = {}
    for L in open(p):
        f = L.split()
        if len(f) == 3:
            d[f[0]] = (float(f[1]), float(f[2]))
    return d
a, b = load(sys.argv[1]), load(sys.argv[2])
closer = further = same = 0
exact_a = exact_b = 0
worst = []
for k in a:
    if k not in b:
        continue
    x, y = abs(a[k][0] - a[k][1]), abs(b[k][0] - b[k][1])
    exact_a += (x == 0)
    exact_b += (y == 0)
    if y < x:
        closer += 1
    elif y > x:
        further += 1
        worst.append((y - x, k, x, y))
    else:
        same += 1
worst.sort(reverse=True)
print("|published objective - the checker's long double objective of the")
print(f"same point|, over {len(a)} instances that solve to OPTIMAL:")
print(f"  closer={closer}  further={further}  unchanged={same}")
print(f"  EXACT agreement with the checker: before={exact_a}  after={exact_b}")
if worst:
    print("  the ones that move the wrong way, largest first:")
for d, k, x, y in worst[:10]:
    print(f"    {k}: {x:.6g} -> {y:.6g}")
PY
} | tee "$here/objective-vs-checker.txt"
