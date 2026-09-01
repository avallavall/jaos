#!/usr/bin/env bash
# Do the four scratch-and-bitmap asserts hold on the whole population?
#
# D234 adds four checks to `src/simplex.c`: `alpha`'s pattern names every
# nonzero, `rho`'s does too, `c1` is all zero after its incremental clear, and
# `nbmark` matches `status` at every successful `refresh`.
#
# The last one is the reason this script exists. D223 put that walk inline in
# `dual_ratio_test`, which is the dual path only; `run_primal`,
# `run_primal_phase1` and `primal_cleanup` all reach `pivot()` without passing
# it. Moving it to `refresh` covers the primal for the first time, so the
# primal has to be RUN, and `bench/run` only ever runs the dual.
#
# Two arms:
#   dual     bench/run with -UNDEBUG over all 139 gate instances
#   primal   02-145`s probe over 33 instances in BOTH methods
#
# Both build in a throwaway worktree, so the main tree`s `build/` is untouched
# and `make` cannot serve objects built without the flag (D154).
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-147/run-scratch-population.sh
# Writes population.txt beside this script. Exit 0 when nothing fired, 1 when
# something did, 2 when the harness failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-147"
probe="$JAOS_ROOT/bench/measurements/02-145/probe.c"
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

CC=gcc-14
# The 32 smallest of the standard set plus beaconfd, the same set 02-145 used.
# The primal does not finish the heavy instances, which is why this is a
# subset and the dual arm is not.
PROBE_SET="afiro sc50b sc50a kb2 sc105 adlittle stocfor1 blend scagr7 sc205 \
share2b recipe lotfi vtp-base share1b boeing2 bore3d scorpion capri brandy \
sctap1 scagr25 israel scfxm1 bandm e226 grow7 etamacro agg finnis scsd1 \
standata beaconfd"

git worktree add --detach "$wt" "$(git rev-parse HEAD)" >/dev/null 2>&1 || exit 2
cp src/simplex.c "$wt/src/" || exit 2
for d in instances instances-kennington instances-infeas; do
    ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || exit 2
done

out="$here/population.txt"
{
echo "# 02-147 -- the four scratch asserts of D234 over the population"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copy of src/simplex.c"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# no baseline is read: this asks whether anything ABORTS"
echo
} > "$out"

cd "$wt" || exit 2
make clean >/dev/null 2>&1
make build/bench/run EXTRA_CFLAGS=-UNDEBUG > "$D/build.log" 2>&1 || {
    echo "BUILD FAILED" | tee -a "$out"
    grep -E 'error:' "$D/build.log" | head -10; exit 2; }
echo "== build: bench/run with -UNDEBUG, from clean" >> "$out"

fail=0
run_set() {  # $1 = label, $2.. = extra args
    local label=$1; shift
    echo "== dual: $label"
    timeout 5400 ./build/bench/run -j 12 -o "$D/$label.txt" "$@" \
        > "$D/$label.log" 2>&1
    local rc=$? a
    a=$(grep -c 'Assertion' "$D/$label.log")
    {
      echo "== dual: $label"
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

run_set netlib
run_set netlib-infeas -e infeasible -m bench/netlib-infeas.manifest \
    -d bench/instances-infeas
run_set netlib-kennington -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington

# The primal arm. The dev objects carry no -DNDEBUG, and the probe drives
# `cfg.force_primal` for the second half of each instance.
echo "== primal + dual: the probe"
make build/dev/test_simplex >> "$D/build.log" 2>&1 || {
    echo "DEV BUILD FAILED" | tee -a "$out"; exit 2; }
$CC -std=c23 -Wall -Wextra -ffp-contract=off -Og -g -Iinclude -Isrc \
    "$probe" $(ls build/dev/*.o | grep -v '/unity\.o$') -o probe -lm \
    >> "$D/build.log" 2>&1 || {
    echo "PROBE BUILD FAILED" | tee -a "$out"
    grep -E 'error:' "$D/build.log" | head -10; exit 2; }
# shellcheck disable=SC2086
timeout 3600 ./probe $PROBE_SET > "$D/probe.log" 2>&1
prc=$?
pa=$(grep -c 'Assertion' "$D/probe.log")
{
  echo "== probe: 33 instances, both methods"
  echo "   exit=$prc  solves=$(grep -c '^done' "$D/probe.log")"
  echo "   assertion lines=$pa"
} >> "$out"
if [ "$pa" != "0" ]; then
    grep -m5 'Assertion' "$D/probe.log" | sed 's/^/   FIRED: /' >> "$out"
    fail=1
fi
[ $prc -ne 0 ] && fail=1
echo >> "$out"

{
echo "== verdict"
if [ $fail -eq 0 ]; then
    echo "every arm completed and no assert fired"
else
    echo "SOMETHING FIRED OR ABORTED"
fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "scratch-population exit=$fail  ->  $out"
exit $fail
