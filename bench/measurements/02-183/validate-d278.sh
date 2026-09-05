#!/usr/bin/env bash
# The test D278 adds, watched going red without the change.
#
# A test that passes beside a feature is not evidence the feature is what
# makes it pass. This puts HEAD's `src/lpfmt.c` back, rebuilds `test_lp`
# against it, and requires the new test to FAIL -- HEAD's reader refuses
# `tests/data/g_const.lp` at its first constraint, so the read returns
# JAOS_ERR_INVALID_INPUT and the test's first assertion goes. Then it
# restores and requires the test to pass again.
#
# The second arm is the one that says the change is narrow: the six
# rejection cases that remain must still be refused, in both trees. A reader
# that started accepting a constant by loosening the term rule would take
# `el_badchar.lp` with it.
#
# Writes validate-d278.txt beside this script. Exit 0 when both arms
# behaved, 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-183/validate-d278.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/validate-d278.txt"

T=test_a_constant_in_a_constraint_folds_into_the_rhs
R1=test_rejection_reasons_are_specific
R2=test_rejections_carry_line_numbers

save=$(mktemp -d) || exit 2
cp src/lpfmt.c "$save/lpfmt.c" || exit 2
trap 'cp "$save/lpfmt.c" src/lpfmt.c; rm -rf "$save"' EXIT

run_suite() {   # $1 = label
    make -o record-check build/dev/test_lp >/dev/null 2>&1 || {
        echo "$1: BUILD FAILED"; echo "exit=2"; return; }
    ./build/dev/test_lp > "$save/run.$1" 2>&1
    echo "exit=$?" >> "$save/run.$1"
    grep -E "$T|$R1|$R2|^exit=" "$save/run.$1"
}

{
    echo "# D278 -- does the new test catch what it was written for?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo

    echo "== candidate: the reader folds a constant"
    cand=$(run_suite cand)
    echo "$cand"
    echo

    echo "== HEAD: the reader refuses one"
    git show HEAD:src/lpfmt.c > src/lpfmt.c || exit 2
    head_run=$(run_suite head)
    echo "$head_run"
    echo

    cp "$save/lpfmt.c" src/lpfmt.c
    echo "== restored"
    again=$(run_suite again)
    echo "$again"
    echo

    fail=0
    echo "$cand"  | grep -q "$T:PASS" || { echo "BROKEN: $T is not green on the candidate"; fail=1; }
    echo "$again" | grep -q "$T:PASS" || { echo "BROKEN: $T is not green after restoring"; fail=1; }
    echo "$head_run" | grep -q "$T:FAIL" || {
        echo "NOT EVIDENCE: $T stayed green against HEAD's reader"; fail=1; }

    # The narrowness arm. Both rejection suites must be green on the
    # candidate: the five files that must still be refused still are.
    for r in "$R1" "$R2"; do
        echo "$cand" | grep -q "$r:PASS" || {
            echo "TOO WIDE: $r is red on the candidate -- the fold took a"
            echo "          refusal with it"; fail=1; }
    done

    if [ $fail -eq 0 ]; then
        echo "the test is red without the change, and no refusal moved with it"
    fi
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -6 "$out"
grep -q 'verdict-exit=0' "$out"
