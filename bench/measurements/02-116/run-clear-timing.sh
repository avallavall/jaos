#!/bin/bash
# Did D199's memset-to-scatter clear buy wall-clock time, or only work units?
#
# D199 replaced `primal_phase1_costs`'s `memset` over all `nvar` doubles with a
# scatter over the positions the last call set, and it was accepted on a work
# geometric mean of 0.9452 with byte-identical digests. No time ratio was
# taken. A review objected, with an argument worth testing: a `memset` moves
# 8*nvar bytes at 32-64 B/cycle, the replacement is `cleared` scattered 8-byte
# stores into an array of 0.5-2 MB, so break-even sits near `cleared` about
# `nvar/12` -- and the sampled density on this set is 11-13% of nvar. That is
# the one case where units and seconds can move in opposite directions.
#
# The gate cannot answer this. A cold start is dual feasible, so `make netlib`
# never enters phase 1 at all. `bench/primal` is the only campaign that does.
#
# Instances, and why these:
#   MOVERS   phase 1 is most of the primal solve, so the changed code is what
#            the seconds are made of -- ganges 94%, fit2d 89%, pilot-ja 42%,
#            fit2p 38%, pilot 37% of primal iterations.
#   CONTROLS the same harness on solves the change provably cannot speed up --
#            grow22 1% and grow15 4%. If these move as much as the movers, the
#            reading is the machine and not the change.
#
# Protocol is jaos-measure's: -j 1, two binaries on one machine in one session,
# alternated, minimum over rounds rather than mean, geometric mean of
# per-instance ratios. This host repeats to 6.27% (D93), so a mover ratio
# inside that band is not a result.
#
# The harness is each tree's own `bench/primal`. It carries the log-callback
# timing bias that a later commit removed, but it carries it identically on
# both sides, so it cancels in a ratio.
#
# Worktrees under mktemp -d, OUTSIDE the repository (make clean is rm -rf build).
set -u
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
cd "$root" || exit 2

NEW=f135e8b       # the scatter (D199)
OLD=4d1ca2d       # its parent, the memset
MOVERS="ganges fit2d pilot-ja fit2p pilot"
CONTROLS="grow15 grow22"
ROUNDS=5

D=$(mktemp -d) || exit 2
cleanup() {
    cd "$root" || exit
    git worktree remove --force "$D/old" 2>/dev/null
    git worktree remove --force "$D/new" 2>/dev/null
    git worktree prune
    rm -rf "$D"
}
trap cleanup EXIT

build() {   # $1 = ref, $2 = name
    git worktree add --detach "$D/$2" "$1" > /dev/null 2>&1 || return 1
    ln -s "$root/bench/instances" "$D/$2/bench/instances"
    ( cd "$D/$2" && make build/bench/primal > /dev/null 2>&1 ) || return 1
    grep -q 'n_c1_at' "$D/$2/src/simplex.c" && echo "  $2 ($1): scatter" \
        || echo "  $2 ($1): memset"
}

echo "# D199 memset-to-scatter: does it buy seconds?"
echo "# host repeatability 6.27% (D93). -j 1, min over $ROUNDS alternating rounds."
echo
echo "canary -- the two trees must differ in the clear, or this measures one binary twice:"
build "$OLD" old || { echo "OLD BUILD FAILED"; exit 2; }
build "$NEW" new || { echo "NEW BUILD FAILED"; exit 2; }
if cmp -s "$D/old/build/bench/primal" "$D/new/build/bench/primal"; then
    echo "  STOP: the two binaries are byte-identical"; exit 2
fi
echo "  ok: the two binaries differ"
echo

ALL="$MOVERS $CONTROLS"
for i in $ALL; do eval "min_old_$(echo $i | tr -c 'a-zA-Z0-9' _)=99999"; done
for i in $ALL; do eval "min_new_$(echo $i | tr -c 'a-zA-Z0-9' _)=99999"; done

for r in $(seq 1 $ROUNDS); do
    for t in old new; do
        ( cd "$D/$t" && ./build/bench/primal -j 1 -o "$D/$t-$r.txt" $ALL \
              > "$D/$t-$r.out" 2>&1 )
        while read -r name secs; do
            key=$(echo "$name" | tr -c 'a-zA-Z0-9' _)
            cur=$(eval echo "\$min_${t}_${key}")
            keep=$(awk -v a="$secs" -v b="$cur" 'BEGIN{print (a<b)?a:b}')
            eval "min_${t}_${key}=$keep"
        done < <(awk '/ dual .* s, primal /{print $1, $6}' "$D/$t-$r.out")
    done
    echo "round $r done"
done

echo
report() {
    echo "-- $1 --"
    printf "%-12s %10s %10s %8s\n" instance memset scatter ratio
    sum=0; n=0
    for i in $2; do
        key=$(echo "$i" | tr -c 'a-zA-Z0-9' _)
        o=$(eval echo "\$min_old_$key"); w=$(eval echo "\$min_new_$key")
        [ "$o" = "99999" ] && { printf "%-12s %10s\n" "$i" "NO READING"; continue; }
        printf "%-12s %10.4f %10.4f %8.4f\n" "$i" "$o" "$w" \
            "$(awk -v a="$w" -v b="$o" 'BEGIN{print a/b}')"
        sum=$(awk -v s="$sum" -v a="$w" -v b="$o" 'BEGIN{print s+log(a/b)}')
        n=$((n+1))
    done
    [ "$n" -gt 0 ] && awk -v s="$sum" -v n="$n" \
        'BEGIN{printf "geometric mean of per-instance ratios: %.4f\n", exp(s/n)}'
    echo
}
report "MOVERS (phase 1 is most of the solve)" "$MOVERS"
report "CONTROLS (the change cannot reach these)" "$CONTROLS"
echo "a ratio below 1.0 means the scatter is faster."
