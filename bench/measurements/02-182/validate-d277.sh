#!/usr/bin/env bash
# The three tests D277 adds, each watched going red for the right reason.
#
# A test that passes beside a repair is not evidence that it would catch the
# defect. Two arms here.
#
# Arm 1, the two accuracy tests: put HEAD's `long double` checker back,
# rebuild `test_check` against it, and require BOTH to fail. Then restore and
# require both to pass again.
#
# Arm 2, the overflow test: take out the one guard it exists for -- the
# `isfinite(t)` early return in `bound_term` -- and require the suite to go
# red. It does not merely fail there: with asserts on it ABORTS, because the
# accumulator becomes a NaN and the magnitude assert on the gap halves
# catches it. An abort prints no Unity failure line, so this arm reads the
# binary's EXIT CODE and not only the test output. Same rule as 02-139's
# clamp arm, and for the same reason.
#
# Writes validate-d277.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-182/validate-d277.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/validate-d277.txt"

T1=test_the_dual_objective_keeps_a_term_the_wide_type_would_lose
T2=test_the_ray_rate_keeps_a_term_the_wide_type_would_lose
T3=test_an_overflowing_gap_term_does_not_poison_the_accumulator

save=$(mktemp -d) || exit 2
cp src/check.c "$save/check.c" || exit 2
trap 'cp "$save/check.c" src/check.c; rm -rf "$save"' EXIT

BREAK_GUARD='
import io, sys
p = "src/check.c"
s = io.open(p, encoding="utf-8", newline="").read()
old = """    if (!isfinite(t)) {
        *e = 0.0;
        return t;
    }
"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, ""))
print("  bound_term no longer stops at a non-finite product")
'

run_suite() {   # $1 = label; prints the three lines, then "exit=N"
    make -o record-check build/dev/test_check >/dev/null 2>&1 || {
        echo "$1: BUILD FAILED"; echo "exit=2"; return; }
    ./build/dev/test_check > "$save/run.$1" 2>&1
    echo "exit=$?" >> "$save/run.$1"
    grep -E "$T1|$T2|$T3|^exit=" "$save/run.$1"
}

{
    echo "# D277 -- do the three new tests catch what they were written for?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo

    echo "== candidate, long double uses: $(grep -c 'long double' src/check.c)"
    cand=$(run_suite cand)
    echo "$cand"
    echo

    echo "== arm 1: HEAD's long double checker"
    git show HEAD:src/check.c > src/check.c || exit 2
    echo "   long double uses: $(grep -c 'long double' src/check.c)"
    arm1=$(run_suite arm1)
    echo "$arm1"
    echo

    cp "$save/check.c" src/check.c
    echo "== arm 2: the candidate with bound_term's non-finite guard removed"
    python3 -c "$BREAK_GUARD" || exit 2
    arm2=$(run_suite arm2)
    echo "$arm2"
    # Exit 134 is SIGABRT, and a double free gives 134 too. The assertion
    # text is what says WHICH abort this is, so it is read and required.
    arm2_assert=$(grep -o 'Assertion .* failed' "$save/run.arm2" | head -1)
    echo "  abort says: ${arm2_assert:-<no assertion text>}"
    echo

    cp "$save/check.c" src/check.c
    echo "== restored, long double uses: $(grep -c 'long double' src/check.c)"
    again=$(run_suite again)
    echo "$again"
    echo

    fail=0
    for t in "$T1" "$T2" "$T3"; do
        echo "$cand" | grep -q "$t:PASS" || {
            echo "BROKEN: $t is not green on the candidate"; fail=1; }
        echo "$again" | grep -q "$t:PASS" || {
            echo "BROKEN: $t is not green after restoring"; fail=1; }
    done
    for t in "$T1" "$T2"; do
        echo "$arm1" | grep -q "$t:FAIL" || {
            echo "NOT EVIDENCE: $t stayed green against HEAD's checker"; fail=1; }
    done
    # Arm 2 is red either by a FAIL line or by an abort. An abort is the
    # expected shape with asserts on, and it prints no FAIL line at all --
    # so a nonzero exit is what this arm requires, and a clean exit 0 means
    # the guard is not what makes the test pass.
    if echo "$arm2" | grep -q "^exit=0$"; then
        echo "NOT EVIDENCE: the suite exits 0 with bound_term's guard removed"
        fail=1
    fi
    case "$arm2_assert" in
    *"a.pos >= 0.0"*) ;;   # the magnitude assert on the gap halves
    *) echo "WRONG ABORT: expected the gap-half magnitude assert, got"
       echo "             '${arm2_assert:-<none>}'"
       fail=1 ;;
    esac
    if [ $fail -eq 0 ]; then
        echo "every arm behaved: each test is red without the thing it tests"
    fi
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -8 "$out"
grep -q 'verdict-exit=0' "$out"
