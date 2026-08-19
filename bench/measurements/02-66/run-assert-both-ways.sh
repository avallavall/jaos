#!/bin/bash
# The row_traffic sweep assert, validated on both sides.
#
# A green assert proves nothing on its own: an assert that cannot fire is
# indistinguishable from one that holds. So this runs the property twice --
# once on the tree as it stands, and once with ONLY the accumulation reverted
# to the pre-repair form, the assert itself untouched. The second must abort.
#
# The reverted copy is built from a temp directory, never in the tree.
#
# Run from anywhere; it locates the repository itself.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9

D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"' EXIT
FLAGS="-std=c23 -Wall -Wextra -ffp-contract=off -O2 -Iinclude -Isrc"

# -UNDEBUG turns the asserts on; everything else matches the release build.
echo "==== 1. the tree as it stands, asserts ON, all 139 instances ===="
gcc-14 $FLAGS -UNDEBUG src/*.c bench/run.c -o "$D/run-new" -lm 2>"$D/b1.log" \
    || { echo "build failed"; tail -20 "$D/b1.log"; exit 2; }

for spec in "bench/instances bench/netlib.manifest" \
            "bench/instances-infeas bench/netlib-infeas.manifest -e infeasible" \
            "bench/instances-kennington bench/netlib-kennington.manifest"; do
    set -- $spec; d=$1; m=$2; shift 2
    "$D/run-new" -d "$d" -m "$m" "$@" -j 12 >"$D/o" 2>"$D/e"
    printf '  %-24s rc=%-3s aborts=%-3s solved=%s\n' "$(basename "$d")" "$?" \
        "$(grep -c Assertion "$D/e")" "$(grep -cE 'optimal|infeasible' "$D/o")"
done

echo
echo "==== 2. the case the assert MUST reject ===="
mkdir -p "$D/old"
cp src/*.c src/*.h "$D/old/" 2>/dev/null
python3 - "$D/old/presolve.c" <<'PY' || exit 2
import sys, io
p = sys.argv[1]
s = io.open(p, encoding='utf-8', newline='').read()
new = """                    double moved = 0.0;
                    if (lo_absorbs && isfinite(cmax) && fabs(cmax) > moved)
                        moved = fabs(cmax);
                    if (hi_absorbs && isfinite(cmin) && fabs(cmin) > moved)
                        moved = fabs(cmin);
                    row_traffic[i] += moved;"""
old = """                    row_traffic[i] += fabs(cmax) > fabs(cmin) ? fabs(cmax)
                                                              : fabs(cmin);"""
if s.count(new) != 1:
    sys.exit("revert target not found -- this validation is VOID, not passing")
io.open(p, 'w', encoding='utf-8', newline='').write(s.replace(new, old, 1))
print("  (accumulation reverted to the pre-repair form; the sweep kept)")
PY
gcc-14 $FLAGS -UNDEBUG "$D/old"/*.c bench/run.c -o "$D/run-old" -lm 2>"$D/b2.log" \
    || { echo "  build failed"; tail -10 "$D/b2.log"; exit 2; }
"$D/run-old" -d bench/instances -m bench/netlib.manifest -j 12 >/dev/null 2>"$D/e2"
n=$(grep -c Assertion "$D/e2")
echo "  netlib with the OLD accumulation: aborts=$n of 94"
grep -m1 'Assertion' "$D/e2" | sed 's/^/    /'
[ "$n" -gt 0 ] || { echo "  THE ASSERT DID NOT FIRE -- it is not testing the repair"; exit 1; }

echo
echo "==== 3. the shape that WOULD defeat the assert on a legal model ===="
# row_traffic sums magnitudes, the bounds sum signed values, so two terms of
# opposite sign cancel in the ends and add in the budget. Found by
# numerics-reviewer while reviewing the diff; the assert's defence is measured
# headroom, not this being impossible.
cat > "$D/c.c" <<'CC'
#include <stdio.h>
#include <math.h>
int main(void) {
    double rl = 0, ru = 0, traffic = 0;
    const double t[2] = { 1e308, -1e308 };
    for (int k = 0; k < 2; k++) {
        rl -= t[k]; ru -= t[k]; traffic += fabs(t[k]);
        printf("  after %-9g rl=%-10g ru=%-10g traffic=%g\n",
               t[k], rl, ru, traffic);
    }
    printf("  either end finite=%d  traffic finite=%d  ASSERT FIRES=%d\n",
           (isfinite(rl) || isfinite(ru)), isfinite(traffic),
           !(!(isfinite(rl) || isfinite(ru)) || isfinite(traffic)));
    return 0;
}
CC
gcc-14 -O0 -ffp-contract=off "$D/c.c" -o "$D/c" -lm && "$D/c"
