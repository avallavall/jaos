#!/usr/bin/env bash
# The five checker tests of the assert debt, and the proof each FAILS when
# the sentence it states is broken.
#
# `jaos-testing`: a green suite is not evidence until it has been watched
# going red for the right reason. Same shape as 02-137's arms and the same
# rule.
#
# Five breakers and six arms, because one breaker is run twice. Each breaks
# exactly the contract its test states. Where two tests read the same
# machinery the overlap is recorded rather than hidden: an arm passes when
# its OWN test is red, and the file lists every test that moved with it.
#
# `certified_step`'s clamp is the one worth reading, and it is run twice for
# a reason. With asserts on, removing the clamp does not fail a test, it
# ABORTS the suite -- so every arm records the test binary's exit code and
# not only Unity's failure count, because an abort prints no count. With
# `-DNDEBUG`, which is the shipping build, the same break leaves the suite
# GREEN. That is the measurement: the assert is the only thing enforcing
# that sentence, and the test beside it pins the reported value rather than
# catching the defect.
#
# Not a gate tool. Run from the repository root:
#   bash bench/measurements/02-139/run-check-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-139"
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

# The files the tests and the code under test live in, taken from the
# working tree so this runs before the change is committed as well as after.
CARRY="src/check.c tests/test_check.c"

# --------------------------------------------------------------------- #
# The five breakers                                                      #
# --------------------------------------------------------------------- #

# 1. The exemption stops waiving only the condition and starts dropping the
#    term as well. This is the D22 defect.
BREAK_EXEMPT='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """        const double c = w * lo;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
new = """        if (negligible) return 0.0;
        const double c = w * lo;
        jm_obj_add(&a->dual_obj, &a->dual_objc, c);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a negligible multiplier no longer contributes w * bound")
'

# 2. `note_dropped` gains the magnitude exemption D47 refused.
BREAK_NOTE_DROPPED='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """static void note_dropped(dual_acc *a, double w)
{
    a->dropped_n++;"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
new = """static void note_dropped(dual_acc *a, double w)
{
    if (fabs(w) <= 1e-12) return;
    a->dropped_n++;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  note_dropped ignores a multiplier under 1e-12")
'

# 3. `certified_step` stops clamping its room at zero, so a point sitting a
#    tolerance outside a row bound yields a negative distance.
BREAK_CLAMP='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """            limit = (room > 0.0 ? room : 0.0) / per_t;"""
assert s.count(old) == 1, "upper-branch clamp matched %d times" % s.count(old)
s = s.replace(old, """            limit = room / per_t;""")
old2 = """            limit = (room > 0.0 ? room : 0.0) / -per_t;"""
assert s.count(old2) == 1, "lower-branch clamp matched %d times" % s.count(old2)
s = s.replace(old2, """            limit = room / -per_t;""")
open(p, "w", encoding="utf-8").write(s)
print("  certified_step returns whatever room is, negative included")
'

# 4. The implied bound comes out one unit too tight, so the box no longer
#    contains every feasible point.
BREAK_IMPLIED_TIGHT='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """                const double lim = (m->row_lower[i] - rest_up) / aij;"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
new = """                const double lim = (m->row_lower[i] - rest_up) / aij + 1.0;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the implied lower bound is one unit tighter than the row implies")
'

# 5. The infinite terms stop being counted, so a row carrying one of them
#    can no longer have its own share taken out.
BREAK_INF_COUNT='
p = "src/check.c"
s = open(p, encoding="utf-8").read()
old = """            if (isfinite(t_lo)) {
                const double p = aij * t_lo;
                const double e = jm_two_product_residue(aij, t_lo, p);
                jm_obj_add(&lo_sum[i], &lo_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&lo_sum[i], &lo_comp[i], e);
            } else {
                lo_inf[i]++;
            }
            if (isfinite(t_up)) {
                const double p = aij * t_up;
                const double e = jm_two_product_residue(aij, t_up, p);
                jm_obj_add(&up_sum[i], &up_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&up_sum[i], &up_comp[i], e);
            } else {
                up_inf[i]++;
            }"""
assert s.count(old) == 1, "anchor matched %d times" % s.count(old)
new = """            if (isfinite(t_lo)) {
                const double p = aij * t_lo;
                const double e = jm_two_product_residue(aij, t_lo, p);
                jm_obj_add(&lo_sum[i], &lo_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&lo_sum[i], &lo_comp[i], e);
            }
            if (isfinite(t_up)) {
                const double p = aij * t_up;
                const double e = jm_two_product_residue(aij, t_up, p);
                jm_obj_add(&up_sum[i], &up_comp[i], p);
                if (e != 0.0)
                    jm_obj_add(&up_sum[i], &up_comp[i], e);
            }"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  an infinite term is neither summed nor counted")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker source or "", $3 = EXTRA_CFLAGS or ""
    # Split, not one `local`: bash expands every word of a `local`
    # before it assigns any of them, so "$D/wt-$tag" would read an
    # unset `tag` when arm() is called from the top level.
    local tag=$1 breaker=$2 extra=${3:-}
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      if [ -n "$extra" ]; then make build/dev/test_check EXTRA_CFLAGS="$extra" >/dev/null 2>&1
      else make build/dev/test_check >/dev/null 2>&1; fi )         || { echo "  BUILD FAILED"; return 2; }
    ( cd "$wt" && timeout 300 ./build/dev/test_check ) > "$D/$tag.log" 2>&1
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
echo "# 02-139 -- the five checker tests, and the arm that makes each go red"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description, $4 = expectation,
              # $5 = EXTRA_CFLAGS. $4 is a test name, or "" for a green arm,
              # or ABORT for one the assert stops.
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
        # the intact arm: nothing may be red and the binary must exit clean
        if [ "$(suite_fails "$D/$tag.log")" = "0" ] && \
           [ "$(suite_rc "$D/$tag.rc")" = "0" ]; then
            echo "   PASS  nothing red, exit 0" >> "$out"
        else
            echo "   FAIL  the intact suite is not green" >> "$out"; fail=1
        fi
    elif [ "$want" = "ABORT" ]; then
        # the clamp arm: the assert fires, so there is no failure count
        if [ "$(suite_rc "$D/$tag.rc")" != "0" ] && \
           [ -z "$(suite_line "$D/$tag.log")" ]; then
            echo "   PASS  the suite aborted before printing a count" >> "$out"
        else
            echo "   FAIL  expected an abort with no summary line" >> "$out"; fail=1
        fi
    else
        if [ "$(is_red "$D/$tag.log" "$want")" = "yes" ]; then
            echo "   PASS  $want is red" >> "$out"
        else
            echo "   FAIL  $want did not go red" >> "$out"; fail=1
        fi
    fi
    echo >> "$out"
}

run_arm intact "" "nothing broken: the tree as it stands" ""

run_arm exempt-drops-term "$BREAK_EXEMPT" \
    "an exempt multiplier stops contributing w * bound" \
    test_an_exempt_multiplier_still_moves_the_dual_objective

run_arm dropped-exemption "$BREAK_NOTE_DROPPED" \
    "note_dropped ignores anything under 1e-12" \
    test_a_dropped_term_has_no_magnitude_exemption

run_arm no-clamp "$BREAK_CLAMP" \
    "certified_step stops clamping its room at zero" \
    ABORT

run_arm implied-too-tight "$BREAK_IMPLIED_TIGHT" \
    "the implied lower bound is one unit too tight" \
    test_the_implied_box_is_exactly_what_the_row_implies

run_arm infinity-not-counted "$BREAK_INF_COUNT" \
    "an infinite term stops being counted" \
    test_an_infinite_term_is_counted_not_summed

# The clamp arm again with the asserts compiled out, which is the shipping
# build. If the suite is green here, the assert is the ONLY thing enforcing
# that sentence and the test beside it pins a reported value rather than
# catching the defect. That is a fact about the code and this is where it
# gets measured instead of assumed.
run_arm no-clamp-ndebug "$BREAK_CLAMP"     "the same break, with the asserts compiled out"     "" -DNDEBUG

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "check-controls exit=$fail  ->  $out"
exit $fail
