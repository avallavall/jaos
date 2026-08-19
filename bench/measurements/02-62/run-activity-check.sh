#!/bin/bash
# The debug-build row-activity check: validate it, then run it.
#
# An instrument that finds nothing is worth nothing until it is shown able to
# find something. So this runs FOUR builds, in this order:
#
#   1. INJECT-OVERWRITE  a fault build where JM_PS_EMPTY_ROW overwrites a row
#      whose share already arrived — the exact defect class the check exists
#      for. The check MUST fire.
#   2. INJECT-DRIFT      a fault build that perturbs one published activity by
#      a hair more than the window. The check MUST fire.
#   3. CLEAN             the shipping tree, all three sets. This is where the
#      finding is: 138 of 139 pass and `pilotnov` does not.
#   4. NO-CHECK CONTROL  the same fault with -DNDEBUG, to confirm 1 and 2
#      abort because of the CHECK and not because the injected fault broke
#      something else first.
#
# The check is opt-in (-DJAOS_VERIFY_ACTIVITY) and every build here passes it.
# It is not on in a plain assert build, because pilotnov violates it and D152
# had just bought the property that all 94 standard instances run under
# -UNDEBUG.
#
# Every instance runs in its own process so one abort cannot hide another.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
wt="$root/build/diag/wt-02-62"
out="$here/activity-check.txt"
cd "$root" || exit 9

git worktree remove --force "$wt" >/dev/null 2>&1
git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed"; exit 2; }
git diff > "$wt/cand.diff"
cd "$wt" || exit 2
[ -s cand.diff ] && { git apply cand.diff || { echo "candidate did not apply"; exit 2; }; }
for dir in instances instances-infeas instances-kennington; do
    [ -d "$root/bench/$dir" ] && { rm -rf "bench/$dir"; ln -s "$root/bench/$dir" "bench/$dir"; }
done
cp src/presolve.c /tmp/presolve.pristine.c
d=$(mktemp -d)

# ---- the two fault builds -------------------------------------------------
inject () {
    cp /tmp/presolve.pristine.c src/presolve.c
    python3 - "$1" << 'PY'
import sys
mode = sys.argv[1]
p = "src/presolve.c"
s = open(p, encoding="utf-8").read()
if mode == "overwrite":
    # An empty row assigning outright over a share that already arrived is
    # the class the check exists for. Force the collision by seeding it.
    m = "        orig->sol_row[i] = 0.0;"
    assert s.count(m) == 1, "empty-row assignment not found"
    s = s.replace(m, "        orig->sol_row[i] = 0.0;   /* INJECTED: was 1.0 */\n"
                     "        orig->sol_row[i] = 1.0;")
elif mode == "drift":
    # One published activity moved by more than any window can absorb.
    m = "        assert(fabs(orig->sol_row[i] - act[i]) <= window);"
    assert s.count(m) == 1, "the check body was not found"
    s = s.replace(m, "        if (i == 0) act[i] += 1.0;   /* INJECTED */\n" + m)
open(p, "w", encoding="utf-8").write(s)
print(f"injected: {mode}")
PY
}

build () {
    gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 $2 \
        -DJAOS_VERIFY_ACTIVITY -Iinclude -Isrc src/*.c bench/run.c -o "$d/$1" -lm 2> "$d/$1.build.log"
}

# An instance can exit non-zero for two unrelated reasons: the assert
# aborting (what this measures) or the gate judging the instance failed
# (a script artifact if the expectation flag is wrong). They are counted
# separately, because conflating them is what made the first run of this
# script report 29 of 29 infeasible instances as assert failures when the
# real cause was a missing `-e infeasible`.
count_aborts () {
    binary=$1; manifest=$2; dir=$3; shift 3
    fired=""; gatefail=""; n=0
    while read -r name _; do
        case "$name" in \#*|"") continue;; esac
        n=$((n + 1))
        "$d/$binary" -j 1 -m "$manifest" -d "$dir" "$@" "$name" > "$d/o.log" 2>&1
        rc=$?
        if [ $rc -ne 0 ]; then
            if grep -qE "Assertion|assert" "$d/o.log"; then
                fired="$fired $name"
            else
                gatefail="$gatefail $name"
            fi
        fi
    done < <(awk '{print $1}' "$manifest")
    echo "asserts $(echo $fired | wc -w)/$n --$fired | gate-fail $(echo $gatefail | wc -w) --$gatefail"
}

{
echo "# The row-activity check, validated against the cases it must reject"
echo "# before it is believed on the cases it must accept."
echo

for mode in overwrite drift; do
    inject $mode > /dev/null
    if build "fault-$mode" "-UNDEBUG"; then
        r=$(count_aborts "fault-$mode" bench/netlib.manifest bench/instances)
        echo "1/2. INJECT $mode, asserts ON  -> aborted on $r"
        # and with the check compiled out, to show the fault alone is quiet
        if build "faultnc-$mode" "-DNDEBUG"; then
            r2=$(count_aborts "faultnc-$mode" bench/netlib.manifest bench/instances)
            echo "     the same fault with asserts OFF -> aborted on $r2"
        fi
    else
        echo "1/2. INJECT $mode: BUILD FAILED"
        tail -5 "$d/fault-$mode.build.log"
    fi
    echo
done

cp /tmp/presolve.pristine.c src/presolve.c
if build clean "-UNDEBUG"; then
    echo "3. CLEAN tree, asserts ON, all three sets:"
    echo "   netlib            $(count_aborts clean bench/netlib.manifest bench/instances)"
    echo "   netlib-infeas     $(count_aborts clean bench/netlib-infeas.manifest bench/instances-infeas -e infeasible)"
    echo "   netlib-kennington $(count_aborts clean bench/netlib-kennington.manifest bench/instances-kennington)"
else
    echo "3. CLEAN: BUILD FAILED"
    tail -20 "$d/clean.build.log"
fi
} 2>&1 | tee "$out"

rm -rf "$d"
cd "$root" || exit 2
git worktree remove --force "$wt" >/dev/null 2>&1
echo "saved to $out"
