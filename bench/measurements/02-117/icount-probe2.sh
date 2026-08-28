#!/bin/bash
# Does counting ONLY inside jaos_solve make the instruction count deterministic?
# Two runs on two instances. Identical or the instrument is not usable.
set -u
JAOS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$JAOS_ROOT" || exit 2
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
for inst in afiro adlittle; do
  for k in 1 2; do
    valgrind --tool=callgrind --toggle-collect=jaos_solve --callgrind-out-file="$D/cg.$inst.$k" \
        build/bench/run -j 1 -o "$D/out.$inst.$k" $inst > /dev/null 2>&1
    ir=$(grep -E '^summary:' "$D/cg.$inst.$k" | awk '{print $2}')
    printf "%-10s run %d: Ir inside jaos_solve = %s\n" "$inst" "$k" "$ir"
  done
done
echo "--- is the solver's own count sensitive to ASLR? (setarch -R disables it) ---"
setarch "$(uname -m)" -R valgrind --tool=callgrind --toggle-collect=jaos_solve --callgrind-out-file="$D/cg.noaslr" \
    build/bench/run -j 1 -o "$D/out.noaslr" afiro > /dev/null 2>&1
grep -E '^summary:' "$D/cg.noaslr" | awk '{print "afiro no-ASLR:", $2}'
