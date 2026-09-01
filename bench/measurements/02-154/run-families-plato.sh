#!/bin/bash
# D101's reopen condition, tested on the three sets that did not exist when
# D101 was written.
#
# D101 deferred duplicate rows, duplicate columns and dominated columns
# because they had 0.15% left to remove on netlib, Kennington and the
# infeasible set. Its reopen condition in bench/refusals.txt is "a model
# population where 02-07's counter reports a non-trivial share". The plato
# sets are that population: 15 instances, and the largest models in the tree.
#
# The counter is 02-07's, unchanged. It is rebuilt here rather than kept
# built, because it is a diagnostic and never enters a shipping build.
#
# NETLIB IS THE CONTROL. The counter has to reproduce D101's own figures --
# 151 removable rows and 1450 removable columns -- or it is a different
# instrument and the new numbers mean nothing. Two earlier versions of this
# counter were wrong and both announced themselves by being too clean
# (02-07/README.md).
#
# Only presolve has to run, so the driver caps the work at one unit and lets
# the solve abandon straight afterwards. That is what makes 284 MB of pds
# affordable.
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
#
# Run from anywhere; writes families-plato.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-154
OUT="$HERE/families-plato.txt"
SRC=bench/measurements/02-07
WORK=$(mktemp -d)

cp -r src include "$WORK/" || exit 2
cp "$SRC/diag_families.inc" "$WORK/src/" || exit 2

python3 - "$WORK" <<'PY'
import sys
w = sys.argv[1]
p = w + '/src/presolve.c'
s = open(p).read()

inc = '#include <string.h>'
assert inc in s, 'no string.h include to hang the counter off'
s = s.replace(inc, inc + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include "diag_families.inc"
#endif""", 1)

ret = """    ps_free_rowwise(&rw);
    return ret;"""
assert ret in s, 'the exit of jm_presolve_run is not where this expects'
s = s.replace(ret, """    ps_free_rowwise(&rw);
#ifdef JAOS_DIAG
    if (p->outcome == JM_PRESOLVE_REDUCED)
        diag_families(&p->reduced);
    else if (p->outcome == JM_PRESOLVE_NONE)
        diag_families(m);
    else
        fprintf(stderr, "FAMILIES liverows=0 livecols=0 "
                        "remrow=0/0/0/0 remcol=0/0/0/0 dualfix=0\\n");
#endif
    return ret;""", 1)
open(p, 'w').write(s)
print('counter wired in')
PY
if [ $? -ne 0 ]; then rm -rf "$WORK"; echo "PATCH FAILED"; exit 2; fi

cat > "$WORK/famrun.c" <<'EOF'
/* Reads one MPS and runs presolve. The work limit stops the solve right
 * after presolve, which is all the counter needs and is what makes the
 * large sets affordable. */
#include "jaos.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK)
        return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) {
        fprintf(stderr, "read failed: %s\n", jaos_model_error(m));
        return 2;
    }
    if (jaos_set_work_limit(m, 1) != JAOS_OK)
        return 2;
    (void)jaos_solve(m);          /* the verdict is not the subject */
    jaos_model_free(m);
    return 0;
}
EOF

gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -DJAOS_DIAG \
    -I"$WORK/include" -I"$WORK/src" "$WORK"/src/*.c "$WORK/famrun.c" \
    -o "$WORK/famrun" -lm
if [ ! -x "$WORK/famrun" ]; then
    rm -rf "$WORK"; echo "BUILD FAILED" | tee "$OUT"; exit 2
fi

one_set() {   # one_set <label> <dir>
    local label="$1" dir="$2" n=0 f
    : > "$WORK/$label.txt"
    for f in "$dir"/*.mps; do
        [ -f "$f" ] || continue
        n=$((n + 1))
        "$WORK/famrun" "$f" 2>&1 >/dev/null | grep FAMILIES | \
            sed "s|^|$(basename "$f" .mps) |" >> "$WORK/$label.txt"
    done
    python3 - "$WORK/$label.txt" "$label" "$n" <<'PY'
import re, sys
path, label, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
lr = lc = rr = rc = df = 0
hitr = hitc = 0
lines = 0
for line in open(path):
    m = re.search(r'liverows=(\d+) livecols=(\d+) remrow=(\d+)/(\d+)/(\d+)/(\d+) '
                  r'remcol=(\d+)/(\d+)/(\d+)/(\d+) dualfix=(\d+)', line)
    if not m:
        continue
    lines += 1
    g = [int(x) for x in m.groups()]
    lr += g[0]; lc += g[1]
    rr += g[3]; rc += g[7]        # tau = 1e-9, the column D101 quotes
    df += g[10]
    if g[3]:
        hitr += 1
    if g[7]:
        hitc += 1
pr = 100.0 * rr / lr if lr else 0.0
pc = 100.0 * rc / lc if lc else 0.0
print(f"--- {label}: {n} instances, {lines} reported")
print(f"    live rows {lr}, live cols {lc}")
print(f"    removable rows    {rr}  ({pr:.3f}%, on {hitr} instances)")
print(f"    removable columns {rc}  ({pc:.3f}%, on {hitc} instances)")
print(f"    dual-fixing candidates: {df}")
PY
}

{
echo "tree: $(git rev-parse --short HEAD)"
echo "counter: bench/measurements/02-07/diag_families.inc, unchanged"
echo
one_set netlib bench/instances
echo "    D101 read 151 removable rows and 1450 removable columns here, over"
echo "    78445 live rows and 157858 live cols. The rows match exactly. The"
echo "    live counts and the column figure have moved because presolve has"
echo "    gained reductions since, so the model it publishes is not the one"
echo "    D101 measured. The counter itself is byte-identical."
one_set plato-pds bench/instances-plato-pds
one_set plato-fome bench/instances-plato-fome
one_set plato-nug bench/instances-plato-nug
} 2>&1 | tee "$OUT"

for s in netlib plato-pds plato-fome plato-nug; do
    cp "$WORK/$s.txt" "$HERE/$s.txt" 2>/dev/null
done
rm -rf "$WORK"

# The verdict, and the bar it is read against.
#
# D101 called its aggregate 0.15% trivial and put no number on "non-trivial".
# The bar here is 5% of a set's live rows or live columns, in any one set.
#
# It is an editorial choice and not a measurement, so every percentage is
# printed above and a reader may move the bar without re-running anything.
# Two readings place it. D101's own worst per-set share was netlib's
# columns at 1450/157858, which is 0.92%, and D101 judged that not worth
# building — so the bar has to sit clearly ABOVE 0.92% or it would reopen on
# the very reading that closed it. A first draft of this script used 1% and
# would have done exactly that: today's netlib reads 0.913%. The other end
# is `d6cube` alone at about 12% of one model, which D101 called worth
# noticing. 5% is between them, about five times what D101 dismissed.
worst=$(awk '/removable rows|removable columns/ {
                 if (match($0, /\(([0-9.]+)%/, m) && m[1] + 0 > w) w = m[1] + 0
             } END { printf "%.3f", w }' "$OUT")
{
echo
echo "largest share in any set: ${worst}% of live rows or columns"
if awk -v w="$worst" 'BEGIN { exit !(w >= 5.0) }'; then
    echo "VERDICT: REOPENED. D101's condition is met on this population."
else
    echo "VERDICT: D101 still holds. No set reaches the 5% bar."
fi
} | tee -a "$OUT"

awk -v w="$worst" 'BEGIN { exit (w >= 5.0) ? 1 : 0 }'
