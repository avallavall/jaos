#!/bin/bash
# The asserts D223 adds for `jaos_internal.h`'s contracts, and the proof that
# each catches the defect it exists for.
#
# Third in the series after 02-134 and 02-135, and it carries both of their
# lessons: the canary arm comes first, because "0 fired" and "-UNDEBUG never
# reached the compiler" produce identical output (D82); and every breaker is
# read against the code it breaks and against the SET it runs on, because a
# quiet arm is a reading of the instances until something fires.
#
# The contracts live in `jaos_internal.h` and the asserts live in the files
# that implement them, so this touches src/simplex.c, src/presolve.c and
# src/scale.c.
#
# The arms:
#
#   canary     an assert false by construction. MUST fire, or nothing below
#              is about an assert.
#   live       the candidate, standard 94. 0 must fire.
#   live-inf   the candidate, infeasible 29. 0 must fire — presolve's two
#              asserts are reached there and the standard set alone would be
#              a reading of the wrong population.
#   harris     `jm_harris_pick` handed a negative numerator. Its precondition
#              assert must fire.
#   pattern    `jm_pattern_order` handed a dirty scratch word. Its
#              clean-on-entry assert must fire.
#   nonbasic   one bit of `nbmark` flipped after a basis change, so the
#              bitmap stops equalling {v : status[v] != JM_BASIC}.
#   scale      one scale factor multiplied by 3, so it stops being an exact
#              power of two.
#   psindex    a presolve record pushed with a reduced-space index.
#
# Own trees, outside the repository. `bench/results/` is never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-136"
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

arm() {   # $1 = tag, $2 = breaker or "", $3.. = runner arguments
    local tag=$1 breaker=$2 wt="$D/wt-$tag"
    shift 2
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    for d in instances instances-infeas instances-kennington; do
        [ -d "$JAOS_ROOT/bench/$d" ] && ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d"
    done
    for f in simplex.c presolve.c scale.c model.c check.c jaos_internal.h; do
        cp "$JAOS_ROOT/src/$f" "$wt/src/$f"
    done
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG >/dev/null 2>&1 ) || {
        echo "  BUILD FAILED"; return 2; }
    # `timeout`, because a breaker that corrupts state instead of tripping a
    # check does not announce itself: the first `harris` breaker wrote its
    # negative value AFTER the assert, so the assert passed on clean data and
    # the solver took wrong steps for 24 minutes on a set that takes 85
    # seconds. An arm that runs long has failed, and it has to say so rather
    # than hold the whole control open.
    ( cd "$wt" && timeout 600 ./build/bench/run -j 12 "$@" -o "$D/$tag.txt" ) \
        > "$D/$tag.log" 2>&1
    if [ $? -eq 124 ]; then
        echo "  TIMED OUT after 600s -- the breaker corrupts state instead of"
        echo "  tripping the check, or the assert it targets is not reached"
        return 1
    fi
    return 0
}

BREAK_CANARY='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = "    assert(best >= 0 && best < n);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, old + "\n    assert(n != n);")
open(p, "w", encoding="utf-8").write(s)
print("  canary planted")
'

# A numerator that is negative when the precondition is checked.
#
# The first version of this breaker wrote the negative value AFTER the assert
# block. The assert passed on clean data, the corrupted value went into the
# ratio test, and the solver took wrong steps for 24 minutes on a set that
# takes 85 seconds -- a hang, not a firing. The write has to happen before
# the thing that is supposed to catch it, which is obvious in hindsight and
# was not obvious while writing it. The give-away was the runtime, not the
# output: a breaker that corrupts state rather than tripping a check does not
# announce itself.
BREAK_HARRIS='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = "        assert(num[k] >= 0.0);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "        if (k == 0) ((double *)num)[0] = -1.0;\n" + old)
open(p, "w", encoding="utf-8").write(s)
print("  a negative numerator reaches the precondition check")
'

# A scratch word left set from a previous call.
BREAK_PATTERN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    /* The touched range, so a small pattern does not pay for the bitmap. */
    int64_t lo = (limit + 63) / 64, hi = -1;"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, """    mark[0] |= UINT64_C(1);
    /* The touched range, so a small pattern does not pay for the bitmap. */
    int64_t lo = (limit + 63) / 64, hi = -1;""")
open(p, "w", encoding="utf-8").write(s)
print("  jm_pattern_order gets a dirty scratch word")
'

# The nonbasic bitmap stops tracking status.
BREAK_NONBASIC='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = "void jm_nonbasic_remove(uint64_t *mark, int64_t v)\n{\n    mark[v >> 6] &= ~(UINT64_C(1) << (v & 63));\n}"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "void jm_nonbasic_remove(uint64_t *mark, int64_t v)\n{\n    (void)mark; (void)v;\n}")
open(p, "w", encoding="utf-8").write(s)
print("  nbmark no longer drops an entering variable")
'

# A factor that is not an exact power of two.
BREAK_SCALE='
p = "src/scale.c"
s = open(p, encoding="utf-8").read()
old = "    return ldexp(1.0, (int)r);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "    return ldexp(1.0, (int)r) * 3.0;")
open(p, "w", encoding="utf-8").write(s)
print("  one scale factor is no longer a power of two")
'

# A presolve record carrying an index the caller model does not have.
BREAK_PSINDEX='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = "    assert(rec.index >= 0);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "    if (p->arena_len == 0) rec.index = -1;\n    assert(rec.index >= 0);")
open(p, "w", encoding="utf-8").write(s)
print("  the first presolve record carries an impossible index")
'

INFEAS="-m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas"

{
echo "# The asserts D223 adds for jaos_internal.h's contracts, and the control"
echo "# that proves each fires. tree $(git rev-parse --short "$ref") plus the"
echo "# working tree's src/, EXTRA_CFLAGS=-UNDEBUG, own worktree and make clean"
echo "# per arm."
echo

run_arm() {
    local tag=$1
    printf '## %s\n' "$tag"
    if ! arm "$@"; then echo "  COULD NOT RUN"; return; fi
    local n
    n=$(grep -cE 'Assertion .* failed' "$D/$tag.log" 2>/dev/null)
    echo "  assertion failures: $n"
    [ "$n" -gt 0 ] && grep -oE "Assertion \`[^']*' failed" "$D/$tag.log" | sort -u | sed 's/^/    /'
    echo "  record lines written: $(grep -cE '^[a-z0-9][A-Za-z0-9_.-]*[[:space:]]+[a-z]' "$D/$tag.txt" 2>/dev/null)"
}

run_arm canary   "$BREAK_CANARY"
run_arm live     ""
# shellcheck disable=SC2086
run_arm live-inf "" $INFEAS
run_arm harris   "$BREAK_HARRIS"
run_arm pattern  "$BREAK_PATTERN"
run_arm nonbasic "$BREAK_NONBASIC"
run_arm scale    "$BREAK_SCALE"
# shellcheck disable=SC2086
run_arm psindex  "$BREAK_PSINDEX" $INFEAS
} 2>&1 | tee "$here/header-assert-control.txt"

count_of() { awk -v t="## $1" '$0==t{f=1;next} f&&/assertion failures/{print $NF; exit}' "$here/header-assert-control.txt"; }
ok=0
[ "$(count_of canary)" -gt 0 ] 2>/dev/null || {
    echo "STOP: the canary did not fire, so -UNDEBUG never reached the compiler"
    echo "      and no arm below is a statement about an assert."; exit 2; }
for t in live live-inf; do
    [ "$(count_of "$t")" = 0 ] || { echo "FAIL: $(count_of "$t") fired on the unmodified candidate ($t)"; ok=1; }
done
for t in harris pattern nonbasic scale psindex; do
    [ "$(count_of "$t")" -gt 0 ] 2>/dev/null || { echo "FAIL: the $t arm fired nothing"; ok=1; }
done
# Each arm must name ITS OWN assert, not merely some assert.
grep -q 'num\[k\] >= 0.0' "$here/header-assert-control.txt" || { echo "FAIL: harris arm did not name the numerator assert"; ok=1; }
grep -q 'mark\[w\] == 0' "$here/header-assert-control.txt" || { echo "FAIL: pattern arm did not name the clean-scratch assert"; ok=1; }
grep -q 'in_map ==' "$here/header-assert-control.txt" || { echo "FAIL: nonbasic arm did not name the set-equality assert"; ok=1; }
grep -q 'frexp' "$here/header-assert-control.txt" || { echo "FAIL: scale arm did not name the power-of-two assert"; ok=1; }
grep -q 'rec.index' "$here/header-assert-control.txt" || { echo "FAIL: psindex arm did not name the index assert"; ok=1; }
[ "$ok" = 0 ] && echo "EVERY ASSERT CATCHES THE DEFECT IT EXISTS FOR" || echo "AT LEAST ONE ARM FAILED"
exit "$ok"
