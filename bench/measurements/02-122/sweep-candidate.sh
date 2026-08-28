#!/bin/bash
# The candidate as it actually stands, swept. `sweep-cheap.sh` measured a
# version `numerics-reviewer` then rejected on two counts:
#
#   1. the floor could empty the candidate set, and -1 out of either ratio
#      test is read by both callers as "no declared bound stops this column",
#      which they refuse on -- so the repair for one false refusal could
#      manufacture another;
#   2. `*step` came from the floored pass, and the callers use it to decide a
#      bound flip, which then moves every row including the one the floor had
#      just called meaningless. That can push a basic past a declared bound.
#
# The candidate keeps pass 0's answer: `*step` is read off every row, and
# pass 0's winner stands when the floor leaves nothing. So -1 keeps its one
# old meaning and a flip cannot overshoot.
#
# This sweeps the WORKING TREE, not HEAD: the diff is applied into a worktree
# and `PIVOT_MARGIN` is made an environment read there, so one binary serves
# every setting. C=0 must still reproduce bench/results/primal.txt exactly.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
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

python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
OLD = "constexpr double PIVOT_MARGIN  = 1.0;"
assert s.count(OLD) == 1, "anchor matched %d times" % s.count(OLD)
NEW = """#include <stdlib.h>
static double sweep_margin(void)
{
    static int done = 0;
    static double v = 1.0;
    if (!done) {
        const char *e = getenv("JAOS_PIVOT_MARGIN");
        v = e != nullptr ? atof(e) : 1.0;
        done = 1;
    }
    return v;
}
#define PIVOT_MARGIN sweep_margin()"""
open(p, 'w', encoding='utf-8').write(s.replace(OLD, NEW))
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/primal >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/primal 2>&1 | grep -E 'error' | head; exit 2; }

for C in ${*:-0 1 3e-1 2}; do
    echo "### PIVOT_MARGIN=$C"
    JAOS_PIVOT_MARGIN="$C" ./build/bench/primal -j 12 \
        -o "$here/cand-$C.txt" >"$here/cand-$C.log" 2>&1
    echo "exit=$?  $(grep -c '^' "$here/cand-$C.txt" 2>/dev/null) record lines"
    echo
done
