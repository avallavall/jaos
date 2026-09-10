#!/usr/bin/env bash
set -u

JAOS=${1:-build/cli/jaos}
DATA=tests/data
fail=0
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

pass() { echo "ok   $1"; }
flunk() { echo "FAIL $1"; fail=1; }

out=""
err=""
expect_exit() {
    local want=$1 name=$2; shift 2
    out=$("$@" 2>"$tmp/err"); local got=$?
    err=$(cat "$tmp/err")
    if [ "$got" -eq "$want" ]; then pass "$name"
    else flunk "$name (exit $got, wanted $want)"; echo "$err" | sed 's/^/     /'; fi
}

line_of() { printf '%s\n' "$out" | grep "^$1 "; }

faulty=0
case "${JAOS_CLI_TEST_FLAGS:-}" in *JAOS_PRESOLVE_FAULT*) faulty=1 ;; esac
nopresolve=0
case "${JAOS_CLI_TEST_FLAGS:-}" in *JAOS_NO_PRESOLVE*) nopresolve=1 ;; esac
[ "$faulty" -eq 1 ] && echo "note fault build: positive analysis checks skipped"

if [ ! -x "$JAOS" ]; then
    echo "FAIL $JAOS is not an executable; build it with make cli"
    exit 1
fi

want=$(sed -n 's/^#define JAOS_VERSION_STRING "\([^"]*\)".*/\1/p' include/jaos.h)
expect_exit 0 "--version exits 0" "$JAOS" --version
[ -n "$want" ] && [ "$out" = "$want" ] \
    && pass "--version prints $want" \
    || flunk "--version printed '$out', jaos.h says '$want'"

expect_exit 0 "--help exits 0" "$JAOS" --help
printf '%s\n' "$out" | grep -q '^Usage:' \
    && pass "--help prints the usage" || flunk "--help printed no usage"
full_help=$(printf '%s\n' "$out" | wc -l)

for v in solve convert check iis relax verify stats ranging; do
    expect_exit 0 "help $v exits 0" "$JAOS" help "$v"
    printf '%s\n' "$out" | grep -q "^  jaos $v " \
        && pass "and shows its own synopsis" \
        || flunk "help $v has no '  jaos $v ' line"
    [ "$(printf '%s\n' "$out" | wc -l)" -lt "$full_help" ] \
        && pass "and is shorter than the whole text" \
        || flunk "help $v printed everything"
done
"$JAOS" help convert | grep -q '^solve reads FILE' \
    && flunk "help convert printed solve's text" \
    || pass "help convert leaves solve's text out"
"$JAOS" --help | grep -q '^solve reads FILE' \
    && pass "and the full help still has it" \
    || flunk "the full help lost solve's text"
expect_exit 5 "help of an unknown command is a usage error" \
    "$JAOS" help frobnicate
expect_exit 5 "help of two commands is a usage error" \
    "$JAOS" help solve check

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
expect_exit 5 "--primal-tol refuses a negative" \
    "$JAOS" solve "$DATA/solve1.mps" --primal-tol -1e-7

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
want=5
[ "$nopresolve" -eq 0 ] && [ -n "$(line_of presolve_rounds)" ] && want=9
[ "$(printf '%s\n' "$out" | wc -l)" -eq "$want" ] \
    && pass "$want lines, one fact each" \
    || flunk "expected $want lines, got: $out"
[ "$(printf '%s\n' "$out" | tail -n 1 | cut -d' ' -f1)" = "time" ] \
    && pass "time is the last line" || flunk "time is not the last line"
mps_objective=$(line_of objective)

"$JAOS" solve "$DATA/solve1.mps" | grep -v '^time ' > "$tmp/run1"
"$JAOS" solve "$DATA/solve1.mps" | grep -v '^time ' > "$tmp/run2"
cmp -s "$tmp/run1" "$tmp/run2" \
    && pass "two runs agree without the time line" \
    || { flunk "two runs differ"; diff "$tmp/run1" "$tmp/run2"; }

expect_exit 0 "--quiet exits 0" "$JAOS" solve --quiet "$DATA/solve1.mps"
[ "$out" = "status optimal" ] \
    && pass "--quiet prints the status line only" \
    || flunk "--quiet printed: $out"

"$JAOS" solve "$DATA/solve1.mps" --log detail 2>"$tmp/log" \
    | grep -v '^time ' > "$tmp/run3"
[ -s "$tmp/log" ] && pass "--log detail writes to stderr" \
    || flunk "--log detail wrote nothing to stderr"
cmp -s "$tmp/run1" "$tmp/run3" \
    && pass "--log leaves stdout unchanged" \
    || { flunk "--log changed stdout"; diff "$tmp/run1" "$tmp/run3"; }

expect_exit 0 "a gzip MPS file solves" "$JAOS" solve "$DATA/solve1.mps.gz"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "the gzip file gives the same objective line" \
    || flunk "gzip objective '$(line_of objective)' vs '$mps_objective'"

expect_exit 3 "a solve stopped by --work-limit exits 3" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 1
[ "$(line_of status)" = "status work_limit" ] \
    && pass "it prints 'status work_limit'" \
    || flunk "status line is '$(line_of status)'"
[ -z "$(line_of objective)" ] \
    && pass "no objective line for a stopped solve" \
    || flunk "a stopped solve printed '$(line_of objective)'"

expect_exit 1 "an infeasible model exits 1" "$JAOS" solve "$DATA/t1.mps"
[ "$(line_of status)" = "status infeasible" ] \
    && pass "it prints 'status infeasible'" \
    || flunk "status line is '$(line_of status)'"
[ -z "$(line_of objective)" ] \
    && pass "no objective line without an optimum" \
    || flunk "an infeasible solve printed '$(line_of objective)'"

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
expect_exit 3 "--solution on a work-limited solve keeps exit 3" \
    "$JAOS" solve "$DATA/solve1.mps" --work-limit 1 --solution "$tmp/w.sol"
[ ! -e "$tmp/w.sol" ] && pass "and writes no file" \
    || flunk "a solution file was written for a solve that did not finish"
[ -n "$err" ] && pass "and says so on stderr" \
    || flunk "nothing on stderr about the missing solution file"

expect_exit 5 "--solution to an unwritable path is an error" \
    "$JAOS" solve "$DATA/solve1.mps" --solution "$tmp/no/such/dir/c.sol"

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "a mixed-integer model solves" "$JAOS" solve "$DATA/t4_int.mps"
[ "$(line_of objective)" = "objective 3.5" ] && pass "to its integer optimum" \
    || flunk "MIP objective '$(line_of objective)'"
[ -n "$(line_of nodes)" ] && [ "$(line_of bound)" = "bound 3.5" ] \
    && pass "and prints its nodes and bound" \
    || flunk "MIP lines: $(line_of nodes) / $(line_of bound)"
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
expect_exit 0 "cuts to depth 3 still solve it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 3
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cut depth: $(line_of objective)"
expect_exit 5 "--cut-depth refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-depth -1
expect_exit 0 "a round of cover cuts still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "cover rounds: $(line_of objective)"
expect_exit 0 "--clique-rounds 1 solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --clique-rounds 1
expect_exit 5 "--clique-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --clique-rounds -1
expect_exit 0 "--zero-half-rounds 1 solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --clique-rounds 0 --zero-half-rounds 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "zero-half: $(line_of objective)"
expect_exit 0 "--flow-cover-rounds 1 solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --clique-rounds 0 --flow-cover-rounds 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "flow-cover: $(line_of objective)"
expect_exit 5 "--flow-cover-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --flow-cover-rounds -1
"$JAOS" options | grep -q '^mip_flow_cover_rounds ' \
    && pass "jaos options lists mip_flow_cover_rounds" \
    || flunk "jaos options does not list mip_flow_cover_rounds"
expect_exit 5 "--zero-half-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --zero-half-rounds -1
"$JAOS" options | grep -q '^mip_zero_half_rounds ' \
    && pass "jaos options lists mip_zero_half_rounds" \
    || flunk "jaos options does not list mip_zero_half_rounds"
expect_exit 5 "--cover-rounds refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --cover-rounds -1
expect_exit 0 "a node cut cap still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cut-depth 2 --node-cut-cap 1
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "node cut cap: $(line_of objective)"
expect_exit 5 "--node-cut-cap refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --node-cut-cap -1
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
# error.
expect_exit 0 "the feasibility pump still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --no-heuristics --dive-heuristic 0 --feaspump 5
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "feaspump: $(line_of objective)"
expect_exit 5 "--feaspump refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --feaspump -1
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
expect_exit 0 "--no-tighten still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-tighten
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-tighten: $(line_of objective)"
expect_exit 0 "--tighten is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --tighten
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "tighten: $(line_of objective)"
"$JAOS" options | grep -q '^mip_tighten ' \
    && pass "jaos options lists mip_tighten" \
    || flunk "jaos options does not list mip_tighten"
expect_exit 0 "--no-probing still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-probing
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-probing: $(line_of objective)"
expect_exit 0 "--probing is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --probing
"$JAOS" options | grep -q '^mip_probing ' \
    && pass "jaos options lists mip_probing" \
    || flunk "jaos options does not list mip_probing"
expect_exit 0 "--probing-cap is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --probing --probing-cap 0.5
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "probing-cap: $(line_of objective)"
expect_exit 5 "--probing-cap refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --probing-cap -1
"$JAOS" options | grep -q '^mip_probing_cap ' \
    && pass "jaos options lists mip_probing_cap" \
    || flunk "jaos options does not list mip_probing_cap"
expect_exit 0 "--threads 1 is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --threads 1
expect_exit 0 "--threads 3 is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --threads 3
expect_exit 5 "--threads 0 is refused" \
    "$JAOS" solve "$DATA/nl_int.lp" --threads 0
expect_exit 0 "--algorithm concurrent on one thread" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm concurrent --threads 1
conc_one="$(line_of objective) $(line_of work_units)"
expect_exit 0 "and on three threads" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm concurrent --threads 3
[ "$(line_of objective) $(line_of work_units)" = "$conc_one" ] \
    && pass "gives the same objective and the same work units" \
    || flunk "three threads: $(line_of objective) $(line_of work_units) against $conc_one"
"$JAOS" options | grep -q '^threads ' \
    && pass "jaos options lists threads" \
    || flunk "jaos options does not list threads"
expect_exit 0 "an .nl file is read by its extension" \
    "$JAOS" solve "$DATA/t_lin.nl"
[ "$(line_of objective)" = "objective -4" ] && pass "to -4" \
    || flunk "nl: $(line_of objective)"
expect_exit 5 "a nonlinear .nl is refused" \
    "$JAOS" solve "$DATA/e_nonlin.nl"
expect_exit 0 "--no-orbital still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-orbital
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-orbital: $(line_of objective)"
expect_exit 0 "--orbital is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --orbital
"$JAOS" options | grep -q '^mip_orbital ' \
    && pass "jaos options lists mip_orbital" \
    || flunk "jaos options does not list mip_orbital"
expect_exit 0 "--no-symmetry still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-symmetry
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-symmetry: $(line_of objective)"
expect_exit 0 "--symmetry is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --symmetry
[ -n "$(line_of symmetry_orbits)" ] && pass "and reports the orbits" \
    || flunk "no symmetry_orbits line"
"$JAOS" options | grep -q '^mip_symmetry ' \
    && pass "jaos options lists mip_symmetry" \
    || flunk "jaos options does not list mip_symmetry"
expect_exit 0 "--no-conflicts still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-conflicts
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-conflicts: $(line_of objective)"
expect_exit 0 "--conflicts is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --conflicts
"$JAOS" options | grep -q '^mip_conflicts ' \
    && pass "jaos options lists mip_conflicts" \
    || flunk "jaos options does not list mip_conflicts"
expect_exit 0 "--no-clique-fix still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --no-clique-fix
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "no-clique-fix: $(line_of objective)"
expect_exit 0 "--clique-fix is accepted" \
    "$JAOS" solve "$DATA/nl_int.lp" --clique-fix
"$JAOS" options | grep -q '^mip_clique_fix ' \
    && pass "jaos options lists mip_clique_fix" \
    || flunk "jaos options does not list mip_clique_fix"
expect_exit 0 "propagation at the root alone still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --propagate 4 --propagate-depth 0
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "propagate depth: $(line_of objective)"
expect_exit 5 "--propagate refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --propagate -1
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
rm -f "$tmp"/pl-*.pt
expect_exit 0 "--pool-out writes the pool" \
    "$JAOS" solve "$DATA/nl_int.lp" --pool-size 2 --pool-out "$tmp/pl"
[ -n "$(line_of pool_files)" ] && pass "and says how many" \
    || flunk "no pool_files line"
[ "$(ls "$tmp"/pl-*.pt 2>/dev/null | wc -l)" \
  -eq "$(line_of pool_files | cut -d' ' -f2)" ] \
    && pass "and the count matches the files" \
    || flunk "pool_files says $(line_of pool_files), files: $(ls "$tmp"/pl-*.pt 2>/dev/null | wc -l)"
expect_exit 0 "and the best one checks out" \
    "$JAOS" check "$DATA/nl_int.lp" --point "$tmp/pl-0.pt"
[ "$(line_of primal_feasible)" = "primal_feasible yes" ] \
    && pass "as a feasible point" \
    || flunk "the pool point printed '$(line_of primal_feasible)'"
rm -f "$tmp"/lp-*.pt
expect_exit 0 "--pool-out on an LP writes nothing" \
    "$JAOS" solve "$DATA/solve1.mps" --pool-out "$tmp/lp"
[ -z "$(ls "$tmp"/lp-*.pt 2>/dev/null)" ] && pass "and leaves no files" \
    || flunk "an LP wrote pool files"
[ -n "$err" ] && pass "and says why on stderr" || flunk "no message on stderr"
expect_exit 3 "a node limit stops the tree" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --node-limit 1
[ "$(line_of status)" = "status node_limit" ] && [ "$(line_of nodes)" = "nodes 1" ] \
    && [ "$(line_of first_incumbent)" = "first_incumbent 1" ] \
    && pass "as node_limit after one node with an incumbent" \
    || flunk "node limit: $(line_of status) / $(line_of nodes) / $(line_of first_incumbent)"
expect_exit 5 "--node-limit refuses zero" \
    "$JAOS" solve "$DATA/nl_int.lp" --node-limit 0
expect_exit 0 "most-fractional branching still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --branching most-fractional
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "most-fractional: $(line_of objective)"
expect_exit 0 "and pseudocost branching by name" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --branching pseudocost
expect_exit 5 "--branching refuses an unknown rule" \
    "$JAOS" solve "$DATA/nl_int.lp" --branching random
expect_exit 0 "stats counts the SOS sets" \
    "$JAOS" stats "$DATA/g_sos.lp"
[ "$(line_of sos_sets)" = "sos_sets 2" ] && pass "two of them" || flunk "stats sos: $(line_of sos_sets)"
expect_exit 0 "stats counts semi-continuous columns and indicator rows" \
    "$JAOS" stats "$DATA/g_ind.lp"
[ "$(line_of indicator_rows)" = "indicator_rows 1" ] && [ "$(line_of semicontinuous_columns)" = "semicontinuous_columns 0" ] \
    && pass "one row, no column" || flunk "stats ind: $(line_of indicator_rows)"
expect_exit 0 "options prints every option" \
    "$JAOS" options
[ "$(line_of mip_cut_rounds)" = "mip_cut_rounds 1" ] && pass "with its default" \
    || flunk "options: $(line_of mip_cut_rounds)"
[ "$(printf '%s\n' "$out" | wc -l)" -gt 40 ] && pass "all of them" || flunk "options: too few lines"
expect_exit 0 "options reflects --opt" \
    "$JAOS" options --opt mip_cut_rounds=7 --opt algorithm=primal
[ "$(line_of mip_cut_rounds)" = "mip_cut_rounds 7" ] && [ "$(line_of algorithm)" = "algorithm primal" ] \
    && pass "in its lines" || flunk "options --opt: $(line_of algorithm)"
printf '%s\n' "$out" > "$tmp/saved.txt"
expect_exit 0 "and --params reads what options printed" \
    "$JAOS" options --params "$tmp/saved.txt"
[ "$(line_of algorithm)" = "algorithm primal" ] && pass "back unchanged" \
    || flunk "options round trip: $(line_of algorithm)"
expect_exit 5 "options refuses an unknown option" \
    "$JAOS" options --opt nonsense=1
expect_exit 0 "--opt sets options by name" \
    "$JAOS" solve "$DATA/g_int.lp" --opt mip_cut_rounds=0 --opt mip_heuristics=false --opt algorithm=dual
[ "$(line_of objective)" = "objective 2" ] && pass "and the answer stands" \
    || flunk "--opt: $(line_of objective)"
expect_exit 5 "--opt refuses an unknown option" \
    "$JAOS" solve "$DATA/g_int.lp" --opt nonsense=1
expect_exit 5 "--opt refuses a value of the wrong kind" \
    "$JAOS" solve "$DATA/g_int.lp" --opt mip_cut_rounds=many
expect_exit 5 "--opt wants NAME=VALUE" \
    "$JAOS" solve "$DATA/g_int.lp" --opt mip_cut_rounds
printf 'mip_cut_rounds 0\n# a comment\nalgorithm = primal\n' > "$tmp/opts.txt"
expect_exit 0 "--params reads a file of options" \
    "$JAOS" solve "$DATA/g_int.lp" --params "$tmp/opts.txt"
printf 'mip_cut_rounds zero\n' > "$tmp/bad.txt"
expect_exit 5 "--params names the bad line" \
    "$JAOS" solve "$DATA/g_int.lp" --params "$tmp/bad.txt"
expect_exit 0 "check --point takes a SCIP solution file" \
    "$JAOS" check "$DATA/g1.lp" --point "$DATA/sol_scip.sol"
expect_exit 0 "and a HiGHS one with its duals" \
    "$JAOS" check "$DATA/g1.lp" --point "$DATA/sol_highs.sol" --duals "$DATA/sol_highs.sol"
expect_exit 0 "an indicator LP file solves through the tree" \
    "$JAOS" solve "$DATA/g_ind.lp"
[ "$(line_of objective)" = "objective 10" ] && pass "to 10, the row switched off" \
    || flunk "indicator: $(line_of objective)"
expect_exit 0 "an SOS2 LP file solves through the tree" \
    "$JAOS" solve "$DATA/g_sos.lp"
[ "$(line_of objective)" = "objective 2" ] && pass "to 2, two adjacent members" \
    || flunk "sos: $(line_of objective)"
expect_exit 0 "a semi-continuous LP file solves through the tree" \
    "$JAOS" solve "$DATA/g_semi.lp"
[ "$(line_of objective)" = "objective 2" ] && pass "to 2, the floor of x" \
    || flunk "semi-continuous: $(line_of objective)"
expect_exit 0 "--algorithm dual solves an LP" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm dual
dual_obj="$(line_of objective)"
expect_exit 0 "--algorithm primal solves it too" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm primal
[ "$(line_of objective)" = "$dual_obj" ] && pass "to the same objective" \
    || flunk "primal: $(line_of objective) against $dual_obj"
expect_exit 0 "--algorithm barrier solves it as well" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm barrier
[ "$(line_of objective)" = "$dual_obj" ] && pass "to the same objective" \
    || flunk "barrier: $(line_of objective) against $dual_obj"
expect_exit 0 "--algorithm concurrent solves it as well" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm concurrent
[ "$(line_of objective)" = "$dual_obj" ] && pass "to the same objective" \
    || flunk "concurrent: $(line_of objective) against $dual_obj"
expect_exit 5 "--algorithm concurrent refuses a quadratic objective" \
    "$JAOS" solve "$DATA/g_quad.lp" --algorithm concurrent
expect_exit 5 "--algorithm needs a name it knows" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm sideways
expect_exit 0 "a separable QP in LP format solves through the barrier" \
    "$JAOS" solve "$DATA/g_quad.lp"
case "$(line_of objective)" in
    "objective 4.0000"*|"objective 3.9999"*) pass "to objective 4" ;;
    *) flunk "QP objective '$(line_of objective)'" ;;
esac
expect_exit 0 "a mixed-integer QP solves through the tree" \
    "$JAOS" solve "$DATA/g_miqp.lp"
case "$(line_of objective)" in
    "objective -8"|"objective -8.0000"*|"objective -7.9999"*) pass "to objective -8" ;;
    *) flunk "MIQP objective '$(line_of objective)'" ;;
esac
expect_exit 5 "--algorithm primal refuses a quadratic objective" \
    "$JAOS" solve "$DATA/g_quad.lp" --algorithm primal
expect_exit 0 "convert of a QP to MPS exits 0" \
    "$JAOS" convert "$DATA/g_quad.lp" "$tmp/gq.mps"
grep -q '^QUADOBJ$' "$tmp/gq.mps" \
    && pass "and writes a QUADOBJ section" \
    || flunk "no QUADOBJ in the converted MPS"
expect_exit 0 "the converted QP solves" "$JAOS" solve "$tmp/gq.mps"
case "$(line_of objective)" in
    "objective 4.0000"*|"objective 3.9999"*) pass "to the same objective" ;;
    *) flunk "converted QP objective '$(line_of objective)'" ;;
esac
expect_exit 0 "stats of a QP exits 0" "$JAOS" stats "$DATA/g_quad.lp"
[ "$(line_of quadratic_columns)" = "quadratic_columns 2" ] \
    && pass "and counts the quadratic columns" \
    || flunk "stats printed '$(line_of quadratic_columns)'"
expect_exit 0 "--algorithm pdlp solves it as well" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm pdlp
[ "$(line_of objective)" = "$dual_obj" ] && pass "to the same objective" \
    || flunk "pdlp: $(line_of objective) against $dual_obj"
expect_exit 1 "--algorithm pdlp on an infeasible model exits 1" \
    "$JAOS" solve "$DATA/t1.mps" --algorithm pdlp
expect_exit 1 "--algorithm barrier on an infeasible model exits 1" \
    "$JAOS" solve "$DATA/t1.mps" --algorithm barrier
[ "$(line_of status)" = "status infeasible" ] \
    && pass "and prints 'status infeasible'" \
    || flunk "barrier on an infeasible model printed '$(line_of status)'"
expect_exit 2 "--algorithm barrier on an unbounded model exits 2" \
    "$JAOS" solve "$DATA/unbounded.mps" --algorithm barrier
[ "$(line_of status)" = "status unbounded" ] \
    && pass "and prints 'status unbounded'" \
    || flunk "barrier on an unbounded model printed '$(line_of status)'"
expect_exit 5 "--algorithm refuses an unknown method" \
    "$JAOS" solve "$DATA/solve1.mps" --algorithm newton
expect_exit 0 "reliability zero still solves it" \
    "$JAOS" solve "$DATA/nl_int.lp" --cut-rounds 0 --cover-rounds 0 --mir-rounds 0 --cut-depth 0 --reliability 0
[ "$(line_of objective)" = "objective 3" ] && pass "to 3" \
    || flunk "reliability 0: $(line_of objective)"
expect_exit 5 "--reliability refuses a negative" \
    "$JAOS" solve "$DATA/nl_int.lp" --reliability -1
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
expect_exit 1 "--start from a certificate exits 1, the model's own verdict" \
    "$JAOS" solve "$DATA/t1.mps" --start "$tmp/b.sol"
[ "$(line_of status)" = "status infeasible" ] \
    && pass "and the warm run reaches the same verdict" \
    || flunk "warm from a certificate printed '$(line_of status)'"
grep -q '^basis ' "$tmp/b.sol" \
    && pass "the certificate file carries a basis section" \
    || flunk "no basis records in the certificate file"

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "solve --check exits with the solve's code" \
    "$JAOS" solve "$DATA/solve1.mps" --check
[ "$(line_of check_ok)" = "check_ok yes" ] \
    && pass "and says the answer checks out" \
    || flunk "solve --check printed '$(line_of check_ok)'"
[ -n "$(line_of primal_feasible)" ] && [ -n "$(line_of gap_certified)" ] \
    && pass "and prints the checker's own report" \
    || flunk "solve --check printed no report"
expect_exit 1 "--check on an infeasible model still exits 1" \
    "$JAOS" solve "$DATA/t1.mps" --check
[ -z "$(line_of check_ok)" ] && pass "and checks nothing, since there is no optimum" \
    || flunk "solve --check judged a non-optimum"
[ -n "$err" ] && pass "and says so on stderr" || flunk "no message on stderr"
fi

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--write-basis writes a basis file" \
    "$JAOS" solve "$DATA/solve1.mps" --write-basis "$tmp/a.bas"
[ -s "$tmp/a.bas" ] && pass "and the file is not empty" \
    || flunk "--write-basis left nothing"
grep -q '^ENDATA$' "$tmp/a.bas" && pass "and it ends with ENDATA" \
    || flunk "no ENDATA in the basis file"
grep -qE '^ (XU|XL|UL) ' "$tmp/a.bas" && pass "and carries cards" \
    || flunk "no cards in the basis file"
expect_exit 0 "--basis warm-starts from it" \
    "$JAOS" solve "$DATA/solve1.mps" --basis "$tmp/a.bas"
[ "$(line_of iterations)" = "iterations 0" ] \
    && pass "and needs no iteration" \
    || flunk "--basis printed '$(line_of iterations)'"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line" \
    || flunk "--basis objective '$(line_of objective)' vs '$mps_objective'"
expect_exit 5 "--basis with a file for another model exits 5" \
    "$JAOS" solve "$DATA/g2.lp" --basis "$tmp/a.bas"
expect_exit 5 "--basis and --start together is a usage error" \
    "$JAOS" solve "$DATA/solve1.mps" --basis "$tmp/a.bas" --start "$tmp/a.sol"
expect_exit 1 "--write-basis after an infeasible answer exits 1" \
    "$JAOS" solve "$DATA/t1.mps" --write-basis "$tmp/b.bas"
fi
expect_exit 5 "--basis with a missing file exits 5" \
    "$JAOS" solve "$DATA/solve1.mps" --basis "$tmp/no-such.bas"

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

expect_exit 0 "convert MPS to NL exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1.nl"
[ -f "$tmp/solve1.col" ] && [ -f "$tmp/solve1.row" ] \
    && pass "and writes the .col and .row files beside it" \
    || flunk "no .col or .row beside the written .nl"
expect_exit 0 "the written NL solves" "$JAOS" solve "$tmp/solve1.nl"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line as the MPS" \
    || flunk "NL objective '$(line_of objective)' vs MPS '$mps_objective'"
expect_exit 0 "convert MPS to QPLIB exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1.qplib"
expect_exit 0 "the written QPLIB solves" "$JAOS" solve "$tmp/solve1.qplib"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line as the MPS" \
    || flunk "QPLIB objective '$(line_of objective)' vs MPS '$mps_objective'"
expect_exit 0 "a QP converts to QPLIB and solves" \
    "$JAOS" convert "$DATA/g_quad.lp" "$tmp/gq.qplib"
expect_exit 0 "the QPLIB QP solves" "$JAOS" solve "$tmp/gq.qplib"
case "$(line_of objective)" in
    "objective 4.0000"*|"objective 3.9999"*) pass "to objective 4" ;;
    *) flunk "QPLIB QP objective '$(line_of objective)'" ;;
esac
expect_exit 0 "convert to OSiL exits 0" \
    "$JAOS" convert "$DATA/g_quad.lp" "$tmp/gq.osil"
grep -q '<qTerm idx="-1"' "$tmp/gq.osil" \
    && pass "and the OSiL carries the quadratic term" \
    || flunk "no qTerm in the written OSiL"
expect_exit 0 "the written OSiL solves" "$JAOS" solve "$tmp/gq.osil"
case "$(line_of objective)" in
    "objective 4.0000"*|"objective 3.9999"*) pass "to objective 4" ;;
    *) flunk "OSiL QP objective '$(line_of objective)'" ;;
esac
expect_exit 5 "a QP does not convert to .nl" \
    "$JAOS" convert "$DATA/g_quad.lp" "$tmp/gq.nl"
case "$err" in
    *quadratic*) pass "and says the objective has a quadratic term" ;;
    *) flunk "the refused convert said '$err'" ;;
esac
[ -f "$tmp/gq.nl" ] && flunk "the refused convert left a .nl behind" \
    || pass "and wrote no .nl"
expect_exit 0 "convert OSiL back to MPS exits 0" \
    "$JAOS" convert "$tmp/gq.osil" "$tmp/gq_osil.mps"
expect_exit 0 "the MPS written from OSiL solves" "$JAOS" solve "$tmp/gq_osil.mps"
expect_exit 0 "convert to a compressed OSiL exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1z.osil.gz"
expect_exit 0 "the compressed OSiL solves" "$JAOS" solve "$tmp/solve1z.osil.gz"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line as the MPS" \
    || flunk "OSiL objective '$(line_of objective)' vs MPS '$mps_objective'"
expect_exit 5 "an OSiL with a nonlinear block is refused" \
    "$JAOS" solve "$DATA/e_osil_nonlinear.osil"
expect_exit 5 "convert to an unknown extension is a usage error" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1.xyz"
expect_exit 0 "convert to a compressed NL exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/solve1z.nl.gz"
[ -f "$tmp/solve1z.col" ] \
    && pass "and the names file sits under the name without .gz" \
    || flunk "no solve1z.col beside solve1z.nl.gz"
expect_exit 0 "the compressed NL solves" "$JAOS" solve "$tmp/solve1z.nl.gz"

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

expect_exit 0 "convert to .mps.gz exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/z.mps.gz"
expect_exit 0 "the compressed MPS solves" "$JAOS" solve "$tmp/z.mps.gz"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line" \
    || flunk "gz objective '$(line_of objective)' vs '$mps_objective'"
expect_exit 0 "convert to .lp.gz exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/z.lp.gz"
expect_exit 0 "the compressed LP solves" "$JAOS" solve "$tmp/z.lp.gz"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "and gives the same objective line too" \
    || flunk "gz LP objective '$(line_of objective)' vs '$mps_objective'"
if command -v gzip >/dev/null 2>&1; then
    gzip -t "$tmp/z.mps.gz" 2>/dev/null \
        && pass "the system gzip accepts what JAOS wrote" \
        || flunk "gzip -t refused the file JAOS wrote"
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/z2.mps" >/dev/null 2>&1
    gzip -dc "$tmp/z.mps.gz" > "$tmp/z2.back" 2>/dev/null
    cmp -s "$tmp/z2.mps" "$tmp/z2.back" \
        && pass "and it decompresses to the plain file byte for byte" \
        || flunk "gzip -dc of the .gz differs from the plain write"
fi
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--write-point and --write-duals together" \
    "$JAOS" solve "$DATA/solve1.mps" --write-point "$tmp/wp.pt" \
    --write-duals "$tmp/wd.du"
expect_exit 0 "and check reads both back" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/wp.pt" --duals "$tmp/wd.du"
[ "$(line_of checked_duals)" = "checked_duals yes" ] \
    && [ "$(line_of dual_feasible)" = "dual_feasible yes" ] \
    && pass "with the dual half running and holding" \
    || flunk "check printed '$(line_of checked_duals)' / '$(line_of dual_feasible)'"
fi

expect_exit 0 "--solution takes a .gz name" \
    "$JAOS" solve "$DATA/solve1.mps" --solution "$tmp/z.sol.gz"
[ -s "$tmp/z.sol.gz" ] && pass "and the file is there" \
    || flunk "the .gz solution is missing"
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--write-basis takes one too" \
    "$JAOS" solve "$DATA/solve1.mps" --write-basis "$tmp/z.bas.gz"
[ -s "$tmp/z.bas.gz" ] && pass "and that file is there" \
    || flunk "the .gz basis is missing"
fi

expect_exit 0 "convert --positional exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/pos.lp" --positional
grep -q 'C1' "$tmp/pos.lp" && pass "and the file uses positional names" \
    || flunk "no C1 in the positional file"
grep -q 'X1' "$tmp/pos.lp" && flunk "the positional file kept a real name" \
    || pass "and none of the model's own"
expect_exit 0 "the positional LP solves" "$JAOS" solve "$tmp/pos.lp"
[ "$(line_of objective)" = "$mps_objective" ] \
    && pass "to the same objective, so only the names were lost" \
    || flunk "positional objective '$(line_of objective)'"
expect_exit 0 "convert without it keeps the names" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/named.lp"
grep -q 'X1' "$tmp/named.lp" && pass "and X1 is in the file" \
    || flunk "the plain conversion lost the names"

expect_exit 5 "convert to an unknown extension is a usage error" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/out.txt"
[ ! -e "$tmp/out.txt" ] && pass "and writes nothing" || flunk "out.txt was written"
expect_exit 5 "convert with one argument is a usage error" \
    "$JAOS" convert "$DATA/solve1.mps"
expect_exit 5 "convert of an unreadable input is an error" \
    "$JAOS" convert "$DATA/e_badnum.mps" "$tmp/bad.lp"
[ ! -e "$tmp/bad.lp" ] && pass "and leaves no file" || flunk "bad.lp was written"

expect_exit 5 "a refused LP write exits 5" \
    "$JAOS" convert "$DATA/t3_objname.mps" "$tmp/freerow.lp"
printf '%s\n' "$err" | grep -q "free" && pass "and prints the writer's refusal" \
    || flunk "a refused write said: '$err'"
[ ! -e "$tmp/freerow.lp" ] && pass "and leaves no file" \
    || flunk "a refused write left freerow.lp behind"
expect_exit 0 "the same model converts to MPS" \
    "$JAOS" convert "$DATA/t3_objname.mps" "$tmp/freerow.mps"
expect_exit 0 "a ranged row converts to LP" \
    "$JAOS" convert "$DATA/g_ranged.lp" "$tmp/ranged.lp"
expect_exit 0 "and the written LP solves" "$JAOS" solve "$tmp/ranged.lp"

expect_exit 0 "convert then diff exits 0" \
    "$JAOS" convert "$DATA/solve1.mps" "$tmp/dd.lp"
expect_exit 0 "diff of a model against its own conversion exits 0" \
    "$JAOS" diff "$DATA/solve1.mps" "$tmp/dd.lp"
[ "$(line_of differences)" = "differences 0" ] \
    && pass "and reports no difference" \
    || flunk "diff printed '$(line_of differences)'"
cmp -s "$DATA/solve1.mps" "$tmp/dd.lp" \
    && flunk "the two files are byte-identical, so the test proves nothing" \
    || pass "and the two files are not byte-identical, so it proves something"
expect_exit 1 "diff of two different models exits 1" \
    "$JAOS" diff "$DATA/solve1.mps" "$DATA/t1.mps"
[ "$(line_of differences)" != "differences 0" ] \
    && pass "and says how many differences" \
    || flunk "diff called two different models the same"
expect_exit 5 "diff with one file is a usage error" \
    "$JAOS" diff "$DATA/solve1.mps"
expect_exit 5 "diff of a missing file exits 5" \
    "$JAOS" diff "$DATA/solve1.mps" "$tmp/no-such.mps"

expect_exit 0 "show --row exits 0" \
    "$JAOS" show "$DATA/solve1.mps" --row DEMAND
[ "$(line_of entries)" = "entries 3" ] && pass "and counts the row's terms" \
    || flunk "show --row printed '$(line_of entries)'"
printf '%s\n' "$out" | grep -q '^term X1 ' \
    && pass "and names them by column" || flunk "no 'term X1' line"
expect_exit 0 "show --col exits 0" "$JAOS" show "$DATA/solve1.mps" --col X1
[ -n "$(line_of cost)" ] && pass "and a column carries its cost" \
    || flunk "show --col printed no cost"
printf '%s\n' "$out" | grep -q '^term DEMAND ' \
    && pass "and names its terms by row" || flunk "no 'term DEMAND' line"
expect_exit 5 "show of a name nothing carries exits 5" \
    "$JAOS" show "$DATA/solve1.mps" --row nosuch
expect_exit 5 "show without --row or --col is a usage error" \
    "$JAOS" show "$DATA/solve1.mps"
expect_exit 5 "show with both is a usage error" \
    "$JAOS" show "$DATA/solve1.mps" --row DEMAND --col X1
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

sed 's/^col X1 [^ ]* /col X1 400 /' "$tmp/a.sol" > "$tmp/bad.sol"
grep -q '^col X1 400 ' "$tmp/bad.sol" || flunk "the tampered file was not built"
expect_exit 1 "check of an infeasible answer exits 1" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/bad.sol"
[ "$(line_of primal_feasible)" = "primal_feasible no" ] \
    && pass "it prints 'primal_feasible no'" \
    || flunk "check printed '$(line_of primal_feasible)'"
fi

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

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--write-point writes a point file" \
    "$JAOS" solve "$DATA/solve1.mps" --write-point "$tmp/p.pt"
[ "$(grep -cv '^#' "$tmp/p.pt")" -eq 3 ] \
    && pass "one line per column and nothing else" \
    || flunk "the point file has $(grep -cv '^#' "$tmp/p.pt") records"
expect_exit 0 "check --point judges it" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/p.pt"
[ "$(printf '%s\n' "$out" | head -n 1)" = "status point" ] \
    && pass "and says which reader ran" \
    || flunk "check --point began with '$(printf '%s\n' "$out" | head -n 1)'"
[ "$(line_of primal_feasible)" = "primal_feasible yes" ] \
    && pass "and calls the point feasible" \
    || flunk "check --point printed '$(line_of primal_feasible)'"
[ "$(line_of checked_duals)" = "checked_duals no" ] \
    && pass "and says the duals were not checked" \
    || flunk "check --point printed '$(line_of checked_duals)'"
awk '$1 == "row" { print $2, $4 }' "$tmp/a.sol" > "$tmp/p.du"
expect_exit 0 "check --point --duals runs the dual half too" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/p.pt" --duals "$tmp/p.du"
[ "$(line_of checked_duals)" = "checked_duals yes" ] \
    && pass "and says so" \
    || flunk "check --duals printed '$(line_of checked_duals)'"
[ "$(line_of dual_feasible)" = "dual_feasible yes" ] \
    && pass "and the answer is dual feasible" \
    || flunk "check --duals printed '$(line_of dual_feasible)'"
awk '!/^#/ { print $1, $2 + 5 }' "$tmp/p.pt" > "$tmp/bad.pt"
expect_exit 1 "check --point of a point that is not feasible exits 1" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/bad.pt"
[ "$(line_of primal_feasible)" = "primal_feasible no" ] \
    && pass "and says so" \
    || flunk "check of a bad point printed '$(line_of primal_feasible)'"
head -2 "$tmp/p.pt" > "$tmp/short.pt"
expect_exit 5 "check --point of a short file exits 5" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/short.pt"
[ -n "$err" ] && pass "and names what is missing" \
    || flunk "no message on stderr"
fi
expect_exit 5 "check --point and a solution file together is a usage error" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" --point "$tmp/p.pt"
expect_exit 5 "check --duals without --point is a usage error" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/a.sol" --duals "$tmp/p.du"
expect_exit 5 "check --point with a missing file exits 5" \
    "$JAOS" check "$DATA/solve1.mps" --point "$tmp/no-such.pt"

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
sed 's/^ray \([^ ]*\) .*/ray \1 0/' "$tmp/t1.sol" > "$tmp/t1zero.sol"
expect_exit 1 "check of a zeroed certificate exits 1" \
    "$JAOS" check "$DATA/t1.mps" "$tmp/t1zero.sol"
[ "$(line_of certified)" = "certified no" ] && pass "and says it certifies nothing" \
    || flunk "check printed '$(line_of certified)'"
expect_exit 5 "check of a certificate against another model exits 5" \
    "$JAOS" check "$DATA/solve1.mps" "$tmp/t1.sol"

expect_exit 0 "iis of an infeasible model exits 0" "$JAOS" iis "$DATA/t1.mps"
[ "$(printf '%s\n' "$out" | head -n 1)" = "status infeasible" ] \
    && pass "its first line is the status" \
    || flunk "iis began with '$(printf '%s\n' "$out" | head -n 1)'"
members=$(line_of members | cut -d' ' -f2)
sides=$(printf '%s\n' "$out" | grep -c '^\(row\|col\) [^ ]* \(lower\|upper\)$')
[ -n "$members" ] && [ "$members" -ge 1 ] && [ "$sides" -eq "$members" ] \
    && pass "one side line per member ($members)" \
    || flunk "members=$members but $sides side lines"
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
if [ "$faulty" -eq 0 ]; then
expect_exit 0 "verify of an infeasible model exits 0" \
    "$JAOS" verify "$DATA/t1.mps" --proof "$tmp/t1.proof"
[ "$(line_of certificate)" = "certificate exact" ] \
    && pass "and says the certificate is exact" \
    || flunk "verify printed '$(line_of certificate)'"
for k in bound_bits capacity_bits blocks largest_block terms; do
    [ -n "$(line_of $k)" ] && pass "verify prints a $k line" \
        || flunk "no $k line in: $out"
done
expect_exit 0 "the derived certificate is judged and holds" \
    "$JAOS" check "$DATA/t1.mps" --proof "$tmp/t1.proof"
[ "$(line_of proof)" = "proof holds" ] \
    && pass "and the independent checker says so" \
    || flunk "check printed '$(line_of proof)'"
grep -q '^ray ' "$tmp/t1.proof" \
    && pass "the proof file carries ray records" \
    || flunk "no ray records in the derived proof file"
fi

expect_exit 5 "iis without a file is a usage error" "$JAOS" iis
expect_exit 5 "iis of a missing file exits 5" "$JAOS" iis "$tmp/no-such.mps"

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "iis --write writes the subsystem" \
    "$JAOS" iis "$DATA/t1.mps" --write "$tmp/sub.mps"
[ -n "$(line_of subsystem_file)" ] && pass "and says where" \
    || flunk "no subsystem_file line"
expect_exit 1 "and the file it wrote is infeasible" \
    "$JAOS" solve "$tmp/sub.mps"
[ "$(line_of status)" = "status infeasible" ] \
    && pass "which is what a subsystem has to be" \
    || flunk "the subsystem solved '$(line_of status)'"
expect_exit 0 "iis --write to LP works too" \
    "$JAOS" iis "$DATA/t1.mps" --write "$tmp/sub.lp"
[ "$(line_of subsystem_rows)" = "subsystem_rows 2" ] \
    && pass "and the subsystem has two of the three rows" \
    || flunk "iis --write printed '$(line_of subsystem_rows)'"
expect_exit 1 "and the LP it wrote is infeasible as well" \
    "$JAOS" solve "$tmp/sub.lp"
expect_exit 0 "iis --write --positional exits 0" \
    "$JAOS" iis "$DATA/t1.mps" --write "$tmp/sp.lp" --positional
grep -q 'C1' "$tmp/sp.lp" && pass "and the subsystem uses positional names" \
    || flunk "no C1 in the positional subsystem"
expect_exit 1 "and it is still infeasible" "$JAOS" solve "$tmp/sp.lp"
expect_exit 0 "relax --apply --positional exits 0" \
    "$JAOS" relax "$DATA/t1.mps" --apply "$tmp/rp.lp" --positional
grep -q 'C1' "$tmp/rp.lp" && pass "and the relaxed model uses them too" \
    || flunk "no C1 in the positional relaxation"
expect_exit 0 "and the relaxed model is feasible" "$JAOS" solve "$tmp/rp.lp"
fi
expect_exit 5 "iis --write to an unknown extension is a usage error" \
    "$JAOS" iis "$DATA/t1.mps" --write "$tmp/sub.txt"
[ ! -e "$tmp/sub.txt" ] && pass "and writes nothing" \
    || flunk "sub.txt was written"
expect_exit 5 "iis --write needs a path" "$JAOS" iis "$DATA/t1.mps" --write
expect_exit 5 "iis refuses an unknown option" \
    "$JAOS" iis "$DATA/t1.mps" --bogus
expect_exit 1 "iis --write on an optimal model exits 1" \
    "$JAOS" iis "$DATA/solve1.mps" --write "$tmp/none.mps"
[ ! -e "$tmp/none.mps" ] && pass "and leaves no file" \
    || flunk "none.mps was written"

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
expect_exit 0 "relax --apply exits 0" \
    "$JAOS" relax "$DATA/t1.mps" --apply "$tmp/relaxed.mps"
expect_exit 0 "and the model it wrote solves" \
    "$JAOS" solve "$tmp/relaxed.mps"
[ "$(line_of status)" = "status optimal" ] \
    && pass "to an optimum, where the original was infeasible" \
    || flunk "the relaxed model solved '$(line_of status)'"
expect_exit 1 "the SOS model is infeasible" "$JAOS" solve "$DATA/relax_sos.mps"
expect_exit 0 "relax of it exits 0" "$JAOS" relax "$DATA/relax_sos.mps" --rows
[ "$(line_of total)" = "total 2" ] \
    && pass "and the SOS set costs a move of 2" \
    || flunk "relax of the SOS model printed '$(line_of total)'"
expect_exit 0 "relax --apply on it exits 0" \
    "$JAOS" relax "$DATA/relax_sos.mps" --rows --apply "$tmp/rsos.mps"
expect_exit 0 "and the model it wrote solves" "$JAOS" solve "$tmp/rsos.mps"
[ "$(line_of status)" = "status optimal" ] \
    && pass "to an optimum, the SOS set carried through" \
    || flunk "the relaxed SOS model solved '$(line_of status)'"
expect_exit 5 "relax --apply to a name neither writer takes is a usage error" \
    "$JAOS" relax "$DATA/t1.mps" --apply "$tmp/relaxed.txt"
expect_exit 5 "relax --apply without a path is a usage error" \
    "$JAOS" relax "$DATA/t1.mps" --apply
expect_exit 5 "relax without a file is a usage error" "$JAOS" relax
expect_exit 5 "relax of a missing file exits 5" "$JAOS" relax "$tmp/no-such.mps"
expect_exit 5 "relax with an unknown option is a usage error" \
    "$JAOS" relax "$DATA/t1.mps" --both

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

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "verify of an unbounded model exits 0" \
    "$JAOS" verify "$DATA/unbounded.mps" --proof "$tmp/u.proof"
[ "$(line_of ray)" = "ray exact" ] && pass "and says the ray is exact" \
    || flunk "verify printed '$(line_of ray)'"
expect_exit 0 "the derived ray is judged and holds" \
    "$JAOS" check "$DATA/unbounded.mps" --proof "$tmp/u.proof"
[ "$(line_of proof)" = "proof holds" ] \
    && pass "and the independent checker says so" \
    || flunk "check printed '$(line_of proof)'"
fi
expect_exit 5 "verify without a file is a usage error" "$JAOS" verify

if [ "$faulty" -eq 0 ]; then
expect_exit 0 "--write-basis then verify --basis exits 0" \
    "$JAOS" solve "$DATA/solve1.mps" --write-basis "$tmp/v.bas"
expect_exit 0 "verify --basis proves the solve's own basis" \
    "$JAOS" verify "$DATA/solve1.mps" --basis "$tmp/v.bas"
[ "$(line_of proof)" = "proof optimal" ] && pass "and says optimal" \
    || flunk "verify --basis printed '$(line_of proof)'"
[ -z "$(line_of status)" ] && pass "and never solved the model" \
    || flunk "verify --basis printed a status line, so it solved"
printf 'NAME          SLACK\nENDATA\n' > "$tmp/slack.bas"
expect_exit 1 "verify --basis of the slack basis exits 1" \
    "$JAOS" verify "$DATA/solve1.mps" --basis "$tmp/slack.bas"
[ "$(line_of proof)" = "proof broken" ] && pass "and says broken" \
    || flunk "verify --basis printed '$(line_of proof)'"
expect_exit 0 "verify --basis writes a proof file of an outside basis" \
    "$JAOS" verify "$DATA/solve1.mps" --basis "$tmp/v.bas" \
    --proof "$tmp/v.proof"
expect_exit 0 "and the independent checker judges it" \
    "$JAOS" check "$DATA/solve1.mps" --proof "$tmp/v.proof"
[ "$(line_of proof)" = "proof holds" ] && pass "and it holds" \
    || flunk "check printed '$(line_of proof)'"
fi
expect_exit 5 "verify --basis needs a path" \
    "$JAOS" verify "$DATA/solve1.mps" --basis
expect_exit 5 "verify --basis with a missing file exits 5" \
    "$JAOS" verify "$DATA/solve1.mps" --basis "$tmp/no-such.bas"

if [ "$faulty" -eq 0 ]; then
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
  sed -i "s/^col z 8$/col z 7/" "$P"
  expect_exit 1 "check --proof refuses a moved value" \
      "$JAOS" check "$DATA/g1.lp" --proof "$P"
  [ "$(line_of proof)" = "proof broken" ] && pass "and says so" \
      || flunk "verdict: $(line_of proof)"
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

expect_exit 5 "ranging of a QP exits 5" "$JAOS" ranging "$DATA/g_quad.lp"
case "$err" in
    *quadratic*) pass "and names the quadratic term" ;;
    *) flunk "ranging of a QP said '$err'" ;;
esac
expect_exit 5 "verify of a QP exits 5" "$JAOS" verify "$DATA/g_quad.lp"
case "$err" in
    *quadratic*) pass "and names the quadratic term" ;;
    *) flunk "verify of a QP said '$err'" ;;
esac

expect_exit 5 "ranging of a MIP exits 5" "$JAOS" ranging "$DATA/t4_int.mps"
case "$err" in
    *MIP*) pass "and says the model is a MIP" ;;
    *) flunk "ranging of a MIP said '$err'" ;;
esac
expect_exit 5 "verify of a MIP exits 5" "$JAOS" verify "$DATA/t4_int.mps"
case "$err" in
    *MIP*) pass "and says the model is a MIP" ;;
    *) flunk "verify of a MIP said '$err'" ;;
esac
expect_exit 5 "ranging of an SOS model exits 5" "$JAOS" ranging "$DATA/g_sos.mps"
case "$err" in
    *MIP*) pass "and says the model is a MIP" ;;
    *) flunk "ranging of an SOS model said '$err'" ;;
esac
"$JAOS" verify "$DATA/solve1.mps" --proof "$tmp/lp.proof" > /dev/null 2>&1
expect_exit 5 "an LP's proof is not checked against a MIP" \
    "$JAOS" check "$DATA/t4_int.mps" --proof "$tmp/lp.proof"
case "$err" in
    *MIP*) pass "and says the model is a MIP" ;;
    *) flunk "check --proof against a MIP said '$err'" ;;
esac

if [ "$fail" -ne 0 ]; then
    echo "tests/cli.sh: FAILED"
    exit 1
fi
echo "tests/cli.sh: all checks passed"
