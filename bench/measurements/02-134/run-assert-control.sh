#!/bin/bash
# The asserts D219 adds to `model.c`, and the proof that each catches the
# defect it exists for.
#
# An assert that never fires is indistinguishable from one that cannot fire.
# D216 established the shape for `lu.c`: run the gate with `-UNDEBUG` at the
# candidate and confirm 0 fire, then break the invariant in a worktree and
# confirm the SAME assert fires and names itself.
#
# The first version of this script reported 0 assertions in all three arms,
# including both broken ones, and read as a finished answer. Two of its arms
# were dead and the third could not be told from a build with asserts off:
#
#   - it broke the pair by making `store_basis` leave `start_row_status`
#     null, and `store_basis`'s own next statement detects exactly that and
#     clears BOTH arrays, restoring the pair. The breaker was undone by the
#     code it was breaking.
#   - it made `publish` publish an objective on its non-OPTIMAL branch, and
#     then ran the standard 94, where every solve is OPTIMAL and that branch
#     never executes.
#   - nothing in it distinguished "the invariant held" from "-UNDEBUG never
#     reached the compiler", which is D82's failure wearing a different hat.
#
# So there are four arms now, and the first one is the instrument's own test:
#
#   canary  an assert that is false by construction, in the function under
#           test. It MUST fire, or the other three arms mean nothing.
#   live    the candidate as committed. 0 must fire.
#   pair    `start_row_status` nulled at the END of `store_basis`, past its
#           guards. JM_BASIS_PAIRED must fire — and it fires in the UNIT
#           SUITE, not in the gate. The gate never touches a model's
#           dimensions after solving it, so `basis_extend` and
#           `basis_survives_or_goes` are unreachable there and `publish`
#           calls `jm_model_remember_basis` once per process, after which
#           nothing reads the pair again. 0 firings over 94 instances is
#           what that looks like, and it is not a statement about the
#           assert. `test_a_dimension_change_the_solve_can_see` solves,
#           adds a row, adds a column and deletes both, which is the path.
#   status  `publish` publishes an objective on its non-OPTIMAL branch, run
#           against the INFEASIBLE set, where that branch is what executes.
#           The status assert must fire.
#
# Own trees, outside the repository. `bench/results/` is never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-134"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

# Every arm carries the WORKING TREE's sources on top of the ref, so this runs
# before the commit as well as after (D217's lesson).
# $1 = tag, $2 = python breaker or "", $3... = extra runner arguments
arm() {
    local tag=$1 breaker=$2 wt="$D/wt-$tag"
    shift 2
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances"
    [ -d "$JAOS_ROOT/bench/instances-infeas" ] && \
        ln -s "$JAOS_ROOT/bench/instances-infeas" "$wt/bench/instances-infeas"
    cp "$JAOS_ROOT/src/model.c" "$wt/src/model.c"
    cp "$JAOS_ROOT/src/simplex.c" "$wt/src/simplex.c"
    cp "$JAOS_ROOT/src/jaos_internal.h" "$wt/src/jaos_internal.h"
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG >/dev/null 2>&1 ) || {
        echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && ./build/bench/run -j 12 "$@" -o "$D/$tag.txt" ) > "$D/$tag.log" 2>&1
    return 0
}

# The same, run against the unit suite instead of the gate, for an invariant
# the instances cannot reach. The dev build has no `-DNDEBUG` at all, so the
# asserts are on without any flag; `record-check` is skipped because the
# worktree's DECISIONS.md predates the entry the candidate source cites.
arm_tests() {
    local tag=$1 breaker=$2 wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances"
    cp "$JAOS_ROOT/src/model.c" "$wt/src/model.c"
    cp "$JAOS_ROOT/src/simplex.c" "$wt/src/simplex.c"
    cp "$JAOS_ROOT/src/jaos_internal.h" "$wt/src/jaos_internal.h"
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make build/dev/test_model >/dev/null 2>&1 ) || { echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && ./build/dev/test_model ) > "$D/$tag.log" 2>&1
    : > "$D/$tag.txt"
    return 0
}

# The instrument's own test: an assert that cannot hold, in the function the
# other arms are about. If this does not fire, -UNDEBUG did not reach the
# compiler and every other reading below is a reading of a release build.
BREAK_CANARY='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = "    assert(m->solve_status == JAOS_SOLVE_OPTIMAL);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, old + "\n    assert(m->num_col != m->num_col);")
open(p, "w", encoding="utf-8").write(s)
print("  canary planted")
'

# Past every guard `store_basis` has, so the pair really is left unequal.
BREAK_PAIR='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = """    if (m->num_row > 0)
        memcpy(m->start_row_status, row,
               (size_t)m->num_row * sizeof *m->start_row_status);
    return JAOS_OK;"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, """    if (m->num_row > 0)
        memcpy(m->start_row_status, row,
               (size_t)m->num_row * sizeof *m->start_row_status);
    free(m->start_row_status);
    m->start_row_status = nullptr;
    return JAOS_OK;""")
open(p, "w", encoding="utf-8").write(s)
print("  pair broken past every guard")
'

# The non-OPTIMAL branch of `publish`, which is what the infeasible set runs.
BREAK_STATUS='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """#endif
        return JAOS_OK;
    }"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, """#endif
        jm_model_publish_objective(m);
        return JAOS_OK;
    }""")
open(p, "w", encoding="utf-8").write(s)
print("  status precondition broken")
'

INFEAS="-m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas"

{
echo "# The asserts D219 adds to model.c, and the control that proves each fires."
echo "# tree $(git rev-parse --short "$ref") plus the working tree's src/,"
echo "# EXTRA_CFLAGS=-UNDEBUG, each arm its own worktree and its own make clean."
echo

run_arm() {   # $1 = how (gate|tests), $2 = tag, $3 = breaker, $4.. = runner args
    local how=$1 tag=$2
    shift
    printf '## %s\n' "$tag"
    if ! "arm${how:+_tests}" "$@"; then
        echo "  COULD NOT RUN"
        return
    fi
    local n
    n=$(grep -cE 'Assertion .* failed' "$D/$tag.log" 2>/dev/null)
    echo "  assertion failures in the run log: $n"
    if [ "$n" -gt 0 ]; then
        grep -oE "Assertion \`[^']*' failed" "$D/$tag.log" | sort -u | sed 's/^/    /'
    fi
    echo "  record lines written: $(grep -cE '^[a-z0-9][A-Za-z0-9_.-]*[[:space:]]+[a-z]' "$D/$tag.txt" 2>/dev/null)"
    grep -qE '^[0-9]+ Tests' "$D/$tag.log" && \
        echo "  unit suite: $(grep -E '^[0-9]+ Tests' "$D/$tag.log" | head -1)"
}

run_arm ""    canary "$BREAK_CANARY"
run_arm ""    live   ""
run_arm tests pair   "$BREAK_PAIR"
run_arm tests pair-live ""
# shellcheck disable=SC2086
run_arm ""    status "$BREAK_STATUS" $INFEAS
} 2>&1 | tee "$here/assert-control.txt"

count_of() { awk -v t="## $1" '$0==t{f=1;next} f&&/assertion failures/{print $NF; exit}' "$here/assert-control.txt"; }
canary_n=$(count_of canary); live_n=$(count_of live)
pair_n=$(count_of pair);     stat_n=$(count_of status)
pairlive_n=$(count_of pair-live)
ok=0
[ "${pairlive_n:-x}" = 0 ] || { echo "FAIL: $pairlive_n assertions fired in the unbroken unit suite"; ok=1; }
[ "${canary_n:-0}" -gt 0 ] 2>/dev/null || {
    echo "STOP: the canary assert did not fire, so -UNDEBUG never reached the"
    echo "      compiler and every arm below it is a release build. Nothing here"
    echo "      is a statement about an assert."; exit 2; }
[ "${live_n:-x}" = 0 ] || { echo "FAIL: $live_n assertions fired on the unmodified candidate"; ok=1; }
[ "${pair_n:-0}" -gt 0 ] 2>/dev/null || { echo "FAIL: breaking the start-array pair fired nothing"; ok=1; }
[ "${stat_n:-0}" -gt 0 ] 2>/dev/null || { echo "FAIL: publishing on a non-OPTIMAL status fired nothing"; ok=1; }
grep -q 'JM_BASIS_PAIRED' "$here/assert-control.txt" || {
    echo "FAIL: the pair arm fired something, but not JM_BASIS_PAIRED"; ok=1; }
grep -q 'solve_status == JAOS_SOLVE_OPTIMAL' "$here/assert-control.txt" || {
    echo "FAIL: the status arm fired something, but not the status assert"; ok=1; }
[ "$ok" = 0 ] && echo "EVERY ASSERT CATCHES THE DEFECT IT EXISTS FOR" || echo "AT LEAST ONE ARM FAILED"
exit "$ok"
