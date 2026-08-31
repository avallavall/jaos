#!/usr/bin/env bash
# The `lu.c` tests of the assert debt, and the proof each FAILS when the
# sentence it states is broken.
#
# `jaos-testing`: a green suite is not evidence until it has been watched
# going red for the right reason. Same shape as 02-137 and 02-139, and the
# same rule about exit codes — a break that trips an assert aborts and prints
# no failure count, so each arm records the test binary's exit status too.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-140/run-lu-controls.sh
# Writes lu-controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-140"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

# Taken from the working tree, so this runs before the change is committed
# as well as after.
CARRY="src/lu.c tests/test_lu.c"

# --------------------------------------------------------------------- #
# The breakers                                                           #
# --------------------------------------------------------------------- #

# Both solves stop refusing to answer from a wrecked factorization. The
# guard is the whole difference between an error and a wrong answer.
# Both anchors carry the line that follows the guard, because the guard
# itself is identical in the two functions and an anchor that matches twice
# is an anchor that cannot be checked (record-check refuses one).
BREAK_RANK_GUARD='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old_ftran = """    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    ftran_prefix(lu, x, y, w);"""
assert s.count(old_ftran) == 1, "ftran guard matched %d" % s.count(old_ftran)
s = s.replace(old_ftran, """
    ftran_prefix(lu, x, y, w);""")
old_btran = """    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    /* B"""
assert s.count(old_btran) == 1, "btran guard matched %d" % s.count(old_btran)
s = s.replace(old_btran, """
    /* B""")
open(p, "w", encoding="utf-8").write(s)
print("  ftran and btran answer from a wrecked factorization")
'

# Only the pattern is left unset, so a caller reading `npat` sees whatever
# it passed in. The buffer itself is still untouched, which is what makes
# this arm narrower than the one above.
BREAK_NPAT='
p = "src/lu.c"
s = open(p, encoding="utf-8").read()
old_ftran = """    if (npat != nullptr)
        *npat = 0;
    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    ftran_prefix(lu, x, y, w);"""
assert s.count(old_ftran) == 1, "ftran npat matched %d" % s.count(old_ftran)
s = s.replace(old_ftran, """    if (lu->rank != n)
        return;

    ftran_prefix(lu, x, y, w);""")
old_btran = """    if (npat != nullptr)
        *npat = 0;
    if (lu->rank != n)
        return;   /* singular, or wrecked by a failed update */

    /* B"""
assert s.count(old_btran) == 1, "btran npat matched %d" % s.count(old_btran)
s = s.replace(old_btran, """    if (lu->rank != n)
        return;

    /* B""")
open(p, "w", encoding="utf-8").write(s)
print("  a wrecked factorization no longer reports an empty pattern")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker source or ""
    # Split, not one `local`: bash expands every word of a `local` before it
    # assigns any of them, so "$D/wt-$tag" would read an unset `tag`.
    local tag=$1 breaker=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/dev/test_lu >/dev/null 2>&1 ) || { echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && timeout 300 ./build/dev/test_lu ) > "$D/$tag.log" 2>&1
    echo $? > "$D/$tag.rc"
    return 0
}

suite_line()  { grep -Eo '[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$1" | tail -1; }
suite_fails() { suite_line "$1" | awk '{print $3}'; }
suite_rc()    { cat "$1" 2>/dev/null; }
red_tests()   { grep -Eo 'test_[a-z_0-9]+:FAIL' "$1" | sed 's/:FAIL//' | sort -u | tr '\n' ' '; }
is_red()      { grep -q "$2:FAIL" "$1" && echo yes || echo no; }

fail=0
out="$here/lu-controls.txt"
{
echo "# 02-140 -- the lu.c tests, and the arm that makes each go red"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description,
              # $4 = a test name that must be red, or "" for a green arm
    local tag=$1 breaker=$2 desc=$3 want=$4
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"
        fail=2
        return
    fi
    {
      echo "   suite:     $(suite_line "$D/$tag.log")  exit=$(suite_rc "$D/$tag.rc")"
      local r
      r="$(red_tests "$D/$tag.log")"
      [ -n "$r" ] && echo "   red tests: $r"
    } >> "$out"

    if [ -z "$want" ]; then
        if [ "$(suite_fails "$D/$tag.log")" = "0" ] && \
           [ "$(suite_rc "$D/$tag.rc")" = "0" ]; then
            echo "   PASS  nothing red, exit 0" >> "$out"
        else
            echo "   FAIL  the intact suite is not green" >> "$out"; fail=1
        fi
    else
        # A break that trips an assert aborts with no summary line, which
        # still counts: the requirement is that the suite does not come back
        # clean, and that this test is named when there is a name to read.
        if [ "$(is_red "$D/$tag.log" "$want")" = "yes" ]; then
            echo "   PASS  $want is red" >> "$out"
        elif [ "$(suite_rc "$D/$tag.rc")" != "0" ] && \
             [ -z "$(suite_line "$D/$tag.log")" ]; then
            echo "   PASS  the suite aborted before printing a count" >> "$out"
        else
            echo "   FAIL  $want did not go red and the suite exited clean" >> "$out"
            fail=1
        fi
    fi
    echo >> "$out"
}

run_arm intact "" "nothing broken: the tree as it stands" ""

run_arm no-rank-guard "$BREAK_RANK_GUARD" \
    "ftran and btran answer from a wrecked factorization" \
    test_a_wrecked_factorization_writes_nothing

run_arm no-npat-reset "$BREAK_NPAT" \
    "a wrecked factorization stops reporting an empty pattern" \
    test_a_wrecked_factorization_writes_nothing

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "lu-controls exit=$fail  ->  $out"
exit $fail
