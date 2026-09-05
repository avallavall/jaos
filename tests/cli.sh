#!/usr/bin/env bash
# The command-line tool's own test. `make test` runs it after the C suite,
# with the path of the binary as its one argument; it exits non-zero when any
# check fails, so `make test` fails with it.
#
# Every input is under tests/data/ and nothing here reaches the network. What
# is checked is the tool's contract (docs/cli.md), not the solver's: the exit
# code per outcome, one fact per line, the reproducibility of stdout without
# its time line, the reader picked by name, the writer picked by name, and a
# refused write leaving no file behind. The solver's own answers are the C
# suite's business.
#
# SPDX-License-Identifier: Apache-2.0
set -u

JAOS=${1:-build/cli/jaos}
DATA=tests/data
fail=0
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }

# expect_exit CODE NAME CMD... : the command must exit with CODE. Its output
# is kept in $out and $err for the checks that follow.
out=""
err=""
expect_exit() {
    local want=$1 name=$2; shift 2
    out=$("$@" 2>"$tmp/err"); local got=$?
    err=$(cat "$tmp/err")
    if [ "$got" -eq "$want" ]; then pass "$name"
    else flunk "$name (exit $got, wanted $want)"; echo "$err" | sed 's/^/     /'; fi
}

# line_of PREFIX : the one line of $out starting with PREFIX, or nothing.
line_of() { printf '%s\n' "$out" | grep "^$1 "; }

# The positive analysis checks -- check, verify and ranging on a correct
# answer -- are skipped under either presolve fault build, the rule the C
# suite applies to every test that reads a postsolved answer. Those builds
# corrupt the answer on purpose: the published basis comes out one long, and
# a wrong count is what jaos_verify and the ranging calls refuse. `make test`
# passes its EXTRA_CFLAGS in JAOS_CLI_TEST_FLAGS so this script can tell.
faulty=0
case "${JAOS_CLI_TEST_FLAGS:-}" in *JAOS_PRESOLVE_FAULT*) faulty=1 ;; esac
[ "$faulty" -eq 1 ] && echo "note fault build: positive analysis checks skipped"

# The binary must exist; without it every check below fails for one reason.
if [ ! -x "$JAOS" ]; then
    echo "FAIL $JAOS is not an executable; build it with make cli"
    exit 1
fi

# ---------------------------------------------------------------- version
# The string is jaos_version()'s, which is JAOS_VERSION_STRING in jaos.h.
want=$(sed -n 's/^#define JAOS_VERSION_STRING "\([^"]*\)".*/\1/p' include/jaos.h)
expect_exit 0 "--version exits 0" "$JAOS" --version
[ -n "$want" ] && [ "$out" = "$want" ] \
    && pass "--version prints $want" \
    || flunk "--version printed '$out', jaos.h says '$want'"

expect_exit 0 "--help exits 0" "$JAOS" --help
printf '%s\n' "$out" | grep -q '^Usage:' \
    && pass "--help prints the usage" || flunk "--help printed no usage"

# ------------------------------------------------------------------ usage
expect_exit 5 "no arguments is a usage error" "$JAOS"
expect_exit 5 "an unknown command is a usage error" "$JAOS" frobnicate
expect_exit 5 "an unknown option is a usage error" \
    "$JAOS" solve "$DATA/solve1.mps" --bogus
expect_exit 5 "solve without a file is a usage error" "$JAOS" solve
expect_exit 5 "a missing file is an error" "$JAOS" solve "$tmp/no-such.mps"
[ -n "$err" ] && pass "a missing file says so on stderr" \
    || flunk "a missing file said nothing on stderr"
expect_exit 5 "an unreadable model is an error" \
    "$JAOS" solve "$DATA/e_badnum.mps"
expect_exit 5 "--work-limit refuses zero" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 0
expect_exit 5 "--work-limit refuses a word" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit ten
expect_exit 5 "--work-limit refuses a partial number" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 10x
expect_exit 5 "--time-limit refuses a negative" \
    "$JAOS" solve "$DATA/solve1.mps" --time-limit -1
expect_exit 5 "--log refuses an unknown level" \
    "$JAOS" solve "$DATA/solve1.mps" --log loud
expect_exit 5 "a missing option value is a usage error" \
    "$JAOS" solve "$DATA/solve1.mps" --solution
# The library refuses this one, and the tool passes the refusal on.
expect_exit 5 "--primal-tol refuses a negative" \
    "$JAOS" solve "$DATA/solve1.mps" --primal-tol -1e-7

# ------------------------------------------------------------------ solve
# solve1.mps is optimal at 29 (the file's header works it out).
expect_exit 0 "solve of an optimal model exits 0" "$JAOS" solve "$DATA/solve1.mps"
[ "$(line_of status)" = "status optimal" ] \
    && pass "it prints 'status optimal'" \
    || flunk "status line is '$(line_of status)'"
printf '%s\n' "$out" | awk '$1 == "objective" { d = $2 - 29; if (d < 0) d = -d;
    found = 1; exit !(d < 1e-6) } END { if (!found) exit 1 }' \
    && pass "the objective line reads 29" \
    || flunk "objective line is '$(line_of objective)'"
for k in iterations work_units time; do
    [ -n "$(line_of $k)" ] && pass "it prints a $k line" \
        || flunk "no $k line in: $out"
done
[ "$(printf '%s\n' "$out" | wc -l)" -eq 5 ] \
    && pass "five lines, one fact each" \
    || flunk "expected five lines, got: $out"
[ "$(printf '%s\n' "$out" | tail -n 1 | cut -d' ' -f1)" = "time" ] \
    && pass "time is the last line" || flunk "time is not the last line"
mps_objective=$(line_of objective)

# Two runs of the same file agree byte for byte once the time line is gone.
"$JAOS" solve "$DATA/solve1.mps" | grep -v '^time ' > "$tmp/run1"
"$JAOS" solve "$DATA/solve1.mps" | grep -v '^time ' > "$tmp/run2"
cmp -s "$tmp/run1" "$tmp/run2" \
    && pass "two runs agree without the time line" \
    || { flunk "two runs differ"; diff "$tmp/run1" "$tmp/run2"; }

expect_exit 0 "--quiet exits 0" "$JAOS" solve --quiet "$DATA/solve1.mps"
[ "$out" = "status optimal" ] \
    && pass "--quiet prints the status line only" \
    || flunk "--quiet printed: $out"

# The log goes to stderr and changes nothing on stdout.
"$JAOS" solve "$DATA/solve1.mps" --log detail 2>"$tmp/log" \
    | grep -v '^time ' > "$tmp/run3"
[ -s "$tmp/log" ] && pass "--log detail writes to stderr" \
    || flunk "--log detail wrote nothing to stderr"
cmp -s "$tmp/run1" "$tmp/run3" \
    && pass "--log leaves stdout unchanged" \
    || { flunk "--log changed stdout"; diff "$tmp/run1" "$tmp/run3"; }

# A gzip file is read by the same reader as its plain form.
expect_exit 0 "a gzip MPS file solves" "$JAOS" solve "$DATA/solve1.mps.gz"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "the gzip file gives the same objective line" \
    || flunk "gzip objective '$(line_of objective)' vs '$mps_objective'"

# A work limit of one unit stops the solve before its first iteration: the
# simplex tests the budget at the top of every iteration, and the first
# factorization has already cost more than one unit. Deterministic, so the
# exit code and the status word are pinned; the counts are not.
expect_exit 3 "a solve stopped by --work-limit exits 3" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 1
[ "$(line_of status)" = "status work_limit" ] \
    && pass "it prints 'status work_limit'" \
    || flunk "status line is '$(line_of status)'"
[ -z "$(line_of objective)" ] \
    && pass "no objective line for a stopped solve" \
    || flunk "a stopped solve printed '$(line_of objective)'"

# ------------------------------------------------------------- infeasible
# t1.mps has no feasible point (tests/test_simplex.c says why).
expect_exit 1 "an infeasible model exits 1" "$JAOS" solve "$DATA/t1.mps"
[ "$(line_of status)" = "status infeasible" ] \
    && pass "it prints 'status infeasible'" \
    || flunk "status line is '$(line_of status)'"
[ -z "$(line_of objective)" ] \
    && pass "no objective line without an optimum" \
    || flunk "an infeasible solve printed '$(line_of objective)'"

# --------------------------------------------------------------- solution
expect_exit 0 "--solution on an optimum exits 0" \
    "$JAOS" solve "$DATA/solve1.mps" --solution "$tmp/a.sol"
[ "$(head -n 1 "$tmp/a.sol" 2>/dev/null)" = "# JAOS solution file, format 1" ] \
    && pass "the solution file starts with its format line" \
    || flunk "solution file head: $(head -n 1 "$tmp/a.sol" 2>&1)"
grep -q '^status optimal$' "$tmp/a.sol" \
    && pass "the solution file carries the status" \
    || flunk "no status in the solution file"

expect_exit 1 "--solution on an infeasible model keeps exit 1" \
    "$JAOS" solve "$DATA/t1.mps" --solution "$tmp/b.sol"
grep -q '^status infeasible$' "$tmp/b.sol" 2>/dev/null \
    && pass "and writes the certificate (D285)" \
    || flunk "no certificate file for an infeasible model: $(head -n 3 "$tmp/b.sol" 2>&1)"
# A solve cut by a budget has no answer of any kind, so no file.
expect_exit 3 "--solution on a work-limited solve keeps exit 3" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 1 --solution "$tmp/w.sol"
[ ! -e "$tmp/w.sol" ] && pass "and writes no file" \
    || flunk "a solution file was written for a solve that did not finish"
[ -n "$err" ] && pass "and says so on stderr" \
    || flunk "nothing on stderr about the missing solution file"

expect_exit 5 "--solution to an unwritable path is an error" \
    "$JAOS" solve "$DATA/solve1.mps" --solution "$tmp/no/such/dir/c.sol"

# ---------------------------------------------------------------- convert
expect_exit 0 "convert MPS to LP exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1.lp"
expect_exit 0 "the written LP solves" "$JAOS" solve "$tmp/solve1.lp"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line as the MPS" \
    || flunk "LP objective '$(line_of objective)' vs MPS '$mps_objective'"

expect_exit 0 "convert MPS to MPS exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1b.mps"
expect_exit 0 "the written MPS solves" "$JAOS" solve "$tmp/solve1b.mps"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line" \
    || flunk "MPS objective '$(line_of objective)' vs '$mps_objective'"

# The reader is picked by name: .lp and .lp.gz go to the LP reader.
expect_exit 0 "an LP file solves" "$JAOS" solve "$DATA/g1.lp"
lp_objective=$(line_of objective)
expect_exit 0 "a gzip LP file solves" "$JAOS" solve "$DATA/g1.lp.gz"
[ -n "$lp_objective" ] && [ "$(line_of objective)" = "$lp_objective" ] \
    && pass ".lp.gz gives the same objective line as .lp" \
    || flunk ".lp.gz objective '$(line_of objective)' vs '$lp_objective'"
expect_exit 0 "convert LP to MPS exits 0" "$JAOS" convert "$DATA/g1.lp" "$tmp/g1.mps"
expect_exit 0 "the MPS written from LP solves" "$JAOS" solve "$tmp/g1.mps"
[ "$(line_of objective)" = "$lp_objective" ] \
    && pass "and gives the same objective line as the LP" \
    || flunk "MPS-from-LP objective '$(line_of objective)' vs '$lp_objective'"

expect_exit 5 "convert to an unknown extension is a usage error" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/out.txt"
[ ! -e "$tmp/out.txt" ] && pass "and writes nothing" || flunk "out.txt was written"
expect_exit 5 "convert with one argument is a usage error" \
    "$JAOS" convert "$DATA/solve1.mps"
expect_exit 5 "convert of an unreadable input is an error" \
    "$JAOS" convert "$DATA/e_badnum.mps" "$tmp/bad.lp"
[ ! -e "$tmp/bad.lp" ] && pass "and leaves no file" || flunk "bad.lp was written"

# The LP writer refuses a free row (docs/format-support.md, "What the LP
# dialect cannot express"), names it, and leaves no file behind. t3_objname.mps
# has one: its first N row is not the objective, so it loads as a free row.
# The MPS writer takes the same model.
expect_exit 5 "a refused LP write exits 5" \
    "$JAOS" convert "$DATA/t3_objname.mps" "$tmp/freerow.lp"
printf '%s\n' "$err" | grep -q "free" && pass "and prints the writer's refusal" \
    || flunk "a refused write said: '$err'"
[ ! -e "$tmp/freerow.lp" ] && pass "and leaves no file" \
    || flunk "a refused write left freerow.lp behind"
expect_exit 0 "the same model converts to MPS" \
    "$JAOS" convert "$DATA/t3_objname.mps" "$tmp/freerow.mps"
# A ranged row is not refused any more (D239): it writes as a two-sided row
# and reads back.
expect_exit 0 "a ranged row converts to LP" \
    "$JAOS" convert "$DATA/g_ranged.lp" "$tmp/ranged.lp"
expect_exit 0 "and the written LP solves" "$JAOS" solve "$tmp/ranged.lp"

# ------------------------------------------------------------------ check
# a.sol is solve1's own answer, written above.
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "check of a model's own solution exits 0" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol"
for k in "primal_feasible yes" "dual_feasible yes" "checked_duals yes"; do
    [ "$(line_of "${k% *}")" = "$k" ] && pass "it prints '$k'" \
        || flunk "check printed '$(line_of "${k% *}")', wanted '$k'"
done
[ "$(printf '%s\n' "$out" | head -n 1)" = "status optimal" ] \
    && pass "check says first what kind of answer the file holds" \
    || flunk "check began with '$(printf '%s\n' "$out" | head -n 1)'"
[ "$(printf '%s\n' "$out" | wc -l)" -eq 19 ] \
    && pass "check prints the status and the report's 18 fields" \
    || flunk "check printed $(printf '%s\n' "$out" | wc -l) lines"
"$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" > "$tmp/chk1"
"$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" > "$tmp/chk2"
cmp -s "$tmp/chk1" "$tmp/chk2" && pass "two check runs agree byte for byte" \
    || { flunk "two check runs differ"; diff "$tmp/chk1" "$tmp/chk2"; }

# The same answer with X1 pushed to 400, past its bound of 100, is not
# primal feasible, and it is the checker that says so, not the reader. The
# record carries the file's own column name (D284).
sed 's/^col X1 [^ ]* /col X1 400 /' "$tmp/a.sol" > "$tmp/bad.sol"
grep -q '^col X1 400 ' "$tmp/bad.sol" || flunk "the tampered file was not built"
expect_exit 1 "check of an infeasible answer exits 1" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/bad.sol"
[ "$(line_of primal_feasible)" = "primal_feasible no" ] \
    && pass "it prints 'primal_feasible no'" \
    || flunk "check printed '$(line_of primal_feasible)'"
fi

# g2.lp has two columns and two rows; solve1's file has three of each, and
# the reader refuses a file that does not fit the model.
expect_exit 5 "check with a solution for another model exits 5" \
    "$JAOS" check "$DATA/g2.lp" "$tmp/a.sol"
[ -n "$err" ] && pass "and says why on stderr" || flunk "no message on stderr"
expect_exit 5 "check with a missing solution file exits 5" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/no-such.sol"
expect_exit 5 "check without SOLUTION is a usage error" \
    "$JAOS" check "$DATA/solve1.mps"
expect_exit 5 "check --tol refuses a word" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" --tol wide
[ "$faulty" -eq 0 ] && expect_exit 0 "check --tol takes a number" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" --tol 1e-6

# An infeasible solve writes its certificate to the solution file, and
# check judges that certificate from the model alone (D285). t1.mps is
# infeasible; its file says so and carries one multiplier per row.
expect_exit 1 "solve --solution on an infeasible model exits 1" \
    "$JAOS" solve "$DATA/t1.mps" --solution "$tmp/t1.sol"
grep -q '^status infeasible$' "$tmp/t1.sol" \
    && pass "and writes a file that says infeasible" \
    || flunk "t1.sol: $(head -n 3 "$tmp/t1.sol" 2>&1)"
[ "$(grep -c '^ray \(LIM1\|LIM2\|EQ1\) ' "$tmp/t1.sol")" -eq 3 ] \
    && pass "with one ray record per row, under the file's names" \
    || flunk "ray records: $(grep '^ray' "$tmp/t1.sol")"
expect_exit 0 "check of a certificate exits 0" \
    "$JAOS" check "$DATA/t1.mps" "$tmp/t1.sol"
[ "$(printf '%s\n' "$out" | head -n 1)" = "status infeasible" ] \
    && pass "and says the file holds a certificate" \
    || flunk "check began with '$(printf '%s\n' "$out" | head -n 1)'"
[ "$(line_of certified)" = "certified yes" ] && pass "and certifies it" \
    || flunk "check printed '$(line_of certified)'"
for k in sup_columns inf_rows gap; do
    [ -n "$(line_of $k)" ] && pass "it prints a $k line" \
        || flunk "no $k line in: $out"
done
# The same certificate with every multiplier zeroed proves nothing, and it
# is the checker that says so: the reader takes it.
sed 's/^ray \([^ ]*\) .*/ray \1 0/' "$tmp/t1.sol" > "$tmp/t1zero.sol"
expect_exit 1 "check of a zeroed certificate exits 1" \
    "$JAOS" check "$DATA/t1.mps" "$tmp/t1zero.sol"
[ "$(line_of certified)" = "certified no" ] && pass "and says it certifies nothing" \
    || flunk "check printed '$(line_of certified)'"
expect_exit 5 "check of a certificate against another model exits 5" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/t1.sol"

# -------------------------------------------------------------------- iis
expect_exit 0 "iis of an infeasible model exits 0" "$JAOS" iis "$DATA/t1.mps"
[ "$(printf '%s\n' "$out" | head -n 1)" = "status infeasible" ] \
    && pass "its first line is the status" \
    || flunk "iis began with '$(printf '%s\n' "$out" | head -n 1)'"
members=$(line_of members | cut -d' ' -f2)
sides=$(printf '%s\n' "$out" | grep -c '^\(row\|col\) [^ ]* \(lower\|upper\)$')
[ -n "$members" ] && [ "$members" -ge 1 ] && [ "$sides" -eq "$members" ] \
    && pass "one side line per member ($members)" \
    || flunk "members=$members but $sides side lines"
# Sides are named as the file names them (D284): t1's rows are LIM1, LIM2
# and EQ1 and its columns X1..X3, and nothing else may appear.
[ "$(printf '%s\n' "$out" | grep -c '^\(row \(LIM1\|LIM2\|EQ1\)\|col X[123]\) \(lower\|upper\)$')" -eq "$sides" ] \
    && pass "every side carries the file's own name" \
    || flunk "a side is not named by the file: $(printf '%s\n' "$out" | grep '^\(row\|col\) ')"
for k in candidates solves work_units from_certificate; do
    [ -n "$(line_of $k)" ] && pass "it prints a $k line" \
        || flunk "no $k line in: $out"
done
"$JAOS" iis "$DATA/t1.mps" > "$tmp/iis1"
"$JAOS" iis "$DATA/t1.mps" > "$tmp/iis2"
cmp -s "$tmp/iis1" "$tmp/iis2" && pass "two iis runs agree byte for byte" \
    || { flunk "two iis runs differ"; diff "$tmp/iis1" "$tmp/iis2"; }

expect_exit 1 "iis of an optimal model exits 1" "$JAOS" iis "$DATA/solve1.mps"
[ "$out" = "status optimal" ] && pass "and prints only the status" \
    || flunk "iis of an optimal model printed: $out"
[ -n "$err" ] && pass "and says so on stderr" || flunk "no message on stderr"
expect_exit 5 "iis without a file is a usage error" "$JAOS" iis
expect_exit 5 "iis of a missing file exits 5" "$JAOS" iis "$tmp/no-such.mps"

# ----------------------------------------------------------------- verify
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "verify of a small optimum exits 0" "$JAOS" verify "$DATA/solve1.mps"
[ "$(line_of proof)" = "proof optimal" ] && pass "it prints 'proof optimal'" \
    || flunk "verify printed '$(line_of proof)'"
[ "$(line_of stage)" = "stage none" ] && pass "and 'stage none'" \
    || flunk "verify printed '$(line_of stage)'"
for k in bound_bits capacity_bits blocks largest_block bytes_held terms; do
    [ -n "$(line_of $k)" ] && pass "it prints a $k line" \
        || flunk "no $k line in: $out"
done
[ -z "$(line_of at_row)" ] && [ -z "$(line_of violation)" ] \
    && pass "a proved basis names no breaking row" \
    || flunk "a proved basis printed a breaking row or a violation"
"$JAOS" verify "$DATA/solve1.mps" > "$tmp/ver1"
"$JAOS" verify "$DATA/solve1.mps" > "$tmp/ver2"
cmp -s "$tmp/ver1" "$tmp/ver2" && pass "two verify runs agree byte for byte" \
    || { flunk "two verify runs differ"; diff "$tmp/ver1" "$tmp/ver2"; }
fi

expect_exit 5 "verify of an infeasible model exits 5" "$JAOS" verify "$DATA/t1.mps"
[ "$out" = "status infeasible" ] && pass "and prints only the status" \
    || flunk "verify of an infeasible model printed: $out"
[ -n "$err" ] && pass "and says so on stderr" || flunk "no message on stderr"
expect_exit 5 "verify without a file is a usage error" "$JAOS" verify

# ---------------------------------------------------------------- ranging
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "ranging of an optimum exits 0" "$JAOS" ranging "$DATA/solve1.mps"
[ "$(printf '%s\n' "$out" | head -n 1)" = "status optimal" ] \
    && pass "its first line is the status" \
    || flunk "ranging began with '$(printf '%s\n' "$out" | head -n 1)'"
# solve1 has three columns and three rows: three lines per block, with the
# right number of fields on each.
[ "$(printf '%s\n' "$out" | grep -c '^cost X[123] [^ ]* [^ ]*$')" -eq 3 ] \
    && pass "three cost lines of two numbers, named by the file" \
    || flunk "cost lines: $(printf '%s\n' "$out" | grep '^cost')"
[ "$(printf '%s\n' "$out" | grep -c '^rhs \(DEMAND\|CAP1\|CAP2\) [^ ]* [^ ]* [^ ]* [^ ]*$')" -eq 3 ] \
    && pass "three rhs lines of four numbers, named by the file" \
    || flunk "rhs lines: $(printf '%s\n' "$out" | grep '^rhs')"
[ "$(printf '%s\n' "$out" | grep -c '^bound X[123] [^ ]* [^ ]* [^ ]* [^ ]*$')" -eq 3 ] \
    && pass "three bound lines of four numbers, named by the file" \
    || flunk "bound lines: $(printf '%s\n' "$out" | grep '^bound')"
[ "$(printf '%s\n' "$out" | wc -l)" -eq 10 ] \
    && pass "and nothing else" \
    || flunk "ranging printed $(printf '%s\n' "$out" | wc -l) lines"
printf '%s\n' "$out" | grep -qi 'nan' && flunk "ranging printed a NaN" \
    || pass "no NaN in the intervals"
# Every interval contains the number it is about (jaos.h): the cost interval
# of column X1 holds its cost of 2, and an unlimited end reads inf or -inf.
printf '%s\n' "$out" | awk '$1 == "cost" && $2 == "X1" {
    lo = ($3 == "-inf") ? -1e300 : $3 + 0; hi = ($4 == "inf") ? 1e300 : $4 + 0;
    found = 1; exit !(lo <= 2 && 2 <= hi) } END { if (!found) exit 1 }' \
    && pass "the cost interval of column X1 contains its cost" \
    || flunk "cost X1 interval does not contain 2: $(line_of 'cost X1')"
"$JAOS" ranging "$DATA/solve1.mps" > "$tmp/rng1"
"$JAOS" ranging "$DATA/solve1.mps" > "$tmp/rng2"
cmp -s "$tmp/rng1" "$tmp/rng2" && pass "two ranging runs agree byte for byte" \
    || { flunk "two ranging runs differ"; diff "$tmp/rng1" "$tmp/rng2"; }
fi

expect_exit 5 "ranging of an infeasible model exits 5" "$JAOS" ranging "$DATA/t1.mps"
[ -n "$err" ] && pass "and says so on stderr" || flunk "no message on stderr"
expect_exit 5 "ranging with two files is a usage error" \
    "$JAOS" ranging "$DATA/solve1.mps" "$DATA/t1.mps"

# ------------------------------------------------------------------ done
if [ "$fail" -ne 0 ]; then
    echo "tests/cli.sh: FAILED"
    exit 1
fi
echo "tests/cli.sh: all checks passed"
