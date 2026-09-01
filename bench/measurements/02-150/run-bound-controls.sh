#!/usr/bin/env bash
# Two contracts about bounds at the start of a solve (D237), over all 139
# instances.
#
#   infinities survive   scaling moves a bound's magnitude, never its
#                        finiteness: isfinite(s->lo[v]) matches the model's.
#                        In `sx_init`.
#   a loan replaced an   `fake` is set only where the bound was absent, so a
#   infinity             faked end always holds the artificial value. In
#                        `build_initial_basis`, which runs LATER.
#
# Both run once per solve and before any iteration, so every solve stops as
# soon as the starting basis is built: 139 instances cost about a minute
# rather than the fifty an assert-enabled Kennington costs when the simplex
# runs too (02-145, 02-148). The runner then calls every instance failed,
# which is expected; the only signal is whether the right assert fired, which
# is why two arms are the asserts INVERTED.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-150/run-bound-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-150"
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

# Stops after the STARTING BASIS is built, not after `sx_init`. The first
# version stopped after `sx_init` and the loan assert never fired -- not
# because it holds but because `build_initial_basis` had not run yet. The
# canary is what said so, which is the whole reason it is here.
STOP_AFTER_BASIS='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
               warm ? "the basis on the model" : "the slack basis");"""
assert s.count(old) == 1, "the start log matched %d times" % s.count(old)
new = """        jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
               warm ? "the basis on the model" : "the slack basis");
        sx_free(&s);
        jm_presolve_free(&p);
        return JAOS_OK;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
'

# A scale factor of zero turns every bound of that column into an absent one.
# The solve then stops enforcing bounds it was given, with nothing downstream
# able to see it.
BREAK_SCALE='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        s->lo[j] = m->col_lower[j] / gamma[j];"""
assert s.count(old) == 1, "the column lower scale matched %d times" % s.count(old)
new = """        s->lo[j] = m->col_lower[j] / (j == 0 ? 0.0 : gamma[j]);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  column 0 is scaled by zero, so its lower bound stops being finite")
'

# The loan is recorded without moving the bound to the artificial value, so
# `real_lower` hands back an infinity the solve never lent.
BREAK_LOAN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """                s->lo[j] = -ARTIFICIAL_BOUND;
                s->fake[j] = FAKE_LO;"""
assert s.count(old) == 1, "the lower loan matched %d times" % s.count(old)
new = """                s->fake[j] = FAKE_LO;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the lower loan is recorded without moving the bound")
'

# NOT defects: each assert inverted, so reaching it must fire.
CANARY_SCALE='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        assert(isfinite(s->lo[j]) == isfinite(m->col_lower[j]));"""
assert s.count(old) == 1, "the scale assert matched %d times" % s.count(old)
new = """        assert(isfinite(s->lo[j]) != isfinite(m->col_lower[j]));"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the scale assert is inverted, so reaching it must fire")
'

CANARY_LOAN='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        assert(s->fake[v] != FAKE_LO || s->lo[v] == -ARTIFICIAL_BOUND);"""
assert s.count(old) == 1, "the loan assert matched %d times" % s.count(old)
new = """        assert(s->fake[v] == FAKE_LO && s->lo[v] != -ARTIFICIAL_BOUND);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the loan assert is inverted, so reaching it must fire")
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
    ( cd "$wt" && python3 -c "$STOP_AFTER_BASIS" ) >/dev/null || return 2
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG ) > "$D/$tag.build" 2>&1 || {
        echo "  BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }
    : > "$D/$tag.log"
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-n.txt" ) \
        >> "$D/$tag.log" 2>&1
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-i.txt" \
        -e infeasible -m bench/netlib-infeas.manifest \
        -d bench/instances-infeas ) >> "$D/$tag.log" 2>&1
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-k.txt" \
        -m bench/netlib-kennington.manifest \
        -d bench/instances-kennington ) >> "$D/$tag.log" 2>&1
    return 0
}

fired_line() { grep -Eo '[a-z_0-9]+: Assertion .*failed' "$1" | head -1; }

fail=0
out="$here/controls.txt"
{
echo "# 02-150 -- two sx_init bound contracts (D237), over all 139 instances"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# every solve stops once the starting basis is built; the runner verdicts are"
echo "# meaningless and the only signal is whether the right assert fired"
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
            echo "   PASS  nothing fired over 139 instances" >> "$out"
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

run_arm intact "" "nothing broken: the tree as it stands, all 139" ""

run_arm canary-scale "$CANARY_SCALE" \
    "the scale assert inverted: reaching it must fire" \
    "isfinite(s->lo[j]) != isfinite(m->col_lower[j])"

run_arm canary-loan "$CANARY_LOAN" \
    "the loan assert inverted: reaching it must fire" \
    "s->fake[v] == FAKE_LO && s->lo[v] != -ARTIFICIAL_BOUND"

run_arm break-scale "$BREAK_SCALE" \
    "column 0 is scaled by zero, so its lower bound stops being finite" \
    "isfinite(s->lo[j]) == isfinite(m->col_lower[j])"

run_arm break-loan "$BREAK_LOAN" \
    "the lower loan is recorded without moving the bound" \
    "s->fake[v] != FAKE_LO || s->lo[v] == -ARTIFICIAL_BOUND"

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "bound-controls exit=$fail  ->  $out"
exit $fail
