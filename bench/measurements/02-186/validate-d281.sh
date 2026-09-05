#!/usr/bin/env bash
# The reversed-bound tests, watched going red without the change.
#
# Three arms, and the third is the one that says the refusal is tested and
# not merely inherited.
#
#   1. HEAD's src/lpfmt.c, which refuses `10 >= x` with "expected <= after
#      the bound value". The new positive test must fail.
#
#   2. The candidate with the SECOND operator's direction check removed, so
#      `3 <= w >= 8` is accepted as an interval it is not. Both rejection
#      suites must go red. Without this arm, `el_bounddir.lp` could be
#      passing because the parser refuses it somewhere else entirely.
#
#   3. Restored: green again.
#
# Writes validate-d281.txt beside this script. Exit 0 when every arm
# behaved, 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-186/validate-d281.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/validate-d281.txt"

T=test_a_bound_can_be_written_value_first_either_way
R1=test_rejection_reasons_are_specific
R2=test_rejections_carry_line_numbers

save=$(mktemp -d) || exit 2
cp src/lpfmt.c "$save/lpfmt.c" || exit 2
trap 'cp "$save/lpfmt.c" src/lpfmt.c; rm -rf "$save"' EXIT

# Arm 2's break: the mirrored form still parses, and the two operators are
# no longer required to agree. Anchored on the comparison alone, which
# carries no quote of its own.
BREAK_DIR='
import io, sys
p = "src/lpfmt.c"
s = io.open(p, encoding="utf-8", newline="").read()
old = """                    if (p->tok.t != rel1)"""
new = """                    if (false)"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, new))
print("  the two operators no longer have to point the same way")
'

run_suite() {   # $1 = label
    make -o record-check build/dev/test_lp >/dev/null 2>&1 || {
        echo "$1: BUILD FAILED"; echo "exit=2"; return; }
    ./build/dev/test_lp > "$save/run.$1" 2>&1
    echo "exit=$?" >> "$save/run.$1"
    grep -E "$T|$R1|$R2|^exit=" "$save/run.$1"
}

{
    echo "# D281 -- do the reversed-bound tests catch what they were written for?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo

    echo "== candidate"
    cand=$(run_suite cand)
    echo "$cand"
    echo

    echo "== arm 1: HEAD, which refuses a value-first bound written with >="
    git show HEAD:src/lpfmt.c > src/lpfmt.c || exit 2
    arm1=$(run_suite arm1)
    echo "$arm1"
    echo

    cp "$save/lpfmt.c" src/lpfmt.c
    echo "== arm 2: the two operators no longer have to agree"
    python3 -c "$BREAK_DIR" || exit 2
    arm2=$(run_suite arm2)
    echo "$arm2"
    echo

    cp "$save/lpfmt.c" src/lpfmt.c
    echo "== arm 3: restored"
    again=$(run_suite again)
    echo "$again"
    echo

    fail=0
    echo "$cand"  | grep -q "$T:PASS" || { echo "BROKEN: $T not green on the candidate"; fail=1; }
    echo "$again" | grep -q "$T:PASS" || { echo "BROKEN: $T not green after restoring"; fail=1; }
    echo "$arm1"  | grep -q "$T:FAIL" || { echo "NOT EVIDENCE: $T stayed green against HEAD"; fail=1; }

    # The refusals must be green on the candidate and red once the direction
    # check is gone. Green in both would mean el_bounddir.lp is refused by
    # something other than the rule it was written for.
    for r in "$R1" "$R2"; do
        echo "$cand" | grep -q "$r:PASS" || {
            echo "TOO WIDE: $r is red on the candidate"; fail=1; }
        echo "$arm2" | grep -q "$r:FAIL" || {
            echo "NOT TESTED: $r stays green with the direction check removed,"
            echo "            so el_bounddir.lp is refused by something else"
            fail=1; }
    done

    [ $fail -eq 0 ] && echo "the mirror is tested, and so is the rule that refuses a mixed pair"
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -6 "$out"
grep -q 'verdict-exit=0' "$out"
