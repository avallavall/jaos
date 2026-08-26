#!/bin/bash
# Is a cachegrind instruction count (a) obtainable on this host, (b) deterministic
# across two runs, (c) affordable? Three questions, one small instance, twice.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
[ -x build/bench/run ] || { echo "no build/bench/run"; exit 2; }
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
for k in 1 2; do
    t0=$(date +%s.%N)
    valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file="$D/cg.$k" \
        build/bench/run -j 1 -o "$D/out.$k" afiro > "$D/log.$k" 2>&1
    t1=$(date +%s.%N)
    ir=$(grep -E '^summary:' "$D/cg.$k" | awk '{print $2}')
    printf "run %d: I refs = %s   wall %.1fs\n" "$k" "$ir" "$(echo "$t1 - $t0" | bc)"
done
echo "--- native, for the slowdown factor ---"
t0=$(date +%s.%N); build/bench/run -j 1 -o "$D/out.n" afiro > /dev/null 2>&1; t1=$(date +%s.%N)
printf "native wall %.3fs\n" "$(echo "$t1 - $t0" | bc)"
