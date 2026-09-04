#!/bin/bash
# D268. The exact-arithmetic claim of absence in docs/claims.txt was silent
# through two commits that shipped half the feature. The replacement guards
# the verifier instead. A claim that never fires is not a guard, so this
# fires it on purpose, one candidate name at a time.
#
# Each name below is a way the verifier could plausibly be built. Every one
# must turn record-check red. A name that does not is a hole.
set -u
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 1

VICTIM=src/exact.c
cp "$VICTIM" /tmp/canary.keep || exit 1

echo "=== control: the tree as it stands ==="
python3 tools/record-check.py 2>&1 | tail -1

for name in jm_bareiss_step jm_exact_solve_lower jm_exact_factor \
            jm_exact_lu_build jm_verify_basis jaos_verify jm_exact_prove \
            jm_fraction_free_step jm_exact_basis_new; do
    printf '\n=== %s ===\n' "$name"
    printf '\nvoid %s(void);\n' "$name" >> "$VICTIM"
    python3 tools/record-check.py 2>&1 | grep -E "claims.txt|record-check: (PASS|[0-9]+ failure)" | tail -2
    cp /tmp/canary.keep "$VICTIM"
done

echo
echo "=== restored ==="
python3 tools/record-check.py 2>&1 | tail -1
diff -q "$VICTIM" /tmp/canary.keep && echo "src/exact.c identical"
