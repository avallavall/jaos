#!/usr/bin/env bash
# The solution reader's tests, watched going red without the reader.
#
# There is no population run and there cannot be one: no gate instance is a
# solution file, and the reader touches nothing the solver does. All three
# sets are byte-identical, which says the change is a no-op and nothing
# more. So the evidence is the tests, and the tests need their own.
#
# Two arms.
#
#   1. The READER's parsed value moved by one ulp. The test compares bit
#      for bit rather than within a tolerance, so it must go red -- and if
#      it does not, the round trip it claims to check is not being checked.
#
#      The first version of this arm cut the WRITER from %.17g to %.15g and
#      the suite ABORTED instead: `wr_num` asserts that what it printed
#      reads back as the value it was given (D226), and 15 digits does not.
#      That is the writer's own guard doing its job, and it means a break
#      there can never reach the reader -- so the arm has to be on the
#      reader's side of the file.
#
#   2. The refusal suite with the name check removed, so a record's name is
#      no longer required to be the one this index gets. The refusal test
#      must go red. Without this arm, "records are in index order" could be
#      passing because the file is rejected for some other reason.
#
# Writes validate-d282.txt beside this script. Exit 0 when both arms
# behaved, 1 when one did not, 2 when the harness itself failed.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-187/validate-d282.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 2
out="$here/validate-d282.txt"

T_RT=test_a_solution_file_reads_back_exactly
T_REJ=test_the_solution_reader_refuses_by_name

save=$(mktemp -d) || exit 2
cp src/write.c "$save/write.c" || exit 2
trap 'cp "$save/write.c" src/write.c; rm -rf "$save"' EXIT

BREAK_PARSE='
import io
p = "src/write.c"
s = io.open(p, encoding="utf-8", newline="").read()
old = "    *out = v;"
new = "    *out = nextafter(v, INFINITY);"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, new))
print("  every number the reader parses comes back one ulp higher")
'

BREAK_NAMECHECK='
import io
p = "src/write.c"
s = io.open(p, encoding="utf-8", newline="").read()
old = "            if (strcmp(tok[1], nm) != 0)"
new = "            if (false)"
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
io.open(p, "w", encoding="utf-8", newline="").write(s.replace(old, new))
print("  a record name is no longer checked against its index")
'

run_suite() {   # $1 = label
    make -o record-check build/dev/test_write >/dev/null 2>&1 || {
        echo "$1: BUILD FAILED"; echo "exit=2"; return; }
    ./build/dev/test_write > "$save/run.$1" 2>&1
    echo "exit=$?" >> "$save/run.$1"
    grep -E "$T_RT|$T_REJ|^exit=" "$save/run.$1"
}

{
    echo "# D282 -- do the solution-reader tests catch what they check?"
    echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "# tree: $(git rev-parse --short HEAD)"
    echo

    echo "== candidate"
    cand=$(run_suite cand)
    echo "$cand"
    echo

    echo "== arm 1: the reader moves every parsed number by one ulp"
    python3 -c "$BREAK_PARSE" || exit 2
    arm1=$(run_suite arm1)
    echo "$arm1"
    echo

    cp "$save/write.c" src/write.c
    echo "== arm 2: a record name is not checked"
    python3 -c "$BREAK_NAMECHECK" || exit 2
    arm2=$(run_suite arm2)
    echo "$arm2"
    echo

    cp "$save/write.c" src/write.c
    echo "== restored"
    again=$(run_suite again)
    echo "$again"
    echo

    fail=0
    for t in "$T_RT" "$T_REJ"; do
        echo "$cand"  | grep -q "$t:PASS" || { echo "BROKEN: $t not green on the candidate"; fail=1; }
        echo "$again" | grep -q "$t:PASS" || { echo "BROKEN: $t not green after restoring"; fail=1; }
    done
    echo "$arm1" | grep -q "$T_RT:FAIL" || {
        echo "NOT A ROUND TRIP: $T_RT stays green with every parsed number moved,"
        echo "                  so it is not comparing what came back with what went out"
        fail=1; }
    echo "$arm2" | grep -q "$T_REJ:FAIL" || {
        echo "NOT TESTED: $T_REJ stays green with the name check removed, so"
        echo "            the wrong-name file is refused by something else"
        fail=1; }

    [ $fail -eq 0 ] && echo "the round trip is a round trip, and the name check is what refuses"
    echo "verdict-exit=$fail"
} > "$out" 2>&1

tail -6 "$out"
grep -q 'verdict-exit=0' "$out"
