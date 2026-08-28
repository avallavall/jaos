#!/bin/bash
# The eight asserts added to src/lu.c run only under a non-NDEBUG build, and
# `make configs` only ever runs the unit suite, whose matrices are small. Two
# things are missing and both are cheap:
#
#   live   the release build with -UNDEBUG, over all 94 netlib instances, so
#          the asserts see real factorizations and real updates
#   control  the same build with the `mult_set` clear loop DELETED. If the
#          step-top assert does not fire there, it is checking nothing
#          (jaos-testing: an instrument that finds nothing is worth nothing
#          until it is shown able to find something)
#
# Both in worktrees; the main tree's records are never written.
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    for s in live control; do git worktree remove --force "$D/$s" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

for side in live control; do
    git worktree add --detach "$D/$side" "$ref" > /dev/null 2>&1 || exit 2
    ln -s "$root/bench/instances" "$D/$side/bench/instances"
    # the uncommitted asserts
    cp "$root/src/lu.c" "$D/$side/src/lu.c"
done

python3 - "$D/control/src/lu.c" <<'PY'
import sys
path = sys.argv[1]
OLD = """        for (int64_t k = 0; k < e.piv_n; k++)
            e.mult_set[e.piv_row[k]] = false;
"""
s = open(path, encoding='utf-8').read()
assert s.count(OLD) == 1, "the clear loop did not match once"
open(path, 'w', encoding='utf-8', newline='').write(s.replace(OLD, "        /* CONTROL: the clear is deleted on purpose. */\n"))
print("control: the mult_set clear loop is gone")
PY
[ $? -ne 0 ] && { echo "SETUP FAILED"; exit 2; }

{
echo
echo "===================== canary: the two trees differ ====================="
if diff -q "$D/live/src/lu.c" "$D/control/src/lu.c" > /dev/null; then
    echo "IDENTICAL -- the control was not patched, STOP"; exit 2
fi
echo "they differ, and both carry the asserts:"
grep -c "assert(" "$D/live/src/lu.c" "$D/control/src/lu.c"
echo

for side in live control; do
    echo "===================== $side ====================="
    ( cd "$D/$side" && make netlib J=12 EXTRA_CFLAGS=-UNDEBUG > "$D/$side.log" 2>&1 )
    echo "$side make exit $?"
    echo "assertion failures seen: $(grep -c 'Assertion' "$D/$side.log")"
    grep -m3 'Assertion' "$D/$side.log"
    grep -E '^gate:|instances:' "$D/$side/bench/results/netlib.txt" 2>/dev/null || echo "  (no record written)"
    echo
done

echo "===================== verdict ====================="
# `grep -c` PRINTS 0 and EXITS 1 when it matches nothing, so `|| echo 0`
# appended a SECOND zero and every numeric test below died with "integer
# expression expected". The first run of this script printed STOP over its
# own passing numbers, which is the summary line lying about the readings
# directly above it.
l=$(grep -c 'Assertion' "$D/live.log" 2>/dev/null)
c=$(grep -c 'Assertion' "$D/control.log" 2>/dev/null)
echo "live asserts fired: $l    control asserts fired: $c"
if [ "$l" -eq 0 ] && [ "$c" -gt 0 ]; then
    echo "PASS: the asserts hold on 94 real instances AND catch the defect they exist for"
elif [ "$l" -gt 0 ]; then
    echo "STOP: an assert fires on a legitimate solve"
else
    echo "STOP: the control did not fire, so the step-top assert checks nothing"
fi
echo "===== done ====="
} 2>&1 | tee "$here/run-assert-control.txt"
