#!/usr/bin/env bash
# The four scratch-and-bitmap asserts of D234, and the edit that fires each.
#
# An arm passes only when the log carries the exact expression glibc prints
# for that assert, and the record names the binary that printed it. D232 is
# why: an arm that reads the exit code alone credits an assert with a catch a
# double free made.
#
# The `nbmark` arm needs the probe rather than the suites, because the walk it
# moves used to sit in `dual_ratio_test` and the point of moving it is the
# primal paths.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-147/run-scratch-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-147"
probe="$JAOS_ROOT/bench/measurements/02-145/probe.c"
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

CC=gcc-14
CARRY="src/simplex.c"
PROBE_SET="afiro sc50b adlittle blend share2b lotfi boeing2 bore3d etamacro"

# --------------------------------------------------------------------- #

# The logical column`s slot is written but never recorded, so the pattern
# misses a nonzero it is supposed to name.
BREAK_ALPHA_PATTERN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        int64_t lg = s->ncol + i;
        if (np < cap)
            s->apat[np] = lg;
        np++;
        s->alpha[lg] = -w;"""
assert s.count(old) == 1, "the logical write matched %d times" % s.count(old)
new = """        int64_t lg = s->ncol + i;
        s->alpha[lg] = -w;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the logical slot is not recorded in apat")
'

# The row pattern loses its last entry, the classic off-by-one on a count
# that came back from a helper.
BREAK_RHO_PATTERN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        s->nrpat = jm_pattern_order(nr, s->rpat, s->rmark, s->nrow, &words);"""
assert s.count(old) == 1, "the rho order matched %d times" % s.count(old)
new = """        s->nrpat = jm_pattern_order(nr, s->rpat, s->rmark, s->nrow, &words) - 1;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the rho pattern drops its last entry")
'

# The incremental clear stops one short, so a phase-1 cost survives on a
# variable that is feasible now.
BREAK_C1_CLEAR='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    for (int64_t k = 0; k < cleared; k++)
        s->c1[s->c1_at[k]] = 0.0;"""
assert s.count(old) == 1, "the c1 clear matched %d times" % s.count(old)
new = """    for (int64_t k = 0; k < cleared - 1; k++)
        s->c1[s->c1_at[k]] = 0.0;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the c1 clear stops one short")
'

# The leaving variable leaves the basis without entering the bitmap. This is
# the defect class D223 named and could only see on the dual path.
BREAK_NBMARK='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    /* The bitmap moves with the status, on the same lines. */
    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    jm_nonbasic_insert(s->nbmark, leaving);"""
assert s.count(old) == 1, "the pivot bitmap matched %d times" % s.count(old)
new = """    /* The bitmap moves with the status, on the same lines. */
    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  pivot leaves the bitmap behind the status")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker or "", $3 = wanted text or ""
    local tag=$1 breaker=$2 want=$3
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances" || return 2
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi

    : > "$D/$tag.log"
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/dev/test_simplex build/dev/test_lp ) > "$D/$tag.build" 2>&1 || {
        echo "  BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }

    local t
    for t in test_simplex test_lp; do
        echo "-- $t" >> "$D/$tag.log"
        ( cd "$wt" && timeout 900 "./build/dev/$t" ) >> "$D/$tag.log" 2>&1
        echo "-- $t exit=$?" >> "$D/$tag.log"
    done
    if [ -n "$want" ] && grep -qF "$want" "$D/$tag.log"; then
        echo "-- probe skipped: the suites already fired it" >> "$D/$tag.log"
        return 0
    fi
    ( cd "$wt" && $CC -std=c23 -Wall -Wextra -ffp-contract=off -Og -g \
        -Iinclude -Isrc "$probe" \
        $(ls build/dev/*.o | grep -v '/unity\.o$') -o probe -lm \
      ) >> "$D/$tag.build" 2>&1 || {
        echo "  PROBE BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }
    echo "-- probe" >> "$D/$tag.log"
    # shellcheck disable=SC2086
    ( cd "$wt" && timeout 1800 ./probe $PROBE_SET ) >> "$D/$tag.log" 2>&1
    echo "-- probe exit=$?" >> "$D/$tag.log"
    return 0
}

suite_lines() { grep -Eo '[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$1" | tr '\n' '|'; }
all_exits()   { grep -Eo -- '-- [a-z_]+ exit=[0-9]+' "$1" | tr '\n' '|'; }
# The FUNCTION glibc prints, not just the expression: `price_all`s assert and
# `build_pricing_row`s are the same text, and a record that cannot tell them
# apart is a record of the wrong thing.
fired_line()  { grep -Eo '[a-z_0-9]+: Assertion .*failed' "$1" | head -1; }
fired_in()    { awk '/^-- /{sec=$2} /Assertion/{print sec; exit}' "$1"; }

fail=0
out="$here/controls.txt"
{
echo "# 02-147 -- the four scratch asserts of D234, and what fires each"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copy of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# an arm passes only on the exact expression glibc prints"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description, $4 = want or ""
    local tag=$1 breaker=$2 desc=$3 want=$4
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker" "$want"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"
        fail=2; echo >> "$out"; return
    fi
    {
      echo "   suites:    $(suite_lines "$D/$tag.log")"
      echo "   exits:     $(all_exits "$D/$tag.log")"
      local a
      a="$(fired_line "$D/$tag.log")"
      [ -n "$a" ] && echo "   fired:     $a"
      [ -n "$a" ] && echo "   fired in:  $(fired_in "$D/$tag.log")"
    } >> "$out"
    if [ -z "$want" ]; then
        if ! grep -q 'Assertion' "$D/$tag.log" && \
           [ "$(grep -Eco '[0-9]+ Tests [1-9][0-9]* Failures' "$D/$tag.log")" = "0" ] && \
           [ "$(grep -Eco -- '-- [a-z_]+ exit=[1-9]' "$D/$tag.log")" = "0" ]; then
            echo "   PASS  nothing fired, both suites and the probe clean" >> "$out"
        else
            echo "   FAIL  this arm was meant to be quiet" >> "$out"; fail=1
        fi
    else
        if grep -qF "$want" "$D/$tag.log"; then
            echo "   PASS  the assert fired" >> "$out"
        else
            echo "   FAIL  nothing fired the assert" >> "$out"; fail=1
        fi
    fi
    echo >> "$out"
}

run_arm intact "" "nothing broken: the tree as it stands" ""

# `alpha`s assert and `rho`s are the same expression, so the wanted text has
# to carry the FUNCTION glibc prints before it. Without that, either arm
# passes on the other one firing, which is the same mistake as reading an
# exit code and calling it an assert (D232).
run_arm alpha-pattern "$BREAK_ALPHA_PATTERN" \
    "the logical slot is written but never recorded in apat" \
    "price_all: Assertion"

run_arm rho-pattern "$BREAK_RHO_PATTERN" \
    "the rho pattern drops its last entry" \
    "build_pricing_row: Assertion"

run_arm c1-clear "$BREAK_C1_CLEAR" \
    "the incremental clear of c1 stops one short" \
    "s->c1[v] == 0.0"

run_arm nbmark "$BREAK_NBMARK" \
    "pivot leaves the bitmap behind the status" \
    "nbmark_consistent(s)"

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "scratch-controls exit=$fail  ->  $out"
exit $fail
