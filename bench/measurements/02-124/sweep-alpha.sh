#!/bin/bash
# PINNED: ab4943f plus the working tree -- the anchors are code, not comments.
#
# Stage 8a's candidate, measured with the control D207's first implementation
# needed and did not have.
#
# `alpha_unusable` adds a walk over column q's nonzeros to compute the traffic
# behind `alpha[q]`, and bills it the way `price_entry` bills the same walk.
# `bench/primal` caps the primal solve at 10x the dual's WORK, so a new charge
# shortens every primal solve and moves instances across that bar for reasons
# that have nothing to do with the change (D203, and D207's own first version
# lost `bnl2` and `tuff` exactly this way).
#
# So the sweep runs the traffic walk at BOTH settings and varies only the
# constant. At C=0 the relative half can never fire -- `a < 0` is false for
# any non-negative `a` -- while the walk still runs and still bills. So:
#
#   C=0 vs the committed record  = the traffic walk's cost, alone
#   C=1 vs C=0                   = the floor's effect, alone
#
# That separation is the whole point. The census says C=1 should reject
# exactly one call, on `scsd1`, and reach nothing on the gate.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root=/mnt/c/Users/vall-/Desktop/projectes/jaos
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT

git diff HEAD -- src tests include > "$D/candidate.patch" || exit 2
[ -s "$D/candidate.patch" ] || { echo "NO DIFF TO MEASURE"; exit 2; }
echo "# candidate: $(git rev-parse --short "$ref") plus $(grep -c '^+' "$D/candidate.patch") added lines"

git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
ln -s "$root/bench/instances" "$D/wt/bench/instances"
cd "$D/wt" || exit 2
git apply "$D/candidate.patch" || { echo "PATCH DID NOT APPLY"; exit 2; }

# Only the ALPHA side reads the environment. The column side keeps the
# constexpr 1.0 that D207 landed, so this sweep varies one thing.
python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
OLD = "    const double rel = PIVOT_MARGIN * DBL_EPSILON * alpha_traffic(s, q);"
assert s.count(OLD) == 1, "anchor matched %d times" % s.count(OLD)
NEW = """    const double rel = alpha_margin() * DBL_EPSILON * alpha_traffic(s, q);"""
s = s.replace(OLD, NEW)

ANCH = "/* Everything that went into `alpha[q] = rho' M_q`"
assert s.count(ANCH) == 1
s = s.replace(ANCH, """#include <stdlib.h>
static double alpha_margin(void)
{
    static int done = 0;
    static double v = 1.0;
    if (!done) {
        const char *e = getenv("JAOS_ALPHA_MARGIN");
        v = e != nullptr ? atof(e) : 1.0;
        done = 1;
    }
    return v;
}

""" + ANCH)
open(p, 'w', encoding='utf-8').write(s)
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal 2>&1 | grep -E 'error' | head; exit 2; }

for C in ${*:-0 1}; do
    echo "### JAOS_ALPHA_MARGIN=$C"
    JAOS_ALPHA_MARGIN="$C" ./build/bench/primal -j 12 \
        -o "$here/alpha-$C.txt" >"$here/alpha-$C.log" 2>&1
    echo "exit=$?  $(grep -c '^' "$here/alpha-$C.txt" 2>/dev/null) record lines"
    echo
done
