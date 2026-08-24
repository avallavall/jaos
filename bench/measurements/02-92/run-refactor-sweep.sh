#!/bin/bash
# D180 — is every answer still right at every refactorization interval?
#
# `REFACTOR_EVERY` decides how many Forrest-Tomlin updates accumulate before
# the basis is factorized again. It changes the numerical trajectory and it
# must not change whether an answer is correct. `TODO.md` carries the sweep as
# a standing debt: three of M1's four defect closures came from running it by
# hand and D119 is the fourth, and no target automates it.
#
# An instance that is `objective=ok checker=ok` at the shipping 64 and not at
# some other interval is a defect the gate cannot see, because the gate builds
# one binary.
#
# Each setting is its own tree and its own binary. `make` does not track a
# change in a constant it did not see, so a sweep that patches in place
# measures one binary N times (D82). The md5 beside each row is the canary and
# the run aborts if two settings share one.
#
# The worktree goes in $(mktemp -d), outside the repository: `make clean` is
# `rm -rf build`, and `make configs` runs it between five configurations, so a
# worktree under build/ is deleted mid-campaign by anyone else's build (D166).
#
# Usage:  run-refactor-sweep.sh [J] [setting ...]      default J=12, 8..256
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9

J="${1:-12}"; shift 2>/dev/null || true
SETTINGS=("$@")
[ ${#SETTINGS[@]} -gt 0 ] || SETTINGS=(8 16 32 64 128 256)

D=$(mktemp -d) || exit 2
trap 'rm -rf "$D"; git -C "$root" worktree prune' EXIT

out="$here/refactor-sweep.txt"
: > "$out"
seen_md5=""

{
echo "# D180 — every gate answer at every refactorization interval, at $(git rev-parse --short HEAD)."
echo "# One tree and one binary per setting. The md5 is the canary."
echo "# J=$J, settings: ${SETTINGS[*]}   (64 ships)"
echo
} | tee -a "$out"

for r in "${SETTINGS[@]}"; do
    wt="$D/wt-$r"
    git worktree add --detach "$wt" HEAD >/dev/null 2>&1 || { echo "worktree add failed for $r"; exit 2; }
    for d in instances instances-infeas instances-kennington; do
        rm -rf "$wt/bench/$d"
        ln -s "$root/bench/$d" "$wt/bench/$d"
    done
    sed -i "s/^constexpr int64_t REFACTOR_EVERY = 64;/constexpr int64_t REFACTOR_EVERY = $r;/" \
        "$wt/src/simplex.c"
    got=$(grep -c "REFACTOR_EVERY = $r;" "$wt/src/simplex.c")
    if [ "$got" -ne 1 ]; then
        echo "PATCH FAILED at $r: the constant was not rewritten" | tee -a "$out"; exit 3
    fi
    ( cd "$wt" && make bench >/dev/null 2>&1 ) || { echo "build failed at $r" | tee -a "$out"; exit 2; }
    md5=$(md5sum "$wt/build/bench/run" | cut -c1-12)
    case " $seen_md5 " in
        *" $md5 "*) echo "CANARY FAILED: setting $r built the same binary as an earlier one ($md5)" | tee -a "$out"; exit 3;;
    esac
    seen_md5="$seen_md5 $md5"

    {
      echo "######## REFACTOR_EVERY = $r   (md5 $md5) ########"
    } | tee -a "$out"
    ( cd "$wt" && ./build/bench/run -j "$J" -o "$D/netlib-$r.txt" >/dev/null 2>&1 )
    ( cd "$wt" && ./build/bench/run -j "$J" -m bench/netlib-infeas.manifest \
          -e infeasible -d bench/instances-infeas -o "$D/infeas-$r.txt" >/dev/null 2>&1 )
    for set in netlib infeas; do
        f="$D/$set-$r.txt"
        n=$(grep -cE "^[a-z0-9]" "$f" 2>/dev/null || true)
        bad_obj=$(grep -c "objective=OUT-OF-TOLERANCE" "$f" 2>/dev/null || true)
        bad_chk=$(grep -c "checker=REJECTED" "$f" 2>/dev/null || true)
        bad_det=$(grep -c "det=DIVERGED" "$f" 2>/dev/null || true)
        iters=$(grep -oE "iters=[0-9]+" "$f" 2>/dev/null | cut -d= -f2 | paste -sd+ | bc)
        echo "  $set: lines=$n  objective-bad=$bad_obj  checker-bad=$bad_chk  det-bad=$bad_det  total-iters=${iters:-0}" | tee -a "$out"
        if [ "$bad_obj" != "0" ] || [ "$bad_chk" != "0" ]; then
            grep -E "objective=OUT-OF-TOLERANCE|checker=REJECTED" "$f" \
                | awk '{print "    FIRING " $1 }' | tee -a "$out"
        fi
    done
    cp "$D/netlib-$r.txt" "$here/record-netlib-$r.txt" 2>/dev/null
    echo | tee -a "$out"
    git worktree remove --force "$wt" >/dev/null 2>&1
done

{
echo "######## which instances change verdict across the sweep ########"
python3 "$here/compare-sweep.py" "$here" "${SETTINGS[@]}"
} | tee -a "$out"
