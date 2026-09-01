#!/usr/bin/env bash
# Do the eleven asserts hold on all 139 gate instances?
#
# The arms in `run-assert-controls.sh` prove each assert fires when its
# contract is broken. They do not prove it stays quiet on the whole
# population: the probe there solves 33 instances, and `census-forcing.sh`
# stops every solve as soon as presolve returns, so the four new `simplex.c`
# asserts had seen a third of the standard set and none of the other two sets.
# D152 is the rule this closes -- an assert-enabled build runs every instance.
#
# `bench/run` compiled with `-UNDEBUG`, in a throwaway worktree so the main
# tree's `build/` is untouched (and so `make` cannot serve objects built
# without that flag, which is D154's trap). Records go to the temporary
# directory and no baseline is read: this asks whether anything aborts, not
# whether anything regressed. The gate answers the second question.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-145/run-assert-population.sh
# Writes population.txt beside this script. Exit 0 when every set completed
# with no assert fired, 1 when one did, 2 when the harness failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-145"
cd "$JAOS_ROOT" || exit 2
D=$(mktemp -d) || exit 2
wt="$D/wt"
cleanup() {
    cd "$JAOS_ROOT" || exit
    git worktree remove --force "$wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

git worktree add --detach "$wt" "$(git rev-parse HEAD)" >/dev/null 2>&1 || exit 2
cp src/simplex.c src/presolve.c "$wt/src/" || exit 2
# Gitignored and 300 MB: linked, never refetched.
for d in instances instances-kennington instances-infeas; do
    ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || exit 2
done

out="$here/population.txt"
{
echo "# 02-145 -- the eleven asserts over all 139 instances, -UNDEBUG"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of src/simplex.c src/presolve.c"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# no baseline is read: this asks whether anything ABORTS"
echo
} > "$out"

cd "$wt" || exit 2
make clean >/dev/null 2>&1
make build/bench/run EXTRA_CFLAGS=-UNDEBUG > "$D/build.log" 2>&1 || {
    echo "BUILD FAILED" | tee -a "$out"
    grep -E 'error:' "$D/build.log" | head -10
    exit 2; }
echo "== build: bench/run with -UNDEBUG, from clean" >> "$out"

fail=0
run_set() {  # $1 = label, $2.. = extra args to bench/run
    local label=$1; shift
    echo "== $label"
    timeout 5400 ./build/bench/run -j 12 -o "$D/$label.txt" "$@" \
        > "$D/$label.log" 2>&1
    local rc=$? a
    a=$(grep -c 'Assertion' "$D/$label.log")
    {
      echo "== $label"
      echo "   exit=$rc  records=$(grep -c 'det=' "$D/$label.txt")"
      echo "   assertion lines=$a"
    } >> "$out"
    if [ "$a" != "0" ]; then
        grep -m5 'Assertion' "$D/$label.log" | sed 's/^/   FIRED: /' >> "$out"
        fail=1
    fi
    [ $rc -ne 0 ] && fail=1
    echo >> "$out"
    return 0
}

# `-e infeasible` is not optional on that set: without it every instance is
# judged against an expectation of OPTIMAL, the runner exits 1 and writes no
# record, and the run says nothing about the asserts at all.
run_set netlib
run_set netlib-infeas -e infeasible -m bench/netlib-infeas.manifest \
    -d bench/instances-infeas
run_set netlib-kennington -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every set completed and no assert fired"
else echo "AT LEAST ONE SET ABORTED OR FIRED AN ASSERT"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "population exit=$fail  ->  $out"
exit $fail
