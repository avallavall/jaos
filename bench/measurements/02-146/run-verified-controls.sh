#!/usr/bin/env bash
# The assert `verified_fresh` carries, and the edits that make it fire.
#
# `census.txt` beside this file says the assert holds on all 139 gate
# instances. A green assert is not evidence until it has been watched going
# red for the right reason, and D232's lesson is that the arm must match the
# exact expression glibc prints rather than any abort.
#
# Four arms plus the intact tree:
#
#   canary              set_verified stops resetting the counter, so the
#                       assert is live at all. This is the ONLY arm that
#                       fires it, and the four below say why.
#   reenter-clears-gone reenter_after_settling stops spending its
#                       verification on BOTH paths.
#   dual-clear-gone     the dual loop drops its pre-pivot clear.
#   phase1-clear-gone   the primal phase-1 loop drops its own.
#   cleanup-clear-gone  only path A of reenter_after_settling drops its clear.
#
# **The four below are expected QUIET, and no source edit found here fires the
# assert with a realistic defect.** That is a measurement and not a gap.
# `census.txt` puts `piv_verified` at 6 in 1033526: the flag is already false
# wherever the two main loops pivot, so their pre-pivot clears are defensive
# rather than load-bearing. Every one of those 6 comes through
# `primal_cleanup`, and after it `reenter_after_settling` either returns or
# ends its round -- it never reaches one of `run()`s seven readers. So the
# sequence the assert catches, verify then pivot then read, is unreachable on
# this population however many clears are removed.
#
# The assert stays for the same reason D232 kept the FORCING one: it is free
# in a shipping build, it states the property the prose got wrong, and the
# canary shows it would catch the sequence if a refactor ever created it.
# What the record must not claim is that a realistic defect proved it.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-146/run-verified-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-146"
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
# `etamacro` and `wood1p` are two of the three instances whose census line
# shows a pivot entered with the flag already set; `pilot87` is the third and
# is too slow for an arm. The rest are small and fast.
PROBE_SET="afiro sc50b adlittle blend share2b lotfi boeing2 etamacro wood1p"

# --------------------------------------------------------------------- #

# The counter stops being reset, so it grows from the first pivot and never
# returns to zero. Any reader that then finds the flag set trips the assert.
BREAK_CANARY='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """    s->verified = v;
#ifndef NDEBUG
    s->dbg_piv_since_verify = 0;
#endif"""
assert s.count(old) == 1, "the setter matched %d times" % s.count(old)
new = """    s->verified = v;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  set_verified no longer resets the counter")
'

# The dual loop pivots without spending its verification first.
BREAK_DUAL_CLEAR='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        /* The basis is about to change, so any verification is spent. */
        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);"""
assert s.count(old) == 1, "the dual clear matched %d times" % s.count(old)
new = """        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the dual loop pivots without clearing the flag")
'

# The primal phase-1 loop, same shape.
BREAK_PHASE1_CLEAR='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """        set_verified(s, false);
        bool took = false;
        /* `d` here holds phase-1 reduced costs, so `pivot()` maintains the
         * phase-1 pricing; the phase-2 costs are recomputed at hand-over. */"""
assert s.count(old) == 1, "the phase-1 clear matched %d times" % s.count(old)
new = """        bool took = false;
        /* `d` here holds phase-1 reduced costs, so `pivot()` maintains the
         * phase-1 pricing; the phase-2 costs are recomputed at hand-over. */"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the primal phase 1 pivots without clearing the flag")
'

# BOTH of reenter_after_settling`s clears. This is the defect the assert
# exists for: path A pivots through primal_cleanup with the flag set, path B
# hands the point to run() without spending it, and run()`s first reader
# trusts a verification a pivot has spent. Two lines rather than one, because
# the two clears guard the same property and either one alone still covers it.
BREAK_REENTER_CLEARS='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """            bool ok = false;
            set_verified(s, false);
            s->needs_refactor = true;"""
assert s.count(old) == 1, "the path A clear matched %d times" % s.count(old)
s = s.replace(old, """            bool ok = false;
            s->needs_refactor = true;""")
old2 = """        /* The point changed, so any verification is spent. */
        set_verified(s, false);"""
assert s.count(old2) == 1, "the path B clear matched %d times" % s.count(old2)
s = s.replace(old2, "")
open(p, "w", encoding="utf-8").write(s)
print("  reenter_after_settling stops spending its verification, both paths")
'

# The clear reenter_after_settling does after primal_cleanup, which is the
# one the old prose was about.
BREAK_CLEANUP_CLEAR='
p = "src/simplex.c"
s = open(p, encoding="utf-8").read()
old = """            bool ok = false;
            set_verified(s, false);
            s->needs_refactor = true;"""
assert s.count(old) == 1, "the reenter clear matched %d times" % s.count(old)
new = """            bool ok = false;
            s->needs_refactor = true;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  reenter_after_settling keeps the flag across primal_cleanup")
'

# --------------------------------------------------------------------- #
WANT="!s->verified || s->dbg_piv_since_verify == 0"

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
fired_line()  { grep -Eo 'Assertion .*failed' "$1" | head -1; }
fired_in()    { awk '/^-- /{sec=$2} /Assertion/{print sec; exit}' "$1"; }

fail=0
out="$here/controls.txt"
{
echo "# 02-146 -- the assert verified_fresh carries, and what makes it fire"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copy of $CARRY"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# an arm passes only when the log carries the assert's own expression"
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

run_arm canary "$BREAK_CANARY" \
    "set_verified stops resetting the counter, so the assert must be live" \
    "$WANT"

# All four below are expected QUIET; the header says why. Together they are
# the evidence that no single-site defect reaches this assert on the gate
# population, which is a stronger statement than any one of them alone.
run_arm reenter-clears-gone "$BREAK_REENTER_CLEARS" \
    "reenter_after_settling stops spending its verification, both paths" ""

run_arm dual-clear-gone "$BREAK_DUAL_CLEAR" \
    "the dual loop pivots without spending its verification" ""

run_arm phase1-clear-gone "$BREAK_PHASE1_CLEAR" \
    "the primal phase 1 pivots without spending its verification" ""

run_arm cleanup-clear-gone "$BREAK_CLEANUP_CLEAR" \
    "reenter_after_settling keeps the flag across primal_cleanup" ""

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "verified-controls exit=$fail  ->  $out"
exit $fail
