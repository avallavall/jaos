#!/bin/bash
# Stage 2a: the three gate sets at a candidate Harris width.
#
# The forced-primal campaign in sweep-delta.sh is not a gate. The gate reaches
# primal_cleanup on three instances -- wood1p, pilot87, etamacro -- and D212
# already moved a digest on wood1p there, so a width cannot be chosen without
# reading them.
#
# A throwaway worktree at HEAD gets the width made an environment read, so one
# binary serves every setting (D154's trap). bench/results is restored from git
# between settings, so each width is diffed against the SAME committed
# baselines and never against the previous width.
#
# Usage: gate-delta.sh [width ...]      default: 0.1 0.5 1
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
root="$JAOS_ROOT"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() { cd "$root" || exit; git worktree remove --force "$D/wt" 2>/dev/null; git worktree prune; rm -rf "$D"; }
trap cleanup EXIT

echo "# gate at HEAD $(git rev-parse --short "$ref"), width read from the environment"
git worktree add --detach "$D/wt" "$ref" >/dev/null 2>&1 || exit 2
for d in instances instances-infeas instances-kennington; do
    ln -s "$root/bench/$d" "$D/wt/bench/$d"
done
cd "$D/wt" || exit 2

python3 - "$D/wt/src/simplex.c" <<'PY'
import sys
sx = sys.argv[1]

def sub(path, old, new):
    s = open(path, encoding='utf-8').read()
    n = s.count(old)
    assert n == 1, "anchor matched %d times in %s: %r" % (n, path, old[:60])
    open(path, 'w', encoding='utf-8').write(s.replace(old, new))

CONST = 'constexpr double PIVOT_MIN     = 1e-9;   /* smallest usable |alpha| */'
sub(sx, CONST, CONST + """
#include <stdlib.h>
static double harris_delta(void)
{
    static int done = 0;
    static double v = 1.0;
    if (!done) {
        const char *e = getenv("JAOS_HARRIS_DELTA");
        v = e != nullptr ? atof(e) : 1.0;
        done = 1;
    }
    return v;
}""")

# The width is a constant since D213, so the anchor is the local that holds
# it. Every width above 1.0 trips primal_pick's assert, which is why this must
# stay a RELEASE build: the bench binaries carry -DNDEBUG (Makefile:107).
sub(sx, "const double width = PRIMAL_HARRIS_DELTA * s->primal_tol;",
        "const double width = harris_delta() * s->primal_tol;")
print("patched")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

make clean >/dev/null 2>&1
make build/bench/run >/dev/null 2>&1 \
    || { echo "BUILD FAILED"; make build/bench/run 2>&1 | grep -E 'error' | head; exit 2; }

# 1 runs first as the control: it is what shipped, so its gate must be
# byte-identical to the committed baselines. If it is not, the environment
# read is not equivalent to the constant and no other width can be believed.
for W in ${*:-1 0.1 0.5}; do
    echo
    echo "########## JAOS_HARRIS_DELTA=$W ##########"
    git checkout -- bench/results
    # The verdict lines go into the record too. Without them an empty
    # gate-<W>.txt cannot be told from a campaign that never ran, which is
    # exactly what "no instance moved" is supposed to look like.
    {
        echo "# JAOS_HARRIS_DELTA=$W against the committed baselines at $(git rev-parse --short HEAD)"
        JAOS_HARRIS_DELTA="$W" make netlib netlib-infeas netlib-kennington J=12 2>&1 \
            | grep -E 'gate:|baseline:' | tail -8
        echo "# per-instance diff:"
        git diff -U0 bench/results/ | grep -E '^[-+][a-z0-9]' \
            | sed -E 's/ presolve=[^ ]*//; s/ ref=.*//' | cut -c1-160
    } | tee "$here/gate-$W.txt"
    echo "--- instance lines that moved: $(grep -cE '^[-+][a-z0-9]' "$here/gate-$W.txt") ---"
done
git checkout -- bench/results
echo
echo "===== done ====="
