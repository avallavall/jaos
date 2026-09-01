#!/usr/bin/env bash
# Two `JM_PS_SINGLETON_ROW` replay contracts (D238), over all 139 instances.
#
#   the divisor is not zero      a singleton row's one live entry is what
#                                `coef` records
#   the owned bound is interior  `zero_works` false plus a fold that only
#                                narrows puts x_j strictly inside the
#                                caller's bound on d0's side
#
# **These are POSTSOLVE asserts, so the whole solve has to run.** There is no
# early return to lean on the way 02-148 and 02-150 could, and Kennington
# under `-UNDEBUG` is about fifty minutes of it (02-145). That is the price of
# judging a postsolve contract and it is worth naming rather than avoiding.
#
# Two arms are the asserts INVERTED. A postsolve branch that no instance
# reaches would leave a quiet arm meaning nothing, and the `this_row_owns`
# branch is exactly the kind that might not fire (D235, D237).
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-151/run-replay-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-151"
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

# The record keeps the row's coefficient at zero, so every dual it publishes
# is an infinity.
BREAK_COEF='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                        .tag = JM_PS_SINGLETON_ROW,
                        .index = i, .index2 = j, .coef = a,"""
assert s.count(old) == 1, "the singleton-row push matched %d times" % s.count(old)
new = """                        .tag = JM_PS_SINGLETON_ROW,
                        .index = i, .index2 = j, .coef = 0.0,"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the singleton-row record keeps a zero coefficient")
'

# TWO edits, because the interior property is guarded twice and either guard
# alone covers it.
#
# `this_row_owns` requires `row_tightens_lo`, and a row that tightened put
# `rec->lo` strictly above the caller`s bound -- so `v0 == rec->lo` is already
# strictly interior whatever `zero_works` says. Breaking `zero_works` alone
# therefore fires nothing, which is measured: that arm came back silent.
#
# So the record must also claim to own a bound it did not induce. With both,
# a column resting on the caller`s own lower bound takes the owned branch and
# is published BASIC while sitting on a bound.
BREAK_INTERIOR='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                const bool tightens_lo = implied_lo > cur_cl[j];"""
assert s.count(old) == 1, "the tightens_lo test matched %d times" % s.count(old)
s = s.replace(old, """                const bool tightens_lo = true;""")
old2 = """        const bool zero_works =
            dc == 0.0 ||
            (dc > 0.0 && v0 == orig->col_lower[j]) ||
            (dc < 0.0 && v0 == orig->col_upper[j]);"""
assert s.count(old2) == 1, "the zero_works test matched %d times" % s.count(old2)
s = s.replace(old2, """        const bool zero_works = dc == 0.0;""")
open(p, "w", encoding="utf-8").write(s)
print("  a record owns a bound it did not induce, and zero_works stops noticing")
'

# NOT defects: each assert inverted, so reaching it must fire.
CANARY_COEF='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """        assert(rec->coef != 0.0);

        double y_i;"""
assert s.count(old) == 1, "the coef assert matched %d times" % s.count(old)
new = """        assert(rec->coef == 0.0);

        double y_i;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the coef assert is inverted, so reaching it must fire")
'

CANARY_INTERIOR='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """            assert(dc > 0.0 ? v0 > orig->col_lower[j]
                            : v0 < orig->col_upper[j]);"""
assert s.count(old) == 1, "the interior assert matched %d times" % s.count(old)
new = """            assert(dc > 0.0 ? v0 <= orig->col_lower[j]
                            : v0 >= orig->col_upper[j]);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the interior assert is inverted, so reaching it must fire")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker or ""
    local tag=$1 breaker=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    cp src/simplex.c src/presolve.c "$wt/src/" || return 2
    for d in instances instances-kennington instances-infeas; do
        ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || return 2
    done
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG ) > "$D/$tag.build" 2>&1 || {
        echo "  BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }
    : > "$D/$tag.log"
    ( cd "$wt" && timeout 5400 ./build/bench/run -j 12 -o "$D/$tag-n.txt" ) \
        >> "$D/$tag.log" 2>&1
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-i.txt" \
        -e infeasible -m bench/netlib-infeas.manifest \
        -d bench/instances-infeas ) >> "$D/$tag.log" 2>&1
    return 0
}

fired_line() { grep -Eo '[a-z_0-9]+: Assertion .*failed' "$1" | head -1; }

fail=0
out="$here/controls.txt"
{
echo "# 02-151 -- two SINGLETON_ROW replay contracts (D238)"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# postsolve asserts, so the whole solve runs: 94 standard and 29"
echo "# infeasible per arm. Kennington is left out because six arms of it is"
echo "# five hours; the two asserts are presolve-family and the standard set"
echo "# carries every family (bench/README.md)"
echo
} > "$out"

run_arm() {   # $1 = tag, $2 = breaker, $3 = description, $4 = want or ""
    local tag=$1 breaker=$2 desc=$3 want=$4
    echo "== $tag"
    { echo "== $tag"; echo "   $desc"; } >> "$out"
    arm "$tag" "$breaker"
    local rc=$?
    if [ $rc -ne 0 ]; then
        echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"
        fail=2; echo >> "$out"; return
    fi
    {
      echo "   assertion lines=$(grep -c 'Assertion' "$D/$tag.log")"
      local a
      a="$(fired_line "$D/$tag.log")"
      [ -n "$a" ] && echo "   fired:     $a"
    } >> "$out"
    if [ -z "$want" ]; then
        if ! grep -q 'Assertion' "$D/$tag.log"; then
            echo "   PASS  nothing fired" >> "$out"
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

run_arm canary-coef "$CANARY_COEF" \
    "the coef assert inverted: reaching it must fire" \
    "rec->coef == 0.0"

run_arm canary-interior "$CANARY_INTERIOR" \
    "the interior assert inverted: reaching it must fire" \
    "v0 <= orig->col_lower[j]"

run_arm break-coef "$BREAK_COEF" \
    "the singleton-row record keeps a zero coefficient" \
    "rec->coef != 0.0"

run_arm break-interior "$BREAK_INTERIOR" \
    "a record owns a bound it did not induce, and zero_works stops noticing" \
    "v0 > orig->col_lower[j]"

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "replay-controls exit=$fail  ->  $out"
exit $fail
