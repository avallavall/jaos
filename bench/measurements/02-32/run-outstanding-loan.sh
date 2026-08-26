#!/bin/bash
# PINNED: 5b92ead -- this script is evidence for that commit's tree; its anchors do not
# match later trees. Re-run it in a worktree of that commit, not against HEAD.
# Is a loan still outstanding when publish() writes the duals?
#
# TODO.md §5a item 1. `refresh` re-runs `shift_to_feasible` over every
# variable when `repair_singular_basis` fired, and both `take_best_if_better`
# and `restore_settled` call it AFTER their own `repay_shifts`. On that path
# `reenter_after_settling` returns with loans back in the costs and nothing
# settles them before `classify_optimum` and `publish`. The objective is safe
# — `publish` builds it from `m->col_cost` — but `sol_dual` is a BTRAN of
# `s->cost` and `sol_redcost` is `s->d`, and both would carry the loan.
#
# Two parts, and the second is the one that makes the first mean anything:
#
#   PART 1  the assert under test, over all three sets, in an assert-enabled
#           build. It must never fire.
#   PART 2  a negative control. An instrument that finds nothing is worth
#           nothing until it is shown able to find something, so a COPY of
#           the tree gets one line that leaves a loan outstanding on the
#           OPTIMAL path and the assert must abort on it.
#
# Part 1 runs against src/ as it stands; part 2 writes only into a copy.
# The records go beside this script, never into bench/results/: the build
# aborts 11 instances on a pre-existing assert and its record is not a gate
# result.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
out="$here/outstanding-loan.txt"
cd "$root" || exit 9

exec > >(tee "$out") 2>&1

echo "OUTSTANDING LOAN AT publish() -- $(git -C "$root" rev-parse --short HEAD)"
echo

# ---------------------------------------------------------------- PART 1 --
# make cannot see a CFLAGS change (D82), so the tree is cleaned first or the
# sweep measures one binary twice.
make clean > /dev/null 2>&1
make build/bench/run EXTRA_CFLAGS=-UNDEBUG > "$here/build.log" 2>&1 || {
    echo "BUILD FAILED"; tail -40 "$here/build.log"; exit 1; }

# The canary: without it an empty result and a build with the asserts
# compiled out look exactly alike.
nm build/bench/run 2>/dev/null | grep -q assert_fail || {
    echo "CANARY FAILED: no __assert_fail symbol, asserts are not compiled in"
    exit 2; }
echo "canary: __assert_fail is linked, asserts are live"
echo

d=$(mktemp -d)
./build/bench/run -j 12 -o "$d/netlib.txt" \
    -b bench/netlib.baseline > "$d/netlib.log" 2>&1
./build/bench/run -j 12 -m bench/netlib-infeas.manifest -e infeasible \
    -d bench/instances-infeas -b bench/netlib-infeas.baseline \
    -o "$d/infeas.txt" > "$d/infeas.log" 2>&1
./build/bench/run -j 12 -m bench/netlib-kennington.manifest \
    -d bench/instances-kennington -b bench/netlib-kennington.baseline \
    -o "$d/kennington.txt" > "$d/kennington.log" 2>&1

printf '%-20s %9s %9s %16s\n' set answered aborted publish-assert
for f in netlib infeas kennington; do
    # WORKER-FAILED is printed once per instance and again on every predicate
    # the baseline diff below it reports, so the instances are counted by
    # name rather than by line.
    printf '%-20s %9d %9d %16d\n' "$f" \
        "$(grep -c '^\[' "$d/$f.log")" \
        "$(grep 'WORKER-FAILED' "$d/$f.log" | awk '{print $1}' | sort -u | wc -l)" \
        "$(grep -c ': publish: Assertion' "$d/$f.log")"
done
echo
echo "-- every assert that did fire, by text --"
cat "$d"/*.log | grep -o "[a-z_]*\.c:[0-9]*: [A-Za-z_]*: Assertion \`[^']*'" \
    | sort | uniq -c
echo
echo "-- baseline diff, assert build --"
for f in netlib infeas kennington; do
    printf '%-12s %s\n' "$f" "$(tail -1 "$d/$f.log")"
done
rm -rf "$d"

# ---------------------------------------------------------------- PART 2 --
echo
echo "== NEGATIVE CONTROL: the assert must be able to fire =="
c="$root/build/diag/02-32"
rm -rf "$c"; mkdir -p "$c/src" "$c/include"
cp "$root"/src/*.c "$root"/src/*.h "$c/src/" || exit 3
cp "$root"/include/*.h "$c/include/"         || exit 3

python3 - "$c/src/simplex.c" <<'PY' || exit 4
import sys
p = sys.argv[1]
src = open(p).read()
anchor = """        if (st == JAOS_OK)
            st = publish(&s, outcome, &p);"""
inject = """        if (st == JAOS_OK) {
            if (outcome == JAOS_SOLVE_OPTIMAL) {   /* NEGATIVE CONTROL */
                s.cost[0] += 1.0;
                s.shift[0] += 1.0;
            }
            st = publish(&s, outcome, &p);
        }"""
assert src.count(anchor) == 1, "anchor not unique: %d" % src.count(anchor)
open(p, "w").write(src.replace(anchor, inject))
print("one loan left outstanding on the OPTIMAL path, immediately before publish")
PY

gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -UNDEBUG \
       -I"$c/include" -I"$c/src" "$c"/src/*.c bench/run.c -o "$c/run" -lm \
       2>&1 | tail -5
[ -x "$c/run" ] || { echo "CONTROL BUILD FAILED"; exit 5; }
"$c/run" -j 1 -o /dev/null afiro 25fv47 adlittle 2>&1 | tail -5
rm -rf "$c"
