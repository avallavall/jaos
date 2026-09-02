#!/bin/bash
# Per-round trajectory of reenter_after_settling on the seven instances
# that exhaust all 128 rounds: still descending, or oscillating?
set -u
cd "$(dirname "$0")/../../.." || exit 2
W=$(mktemp -d)

cp -r src include "$W/" || exit 2
cp bench/primal.c "$W/" || exit 2

python3 - "$W" <<'PY'
import sys
p = sys.argv[1] + '/src/simplex.c'
s = open(p).read()

anchor = '#define _POSIX_C_SOURCE 200809L'
assert s.count(anchor) == 1
s = s.replace(anchor, anchor + chr(10) + '#ifdef JAOS_DIAG' + chr(10) +
              '#include <stdio.h>' + chr(10) + '#endif', 1)

old = """    for (int64_t round = 0; round < rounds; round++) {
        /* Asked before anything is saved: the saving is the whole cost of a
         * round with nothing to repair. */"""
assert s.count(old) == 1
s = s.replace(old, """    for (int64_t round = 0; round < rounds; round++) {
#ifdef JAOS_DIAG
        fprintf(stderr, "DIAG-ROUND r=%lld viol=%.17g obj=%.17g\\n",
                (long long)round, settled_dual_violation(s),
                settled_objective(s));
#endif
        /* Asked before anything is saved: the saving is the whole cost of a
         * round with nothing to repair. */""", 1)
open(p, 'w').write(s)
print('patched', file=sys.stderr)
PY
[ $? -eq 0 ] || { rm -rf "$W"; echo "PATCH FAILED"; exit 2; }

gcc-14 -std=c23 -ffp-contract=off -O2 -g -DNDEBUG -DJAOS_DIAG \
    -I"$W/include" -I"$W/src" "$W"/src/*.c "$W/primal.c" \
    -o "$W/primal" -lm 2>&1 | grep -E "error" | head -5
[ -x "$W/primal" ] || { rm -rf "$W"; echo "BUILD FAILED"; exit 2; }

for inst in cycle d6cube modszk1 scsd8 stocfor3 truss woodw; do
    echo "== $inst"
    "$W/primal" "$inst" 2>&1 | grep "DIAG-ROUND" | \
        awk 'NR==1 || NR==2 || NR%16==0 || NR>122' | head -14
done
rm -rf "$W"
