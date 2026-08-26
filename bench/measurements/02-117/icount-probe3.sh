#!/bin/bash
# The LTO build inlines jaos_solve. Which symbol survives, and does a wildcard
# toggle count it deterministically? Also try the deepest solver entry.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 2
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
echo "--- symbols that survive in build/bench/run ---"
nm -C build/bench/run 2>/dev/null | grep -E ' (T|t) (jaos_solve|jm_dual_simplex|jm_lu_ftran|run_primal|publish)' | head
for pat in 'jaos_solve*' 'jm_dual_simplex*'; do
  for k in 1 2; do
    valgrind --tool=callgrind --toggle-collect="$pat" --callgrind-out-file="$D/cg" \
        build/bench/run -j 1 -o "$D/out" adlittle > /dev/null 2>&1
    ir=$(grep -E '^summary:' "$D/cg" | awk '{print $2}')
    printf "%-20s run %d: Ir = %s\n" "$pat" "$k" "$ir"
  done
done
