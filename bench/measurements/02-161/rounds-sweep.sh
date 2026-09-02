#!/bin/bash
# Does SETTLE_ROUNDS_PRIMAL at 256 or 512 convert any of the four
# still-descending instances? In-place edit per setting with make clean
# between, the 02-157 discipline; restores the source and the record.
set -u
cd "$(dirname "$0")/../../.." || exit 2

set_rounds() {
    python3 - "$1" <<'PY'
import sys
v = sys.argv[1]
p = 'src/simplex.c'
s = open(p).read()
import re
m = re.search(r'constexpr int64_t SETTLE_ROUNDS_PRIMAL = (\d+);', s)
assert m, 'constant not found'
s = s.replace(m.group(0), 'constexpr int64_t SETTLE_ROUNDS_PRIMAL = %s;' % v, 1)
open(p, 'w').write(s)
print('SETTLE_ROUNDS_PRIMAL = %s' % v)
PY
}

for setting in 256 512; do
    echo "=== SETTLE_ROUNDS_PRIMAL = $setting ==="
    set_rounds "$setting"
    make clean >/dev/null 2>&1
    make primal J=12 2>&1 | tail -2
    grep -E "^(cycle|d6cube|modszk1|scsd8|stocfor3|truss|woodw) " \
        bench/results/primal.txt | awk '{print "  " $1 "  " $2}'
    grep "^measured" bench/results/primal.txt
done

set_rounds 128
make clean >/dev/null 2>&1
git checkout -- bench/results/primal.txt
echo "=== restored; source diff (must be empty): ==="
git diff --stat src/simplex.c
echo "=== done ==="
