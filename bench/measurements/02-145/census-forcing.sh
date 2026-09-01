#!/usr/bin/env bash
# How often does a FORCING row meet a column at a DERIVED bound?
#
# `run-assert-controls.sh`'s `forcing-derived-bound` arm removes both guards
# on that shape and stays green. A quiet arm measures the instances, not the
# assert, so this counts what the arm actually saw. Four counters inside the
# FORCING branch, over all 139 gate instances:
#
#   seen     rows whose range touches a bound, so FORCING is considered
#   pending  rejected because a live column carries `col_pending_dual`
#   derived  rejected because a live column sits at a bound the model did
#            not carry -- the population the assert is for
#   taken    rows the branch actually applied
#   pinned   live columns pinned by an applied row: the assert's exposure
#
# `derived` at zero with `seen` well above zero is a statement about this
# population. `seen` at zero would mean the census measured nothing.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-145/census-forcing.sh
# Writes census.txt beside this script. Exit 0 when the census ran, 2 when
# the harness failed.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
here="$JAOS_ROOT/bench/measurements/02-145"
cd "$JAOS_ROOT" || exit 2
ref="$(git rev-parse HEAD)"
D=$(mktemp -d) || exit 2
cleanup() {
    cd "$JAOS_ROOT" || exit
    git worktree remove --force "$D/wt" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

CC=gcc-14

COUNTERS='
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()

old = """#include <assert.h>"""
assert s.count(old) == 1, "the assert include matched %d times" % s.count(old)
new = """#include <assert.h>
#include <stdio.h>
static long ps_fc_seen, ps_fc_pending, ps_fc_derived, ps_fc_taken,
            ps_fc_pinned;
__attribute__((destructor)) static void ps_fc_report(void)
{
    fprintf(stderr,
            "FORCING-CENSUS seen=%ld pending=%ld derived=%ld taken=%ld "
            "pinned=%ld\\n",
            ps_fc_seen, ps_fc_pending, ps_fc_derived, ps_fc_taken,
            ps_fc_pinned);
}"""
s = s.replace(old, new)

old = """            if (force_hi || force_lo) {"""
assert s.count(old) == 1, "the forcing entry matched %d times" % s.count(old)
new = """            if (force_hi || force_lo) {
                ps_fc_seen++;"""
s = s.replace(old, new)

old = """                    if (col_pending_dual[j]) {
                        at_own_bounds = false;
                        break;
                    }"""
assert s.count(old) == 1, "the pending test matched %d times" % s.count(old)
new = """                    if (col_pending_dual[j]) {
                        ps_fc_pending++;
                        at_own_bounds = false;
                        break;
                    }"""
s = s.replace(old, new)

old = """                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        at_own_bounds = false;
                        break;
                    }"""
assert s.count(old) == 1, "the bound test matched %d times" % s.count(old)
new = """                    if (want_lo ? (cur_cl[j] != m->col_lower[j])
                                : (cur_cu[j] != m->col_upper[j])) {
                        ps_fc_derived++;
                        at_own_bounds = false;
                        break;
                    }"""
s = s.replace(old, new)

old = """                if (at_own_bounds) {
                    int64_t nfix = 0;"""
assert s.count(old) == 1, "the accepted branch matched %d times" % s.count(old)
new = """                if (at_own_bounds) {
                    ps_fc_taken++;
                    int64_t nfix = 0;"""
s = s.replace(old, new)

old = """                        assert(v == m->col_lower[j] || v == m->col_upper[j]);"""
assert s.count(old) == 1, "the assert site matched %d times" % s.count(old)
new = """                        ps_fc_pinned++;
                        assert(v == m->col_lower[j] || v == m->col_upper[j]);"""
s = s.replace(old, new)

open(p, "w", encoding="utf-8").write(s)

# The simplex is not wanted here, and it is what makes 139 instances cost an
# hour instead of a minute. Every solve stops as soon as presolve returns.
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
print("  six counters inserted, and the solve stops after presolve")
'

wt="$D/wt"
git worktree add --detach "$wt" "$ref" >/dev/null 2>&1 || exit 2
for f in src/simplex.c src/presolve.c; do
    cp "$JAOS_ROOT/$f" "$wt/$f" || exit 2
done
for d in instances instances-kennington instances-infeas; do
    ln -s "$JAOS_ROOT/bench/$d" "$wt/bench/$d" || exit 2
done
( cd "$wt" && python3 -c "$COUNTERS" ) || exit 2

( cd "$wt" && make clean >/dev/null 2>&1
  make build/dev/test_presolve ) > "$D/build" 2>&1 || {
    echo "BUILD FAILED"; grep -E 'error:' "$D/build" | head -10; exit 2; }
( cd "$wt" && $CC -std=c23 -Wall -Wextra -ffp-contract=off -Og -g \
    -Iinclude -Isrc "$here/probe.c" \
    $(ls build/dev/*.o | grep -v '/unity\.o$') -o probe -lm \
  ) >> "$D/build" 2>&1 || {
    echo "PROBE BUILD FAILED"; grep -E 'error:' "$D/build" | head -10; exit 2; }

# One process per set, so each prints its own census line at exit.
out="$here/census.txt"
{
echo "# 02-145 -- how often a FORCING row meets a column at a derived bound"
echo "# tree: $(git rev-parse --short HEAD)$(git diff --quiet || echo ' WITH UNCOMMITTED CHANGES')"
echo "#       plus the working-tree copies of src/simplex.c src/presolve.c"
echo "# date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "# dual method only: presolve runs the same either way, and the primal"
echo "# does not finish the heavier instances"
echo
} > "$out"

for d in instances instances-kennington instances-infeas; do
    n=$(ls "$JAOS_ROOT/bench/$d"/*.mps 2>/dev/null | wc -l)
    echo "== bench/$d  ($n instances)" >> "$out"
    ( cd "$wt" && timeout 3600 ./probe --dual-only bench/"$d"/*.mps ) \
        > "$D/$d.out" 2> "$D/$d.err"
    echo "   probe exit=$?" >> "$out"
    echo "   solved=$(grep -c '^done' "$D/$d.out")" >> "$out"
    grep -E 'FORCING-CENSUS' "$D/$d.err" | sed 's/^/   /' >> "$out"
    grep -E 'Assertion' "$D/$d.err" | sed 's/^/   ABORT: /' >> "$out"
    echo >> "$out"
done

cat "$out"
echo "census -> $out"
