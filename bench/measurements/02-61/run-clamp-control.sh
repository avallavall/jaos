#!/bin/bash
# The clamp is only evidence if removing it breaks something.
#
# The new assert says the published value lies inside the column's own
# recorded box. With the clamp in place, all 94 standard instances run under
# asserts. This removes ONLY the clamp, keeps the assert, and re-runs.
#
# EXPECT TWO, not eleven, and the difference is the finding this record
# corrects. Eleven instances trip the OLD assert, which asked whether the
# intersection is empty. Only `bnl1` and `finnis` ever published a value
# outside the column's own box — 10 records of the 138 empty intersections
# (gap-probe.txt). On the other 128 `want_lo` equals `rec->lo` with the box
# open above it, so the intersection is empty while the published value was
# inside the box all along.
#
# If fewer than two fire, the clamp is not load-bearing and the assert is not
# enforcing anything.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-61-control"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
git diff > "$wt/cand.diff"
cd "$wt" || exit 2
[ -s cand.diff ] && { git apply cand.diff || { echo "candidate did not apply"; exit 2; }; }
for dir in instances; do
    rm -rf "bench/$dir" && ln -s "$root/bench/$dir" "bench/$dir"
done

# Remove the clamp, keep the assert: xv goes back to want_lo.
python3 - "$wt" << 'PY'
import sys
p = sys.argv[1] + "/src/presolve.c"
s = open(p, encoding="utf-8").read()
clamp = """        const double xv = want_lo < rec->lo ? rec->lo
                        : want_lo > rec->hi ? rec->hi
                                            : want_lo;"""
assert s.count(clamp) == 1, "the clamp is not in the tree being controlled"
s = s.replace(clamp, "        const double xv = want_lo;")
open(p, "w", encoding="utf-8").write(s)
print("clamp removed, assert kept")
PY
[ $? -eq 0 ] || exit 2

d=$(mktemp -d)
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -UNDEBUG \
    -Iinclude -Isrc src/*.c bench/run.c -o "$d/run-assert" -lm \
    2> "$d/build.log" || { echo "build failed"; tail -20 "$d/build.log"; exit 2; }

echo "=== clamp REMOVED, the box assert kept ==="
fired=""; n=0; clean=0
while read -r name _; do
    case "$name" in \#*|"") continue;; esac
    n=$((n + 1))
    "$d/run-assert" -j 1 "$name" > "$d/$name.log" 2>&1
    if [ $? -eq 0 ]; then
        clean=$((clean + 1))
    else
        fired="$fired $name"
    fi
done < <(awk '{print $1}' bench/netlib.manifest)

echo "instances run:  $n"
echo "clean:          $clean"
echo "assert fired:   $(echo $fired | wc -w) --$fired"
echo
if [ "$fired" = " bnl1 finnis" ]; then
    echo "CONTROL OK — the clamp is load-bearing and the assert enforces it"
else
    echo "*** expected exactly bnl1 and finnis; the control did not reproduce ***"
fi

rm -rf "$d"
cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
exit 0
