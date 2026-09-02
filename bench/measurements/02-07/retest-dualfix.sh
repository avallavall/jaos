#!/bin/bash
# D246's reopen condition, executable. Dual fixing was refused because the
# validated counter reports candidates at 1.09% of fome's live columns and
# 0.67% of netlib's, against the 5% bar 02-154 argues. This re-runs the
# counter on the current tree over the two sets where the count is nonzero.
#
# Exit 0: the refusal holds (every set under 5%).
# Exit 1: a set reaches 5% -- reopen D246.
# Exit 2: could not run, or netlib stopped reproducing while presolve is
#         unchanged -- then this is a different instrument and its numbers
#         mean nothing until 02-07/run-validate.sh passes again.
#
# NETLIB IS THE CONTROL only in the calibrated sense: its count moves
# whenever presolve gains a family (the counter reads what presolve leaves),
# so the control is run-validate.sh's known-answer model, checked first.
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-07
BAR=5.0

"$HERE/run-validate.sh" >/dev/null 2>&1
grep -q "VERDICT: PASS" "$HERE/counts/validate.txt" || {
    echo "dualfix retest: the counter no longer passes its calibration"; exit 2; }

WORK=$(mktemp -d)
cp -r src include "$WORK/" || exit 2
cp "$HERE/diag_families.inc" "$WORK/src/" || exit 2

python3 - "$WORK" <<'PY'
import sys
w = sys.argv[1]
p = w + '/src/presolve.c'
s = open(p).read()
inc = '#include <string.h>'
assert inc in s
s = s.replace(inc, inc + """

#ifdef JAOS_DIAG
#include <stdio.h>
#include "diag_families.inc"
#endif""", 1)
ret = """    ps_free_rowwise(&rw);
    return ret;"""
assert ret in s
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
PY
[ $? -eq 0 ] || { rm -rf "$WORK"; echo "PATCH FAILED"; exit 2; }

cat > "$WORK/famrun.c" <<'EOF'
#include "jaos.h"
#include <stdio.h>
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    jaos_model *m = NULL;
    if (jaos_model_new(&m) != JAOS_OK) return 2;
    if (jaos_read_mps(m, argv[1]) != JAOS_OK) return 2;
    if (jaos_set_work_limit(m, 1) != JAOS_OK) return 2;
    (void)jaos_solve(m);
    jaos_model_free(m);
    return 0;
}
EOF
gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -DJAOS_DIAG \
    -I"$WORK/include" -I"$WORK/src" "$WORK"/src/*.c "$WORK/famrun.c" \
    -o "$WORK/famrun" -lm || { rm -rf "$WORK"; echo "BUILD FAILED"; exit 2; }

rc=0
for set in "netlib bench/instances" "fome bench/instances-plato-fome"; do
    label=${set%% *}; dir=${set##* }
    [ -d "$dir" ] || { echo "dualfix retest: $dir missing"; rc=2; continue; }
    : > "$WORK/lines.txt"
    for f in "$dir"/*.mps; do
        "$WORK/famrun" "$f" 2>&1 >/dev/null | grep FAMILIES >> "$WORK/lines.txt"
    done
    read -r df lc < <(awk '{
        for (i = 1; i <= NF; i++) {
            if ($i ~ /^dualfix=/)  { sub(/^dualfix=/,  "", $i); d += $i }
            if ($i ~ /^livecols=/) { sub(/^livecols=/, "", $i); c += $i }
        }} END { print d+0, c+0 }' "$WORK/lines.txt")
    share=$(awk -v d="$df" -v c="$lc" 'BEGIN { printf "%.2f", (c ? 100.0*d/c : 0) }')
    echo "$label: dualfix=$df of $lc live columns = $share%"
    over=$(awk -v s="$share" -v b="$BAR" 'BEGIN { print (s >= b) ? 1 : 0 }')
    [ "$over" -eq 1 ] && rc=1
done
rm -rf "$WORK"
if [ $rc -eq 0 ]; then echo "dualfix retest: refusal holds, every set under $BAR%";
elif [ $rc -eq 1 ]; then echo "dualfix retest: the $BAR% bar is reached -- reopen D246"; fi
exit $rc
