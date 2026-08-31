#!/usr/bin/env bash
# The `jaos_internal.h` tests of the assert debt, and the proof each FAILS
# when the sentence it states is broken.
#
# `jaos-testing`: a green suite is not evidence until it has been watched
# going red for the right reason. Same shape as 02-137, 02-139, 02-140 and
# 02-141, and the same rule about exit codes — one of the breaks below trips
# an assert rather than failing a test, and an abort prints no count.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-142/run-internal-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-142"
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

CARRY="src/simplex.c tests/test_simplex.c"

# --------------------------------------------------------------------- #
# The breakers                                                           #
# --------------------------------------------------------------------- #

# `jm_pattern_order` stops clearing the word it read, so the bitmap comes
# back dirty. The next caller then gets a position its own input never named.
# On a build with asserts this trips the entry check rather than failing a
# test, which is itself worth recording.
BREAK_LEAVES_MARKS='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        if (bits == 0)
            continue;
        mark[w] = 0;"""
assert s.count(old) == 1, "read-back clear matched %d times" % s.count(old)
new = """        if (bits == 0)
            continue;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  jm_pattern_order leaves its bitmap dirty")
'

# The range test goes off by one, so a position exactly at `limit` is kept.
# `limit` bounds the positions, so this is the classic form of the mistake.
BREAK_RANGE='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        if (p < 0 || p >= limit)
            continue;"""
assert s.count(old) == 1, "range test matched %d times" % s.count(old)
new = """        if (p < 0 || p > limit)
            continue;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  jm_pattern_order keeps a position at limit")
'

# `jm_nonbasic_insert` picks the wrong word. Bit 64 is the first that
# straddles, which is why the test moves variables 64 and 65.
BREAK_NONBASIC_WORD='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """void jm_nonbasic_insert(uint64_t *mark, int64_t v)
{
    mark[v >> 6] |= UINT64_C(1) << (v & 63);
}"""
assert s.count(old) == 1, "insert matched %d times" % s.count(old)
new = """void jm_nonbasic_insert(uint64_t *mark, int64_t v)
{
    mark[v >> 7] |= UINT64_C(1) << (v & 63);
}"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  jm_nonbasic_insert writes the wrong word")
'

# The iteration split stops being written, so every exit reports the zero the
# counters were initialised to. That is the shape D194 published.
BREAK_SPLIT_UNWRITTEN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    m->solve_primal_iters = s.n_primal_iters;
    m->solve_phase1_iters = s.n_phase1_iters;"""
assert s.count(old) == 1, "split write matched %d times" % s.count(old)
open(p, "w", encoding="utf-8").write(s.replace(old, ""))
print("  the primal iteration split is never written back")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker source or ""
    # Split, not one `local`: bash expands every word of a `local` before it
    # assigns any of them, so "$D/wt-$tag" would read an unset `tag`.
    local tag=$1 breaker=$2 extra=${3:-}
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      if [ -n "$extra" ]; then
          make build/dev/test_simplex EXTRA_CFLAGS="$extra" > "$D/$tag.build" 2>&1
      else
          make build/dev/test_simplex > "$D/$tag.build" 2>&1
      fi ) || {
        echo "  BUILD FAILED:"; grep -E ' error' "$D/$tag.build" | head -5
        return 2; }
    ( cd "$wt" && timeout 600 ./build/dev/test_simplex ) > "$D/$tag.log" 2>&1
    echo $? > "$D/$tag.rc"
    return 0
}

suite_line()  { grep -Eo '[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$1" | tail -1; }
suite_fails() { suite_line "$1" | awk '{print $3}'; }
suite_rc()    { cat "$1" 2>/dev/null; }
red_tests()   { grep -Eo 'test_[a-z_0-9]+:FAIL' "$1" | sed 's/:FAIL//' | sort -u | tr '\n' ' '; }
is_red()      { grep -q "$2:FAIL" "$1" && echo yes || echo no; }

fail=0
out="$here/controls.txt"
{
echo "# 02-142 -- the jaos_internal.h tests, and the arm that makes each go red"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description,
              # $4 = a test name that must be red, or "" for a green arm
    local tag=$1 breaker=$2 desc=$3 want=$4 extra=${5:-}
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker" "$extra"
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

run_arm dirty-bitmap "$BREAK_LEAVES_MARKS" \
    "jm_pattern_order leaves its bitmap dirty" \
    test_pattern_order_sorts_deduplicates_and_leaves_no_marks

run_arm off-by-one-range "$BREAK_RANGE" \
    "jm_pattern_order keeps a position at limit" \
    test_pattern_order_sorts_deduplicates_and_leaves_no_marks

run_arm wrong-word "$BREAK_NONBASIC_WORD" \
    "jm_nonbasic_insert writes the wrong word" \
    test_the_nonbasic_bitmap_matches_a_rebuild_after_a_basis_change

run_arm split-unwritten "$BREAK_SPLIT_UNWRITTEN" \
    "the primal iteration split is never written back" \
    test_the_iteration_split_is_written_on_an_interrupted_exit

# Both jm_pattern_order breaks trip an assert rather than failing a test, so
# the suite aborts and prints no count. That is a passing control and only
# half an answer: it says the assert fires, not that anything catches the
# defect where asserts are compiled out. D227 established that running the
# same breaker under -DNDEBUG is what turns the half into a measurement, and
# -DNDEBUG is what RELEASE_CFLAGS carries.
run_arm dirty-bitmap-ndebug "$BREAK_LEAVES_MARKS"     "the same break with the asserts compiled out"     test_pattern_order_sorts_deduplicates_and_leaves_no_marks -DNDEBUG

run_arm off-by-one-ndebug "$BREAK_RANGE"     "the same break with the asserts compiled out"     test_pattern_order_sorts_deduplicates_and_leaves_no_marks -DNDEBUG

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "internal-controls exit=$fail  ->  $out"
exit $fail
