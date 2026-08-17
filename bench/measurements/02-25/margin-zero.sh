#!/bin/bash
# Does PRESOLVE_IMPLIED_FREE_ULPS explain why D106 fires zero times on fome?
#
# 02-25's counter says fome11/12/13 carry 166/332/664 equality implied-free
# column singleton candidates -- exactly D106's shape, exactly proportional to
# the doubling -- and the baseline run says presolve removes ZERO rows from all
# three. Columns are removed there, but by other families: D106 removes a column
# AND its row, and no row goes.
#
# The margin is the first suspect. docs/tolerances.md records it as a switch,
# not a dial: over the standard set, rows removed read 9992 at margin 0 and 8639
# at 1, 8, 64 and 4096.
#
# **The canary is maros-r7**, from that same entry: 4 of its 984 candidate rows
# sit at exact equality, so margin 0 must read 984 rows-removed where 8 reads
# 980. If the canary does not move, this script measured one binary twice and
# says so rather than reporting.
#
# make clean between settings, because EXTRA_CFLAGS does not invalidate objects.
#
# Usage (inside WSL, from anywhere): bash margin-zero.sh
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../../.." && pwd)
cd "$root" || exit 9
out="$here/counts"
mkdir -p "$out"

run_at() {   # run_at <label> <extra-cflags>
    local label=$1 flags=$2
    make clean > /dev/null 2>&1
    if ! make bench EXTRA_CFLAGS="$flags" > /tmp/mz-build-$label.log 2>&1; then
        echo "BUILD FAILED at $label"; tail -5 /tmp/mz-build-$label.log; exit 2
    fi
    md5sum build/release/presolve.o 2>/dev/null | cut -c1-12 \
        | sed "s/^/  $label presolve.o md5: /"
    ./build/bench/run -o /dev/null maros-r7 2>&1 | grep -oE 'presolve=[0-9/]+->[0-9/]+' \
        | sed "s/^/  $label maros-r7 /"
    ./build/bench/run -m bench/plato-fome.manifest -d bench/instances-plato-fome \
        -e noref -o /dev/null fome11 2>&1 | grep -oE 'presolve=[0-9/]+->[0-9/]+|iters=[0-9]+|work=[0-9]+' \
        | tr '\n' ' ' | sed "s/^/  $label fome11 /;s/$/\n/"
}

echo "===== margin 8 (shipping) ====="
run_at m8 "" | tee "$out/margin-8.txt"
echo
echo "===== margin 0 ====="
run_at m0 "-DJAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=0" | tee "$out/margin-0.txt"

echo
echo "===== restoring the shipping build ====="
make clean > /dev/null 2>&1
make bench > /dev/null 2>&1 && echo "restored"
