#!/usr/bin/env bash
# The OBJNAME tests, watched going red without the feature.
#
# Three arms, and the second is the one that makes this more than a
# formality.
#
#   1. HEAD's src/mps.c refuses `OBJNAME` as an unsupported section, so both
#      golden tests fail at their first assertion. Necessary, and weak: any
#      change that broke the reader would do the same.
#
#   2. The candidate with the OBJNAME rule REMOVED but the section still
#      accepted -- that is, the reader parses OBJNAME and then ignores it,
#      taking the first N row as it always did. Both golden tests must still
#      fail, and they must fail on the COST, not on the row count. This is
#      the arm that says the tests are about which row was chosen and not
#      merely about the file parsing.
#
#   3. Restored: green again.
#
# Writes validate-d280.txt beside this script. Exit 0 when every arm
# behaved, 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-185/validate-d280.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/validate-d280.txt"

T1=test_t3_objname_picks_the_second_free_row
T2=test_objname_on_the_next_line_is_the_same_model

save=$(mktemp -d) || exit 2
cp src/mps.c "$save/mps.c" || exit 2
trap 'cp "$save/mps.c" src/mps.c; rm -rf "$save"' EXIT

# Arm 2's break: the section is still read, the name is still stored, and
# the one clause in the ROWS rule that consults it becomes `true`, so the
# first free row wins again. Anchored on that clause alone, which carries
# no quote of its own -- an anchor holding an escaped one does not match
# the source and `make record-check` says so.
BREAK_RULE='
import io, sys
p = "src/mps.c"
s = io.open(p, encoding="utf-8", newline="").read()
old = """        (r->objname == nullptr || strcmp(tok[1], r->objname) == 0);"""
new = """        true;   /* the rule removed: the first free row wins again */"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, new))
print("  the ROWS rule no longer consults OBJNAME")
'

run_suite() {   # $1 = label
    make -o record-check build/dev/test_mps >/dev/null 2>&1 || {
        echo "$1: BUILD FAILED"; echo "exit=2"; return; }
    ./build/dev/test_mps > "$save/run.$1" 2>&1
    echo "exit=$?" >> "$save/run.$1"
    grep -E "$T1|$T2|^exit=" "$save/run.$1"
}

{
    echo "# D280 -- do the OBJNAME tests catch what they were written for?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo

    echo "== candidate"
    cand=$(run_suite cand)
    echo "$cand"
    echo

    echo "== arm 1: HEAD, which refuses OBJNAME as an unsupported section"
    git show HEAD:src/mps.c > src/mps.c || exit 2
    arm1=$(run_suite arm1)
    echo "$arm1"
    echo

    cp "$save/mps.c" src/mps.c
    echo "== arm 2: OBJNAME parsed and then ignored"
    python3 -c "$BREAK_RULE" || exit 2
    arm2=$(run_suite arm2)
    echo "$arm2"
    echo "  what arm 2 failed on:"
    grep -E "$T1" "$save/run.arm2" | head -1
    echo

    cp "$save/mps.c" src/mps.c
    echo "== arm 3: restored"
    again=$(run_suite again)
    echo "$again"
    echo

    fail=0
    for t in "$T1" "$T2"; do
        echo "$cand"  | grep -q "$t:PASS" || { echo "BROKEN: $t not green on the candidate"; fail=1; }
        echo "$again" | grep -q "$t:PASS" || { echo "BROKEN: $t not green after restoring"; fail=1; }
        echo "$arm1"  | grep -q "$t:FAIL" || { echo "NOT EVIDENCE: $t stayed green against HEAD"; fail=1; }
        echo "$arm2"  | grep -q "$t:FAIL" || {
            echo "NOT ABOUT THE CHOICE: $t passes when OBJNAME is ignored, so"
            echo "                      it is testing the parse and not the rule"
            fail=1; }
    done

    [ $fail -eq 0 ] && echo "both tests are red without the rule, not merely without the section"
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -6 "$out"
grep -q 'verdict-exit=0' "$out"
