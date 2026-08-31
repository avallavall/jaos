#!/usr/bin/env bash
# The `model.c` tests of the assert debt, and the proof each FAILS when the
# sentence it states is broken.
#
# `jaos-testing`: a green suite is not evidence until it has been watched
# going red for the right reason. Same shape as 02-137, 02-139 and 02-140.
#
# Five breakers. Two of them are the same defect from opposite sides, and
# that pair is the point of this directory: `model_matrix_is_stale` exists so
# the list of operations that discard the derived copies cannot drift from
# the list that must not (D219), and a test is only worth having if it fails
# both ways round.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-141/run-model-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-141"
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

CARRY="src/model.c src/scale.c tests/test_model.c"

# --------------------------------------------------------------------- #
# The breakers                                                           #
# --------------------------------------------------------------------- #

# One matrix operation stops discarding the derived copies. A stale row-wise
# mirror is a solve against a matrix the caller no longer has.
BREAK_STALE_KEPT='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = """static void model_matrix_is_stale(jaos_model *m)
{
    m->rowwise_valid = false;
    m->scale_valid = false;"""
assert s.count(old) == 1, "invalidator matched %d times" % s.count(old)
new = """static void model_matrix_is_stale(jaos_model *m)
{
    m->scale_valid = false;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a matrix change keeps the row-wise mirror")
'

# The other side: a bound change starts discarding them, which is correct in
# the sense of never being wrong and costs a re-scale on every branch of a
# branch-and-bound loop. The test has to catch this too or it is only half a
# test.
BREAK_BOUND_DISCARDS='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = """static void model_answer_is_stale(jaos_model *m)
{"""
assert s.count(old) == 1, "answer invalidator matched %d times" % s.count(old)
new = """static void model_answer_is_stale(jaos_model *m)
{
    m->rowwise_valid = false;
    m->scale_valid = false;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a bound or a cost throws away both derived copies")
'

# The scaling starts reading a cost. This is the defect the byte-identical
# comparison exists for: the factors become a function of data that a bound
# change moves, and every "the scaling survives a bound change" argument
# stops holding.
BREAK_SCALE_READS_COST='
p = "src/scale.c"
s = open(p, encoding="utf-8").read()
# After the factors are computed, not at `scale_valid = true` -- that line
# runs BEFORE the computation and a break placed there is overwritten. The
# first version of this arm was placed there, came back green, and read
# exactly like a passing control (D229).
old = """    if (st != JAOS_OK) {
        /* A usable identity scaling beats a half-computed one. */
        identity_fill(m);
        jm_set_err(m, "out of memory while scaling");
    }"""
assert s.count(old) == 1, "dispatch tail matched %d times" % s.count(old)
new = old + """
    /* THROWAWAY BREAK: doubling keeps every factor a power of two, so the
     * assert below still holds and only the test can catch this. */
    for (int64_t j = 0; j < m->num_col; j++)
        if (m->col_cost[j] != 0.0)
            m->col_scale[j] = m->col_scale[j] * 2.0;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the scaling reads a cost")
'

# Deleting rows starts refusing to leave a column empty.
BREAK_EMPTY_COLUMN='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = "jaos_status jaos_delete_rows(jaos_model *m, int64_t num_del,"
assert s.count(old) == 1, "delete_rows matched %d times" % s.count(old)
i = s.index(old)
j = s.index("\n{", i) + 2
guard = """
    /* THROWAWAY BREAK: refuse to empty a column. */
    if (m != nullptr && rows != nullptr && num_del > 0)
        for (int64_t c = 0; c < m->num_col; c++)
            if (m->a_start[c + 1] - m->a_start[c] == 1)
                return JAOS_ERR_INVALID_INPUT;
"""
open(p, "w", encoding="utf-8").write(s[:j] + guard + s[j:])
print("  jaos_delete_rows refuses to leave a column with no entries")
'

# A column stops being kept in ascending row order after an insertion.
BREAK_COLUMN_ORDER='
p = "src/model.c"
s = open(p, encoding="utf-8").read()
old = "jaos_status jaos_add_cols(jaos_model *m, int64_t num_new,"
assert s.count(old) == 1, "add_cols matched %d times" % s.count(old)
i = s.index(old)
j = s.rindex("return JAOS_OK;", i, s.index("\njaos_status", i + 10))
flip = """/* THROWAWAY BREAK: reverse the last column, so it is no longer ascending. */
    if (m->num_col > 0) {
        int64_t a = m->a_start[m->num_col - 1], b = m->a_start[m->num_col] - 1;
        while (a < b) {
            int64_t ti = m->a_index[a]; m->a_index[a] = m->a_index[b];
            m->a_index[b] = ti;
            double tv = m->a_value[a]; m->a_value[a] = m->a_value[b];
            m->a_value[b] = tv;
            a++; b--;
        }
    }
    """
open(p, "w", encoding="utf-8").write(s[:j] + flip + s[j:])
print("  jaos_add_cols leaves the new column in descending row order")
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
      make build/dev/test_model > "$D/$tag.build" 2>&1 ) || {
        echo "  BUILD FAILED:"; grep -E 'error' "$D/$tag.build" | head -5
        return 2; }
    ( cd "$wt" && timeout 300 ./build/dev/test_model ) > "$D/$tag.log" 2>&1
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
echo "# 02-141 -- the model.c tests, and the arm that makes each go red"
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

run_arm mirror-kept "$BREAK_STALE_KEPT" \
    "a matrix change keeps the row-wise mirror" \
    test_only_a_matrix_change_discards_the_derived_copies

run_arm bound-discards "$BREAK_BOUND_DISCARDS" \
    "a bound or a cost throws both derived copies away" \
    test_only_a_matrix_change_discards_the_derived_copies

run_arm scale-reads-cost "$BREAK_SCALE_READS_COST" \
    "the scaling reads a cost" \
    test_the_scaling_does_not_read_a_bound_or_a_cost

run_arm no-empty-column "$BREAK_EMPTY_COLUMN" \
    "jaos_delete_rows refuses to leave a column empty" \
    test_a_column_left_empty_by_delete_rows_is_not_an_error

run_arm unsorted-column "$BREAK_COLUMN_ORDER" \
    "jaos_add_cols leaves a column in descending order" \
    test_column_order_survives_a_chain_of_mutations

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "model-controls exit=$fail  ->  $out"
exit $fail
