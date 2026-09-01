#!/usr/bin/env bash
# Do D235's four presolve asserts hold on all 139 gate instances?
#
# One of them is a claim of UNREACHABILITY: `assert(isfinite(row_traffic[i]))`
# in the empty-row branch, for the comment "unreachable since D155". The
# comment has been there since D155 and nothing has ever tested it, so this
# run is what turns it into a measurement. If it fires, the fallback is live
# and the assert comes out.
#
# All four are inside presolve, which runs once at the top of every solve and
# is the same work whichever method follows. So each solve stops as soon as
# presolve returns: 139 instances cost a minute rather than the fifty that an
# assert-enabled Kennington costs when the simplex runs too (02-145). The
# patch is the same one 02-146 used.
#
# The runner reports every instance as failed, because a solve that returns
# after presolve has no solution. That is expected and is not what is being
# read. **The only signal here is whether an assert fired**, and the intact
# arm at the end is what says the harness could see one.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-148/run-presolve-population.sh
# Writes population.txt beside this script. Exit 0 when nothing fired and the
# canary did, 1 otherwise, 2 when the harness failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-148"
cd "$JAOS_ROOT" || exit 2
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    for w in "$D"/wt-*; do git worktree remove --force "$w" 2>/dev/null; done
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

# Stops every solve as soon as presolve returns. The simplex is not wanted
# here and it is what makes 139 instances cost an hour.
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
print("  the solve stops after presolve")
'

# The canary. `rounds` is capped by the loop, so forcing the count past the
# structural backstop is the one edit that must trip a debug build. Without
# it a run that fires nothing is measuring the instances, not the asserts.
CANARY='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
old = """    p->counts.rounds = rounds_done;"""
assert s.count(old) == 1, "the rounds write matched %d times" % s.count(old)
new = """    p->counts.rounds = rounds_done + nr + nc + 2;"""
open(p, "w", encoding="utf-8").write(s.replace(old, new))
print("  counts.rounds is pushed past the structural backstop")
'

run_arm() {   # $1 = tag, $2 = extra breaker or ""
    local tag=$1 extra=$2
    local wt="$D/wt-$tag"
    git worktree add --detach "$wt" "$(git rev-parse HEAD)" >/dev/null 2>&1 || return 2
    cp src/simplex.c src/presolve.c "$wt/src/" || return 2
    for d in instances instances-kennington instances-infeas; do
        ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || return 2
    done
    ( cd "$wt" && python3 -c "$STOP_AFTER_PRESOLVE" ) >/dev/null || return 2
    if [ -n "$extra" ]; then
        ( cd "$wt" && python3 -c "$extra" ) >/dev/null || return 2
    fi
    ( cd "$wt" && make clean >/dev/null 2>&1
      make build/bench/run EXTRA_CFLAGS=-UNDEBUG ) > "$D/$tag.build" 2>&1 || {
        echo "  BUILD FAILED"; grep -E 'error:' "$D/$tag.build" | head -5
        return 2; }

    : > "$D/$tag.err"
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-n.txt" ) \
        >> "$D/$tag.err" 2>&1
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-i.txt" \
        -e infeasible -m bench/netlib-infeas.manifest \
        -d bench/instances-infeas ) >> "$D/$tag.err" 2>&1
    ( cd "$wt" && timeout 1800 ./build/bench/run -j 12 -o "$D/$tag-k.txt" \
        -m bench/netlib-kennington.manifest \
        -d bench/instances-kennington ) >> "$D/$tag.err" 2>&1
    return 0
}

out="$here/population.txt"
{
echo "# 02-148 -- D235's four presolve asserts over all 139 instances"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of src/simplex.c src/presolve.c"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# every solve stops when presolve returns; the runner's own verdicts are"
echo "# meaningless here and the only signal is whether an assert fired"
echo
} > "$out"

fail=0

echo "== intact"
run_arm intact ""
rc=$?
if [ $rc -ne 0 ]; then
    echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"; exit 2
fi
n=$(grep -c 'Assertion' "$D/intact.err")
{
  echo "== intact: the four asserts over 139 instances"
  echo "   assertion lines=$n"
  [ "$n" != "0" ] && grep -m5 'Assertion' "$D/intact.err" | sed 's/^/   FIRED: /'
} >> "$out"
[ "$n" != "0" ] && fail=1
echo >> "$out"

echo "== canary"
run_arm canary "$CANARY"
rc=$?
if [ $rc -ne 0 ]; then
    echo "   HARNESS FAILED (rc=$rc)" | tee -a "$out"; exit 2
fi
c=$(grep -c 'Assertion' "$D/canary.err")
{
  echo "== canary: counts.rounds pushed past the backstop"
  echo "   assertion lines=$c"
  [ "$c" != "0" ] && grep -m2 'Assertion' "$D/canary.err" | sed 's/^/   FIRED: /'
} >> "$out"
[ "$c" = "0" ] && fail=1
echo >> "$out"

{
echo "== verdict"
if [ $fail -eq 0 ]; then
    echo "nothing fired on 139 instances, and the canary proves the run could see it"
else
    echo "EITHER AN ASSERT FIRED OR THE CANARY DID NOT"
fi
} >> "$out"

sed -n '/^== verdict/,$p' "$out"
echo "presolve-population exit=$fail  ->  $out"
exit $fail
