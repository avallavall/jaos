#!/usr/bin/env bash
# The controls behind D226, as a script instead of a hand-run procedure.
#
# `jaos-testing`: a clean reading from a new instrument means nothing until
# the instrument has been shown able to report a dirty one. Two probes and
# one unit suite were called clean here, so each needs an arm beside it that
# goes red for the right reason.
#
# Five arms. Arm 1 is the positive one. Arms 2 to 4 each break exactly one
# contract and require THAT contract's checks to be the ones that fail. Arm 5
# runs the recipe again with nothing broken, which is what shows the break
# lived in its own worktree and never touched this tree.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-138/run-controls.sh
# Writes controls.txt beside this script. Exit 0 only when every arm
# behaved; 1 when an arm did not; 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-138"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
CC=${CC:-gcc-14}
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

# The three files the writers live in are not in HEAD yet: src/write.c and
# tests/test_write.c are new and include/jaos.h is modified. A worktree at
# HEAD plus these three is the tree being measured.
CARRY="src/write.c tests/test_write.c include/jaos.h"

# The standard set is the population the README's control table quotes.
SET="bench/instances"

# --------------------------------------------------------------------- #
# The three breakers                                                     #
# --------------------------------------------------------------------- #

# `wr_num` stops looking for the shortest form that reads back exactly and
# prints a flat six digits. That is the contract the MPS round trip rests on.
BREAK_DIGITS='
p = "src/write.c"
s = open(p, encoding="utf-8").read()
i = s.index("static void wr_num(char *buf, double v)")
j = s.index("\n}", i) + 2
body = s[i:j]
assert "%.17g" in body, "wr_num body not found"
new = ("static void wr_num(char *buf, double v)\n"
       "{\n"
       "    snprintf(buf, NUM_LEN, \"%.6g\", v);\n"
       "}\n")
open(p, "w", encoding="utf-8").write(s[:i] + new + s[j:])
print("  wr_num prints a flat six digits, no round-trip check, no fallback")
'

# The LP objective goes back to listing only the columns that cost something.
# LP has no COLUMNS section, so this renumbers every zero-cost column.
BREAK_LP_OBJ='
p = "src/write.c"
s = open(p, encoding="utf-8").read()
old = """        for (int64_t j = 0; j < m->num_col; j++) {
            col_name(m, nm, j);
            lp_term(w, &col, &first, m->col_cost[j], nm);
        }"""
assert s.count(old) == 1, "objective loop matched %d times" % s.count(old)
new = """        for (int64_t j = 0; j < m->num_col; j++) {
            if (m->col_cost[j] == 0.0) continue;
            col_name(m, nm, j);
            lp_term(w, &col, &first, m->col_cost[j], nm);
        }"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the LP objective skips a zero cost again")
'

# `jaos_write_solution` stops checking that the answer is finite. `wr_num`
# asserts its argument reads back and a NaN does not, so this break shows up
# as an abort rather than as a failure count. That is why every arm records
# the test binary's exit code and not only Unity's summary line.
BREAK_SOL_FINITE='
p = "src/write.c"
s = open(p, encoding="utf-8").read()
old = "    if (!isfinite(m->objective))"
assert s.count(old) == 1, "guard matched %d times" % s.count(old)
i = s.index(old)
tail = "    if (w->st != JAOS_OK)\n        return w->st;\n"
j = s.index(tail, i) + len(tail)
open(p, "w", encoding="utf-8").write(s[:i] + s[j:])
print("  jaos_write_solution writes whatever the solve left behind")
'

# --------------------------------------------------------------------- #
# One arm: a worktree, optionally broken, and the three checks on it     #
# --------------------------------------------------------------------- #

arm() {   # $1 = tag, $2 = breaker source or ""
    # Split, not one `local`: bash expands every word of a `local`
    # before it assigns any of them, so "$D/wt-$tag" would read an
    # unset `tag` when arm() is called from the top level.
    local tag=$1 breaker=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    # The instance sets are gitignored, so a fresh worktree has none.
    ln -s "$JAOS_ROOT/$SET" "$wt/$SET" || return 2
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/dev/test_write >/dev/null 2>&1 ) || { echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && timeout 300 ./build/dev/test_write ) > "$D/$tag.suite" 2>&1
    echo $? > "$D/$tag.rc"

    local objs
    objs=$(ls "$wt"/build/dev/*.o | grep -v unity)
    ( cd "$wt" && $CC -std=c23 -Wall -Wextra -ffp-contract=off -g -O1 \
        -Iinclude -Isrc "$here/roundtrip.c" $objs -o build/dev/roundtrip -lm \
        && $CC -std=c23 -Wall -Wextra -ffp-contract=off -g -O1 \
        -Iinclude -Isrc "$here/lpcover.c" $objs -o build/dev/lpcover -lm \
    ) >/dev/null 2>&1 || { echo "  PROBE BUILD FAILED"; return 2; }

    ( cd "$wt" && timeout 900 ./build/dev/roundtrip $SET/*.mps ) > "$D/$tag.rt" 2>&1
    ( cd "$wt" && timeout 900 ./build/dev/lpcover   $SET/*.mps ) > "$D/$tag.lp" 2>&1
    return 0
}

# Unity ends with "N Tests M Failures K Ignored".
suite_line()  { grep -Eo '[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$1" | tail -1; }
suite_fails() { suite_line "$1" | awk '{print $3}'; }
suite_total() { suite_line "$1" | awk '{print $1}'; }
# Unity exits with its failure count; a test that aborts exits 134 and prints
# no summary line at all, so this is the predicate that covers both.
suite_rc()    { cat "$1" 2>/dev/null; }
red_tests()   { grep -Eo 'test_[a-z_0-9]+:FAIL' "$1" | sed 's/:FAIL//' | sort -u | tr '\n' ' '; }
rt_line()     { grep -E 'round-tripped exactly' "$1" | tail -1; }
lp_line()     { grep -E 'round-tripped through LP' "$1" | tail -1; }
rt_ok()       { rt_line "$1" | awk '{print $1}'; }
lp_diff()     { lp_line "$1" | sed -E 's/.*refused, ([0-9]+) differed.*/\1/'; }

fail=0
out="$here/controls.txt"
NINST=$(ls $SET/*.mps 2>/dev/null | wc -l)
{
echo "# D226 controls -- the four checks, and the arm that makes each go red"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the uncommitted $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# set:  $SET ($NINST instances)"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = one-line description
    local tag=$1 breaker=$2 desc=$3
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"
        return 2
    fi
    {
      echo "   suite:     $(suite_line "$D/$tag.suite")  exit=$(suite_rc "$D/$tag.rc")"
      local r
      r="$(red_tests "$D/$tag.suite")"
      [ -n "$r" ] && echo "   red tests: $r"
      echo "   roundtrip: $(rt_line "$D/$tag.rt")"
      echo "   lpcover:   $(lp_line "$D/$tag.lp")"
      echo
    } >> "$out"
    return 0
}

# --------------------------------------------------------------------- #
run_arm intact "" "nothing broken: the tree as it stands" || fail=2

run_arm six-digits "$BREAK_DIGITS" \
    "wr_num cut to a flat six digits" || fail=2

run_arm lp-skips-zero "$BREAK_LP_OBJ" \
    "the LP objective lists only the costed columns" || fail=2

run_arm no-finite-check "$BREAK_SOL_FINITE" \
    "jaos_write_solution stops checking the answer is finite" || fail=2

run_arm restored "" "the recipe again with nothing broken" || fail=2

# --------------------------------------------------------------------- #
# What each arm had to do                                                #
# --------------------------------------------------------------------- #
check() {   # $1 = description, $2 = actual, $3 = relation, $4 = expected
    local ok=1
    case $3 in
        eq) [ "$2" = "$4" ] || ok=0 ;;
        lt) { [ -n "$2" ] && [ "$2" -lt "$4" ]; } 2>/dev/null || ok=0 ;;
        gt) { [ -n "$2" ] && [ "$2" -gt "$4" ]; } 2>/dev/null || ok=0 ;;
    esac
    if [ $ok -eq 1 ]; then
        echo "   PASS  $1 (got '$2')"
    else
        echo "   FAIL  $1 -- expected $3 '$4', got '$2'"
        fail=1
    fi
}

{
echo "== verdict"
N=$(suite_total "$D/intact.suite")
check "intact: the suite is green"              "$(suite_fails "$D/intact.suite")" eq 0
check "intact: the suite exits clean"           "$(suite_rc "$D/intact.rc")"       eq 0
check "intact: every instance round-trips MPS"  "$(rt_ok "$D/intact.rt")"          eq "$NINST"
check "intact: no LP file reads back different" "$(lp_diff "$D/intact.lp")"        eq 0

check "six digits: the suite exits dirty"       "$(suite_rc "$D/six-digits.rc")"       gt 0
check "six digits: the MPS round trip breaks"   "$(rt_ok "$D/six-digits.rt")"          lt "$(rt_ok "$D/intact.rt")"

check "lp skips zero: the suite exits dirty"    "$(suite_rc "$D/lp-skips-zero.rc")"       gt 0
check "lp skips zero: LP files read back wrong" "$(lp_diff "$D/lp-skips-zero.lp")"        gt 0
check "lp skips zero: MPS is untouched"         "$(rt_ok "$D/lp-skips-zero.rt")"          eq "$(rt_ok "$D/intact.rt")"

check "no finite check: the suite exits dirty"  "$(suite_rc "$D/no-finite-check.rc")" gt 0
check "no finite check: MPS is untouched"       "$(rt_ok "$D/no-finite-check.rt")"    eq "$(rt_ok "$D/intact.rt")"

check "restored: the suite is green again"      "$(suite_fails "$D/restored.suite")" eq 0
check "restored: the suite exits clean"         "$(suite_rc "$D/restored.rc")"       eq 0
check "restored: the same test count as intact" "$(suite_total "$D/restored.suite")" eq "$N"
check "restored: the MPS round trip is whole"   "$(rt_ok "$D/restored.rt")"          eq "$(rt_ok "$D/intact.rt")"
echo
if [ $fail -eq 0 ]; then echo "every arm behaved"
else echo "AT LEAST ONE ARM DID NOT BEHAVE"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "controls exit=$fail  ->  $out"
exit $fail
