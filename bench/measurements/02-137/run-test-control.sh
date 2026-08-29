#!/bin/bash
# The tests D224 adds, and the proof that each FAILS when the thing it tests
# is broken.
#
# `jaos-testing`: a green suite is not evidence until you have watched it go
# red for the right reason. Same shape as the assert controls in 02-134 to
# 02-136, and the same rule -- a passing arm means nothing without a failing
# one beside it.
#
# Four arms, one per test. Each breaks exactly the contract its test states
# and requires THAT test to be the one that fails.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-137"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune; rm -rf "$D"
}
trap cleanup EXIT

arm() {   # $1 = tag, $2 = breaker or "", $3 = which test binary
    local tag=$1 breaker=$2 bin=$3 wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    for f in simplex.c presolve.c scale.c model.c check.c alloc.c jaos_internal.h; do
        [ -f "$JAOS_ROOT/src/$f" ] && cp "$JAOS_ROOT/src/$f" "$wt/src/$f"
    done
    cp "$JAOS_ROOT/tests/test_simplex.c" "$wt/tests/test_simplex.c"
    cp "$JAOS_ROOT/tests/test_model.c" "$wt/tests/test_model.c"
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1 && \
      make "build/dev/$bin" >/dev/null 2>&1 ) || { echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && timeout 300 "./build/dev/$bin" ) > "$D/$tag.log" 2>&1
    return 0
}

# `jm_bland_pick` compares the minimum with a window instead of exactly.
BREAK_ULP='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
i = s.index("int64_t jm_bland_pick(")
j = s.index("\n}", i)
body = s[i:j]
old = "        if (t < least)"
assert body.count(old) == 1, "anchor matched %d times in jm_bland_pick" % body.count(old)
s = s[:i] + body.replace(old, "        if (t < least * (1.0 - 1e-12))") + s[j:]
open(p, "w", encoding="utf-8").write(s)
print("  bland finds its minimum through a window instead of exactly")
'

# A non-positive nvar stops returning zero.
BREAK_NVAR='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = "    int64_t nwords = (nvar + 63) / 64;\n    for (int64_t w = 0; w < nwords; w++)\n        mark[w] = 0;"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "    int64_t nwords = 1;\n    for (int64_t w = 0; w < nwords; w++)\n        mark[w] = 0;")
open(p, "w", encoding="utf-8").write(s)
print("  nonbasic_build always touches one word")
'

# A zero-length allocation answers null.
BREAK_ALLOC='
import glob
for p in ["src/alloc.c", "src/model.c", "src/simplex.c"]:
    try: s = open(p, encoding="utf-8").read()
    except OSError: continue
    if "void *jm_alloc_array(" not in s: continue
    i = s.index("void *jm_alloc_array(")
    j = s.index("\n{", i) + 2
    s = s[:j] + "\n    if (n == 0) return nullptr;" + s[j:]
    open(p, "w", encoding="utf-8").write(s)
    print("  jm_alloc_array(0) returns null, patched in " + p)
    break
else:
    raise SystemExit("jm_alloc_array not found")
'

# The overflow guard goes, so the split overflows and the residue is not 0.
BREAK_RESIDUE='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
# `BIG` stays referenced or -Werror rejects the unused constant, which is a
# build failure rather than a test failure and proves nothing. Multiplying it
# by HUGE_VAL gives infinity, and `fabs(a) > inf` is never true, so the guard
# is disabled while the constant is still used.
old = "fabs(a) > BIG || fabs(b) > BIG"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "fabs(a) > BIG * HUGE_VAL || fabs(b) > BIG * HUGE_VAL")
open(p, "w", encoding="utf-8").write(s)
print("  the 2^996 overflow guard can no longer fire")
'

{
echo "# The tests D224 adds, and the proof each fails when its contract breaks."
echo "# tree $(git rev-parse --short "$ref") plus the working tree's src/ and tests/."
echo

run_arm() {   # $1 = tag, $2 = breaker, $3 = binary, $4 = the test that must fail
    local tag=$1 want=$4
    printf '## %s\n' "$tag"
    if ! arm "$1" "$2" "$3"; then echo "  COULD NOT RUN"; return; fi
    local fails
    fails=$(grep -cE ':FAIL' "$D/$tag.log" 2>/dev/null)
    echo "  failures: $fails"
    grep -E ':FAIL' "$D/$tag.log" | sed -E 's/.*:(test_[a-z0-9_]+):FAIL.*/    \1/' | sort -u
    if [ -n "$want" ]; then
        if grep -q "$want:FAIL" "$D/$tag.log"; then
            echo "  -> the named test failed, as required"
        else
            echo "  -> WRONG: $want did not fail"
        fi
    fi
    echo "  suite line: $(grep -E '^[0-9]+ Tests' "$D/$tag.log" | head -1)"
}

# The residue test's contract is "returns 0.0 past 2^996", and TWO guards
# deliver it: the explicit `fabs(a) > BIG` test at the top, and the
# `isfinite(e) ? e : 0.0` at the bottom. Past the bar `SPLIT * a` overflows
# to infinity, so `ah = ca - (ca - a)` is `inf - inf` = NaN and every later
# term is NaN — which the bottom guard catches. So removing the TOP guard
# changes nothing observable, and the `resi` arm below is expected NOT to
# fail. `resi2` removes the bottom one, which is the guard the contract
# actually rests on, and that is the arm that proves the test is live.
BREAK_RESIDUE2='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = "    return isfinite(e) ? e : 0.0;"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
s = s.replace(old, "    return e;")
open(p, "w", encoding="utf-8").write(s)
print("  the NaN fallback is gone")
'

run_arm live ""              test_simplex ""
run_arm ulp  "$BREAK_ULP"    test_simplex test_bland_compares_the_minimum_exactly_at_one_ulp
run_arm nvar "$BREAK_NVAR"   test_simplex test_nonbasic_build_on_no_variables_counts_zero
run_arm allo "$BREAK_ALLOC"  test_simplex test_alloc_array_of_zero_is_not_a_failure
run_arm resi  "$BREAK_RESIDUE"  test_model ""
run_arm resi2 "$BREAK_RESIDUE2" test_model ""
run_arm resi3 "$BREAK_RESIDUE
$BREAK_RESIDUE2" test_model test_two_product_residue_gives_up_rather_than_overflow
} 2>&1 | tee "$here/test-control.txt"

ok=0
grep -q '^## live' "$here/test-control.txt" || ok=1
awk '/^## live$/{f=1;next} f&&/failures:/{exit ($NF==0)?0:1}' "$here/test-control.txt" || {
    echo "FAIL: the unmodified suite is not green"; ok=1; }
for t in ulp nvar allo resi3; do
    grep -A 6 "^## $t\$" "$here/test-control.txt" | grep -q 'the named test failed, as required' || {
        echo "FAIL: the $t arm did not make its own test fail"; ok=1; }
done
# The resi arm is expected to be quiet: it removes a guard that is redundant
# with the one below it, which is a finding about the code and not a failure.
grep -A 4 "^## resi$" "$here/test-control.txt" | grep -q "failures: 0" || {
    echo "NOTE: removing the 2^996 guard now DOES change the result -- the two"
    echo "      guards are no longer redundant, and the comment above is stale."; }
[ "$ok" = 0 ] && echo "EVERY TEST FAILS WHEN ITS CONTRACT BREAKS" || echo "AT LEAST ONE ARM FAILED"
exit "$ok"
