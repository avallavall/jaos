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
# And the same trick for the build with presolve compiled out, which
# reports no reduction because it made none (D329).
nopresolve=0
case "${JAOS_CLI_TEST_FLAGS:-}" in *JAOS_NO_PRESOLVE*) nopresolve=1 ;; esac
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
# Five facts, plus the four presolve lines where presolve fired
# (D329). The rule is still one fact per line and nothing else, and
# the count is stated for both builds rather than relaxed to "at
# least five", which would stop catching a stray line.
want=5
[ "$nopresolve" -eq 0 ] && [ -n "$(line_of presolve_rounds)" ] && want=9
[ "$(printf '%s\n' "$out" | wc -l)" -eq "$want" ] \
    && pass "$want lines, one fact each" \
    || flunk "expected $want lines, got: $out"
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

# A mixed-integer model solves by branch and bound and says so with a
# nodes line and a bound line (D288); t4_int's optimum is 3.5, and its
# relaxation is worth the same, so the marks are what the round trip
# through convert has to carry.
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "a mixed-integer model solves" "$JAOS" solve "$DATA/t4_int.mps"
[ "$(line_of objective)" = "objective 3.5" ] && pass "to its integer optimum" \
    || flunk "MIP objective '$(line_of objective)'"
[ -n "$(line_of nodes)" ] && [ "$(line_of bound)" = "bound 3.5" ] \
    && pass "and prints its nodes and bound" \
    || flunk "MIP lines: $(line_of nodes) / $(line_of bound)"
# The two switches of D289 through the command line: a cuts line beside
# nodes, both switches accepted and the answer unmoved by either, and a
# negative round count refused as a usage error.
[ -n "$(line_of cuts)" ] && pass "and a cuts line" \
    || flunk "no cuts line: $(line_of cuts)"
expect_exit 0 "no cuts and a dive still solve it" \
    "$JAOS" solve "$DATA/t4_int.mps" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --dive
[ "$(line_of objective)" = "objective 3.5" ] && [ "$(line_of cuts)" = "cuts 0" ] \
    && pass "to 3.5 with cuts 0" \
    || flunk "switched-off MIP: $(line_of objective) / $(line_of cuts)"
expect_exit 0 "and the rounding heuristic switches off" \
    "$JAOS" solve "$DATA/t4_int.mps" --no-heuristics
[ "$(line_of objective)" = "objective 3.5" ] \
    && [ "$(line_of heuristic_points)" = "heuristic_points 0" ] \
    && [ -n "$(line_of first_incumbent)" ] \
    && pass "to 3.5 with heuristic_points 0 and a first_incumbent line" \
    || flunk "no-heuristics MIP: $(line_of objective) / $(line_of heuristic_points)"
expect_exit 5 "a negative round count is a usage error" \
    "$JAOS" solve "$DATA/t4_int.mps" --cut-rounds -1
# Cuts below the root (D296): a depth accepted with the answer unmoved, a
# negative depth a usage error.
expect_exit 0 "cuts to depth 3 still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 3
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cut depth: $(line_of objective)"
expect_exit 5 "--cut-depth refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-depth -1
# Cover cuts (D300): a round accepted with the answer unmoved, a negative
# count a usage error.
expect_exit 0 "a round of cover cuts still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cover rounds: $(line_of objective)"
expect_exit 5 "--cover-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --cover-rounds -1
# A cap on a node's cuts (D301): accepted with the answer unmoved, a
# negative cap a usage error.
expect_exit 0 "a node cut cap still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cut-depth 2 --node-cut-cap 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "node cut cap: $(line_of objective)"
expect_exit 5 "--node-cut-cap refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --node-cut-cap -1
# The two cut stalls (D304, D305), the root-cut drop (D306) and the lifted
# cover (D307): each accepted with the answer unmoved, both spellings of
# the two switches; a negative fraction and a word are usage errors.
expect_exit 0 "a cut stall still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 3 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --cut-stall 0.01
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cut stall: $(line_of objective)"
expect_exit 5 "--cut-stall refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-stall -1
expect_exit 0 "a node cut stall still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 3 --node-cut-stall 0.01
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "node cut stall: $(line_of objective)"
expect_exit 5 "--node-cut-stall refuses a word" \
    "$JAOS" solve "$DATA/nl_int.lp" --node-cut-stall some
expect_exit 0 "root cuts that may leave still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 1 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --root-cut-drop
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "root cut drop: $(line_of objective)"
expect_exit 0 "and kept by name" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-root-cut-drop
expect_exit 0 "lifted covers still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --mir-rounds 0 --cover-rounds 1 --cover-lift
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cover lift: $(line_of objective)"
expect_exit 0 "and the extended cover by name" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-cover-lift
# MIR cuts (D309) and the backtracking dive (D308): each accepted with the
# answer unmoved; a negative count is a usage error.
expect_exit 0 "a round of MIR cuts still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --mir-rounds 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "mir rounds: $(line_of objective)"
expect_exit 5 "--mir-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --mir-rounds -1
expect_exit 0 "a backtracking dive still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --dive --dive-backtrack 2
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "dive backtrack: $(line_of objective)"
expect_exit 5 "--dive-backtrack refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive --dive-backtrack -1
# MIR cuts at the nodes (D310) and the dive's resume gap (D311): each
# accepted with the answer unmoved, both spellings of the switch; a
# negative fraction is a usage error.
expect_exit 0 "MIR cuts at the nodes still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 3 --node-mir
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "node mir: $(line_of objective)"
expect_exit 0 "and the Gomory round alone by name" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-node-mir
expect_exit 0 "a dive bounded by the gap still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --dive --dive-gap 0.1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "dive gap: $(line_of objective)"
expect_exit 5 "--dive-gap refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive --dive-gap -1
# The MIR aggregation (D312) and the dive heuristic (D313): each accepted
# with the answer unmoved, and the dive heuristic puts the first incumbent
# at the root; a negative count is a usage error either way.
expect_exit 0 "an aggregated MIR round still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --cut-depth 0 --mir-rounds 2 --mir-aggregate 2
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "mir aggregate: $(line_of objective)"
expect_exit 5 "--mir-aggregate refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --mir-aggregate -1
expect_exit 0 "the dive heuristic still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 5
[ "$(line_of objective)" = "objective 3" ] \
    && [ "$(line_of first_incumbent)" = "first_incumbent 1" ] \
    && pass "to 3 with the first incumbent at the root" \
    || flunk "dive heuristic: $(line_of objective) / $(line_of first_incumbent)"
expect_exit 5 "--dive-heuristic refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive-heuristic -1
# The dive heuristic below the root (D314), RINS (D315) and the dive's
# degradation bound (D316): each accepted with the answer unmoved; a
# negative value is a usage error for all three.
expect_exit 0 "a dive at every node still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 5 --dive-heuristic-depth 20
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "dive heuristic depth: $(line_of objective)"
expect_exit 5 "--dive-heuristic-depth refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive-heuristic-depth -1
expect_exit 0 "RINS still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --dive-heuristic 0 --rins 10
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "rins: $(line_of objective)"
expect_exit 5 "--rins refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --rins -1
expect_exit 0 "a dive bounded by the degradation still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive --dive-degrade 0.01
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "dive degrade: $(line_of objective)"
expect_exit 5 "--dive-degrade refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive --dive-degrade -1
# The feasibility pump (D318): the flag parses and the answer does not
# move. Whether the pump finds a point on THIS model is not checked
# here and must not be: -DJAOS_NO_PRESOLVE hands the tree a different
# shape and the pump finds nothing on it at 5, 20 or 100 rounds,
# which is a heuristic giving up and not a defect. The claim that the
# pump moves the first incumbent to the root is in tests/test_mip.c,
# on a model that test builds itself. A negative count is a usage
# error.
expect_exit 0 "the feasibility pump still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 0 --feaspump 5
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "feaspump: $(line_of objective)"
expect_exit 5 "--feaspump refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --feaspump -1
# The pump's two extensions: the general-integer distance and the
# objective pump each parse and leave the answer alone, for the reason
# the pump's own check above gives; 2 is not a switch and a decay of 1
# would never fade, so both are usage errors.
expect_exit 0 "the general pump still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 0 --feaspump 5 --pump-general 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "pump general: $(line_of objective)"
expect_exit 0 "the objective pump still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 0 --feaspump 5 --pump-obj 0.9
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "pump obj: $(line_of objective)"
expect_exit 5 "--pump-general refuses 2" \
    "$JAOS" solve "$DATA/nl_int.lp" --pump-general 2
expect_exit 5 "--pump-obj refuses 1" \
    "$JAOS" solve "$DATA/nl_int.lp" --pump-obj 1
# The pump's guard (D322), reduced-cost fixing (D323) and bound
# propagation (D324): each parses and leaves the answer alone. What each
# one FINDS on this model is not checked here, for the reason the pump's
# own check above gives -- -DJAOS_NO_PRESOLVE hands the tree a different
# shape, so a count of fixings or of tightened bounds is not the same
# number in the five build configurations. Those counts are asserted in
# tests/test_mip.c, on models that file builds itself. A negative pass
# count is a usage error.
expect_exit 0 "the pump past the guard still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --feaspump 5 --pump-always
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "pump always: $(line_of objective)"
expect_exit 0 "reduced-cost fixing still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --rcfix
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "rcfix: $(line_of objective)"
expect_exit 0 "propagation still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --propagate 4
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "propagate: $(line_of objective)"
expect_exit 0 "every one of the three off still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --propagate 0 --no-rcfix --no-pump-always
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "propagate off: $(line_of objective)"
expect_exit 0 "propagation at the root alone still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --propagate 4 --propagate-depth 0
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "propagate depth: $(line_of objective)"
expect_exit 5 "--propagate refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --propagate -1
# The slack-cut drop (D297), the probe depth (D298) and the pool (D299):
# each accepted with the answer unmoved; a pool line only when asked for;
# a pool of zero and a negative probe depth are usage errors.
expect_exit 0 "slack cuts kept still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 2 --no-cut-drop
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-cut-drop: $(line_of objective)"
expect_exit 0 "probing at the root only still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --reliability 1 --probe-depth 0
[ "$(line_of objective)" = "objective 3" ] && [ -z "$(line_of pool_points)" ] \
    && pass "to 3 with no pool line" \
    || flunk "probe depth: $(line_of objective) / $(line_of pool_points)"
expect_exit 5 "--probe-depth refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --probe-depth -1
expect_exit 0 "a pool of two still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --pool-size 2
[ "$(line_of objective)" = "objective 3" ] && [ -n "$(line_of pool_points)" ] \
    && [ "$(line_of pool_points)" != "pool_points 0" ] \
    && pass "to 3 with a pool_points line" \
    || flunk "pool: $(line_of objective) / $(line_of pool_points)"
expect_exit 5 "--pool-size refuses zero" \
    "$JAOS" solve "$DATA/nl_int.lp" --pool-size 0
# A node limit (D291): nl_int.lp's root is fractional with the cuts off,
# and the rounding finds the optimum there, so a limit of one node stops
# as node_limit with exit 3 and an incumbent on the first node; zero is
# refused.
expect_exit 3 "a node limit stops the tree" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --node-limit 1
[ "$(line_of status)" = "status node_limit" ] && [ "$(line_of nodes)" = "nodes 1" ] \
    && [ "$(line_of first_incumbent)" = "first_incumbent 1" ] \
    && pass "as node_limit after one node with an incumbent" \
    || flunk "node limit: $(line_of status) / $(line_of nodes) / $(line_of first_incumbent)"
expect_exit 5 "--node-limit refuses zero" \
    "$JAOS" solve "$DATA/nl_int.lp" --node-limit 0
# The branching rule (D292): both names accepted and the answer unmoved,
# an unknown name a usage error.
expect_exit 0 "most-fractional branching still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --branching most-fractional
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "most-fractional: $(line_of objective)"
expect_exit 0 "and pseudocost branching by name" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --branching pseudocost
expect_exit 5 "--branching refuses an unknown rule" \
    "$JAOS" solve "$DATA/nl_int.lp" --branching random
# Reliability (D293): zero never probes and still solves; a negative count
# is a usage error.
expect_exit 0 "reliability zero still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --reliability 0
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "reliability 0: $(line_of objective)"
expect_exit 5 "--reliability refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --reliability -1
# A work cap on each probe (D294) and the dive's child rule (D295): both
# accepted with the answer unmoved; a negative cap and an unknown rule are
# usage errors.
expect_exit 0 "a capped probe still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --reliability 1 --probe-cap 0.5
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "probe cap: $(line_of objective)"
expect_exit 5 "--probe-cap refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --probe-cap -1
expect_exit 0 "a dive up first still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --dive --dive-child up
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "dive child up: $(line_of objective)"
expect_exit 5 "--dive-child refuses an unknown rule" \
    "$JAOS" solve "$DATA/nl_int.lp" --dive --dive-child sideways
expect_exit 0 "and converts to LP with its marks" \
    "$JAOS" convert "$DATA/t4_int.mps" "$tmp/t4.lp"
grep -q '^General$' "$tmp/t4.lp" && pass "the LP carries a General section" \
    || flunk "no General section in the converted LP"
expect_exit 0 "and the LP solves to the same optimum" "$JAOS" solve "$tmp/t4.lp"
[ "$(line_of objective)" = "objective 3.5" ] && pass "objective 3.5 again" \
    || flunk "converted MIP objective '$(line_of objective)'"
expect_exit 0 "an LP model prints no nodes line" "$JAOS" solve "$DATA/solve1.mps"
[ -z "$(line_of nodes)" ] && pass "and it does not" || flunk "an LP printed nodes"
fi

# --start warm-starts from the basis a solution file carries: the model's
# own optimum re-solves with no iteration at all, and the objective line
# is the same. A file for another model, or a certificate, is refused.
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--start from the model's own solution exits 0" \
    "$JAOS" solve "$DATA/solve1.mps" --start "$tmp/a.sol"
[ "$(line_of iterations)" = "iterations 0" ] \
    && pass "and the warm start needs no iteration" \
    || flunk "warm start printed '$(line_of iterations)'"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line" \
    || flunk "warm objective '$(line_of objective)' vs '$mps_objective'"
fi
expect_exit 5 "--start with a file for another model exits 5" \
    "$JAOS" solve "$DATA/g2.lp" --start "$tmp/a.sol"
# A certificate file carries the basis its refusal stopped on since D332,
# so --start takes one and the model answers infeasible again, which is
# exit 1. What is refused is a file with no basis in it at all.
expect_exit 1 "--start from a certificate exits 1, the model's own verdict" \
    "$JAOS" solve "$DATA/t1.mps" --start "$tmp/b.sol"
[ "$(line_of status)" = "status infeasible" ] \
    && pass "and the warm run reaches the same verdict" \
    || flunk "warm from a certificate printed '$(line_of status)'"
grep -q '^basis ' "$tmp/b.sol" \
    && pass "the certificate file carries a basis section" \
    || flunk "no basis records in the certificate file"

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

# ------------------------------------------------------------------ relax
expect_exit 0 "relax of an infeasible model exits 0" "$JAOS" relax "$DATA/t1.mps"
for k in total rows_moved cols_moved largest work_units; do
    [ -n "$(line_of $k)" ] && pass "relax prints a $k line" \
        || flunk "no $k line in: $out"
done
moves=$(printf '%s\n' "$out" | grep -c '^\(row\|col\) [^ ]* \(lower\|upper\) ')
counted=$(( $(line_of rows_moved | cut -d' ' -f2) \
          + $(line_of cols_moved | cut -d' ' -f2) ))
[ "$moves" -eq "$counted" ] && pass "one move line per counted move ($moves)" \
    || flunk "rows_moved+cols_moved is $counted and there are $moves lines"
[ "$moves" -ge 1 ] && pass "an infeasible model needs at least one move" \
    || flunk "relax of an infeasible model moved nothing"
# Named as the file names them, the rule every command here follows (D284).
[ "$(printf '%s\n' "$out" | grep -c '^\(row \(LIM1\|LIM2\|EQ1\)\|col X[123]\) \(lower\|upper\) ')" -eq "$moves" ] \
    && pass "every move carries the file's own name" \
    || flunk "a move is not named by the file: $out"
"$JAOS" relax "$DATA/t1.mps" > "$tmp/relax1"
"$JAOS" relax "$DATA/t1.mps" > "$tmp/relax2"
cmp -s "$tmp/relax1" "$tmp/relax2" && pass "two relax runs agree byte for byte" \
    || { flunk "two relax runs differ"; diff "$tmp/relax1" "$tmp/relax2"; }

expect_exit 0 "relax of a feasible model exits 0" "$JAOS" relax "$DATA/solve1.mps"
[ "$(line_of total)" = "total 0" ] && pass "and its total is 0" \
    || flunk "relax of a feasible model printed '$(line_of total)'"
expect_exit 0 "relax --rows exits 0" "$JAOS" relax "$DATA/t1.mps" --rows
[ "$(line_of cols_moved)" = "cols_moved 0" ] \
    && pass "and moves no column bound" \
    || flunk "relax --rows printed '$(line_of cols_moved)'"
expect_exit 0 "relax --cols exits 0" "$JAOS" relax "$DATA/t1.mps" --cols
[ "$(line_of rows_moved)" = "rows_moved 0" ] \
    && pass "and moves no row bound" \
    || flunk "relax --cols printed '$(line_of rows_moved)'"
expect_exit 5 "relax without a file is a usage error" "$JAOS" relax
expect_exit 5 "relax of a missing file exits 5" "$JAOS" relax "$tmp/no-such.mps"
expect_exit 5 "relax with an unknown option is a usage error" \
    "$JAOS" relax "$DATA/t1.mps" --both

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
# --values prints what the proof proved, exactly (D286): solve1's optimum
# is X1=4, X2=3, X3=3 for 29, integers a rational spells without a slash.
expect_exit 0 "verify --values exits 0 on a proved optimum" \
    "$JAOS" verify "$DATA/solve1.mps" --values
[ "$(line_of 'x X1')" = "x X1 4" ] && [ "$(line_of 'x X2')" = "x X2 3" ] \
    && [ "$(line_of 'x X3')" = "x X3 3" ] \
    && pass "it prints every column's exact value under its name" \
    || flunk "exact values: $(printf '%s\n' "$out" | grep '^x ')"
[ "$(printf '%s\n' "$out" | grep -c '^y \(DEMAND\|CAP1\|CAP2\) ')" -eq 3 ] \
    && pass "and every row's exact dual" \
    || flunk "exact duals: $(printf '%s\n' "$out" | grep '^y ')"
[ "$(line_of objective_exact)" = "objective_exact 29" ] \
    && pass "and the exact objective" \
    || flunk "verify printed '$(line_of objective_exact)'"
expect_exit 0 "verify without --values prints no values" \
    "$JAOS" verify "$DATA/solve1.mps"
[ -z "$(line_of objective_exact)" ] && pass "and no objective_exact line" \
    || flunk "verify printed values without being asked"
fi
expect_exit 5 "verify refuses an unknown option" \
    "$JAOS" verify "$DATA/solve1.mps" --bogus

expect_exit 5 "verify of an infeasible model exits 5" "$JAOS" verify "$DATA/t1.mps"
[ "$out" = "status infeasible" ] && pass "and prints only the status" \
    || flunk "verify of an infeasible model printed: $out"
[ -n "$err" ] && pass "and says so on stderr" || flunk "no message on stderr"
expect_exit 5 "verify without a file is a usage error" "$JAOS" verify

# ---------------------------------------------------------------- ranging
if [ "$faulty" -eq 0 ]; then
# The exact proof on disk (D325). verify --proof writes it, check --proof
# judges it from the model alone with no tolerance and no basis read, and
# a value moved by hand is refused. Under either presolve fault build the
# proof itself may be refused, so the positive half is skipped there the
# way every other positive analysis check is.
if [ "$faulty" -eq 0 ]; then
  P="$tmp/g1.proof"
  expect_exit 0 "verify --proof writes a proof file" \
      "$JAOS" verify "$DATA/g1.lp" --proof "$P"
  [ -s "$P" ] && pass "the file is not empty" || flunk "no proof file"
  grep -q "^proof optimal$" "$P" && pass "it says what it proves" \
      || flunk "no proof line"
  grep -q "^objective " "$P" && pass "and carries an exact objective" \
      || flunk "no objective line"
  expect_exit 0 "check --proof takes it" \
      "$JAOS" check "$DATA/g1.lp" --proof "$P"
  [ "$(line_of primal)" = "primal ok" ] && pass "primal ok" \
      || flunk "primal: $(line_of primal)"
  [ "$(line_of dual)" = "dual ok" ] && pass "dual ok" \
      || flunk "dual: $(line_of dual)"
  [ "$(line_of objective)" = "objective ok" ] && pass "objective ok" \
      || flunk "objective: $(line_of objective)"
  # The case it must reject: one value moved by a whole unit.
  sed -i "s/^col z 8$/col z 7/" "$P"
  expect_exit 1 "check --proof refuses a moved value" \
      "$JAOS" check "$DATA/g1.lp" --proof "$P"
  [ "$(line_of proof)" = "proof broken" ] && pass "and says so" \
      || flunk "verdict: $(line_of proof)"
  # A certificate is a proof too, and needs no verify (D328): the
  # solve publishes the ray and every double in it is already an
  # exact rational. bgdbg1 is one whose certificate holds exactly.
  Q="$tmp/inf.proof"
  expect_exit 1 "solve --proof writes an infeasible certificate" \
      "$JAOS" solve bench/instances-infeas/bgdbg1.mps --proof "$Q"
  grep -q "^proof infeasible$" "$Q" && pass "and says what it claims" \
      || flunk "no infeasible line"
  expect_exit 0 "check --proof takes the certificate" \
      "$JAOS" check bench/instances-infeas/bgdbg1.mps --proof "$Q"
  [ "$(line_of claims)" = "claims infeasible" ] && pass "as a certificate" \
      || flunk "claims: $(line_of claims)"
  [ "$(line_of proof)" = "proof holds" ] && pass "and it holds exactly" \
      || flunk "verdict: $(line_of proof)"
  rm -f "$Q"
  rm -f "$P"
fi
expect_exit 5 "check refuses a solution and a proof at once" \
    "$JAOS" check "$DATA/g1.lp" some.sol --proof some.proof
expect_exit 5 "check refuses --proof with no path" \
    "$JAOS" check "$DATA/g1.lp" --proof
# stats (D327): it solves nothing, so it runs under every build. The two
# partitions are what is checked, because a count that is merely printed
# is not evidence that the walk saw every row and every column.
# What presolve removed (D329). afiro loses two singleton rows, so the
# four lines are there under the default build and gone under
# -DJAOS_NO_PRESOLVE, where nothing fires at all. Both arms are checked,
# because a report that is always absent would pass a one-sided test.
expect_exit 0 "solve prints what presolve removed" \
    "$JAOS" solve bench/instances/afiro.mps
if [ "$nopresolve" -eq 1 ]; then
  [ -z "$(line_of presolve_rounds)" ] && pass "and prints none with presolve out" \
      || flunk "presolve lines under -DJAOS_NO_PRESOLVE: $(line_of presolve_rounds)"
else
  [ "$(line_of presolve_rounds)" = "presolve_rounds 1" ] && pass "in one round" \
      || flunk "rounds: $(line_of presolve_rounds)"
  [ "$(line_of presolve_rows)" = "presolve_rows 25" ] && pass "27 rows down to 25" \
      || flunk "rows: $(line_of presolve_rows)"
fi
expect_exit 0 "stats reads a model" \
    "$JAOS" stats "$DATA/g_int.lp"
r=$(line_of rows | cut -d" " -f2)
c=$(line_of columns | cut -d" " -f2)
rk=$(( $(line_of equality_rows | cut -d" " -f2) + $(line_of ranged_rows | cut -d" " -f2) + $(line_of one_sided_rows | cut -d" " -f2) + $(line_of free_rows | cut -d" " -f2) ))
ck=$(( $(line_of fixed_columns | cut -d" " -f2) + $(line_of ranged_columns | cut -d" " -f2) + $(line_of one_sided_columns | cut -d" " -f2) + $(line_of free_columns | cut -d" " -f2) ))
[ "$rk" = "$r" ] && pass "the row kinds partition the rows" \
    || flunk "row kinds $rk of $r"
[ "$ck" = "$c" ] && pass "the column kinds partition the columns" \
    || flunk "column kinds $ck of $c"
[ "$(line_of integer_columns)" = "integer_columns 2" ] && pass "and count the integers" \
    || flunk "integers: $(line_of integer_columns)"
expect_exit 5 "stats takes one file" \
    "$JAOS" stats "$DATA/g_int.lp" "$DATA/g1.lp"
# The tree's two inputs (D326): the cutoff parses, and one nothing can
# beat ends the search with no answer. g_int.lp MINIMIZES, so the cutoff
# nothing reaches is the very negative one.
expect_exit 0 "--cutoff parses" \
    "$JAOS" solve "$DATA/g_int.lp" --cutoff 1e9
expect_exit 1 "a cutoff nothing beats ends infeasible" \
    "$JAOS" solve "$DATA/g_int.lp" --cutoff -1e9
expect_exit 5 "--cutoff needs a number" \
    "$JAOS" solve "$DATA/g_int.lp" --cutoff banana
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
