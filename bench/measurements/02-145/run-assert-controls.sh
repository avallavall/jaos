#!/usr/bin/env bash
# The ten asserts D232 adds to `src/simplex.c` and `src/presolve.c`, and the
# one-line source edit that makes each one fire.
#
# `jaos-testing`: a green suite is not evidence until it has been watched
# going red for the right reason. Same shape as 02-139 to 02-142, with one
# criterion tightened. Those campaigns passed an arm when the suite failed to
# come back clean, which cannot tell one assert from another or from a
# segfault. Here an arm passes only when the log carries the exact expression
# glibc prints for THAT assert, so each arm names the guard it proved.
#
# Two arms need more than the unit suites. The primal phase 1 runs only under
# `cfg.force_primal`, which no test sets, and presolve's FORCING and
# singleton-column families need a real model. `probe.c` beside this script is
# what reaches both; it runs only when the suites did not already fire.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-145/run-assert-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-145"
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
CARRY="src/simplex.c src/presolve.c"

# The 32 smallest of the standard set, smallest first so an abort costs the
# least, plus `beaconfd` for the presolve families. The probe solves each one
# twice, dual and primal: 66 solves, well under a second on this host.
PROBE_SET="afiro sc50b sc50a kb2 sc105 adlittle stocfor1 blend scagr7 sc205 \
share2b recipe lotfi vtp-base share1b boeing2 bore3d scorpion capri brandy \
sctap1 scagr25 israel scfxm1 bandm e226 grow7 etamacro agg finnis scsd1 \
standata beaconfd"

# --------------------------------------------------------------------- #
# The breakers, one per assert                                           #
# --------------------------------------------------------------------- #

# 1. The cumulative iteration cap drops below what D196 measured phase 1
#    needs. This one is a static_assert, so the BUILD is the arm.
BREAK_ITER_CAP='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """constexpr int64_t ITER_SANITY_FACTOR = 200;"""
assert s.count(old) == 1, "iteration factor matched %d times" % s.count(old)
new = """constexpr int64_t ITER_SANITY_FACTOR = 50;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  ITER_SANITY_FACTOR is 50, below the cumulative floor")
'

# 2. `bfrt_walk` retires a candidate with an infinite box: the width test is
#    dropped and the spend test reads an infinite width as zero, which is the
#    shape of a test written against a clamped width. Exactly one candidate is
#    retired, because the spend itself then takes `remaining` to -inf and the
#    next trip breaks -- so `live` stays above zero and `apply_flips` runs.
#
#    Deleting BOTH breaks instead does not reach the assert: the walk drains
#    to `live == 0`, and `dual_ratio_test` returns before it calls
#    `apply_flips`. That arm came back with 47 red tests and no abort.
BREAK_BFRT_INFINITE='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        double width = s->rrange[k];
        if (!isfinite(width))
            break;                     /* no other bound to swap to */
        if (!(remaining - s->rden[k] * width > 0.0))
            break;                     /* swapping would overshoot: it blocks */
        remaining -= s->rden[k] * width;"""
assert s.count(old) == 1, "bfrt walk matched %d times" % s.count(old)
new = """        double width = s->rrange[k];
        const double spend = isfinite(width) ? width : 0.0;
        if (!(remaining - s->rden[k] * spend > 0.0))
            break;                     /* swapping would overshoot: it blocks */
        remaining -= s->rden[k] * width;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  bfrt_walk retires a candidate with an infinite box")
'

# 3. The phase-1 append count starts at `nrow` instead of zero, so the first
#    infeasible basic takes it one past the array `c1_at` is allocated at.
BREAK_C1_AT_COUNT='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    for (int64_t k = 0; k < cleared; k++)
        s->c1[s->c1_at[k]] = 0.0;
    s->n_c1_at = 0;"""
assert s.count(old) == 1, "c1_at reset matched %d times" % s.count(old)
new = """    for (int64_t k = 0; k < cleared; k++)
        s->c1[s->c1_at[k]] = 0.0;
    s->n_c1_at = s->nrow;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the phase-1 append count starts at nrow")
'

# 4. The objective is left lent. The next call finds `cost` still pointing at
#    the phase-1 array, which is what a re-entrant caller would leave behind.
BREAK_COST_LEFT_LENT='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    s->cost = s->c1;
    compute_duals(s, false);
    s->cost = real_cost;"""
assert s.count(old) == 1, "the lend matched %d times" % s.count(old)
new = """    s->cost = s->c1;
    compute_duals(s, false);
    (void)real_cost;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  primal_phase1_duals never restores the objective")
'

# 5. Two bits are set per input position, so the read-back names up to twice
#    as many positions as went in. The output stays ascending and the bitmap
#    still comes back clean, so `k <= n` is the assert this reaches.
BREAK_PATTERN_LONGER='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        mark[w] |= UINT64_C(1) << (p & 63);"""
assert s.count(old) == 1, "the mark set matched %d times" % s.count(old)
new = """        mark[w] |= UINT64_C(3) << (p & 63);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  jm_pattern_order names two positions per input")
'

# 6. The count pass drops the "alive row" half of the predicate the fill pass
#    still applies, so the two disagree on every column under a dead row.
BREAK_COMPACTION_COUNT='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """        int64_t n = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            if (!row_dead[m->a_index[k]])
                n++;"""
assert s.count(old) == 1, "the count pass matched %d times" % s.count(old)
new = """        int64_t n = 0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            n++;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the count pass and the fill pass use different predicates")
'

# 7a. Only the bound comparison goes; the `col_pending_dual` early-out stays.
#     This arm is expected to change nothing, and that is the measurement:
#     the one site that writes a derived column bound also sets
#     `col_pending_dual`, so the outer test rejects every case the inner one
#     would have.
BREAK_FORCING_INNER_ONLY='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                    const bool want_lo =
                        force_hi ? (rw.rval[k] > 0.0) : (rw.rval[k] < 0.0);
                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        at_own_bounds = false;
                        break;
                    }
"""
assert s.count(old) == 1, "the own-bounds test matched %d times" % s.count(old)
open(p, "w", encoding="utf-8").write(s.replace(old, ""))
print("  only the bound comparison is gone; col_pending_dual still guards")
'

# 7b. Both tests go, so a FORCING row may pin a column at a DERIVED bound.
BREAK_FORCING_DERIVED='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                    if (col_pending_dual[j]) {
                        at_own_bounds = false;
                        break;
                    }
                    const bool want_lo =
                        force_hi ? (rw.rval[k] > 0.0) : (rw.rval[k] < 0.0);
                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        at_own_bounds = false;
                        break;
                    }
"""
assert s.count(old) == 1, "the own-bounds block matched %d times" % s.count(old)
open(p, "w", encoding="utf-8").write(s.replace(old, ""))
print("  FORCING accepts a column pinned at a derived bound")
'

# 8. A bounded singleton column is pushed as FREE_COL_SINGLETON. The replay
#    of that family publishes a zero reduced cost and a zero dual, which is
#    only sound for a genuinely free column.
BREAK_FREE_COL_NOT_FREE='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                if (free_col && !row_frozen[i] && row_deg[i] == 1) {"""
assert s.count(old) == 1, "the free-col test matched %d times" % s.count(old)
new = """                if (!row_frozen[i] && row_deg[i] == 1) {"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a bounded singleton column is recorded as free")
'

# 9a. The forcing row claims four more pinned columns than it pushed, so the
#     backward arena walk runs past what belongs to it.
BREAK_FORCING_COUNT_HIGH='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                            .tag = JM_PS_FORCING_ROW, .index = i,
                            .index2 = nfix,"""
assert s.count(old) == 1, "the forcing push matched %d times" % s.count(old)
new = """                            .tag = JM_PS_FORCING_ROW, .index = i,
                            .index2 = nfix + 4,"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the forcing row over-counts its pinned columns by four")
'

# 9b. A pinned column records a zero coefficient, which is the divisor the
#     replay forms its multiplier with.
BREAK_FORCING_ZERO_COEF='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                                .tag = JM_PS_FIXED_COL, .index = j,
                                .value = v, .cost = cur_cost[j],
                                .coef = rw.rval[k] })) {"""
assert s.count(old) == 1, "the fixed-col push matched %d times" % s.count(old)
new = """                                .tag = JM_PS_FIXED_COL, .index = j,
                                .value = v, .cost = cur_cost[j],
                                .coef = 0.0 })) {"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a pinned column records a zero coefficient")
'

# 10. The cost test is dropped from the singleton-column condition, so a
#     column carrying a real cost enters a family that drops it.
BREAK_SINGLETON_COST='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """            if (col_deg[j] == 1 && cur_cost[j] == 0.0) {"""
assert s.count(old) == 1, "the singleton test matched %d times" % s.count(old)
new = """            if (col_deg[j] == 1) {"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  a singleton column with a nonzero cost enters the family")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker or "", $3 = mode, $4 = wanted text
    # Split, not one `local`: bash expands every word of a `local` before it
    # assigns any of them, so "$D/wt-$tag" would read an unset `tag`.
    local tag=$1 breaker=$2 mode=$3 want=$4
    local wt="$D/wt-$tag"
    local brc=0
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    local f
    for f in $CARRY; do cp "$JAOS_ROOT/$f" "$wt/$f" || return 2; done
    # The instance directory is gitignored, so a worktree has none of it.
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances" || return 2
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi

    : > "$D/$tag.log"
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/dev/test_simplex build/dev/test_presolve build/dev/test_lp \
      ) > "$D/$tag.build" 2>&1
    brc=$?
    echo "$brc" > "$D/$tag.buildrc"

    if [ "$mode" = build-fails ]; then
        cat "$D/$tag.build" >> "$D/$tag.log"
        return 0
    fi
    if [ $brc -ne 0 ]; then
        echo "  BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2
    fi

    local t
    for t in test_simplex test_presolve test_lp; do
        echo "-- $t" >> "$D/$tag.log"
        ( cd "$wt" && timeout 900 "./build/dev/$t" ) >> "$D/$tag.log" 2>&1
        echo "-- $t exit=$?" >> "$D/$tag.log"
    done

    # The probe costs minutes, so it runs only when the suites did not
    # already fire the assert this arm is for. A green arm always runs it:
    # the intact tree must survive both methods on every instance.
    if [ -n "$want" ] && grep -qF "$want" "$D/$tag.log"; then
        echo "-- probe skipped: the suites already fired it" >> "$D/$tag.log"
        return 0
    fi
    # Every dev object EXCEPT unity.o, which pulls in Unity's runner and its
    # undefined `setUp`/`tearDown`. The probe is not a test.
    ( cd "$wt" && $CC -std=c23 -Wall -Wextra -ffp-contract=off -Og -g \
        -Iinclude -Isrc "$here/probe.c" \
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

suite_lines()  { grep -Eo '[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "$1" | tr '\n' '|'; }
suite_exits()  { grep -Eo -- '-- test_[a-z_]+ exit=[0-9]+' "$1" | tr '\n' '|'; }
probe_exit()   { grep -Eo -- '-- probe exit=[0-9]+' "$1" | tail -1; }
fired_line()   { grep -Eo 'Assertion .*failed' "$1" | head -1; }
# Which binary fired it. Not decoration: the `cost-left-lent` breaker leaves
# `cost` aliasing `c1`, so teardown frees one pointer twice and glibc aborts
# `test_simplex` at 134 with no assert message at all. The assert fires on
# the probe instead. An arm that only read the exit code would have credited
# the assert with a catch it did not make.
fired_in()     { awk '/^-- /{sec=$2} /Assertion/{print sec; exit}' "$1"; }

fail=0
out="$here/controls.txt"
{
echo "# 02-145 -- the ten asserts of D232, and the arm that fires each one"
echo "# tree: $(git rev-parse --short HEAD 2>/dev/null)$(git diff --quiet 2>/dev/null || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# an arm passes only when the log carries the expression glibc prints"
echo "# for that assert, so each arm names the guard it proved"
echo "# the two forcing arms are deliberately quiet: census.txt beside this"
echo "# file counts the population they measured"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description, $4 = mode,
              # $5 = the exact text that must appear, "" for a green arm
    local tag=$1 breaker=$2 desc=$3 mode=$4 want=$5
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker" "$mode" "$want"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"
        fail=2
        echo >> "$out"
        return
    fi

    if [ "$mode" = build-fails ]; then
        echo "   build:     exit=$(cat "$D/$tag.buildrc")" >> "$out"
        if [ "$(cat "$D/$tag.buildrc")" != "0" ] && \
           grep -qF "$want" "$D/$tag.log"; then
            echo "   PASS  the build refused it: $want" >> "$out"
        else
            echo "   FAIL  the build did not refuse it" >> "$out"; fail=1
        fi
        echo >> "$out"
        return
    fi

    {
      echo "   suites:    $(suite_lines "$D/$tag.log")"
      echo "   exits:     $(suite_exits "$D/$tag.log")  $(probe_exit "$D/$tag.log")"
      local a
      a="$(fired_line "$D/$tag.log")"
      [ -n "$a" ] && echo "   fired:     $a"
      [ -n "$a" ] && echo "   fired in:  $(fired_in "$D/$tag.log")"
    } >> "$out"

    if [ -z "$want" ]; then
        if [ "$(grep -c 'exit=0' "$D/$tag.log")" = "4" ] && \
           ! grep -q 'Assertion' "$D/$tag.log" && \
           [ "$(grep -Eco '[0-9]+ Tests [1-9][0-9]* Failures' "$D/$tag.log")" = "0" ]; then
            echo "   PASS  three suites green, the probe solved every instance" >> "$out"
        else
            echo "   FAIL  the intact tree did not come back clean" >> "$out"; fail=1
        fi
    else
        if grep -qF "$want" "$D/$tag.log"; then
            echo "   PASS  the assert fired: $want" >> "$out"
        else
            echo "   FAIL  nothing fired $want" >> "$out"; fail=1
        fi
    fi
    echo >> "$out"
}

run_arm intact "" "nothing broken: the tree as it stands" run ""

run_arm iter-cap "$BREAK_ITER_CAP" \
    "ITER_SANITY_FACTOR drops to 50, below the cumulative floor (D196)" \
    build-fails "the iteration cap is shared across phases (D196)"

run_arm bfrt-infinite-box "$BREAK_BFRT_INFINITE" \
    "bfrt_walk retires a candidate with an infinite box" \
    run "isfinite(s->rrange[k])"

run_arm pattern-longer "$BREAK_PATTERN_LONGER" \
    "jm_pattern_order names two positions per input" \
    run "k <= n"

run_arm c1-at-overflow "$BREAK_C1_AT_COUNT" \
    "the phase-1 append count starts at nrow" \
    run "s->n_c1_at <= s->nrow"

run_arm cost-left-lent "$BREAK_COST_LEFT_LENT" \
    "primal_phase1_duals never restores the objective" \
    run "s->cost != s->c1"

run_arm compaction-disagrees "$BREAK_COMPACTION_COUNT" \
    "the count pass and the fill pass use different predicates" \
    run "dst == p->reduced.a_start[rj2 + 1]"

# The one assert of the ten that no arm can fire. Both arms below are quiet,
# and `census-forcing.sh` is what makes that a measurement rather than a gap:
# over 139 instances, 7071 applied FORCING rows pin 98415 columns and not one
# of them sits at a bound the model did not carry.
run_arm forcing-inner-only "$BREAK_FORCING_INNER_ONLY" \
    "only the bound comparison is gone; col_pending_dual still guards" \
    run ""

run_arm forcing-both-guards-gone "$BREAK_FORCING_DERIVED" \
    "both guards gone: the shape is absent from all 139 instances" \
    run ""

run_arm free-col-not-free "$BREAK_FREE_COL_NOT_FREE" \
    "a bounded singleton column is recorded as free" \
    run "!isfinite(orig->col_lower[j]) && !isfinite(orig->col_upper[j])"

run_arm forcing-count-high "$BREAK_FORCING_COUNT_HIGH" \
    "the forcing row over-counts its pinned columns by four" \
    run "r - t >= 0"

run_arm forcing-zero-coef "$BREAK_FORCING_ZERO_COEF" \
    "a pinned column records a zero coefficient" \
    run "cr->coef != 0.0"

run_arm singleton-nonzero-cost "$BREAK_SINGLETON_COST" \
    "a singleton column with a nonzero cost enters the family" \
    run "cur_cost[j] == 0.0"

run_arm restored "" "the recipe again with nothing broken" run ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "assert-controls exit=$fail  ->  $out"
exit $fail
