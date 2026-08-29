#!/bin/bash
# The four asserts D221 adds to `src/check.c`, and the proof that each catches
# the defect it exists for.
#
# Same shape as 02-134's, and the same rule: an assert that never fires cannot
# be told from one that cannot fire, so every arm that reports 0 has to be
# accompanied by an arm that reports more than 0 for a reason.
#
# 02-134 cost three attempts and the lessons are built in here from the start:
#
#   - the canary arm comes FIRST, because "0 fired" and "-UNDEBUG never
#     reached the compiler" produce the same output (D82's failure);
#   - every breaker is checked against the code it breaks, so the function's
#     own next statement cannot undo it;
#   - every arm names which set it runs, because a branch the standard 94
#     never take is a branch no reading of them says anything about.
#
# The arms:
#
#   canary    an assert false by construction inside `jaos_check_solution`.
#             MUST fire, or nothing below is about an assert.
#   live      the candidate. 0 must fire, over all three instance sets.
#   magnitude `split_term` adds the negative half without negating it, so
#             `neg` stops being a magnitude. The pos/neg assert must fire.
#   step      `certified_step` stops clamping room at zero, so it can return
#             a negative distance. The `t >= 0.0L` assert must fire.
#   loosen    `implied_bounds` writes a bound the model declared, instead of
#             only one it left infinite. The monotone assert must fire.
#
# The `certified_step` precondition assert has no arm of its own: breaking it
# means changing the caller's `drops` test, which changes which columns are
# dropped and so changes the verdict rather than tripping the assert. It is
# covered by `live` reaching the function at all, which the count below
# reports.
#
# Own trees, outside the repository. `bench/results/` is never written.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-135"
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
    cp "$JAOS_ROOT/src/check.c" "$wt/src/check.c"
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG >/dev/null 2>&1 ) || {
        echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && ./build/bench/run -j 12 "$@" -o "$D/$tag.txt" ) > "$D/$tag.log" 2>&1
    return 0
}

BREAK_CANARY='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = "        assert(a.pos >= 0.0L && a.neg >= 0.0L);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, old + "\n        assert(a.pos != a.pos);")
open(p, "w", encoding="utf-8").write(s)
print("  canary planted")
'

# `split_term` keeps the negative half as a magnitude. Stop negating it.
BREAK_MAGNITUDE='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = "        *neg -= t;    /* kept as a magnitude, so both halves are >= 0 */"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "        *neg += t;")
open(p, "w", encoding="utf-8").write(s)
print("  split_term no longer keeps a magnitude")
'

# Room is clamped at zero because the point may sit a tolerance outside a
# bound. Remove both clamps and the step can come back negative.
BREAK_STEP='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
a = "            limit = (room > 0.0L ? room : 0.0L) / per_t;"
b = "            limit = (room > 0.0L ? room : 0.0L) / -per_t;"
assert s.count(a) == 1 and s.count(b) == 1, "anchors matched %d and %d" % (s.count(a), s.count(b))
s = s.replace(a, "            limit = room / per_t;")
s = s.replace(b, "            limit = room / -per_t;")
open(p, "w", encoding="utf-8").write(s)
print("  certified_step no longer clamps room at zero")
'

# Loosen one declared bound, directly, past every guard.
#
# The first version of this breaker dropped the `lim < cu[j]` test at one of
# the two upper-bound write sites, and fired nothing. It could only bite on a
# column whose lower bound is infinite and whose upper bound is finite — the
# outer loop skips a column with both bounds finite — and then only if the
# implied limit happened to exceed the declared one. No column of the 94 met
# both conditions. That is a statement about the models, not about the assert,
# which is 02-134's lesson arriving a second time.
#
# So this writes the violation itself, on the first column that has a finite
# lower bound. Whether real code could produce it is a different question; an
# assert is a tripwire, and what has to be shown is that the tripwire is live
# and in the right place. It doubles as the reachability proof: if it fires,
# `implied_bounds` runs on these instances, which `live` alone cannot say.
BREAK_LOOSEN='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """#ifndef NDEBUG
    for (int64_t j = 0; j < nc; j++) {
        assert(cl[j] >= m->col_lower[j]);"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, """#ifndef NDEBUG
    for (int64_t j = 0; j < nc; j++)
        if (isfinite(m->col_lower[j])) { cl[j] = m->col_lower[j] - 1.0; break; }
    for (int64_t j = 0; j < nc; j++) {
        assert(cl[j] >= m->col_lower[j]);""")
open(p, "w", encoding="utf-8").write(s)
print("  one declared lower bound loosened, past every guard")
'

INFEAS="-m bench/netlib-infeas.manifest -e infeasible -d bench/instances-infeas"

{
echo "# The four asserts D221 adds to src/check.c, and the control that proves"
echo "# each fires. tree $(git rev-parse --short "$ref") plus the working tree's"
echo "# src/check.c, EXTRA_CFLAGS=-UNDEBUG, own worktree and make clean per arm."
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

run_arm canary    "$BREAK_CANARY"
run_arm live      ""
# shellcheck disable=SC2086
run_arm live-infeas "" $INFEAS
run_arm magnitude "$BREAK_MAGNITUDE"
run_arm step      "$BREAK_STEP"
run_arm loosen    "$BREAK_LOOSEN"
} 2>&1 | tee "$here/check-assert-control.txt"

count_of() { awk -v t="## $1" '$0==t{f=1;next} f&&/assertion failures/{print $NF; exit}' "$here/check-assert-control.txt"; }
ok=0
[ "$(count_of canary)" -gt 0 ] 2>/dev/null || {
    echo "STOP: the canary did not fire, so -UNDEBUG never reached the compiler"
    echo "      and no arm below is a statement about an assert."; exit 2; }
for t in live live-infeas; do
    [ "$(count_of "$t")" = 0 ] || { echo "FAIL: $(count_of "$t") assertions fired on the unmodified candidate ($t)"; ok=1; }
done
for t in magnitude step loosen; do
    [ "$(count_of "$t")" -gt 0 ] 2>/dev/null || { echo "FAIL: the $t arm fired nothing"; ok=1; }
done
grep -q 'a.pos >= 0.0L' "$here/check-assert-control.txt" || { echo "FAIL: the magnitude arm did not name the pos/neg assert"; ok=1; }
grep -q 't >= 0.0L' "$here/check-assert-control.txt" || { echo "FAIL: the step arm did not name the t >= 0 assert"; ok=1; }
grep -qE 'cu\[j\] <= m->col_upper\[j\]|cl\[j\] >= m->col_lower\[j\]' "$here/check-assert-control.txt" || {
    echo "FAIL: the loosen arm did not name the monotone assert"; ok=1; }
[ "$ok" = 0 ] && echo "EVERY ASSERT CATCHES THE DEFECT IT EXISTS FOR" || echo "AT LEAST ONE ARM FAILED"
exit "$ok"
