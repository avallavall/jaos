#!/usr/bin/env bash
# The four presolve asserts of D235, and the edit that fires each.
#
# An arm passes only when the log carries the exact expression glibc prints,
# with the function before it (D232, D234).
#
# The fourth arm is not a defect but an INVERTED assert. `isfinite(row_traffic[i])`
# in the empty-row branch says a fallback is unreachable; an assert that is
# never evaluated also never fires, so the arm flips it to
# `!isfinite(row_traffic[i])` and requires that to fire. That separates "the
# branch runs and the budget is finite" from "the branch never runs", which
# is the difference between a measurement and a gap (02-146, D233).
#
# Every solve stops when presolve returns, the same patch 02-146 used: these
# are presolve asserts and the simplex is what costs the hour.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-148/run-presolve-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-148"
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

STOP_AFTER_PRESOLVE='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    jaos_status pst = jm_presolve_run(m, &p, &pre_work);
    if (pst != JAOS_OK) {
        jm_presolve_free(&p);
        return pst;
    }"""
assert s.count(old) == 1, "the presolve call matched %d times" % s.count(old)
new = """    jaos_status pst = jm_presolve_run(m, &p, &pre_work);
    if (pst != JAOS_OK) {
        jm_presolve_free(&p);
        return pst;
    }
    jm_presolve_free(&p);
    return JAOS_OK;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
'

# The round count outruns the structural backstop.
BREAK_ROUNDS='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """    p->counts.rounds = rounds_done;"""
assert s.count(old) == 1, "the rounds write matched %d times" % s.count(old)
new = """    p->counts.rounds = rounds_done + nr + nc + 2;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  counts.rounds is pushed past the structural backstop")
'

# The budget is spent rather than accumulated, so it goes negative.
BREAK_TRAFFIC_SIGN='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                    row_traffic[i] += moved;"""
assert s.count(old) == 1, "the singleton-col traffic matched %d times" % s.count(old)
new = """                    row_traffic[i] -= moved;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the singleton-col traffic is subtracted instead of added")
'

# Which end absorbed is read off the OTHER end. The copy-paste slip.
BREAK_ABSORBS='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                    const bool lo_absorbs = isfinite(cur_rl[i]);"""
assert s.count(old) == 1, "the lo_absorbs read matched %d times" % s.count(old)
new = """                    const bool lo_absorbs = isfinite(cur_ru[i]);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  lo_absorbs is read off the upper end")
'

# NOT a defect: the assert inverted, so a branch that runs must fire it. An
# assert that is never evaluated is quiet for the wrong reason.
BREAK_FALLBACK_CANARY='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                    assert(isfinite(row_traffic[i]));"""
assert s.count(old) == 1, "the fallback assert matched %d times" % s.count(old)
new = """                    assert(!isfinite(row_traffic[i]));"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the fallback assert is inverted, so reaching it must fire")
'

# --------------------------------------------------------------------- #
arm() {   # $1 = tag, $2 = breaker or ""
    local tag=$1 breaker=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || return 2
    cp src/simplex.c src/presolve.c "$wt/src/" || return 2
    ln -s "$JAOS_ROOT/bench/instances" "$wt/bench/instances" || return 2
    ( cd "$wt" && python3 -c "$STOP_AFTER_PRESOLVE" ) >/dev/null || return 2
    if [ -n "$breaker" ]; then
        ( cd "$wt" && python3 -c "$breaker" ) || { echo "  BREAKER FAILED"; return 2; }
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG ) > "$D/$tag.build" 2>&1 || {
        echo "  BUILD FAILED:"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag.txt" ) \
        > "$D/$tag.log" 2>&1
    echo "runner exit=$?" >> "$D/$tag.log"
    return 0
}

fired_line() { grep -Eo '[a-z_0-9]+: Assertion .*failed' "$1" | head -1; }

fail=0
out="$here/controls.txt"
{
echo "# 02-148 -- the four presolve asserts of D235, and what fires each"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# every solve stops when presolve returns; the runner's verdicts are"
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

run_arm intact "" "nothing broken: the tree as it stands" ""

run_arm rounds "$BREAK_ROUNDS" \
    "the round count outruns the structural backstop" \
    "p->counts.rounds <= nr + nc + 1"

run_arm traffic-sign "$BREAK_TRAFFIC_SIGN" \
    "the singleton-col traffic is subtracted instead of added" \
    "row_traffic[i] >= 0.0"

run_arm absorbs "$BREAK_ABSORBS" \
    "lo_absorbs is read off the upper end" \
    "lo_absorbs || !isfinite(cur_rl[i])"

run_arm fallback-canary "$BREAK_FALLBACK_CANARY" \
    "the fallback assert inverted: reaching it must fire" \
    "!isfinite(row_traffic[i])"

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "presolve-controls exit=$fail  ->  $out"
exit $fail
