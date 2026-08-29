#!/bin/bash
# The paid half of the sweep: `PHASE1_RISE_MAX` at three settings, each its
# own tree and its own binary, the forced-primal campaign at every one.
#
# `rise-sweep.sh` predicted the whole curve from ONE instrumented run, on the
# argument that the rule is a per-iteration gate on a single quantity: an
# instance either never crosses the threshold, and then nothing about it
# changes, or it crosses first at a known iteration. This script is what makes
# that argument checkable rather than plausible. It predicts, per setting,
# exactly which instances stop, and then runs the campaign and compares.
#
# The three settings, and what each is for:
#
#   1e-12   the low end. Predicted to fire on five instances -- dfl001, pilot,
#           pilot-ja, pilot87, woodw -- which are the five that cross 1e-12 in
#           rise-sweep.txt, named in advance
#   1.0     the shipped value. Predicted to fire on pilot87 alone
#   1e+12   above pilot87's own worst rise of 8.06882e+11, so predicted to
#           fire on nothing. This is the strongest control in the set: the
#           campaign at 1e+12 must equal the campaign of the tree BEFORE the
#           rule existed, byte for byte, or the branch is doing something
#           besides what its constant says
#
# `make clean` between every setting, because `make` does not see a change in
# a source constant's value any better than it sees one in EXTRA_CFLAGS, and a
# five-point sweep once read exactly 1.0000x at every setting for that reason
# (D82). Each setting gets its own worktree, so there is no shared build to
# get it wrong with.
#
# Own trees, outside the repository. `bench/results/` is never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-133"
cd "$JAOS_ROOT" || exit 2
# The rule arms are built from the WORKING TREE's `src/simplex.c`, not from a
# ref, so this can be run before the commit as well as after. D217's check
# said STOP twice because it could only ever read the previous commit.
[ -f src/simplex.c ] || { echo "no src/simplex.c" >&2; exit 2; }
grep -q '^constexpr double PHASE1_RISE_MAX' src/simplex.c || {
    echo "the working tree has no PHASE1_RISE_MAX to sweep" >&2; exit 2; }
# The pre-rule tree is the newest commit whose simplex.c does not have it,
# found rather than assumed, so this holds whether the rule is committed yet
# or not.
base=""
for c in $(git rev-list -30 HEAD); do
    if ! git show "$c:src/simplex.c" 2>/dev/null | grep -q 'PHASE1_RISE_MAX'; then
        base=$c; break
    fi
done
[ -n "$base" ] || { echo "no commit in the last 30 predates PHASE1_RISE_MAX" >&2; exit 2; }
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

# One campaign in a tree of its own. $1 = tag, $2 = the commit to check out,
# $3 = the value to force, or "" for the pre-rule arm, which is left as the
# commit had it.
campaign() {
    local tag=$1 at=$2 val=$3 wt="$D/wt-$1"
    git worktree add --detach "$wt" "$at" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances"
    if [ -n "$val" ]; then
        # The working tree's file, so the rule under test is the one on disk.
        cp "$JAOS_ROOT/src/simplex.c" "$wt/src/simplex.c"
        local n
        n=$(grep -c '^constexpr double PHASE1_RISE_MAX' "$wt/src/simplex.c")
        [ "$n" = 1 ] || { echo "ANCHOR: PHASE1_RISE_MAX matched $n times in $tag" >&2; return 2; }
        sed -i "s/^constexpr double PHASE1_RISE_MAX = .*/constexpr double PHASE1_RISE_MAX = $val;/" \
            "$wt/src/simplex.c"
        grep -q "PHASE1_RISE_MAX = $val;" "$wt/src/simplex.c" || {
            echo "SUBSTITUTION FAILED in $tag" >&2; return 2; }
    else
        grep -q 'PHASE1_RISE_MAX' "$wt/src/simplex.c" && {
            echo "the pre-rule arm $tag already has the rule" >&2; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make build/bench/primal >/dev/null 2>&1 ) || {
        echo "BUILD FAILED for $tag" >&2; return 2; }
    ( cd "$wt" && ./build/bench/primal -j 12 -o "$D/$tag.txt" >/dev/null 2>&1 )
    [ -s "$D/$tag.txt" ] || { echo "NO RECORD for $tag" >&2; return 2; }
    return 0
}

verdicts() {   # $1 = record -> "name verdict" per line
    awk 'NF && $1 !~ /^#/ && $2 ~ /^(ok|DISAGREE|overrun|crash|error)$/ {print $1, $2}' "$1" | sort
}

{
echo "# PHASE1_RISE_MAX swept over three settings, forced-primal campaign at each."
echo "# The rule arms carry the WORKING TREE's src/simplex.c on top of"
echo "# $(git rev-parse --short "$ref"); the pre-rule arm is $(git rev-parse --short "$base"),"
echo "# found as the newest commit whose simplex.c does not define the constant."
echo "# Each setting has its own worktree, its own make clean and its own binary."
echo

for spec in "base:$base:" "lo:$ref:1e-12" "ship:$ref:1.0" "hi:$ref:1e+12"; do
    tag=${spec%%:*}; rest=${spec#*:}; at=${rest%%:*}; val=${rest#*:}
    echo "## $tag  (ref $(git rev-parse --short "$at"), PHASE1_RISE_MAX=${val:-as committed})"
    campaign "$tag" "$at" "$val" || { echo "  COULD NOT RUN"; continue; }
    verdicts "$D/$tag.txt" > "$D/$tag.v"
    awk '{c[$2]++} END{for (k in c) printf "  %s %d\n", k, c[k]}' "$D/$tag.v" | sort
done

echo
echo "## which instances the rule stopped, per setting"
echo "# against the pre-rule tree; an instance listed here changed verdict."
for tag in lo ship hi; do
    [ -s "$D/$tag.v" ] && [ -s "$D/base.v" ] || continue
    moved=$(join "$D/base.v" "$D/$tag.v" | awk '$2 != $3 {print $1}' | tr '\n' ' ')
    echo "$tag: ${moved:-none}"
done

echo
echo "## the hi control: every record line identical to the pre-rule tree?"
if [ -s "$D/hi.txt" ] && [ -s "$D/base.txt" ]; then
    if diff -q "$D/base.txt" "$D/hi.txt" >/dev/null; then
        echo "IDENTICAL -- the branch is inert when its constant is out of reach"
    else
        echo "DIFFERS -- the branch changes something besides what its constant says:"
        diff "$D/base.txt" "$D/hi.txt" | head -20
    fi
fi
} 2>&1 | tee "$here/rise-sweep-campaign.txt"

# The verdict, in the shape bench/refusals.txt reads. Every clause is a
# prediction rise-sweep.txt made before this ran.
lo_moved=$(join "$D/base.v" "$D/lo.v" 2>/dev/null | awk '$2 != $3 {print $1}' | sort | tr '\n' ' ')
ship_moved=$(join "$D/base.v" "$D/ship.v" 2>/dev/null | awk '$2 != $3 {print $1}' | sort | tr '\n' ' ')
ok=0
[ "$ship_moved" = "pilot87 " ] || { echo "MISMATCH at 1.0: expected 'pilot87', got '$ship_moved'"; ok=1; }
# At 1e-12 the census names five instances that CROSS. The rule refuses only
# on a point it has recomputed first, so a crossing a refactorization removes
# does not stop anything -- which is the whole reason for that retry. So the
# prediction is a subset relation, not an equality, and `pilot87` is the
# member it must contain.
case " $lo_moved " in
    *" pilot87 "*) ;;
    *) echo "MISMATCH at 1e-12: pilot87 did not stop; got '$lo_moved'"; ok=1;;
esac
for i in $lo_moved; do
    case "$i" in
        dfl001|pilot|pilot-ja|pilot87|woodw) ;;
        *) echo "MISMATCH at 1e-12: $i stopped and never crosses 1e-12 in the census"; ok=1;;
    esac
done
diff -q "$D/base.txt" "$D/hi.txt" >/dev/null 2>&1 || {
    echo "MISMATCH at 1e+12: the record is not identical to the pre-rule tree"; ok=1; }
[ "$ok" = 0 ] && echo "ALL THREE PREDICTIONS HELD" || echo "AT LEAST ONE PREDICTION FAILED"
exit "$ok"
