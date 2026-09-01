#!/usr/bin/env bash
# Two more presolve asserts (D236), over all 139 instances and against the
# edits that fire them.
#
#   boxes only narrow   a live column never leaves the box the caller gave it
#   compaction row      an entry's row index is never -1
#
# Six arms. Two of them are INVERTED asserts rather than defects: an assert
# that is never evaluated is never violated, so a quiet intact arm only means
# something once something proves each check is reached (D235).
#
# Every solve stops when presolve returns, so 139 instances cost a minute
# rather than fifty (02-148). The runner then calls every instance failed,
# which is expected; the only signal is whether the right assert fired.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-149/run-box-controls.sh
# Writes controls.txt beside this script. Exit 0 when every arm behaved,
# 1 when one did not, 2 when the harness itself failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-149"
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

# The fold takes the implied bound without intersecting the current box, so a
# column can come out of a round wider than the caller made it.
BREAK_BOX='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """                const double new_lo = implied_lo > cur_cl[j] ? implied_lo
                                                             : cur_cl[j];"""
assert s.count(old) == 1, "the new_lo intersection matched %d times" % s.count(old)
new = """                const double new_lo = implied_lo;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the fold stops intersecting the implied bound with the box")
'

# The fill pass forgets that a dead row contributes nothing, so `row_map` of
# a dead row lands in the reduced matrix as a row index of -1.
BREAK_COMPACTION='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """            const int64_t i = m->a_index[k];
            if (row_dead[i])
                continue;
            p->reduced.a_index[dst] = p->row_map[i];"""
assert s.count(old) == 1, "the fill skip matched %d times" % s.count(old)
new = """            const int64_t i = m->a_index[k];
            p->reduced.a_index[dst] = p->row_map[i];"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the fill pass keeps a dead rows entry")
'

# NOT defects: the two asserts inverted, so reaching either must fire.
CANARY_BOX='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """        assert(cur_cl[j] >= m->col_lower[j]);"""
assert s.count(old) == 1, "the box assert matched %d times" % s.count(old)
new = """        assert(cur_cl[j] < m->col_lower[j]);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the box assert is inverted, so reaching it must fire")
'

CANARY_COMPACTION='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """            assert(p->reduced.a_index[dst] >= 0);"""
assert s.count(old) == 1, "the compaction assert matched %d times" % s.count(old)
new = """            assert(p->reduced.a_index[dst] < 0);"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  the compaction assert is inverted, so reaching it must fire")
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
    ( cd "$wt" && python3 -c "$STOP_AFTER_PRESOLVE" ) >/dev/null || return 2
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
echo "# 02-149 -- two more presolve asserts (D236), over all 139 instances"
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

run_arm intact "" "nothing broken: the tree as it stands, all 139" ""

run_arm canary-box "$CANARY_BOX" \
    "the box assert inverted: reaching it must fire" \
    "cur_cl[j] < m->col_lower[j]"

run_arm canary-compaction "$CANARY_COMPACTION" \
    "the compaction assert inverted: reaching it must fire" \
    "p->reduced.a_index[dst] < 0"

run_arm break-box "$BREAK_BOX" \
    "the fold stops intersecting the implied bound with the box" \
    "cur_cl[j] >= m->col_lower[j]"

run_arm break-compaction "$BREAK_COMPACTION" \
    "the fill pass keeps a dead row's entry" \
    "p->reduced.a_index[dst] >= 0"

run_arm restored "" "the recipe again with nothing broken" ""

{
echo "== verdict"
if [ $fail -eq 0 ]; then echo "every arm behaved"
elif [ $fail -eq 1 ]; then echo "AT LEAST ONE ARM DID NOT BEHAVE"
else echo "THE HARNESS ITSELF FAILED"; fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "box-controls exit=$fail  ->  $out"
exit $fail
