#!/bin/bash
# Calibrates the family counter against validate.c's known-answer model,
# dual-fixing arm included.
#
# The dual-fixing arm was the least validated of the three: the row and
# column counts had a known answer here since D101, the dual-fixing count
# did not, and an earlier version of exactly that arm called 421615
# Kennington columns fixable by ignoring row senses. validate.c now carries
# one column per rule -- two that must count, three that must not -- and
# this script requires the exact line.
#
# The armed run reintroduces the historical defect: the two-sided-row guard
# is deleted, so a row bounded on both sides is read as bounded above only.
# On this model that admits exactly one false candidate (x7), so the armed
# counter must read dualfix=8 where the real one reads 7 -- proof the guard
# is what the test exercises. The counter digest prints with each run
# because two arms sharing one source once looked like agreement (02-155).
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
#
# Run from anywhere; writes counts/validate.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-07
OUT="$HERE/counts/validate.txt"
WORK=$(mktemp -d)

cp -r src include "$WORK/" || exit 2
cp "$HERE/diag_families.inc" "$WORK/src/" || exit 2
cp "$HERE/validate.c" "$WORK/" || exit 2

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
print('counter wired in', file=sys.stderr)
PY
[ $? -eq 0 ] || { rm -rf "$WORK"; echo "PATCH FAILED" | tee "$OUT"; exit 2; }

build_and_run() {
    gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -DJAOS_DIAG \
        -I"$WORK/include" -I"$WORK/src" "$WORK"/src/*.c "$WORK/validate.c" \
        -o "$WORK/validate" -lm || { echo "BUILD FAILED"; return 2; }
    echo "  [$(md5sum "$WORK/src/diag_families.inc" | cut -c1-8)] $("$WORK/validate" 2>&1 | grep FAMILIES)"
}

{
echo "tree: $(git rev-parse --short HEAD)"
echo

clean=$(build_and_run)
echo "counter as shipped:"
echo "$clean"

python3 - "$WORK" <<'PY'
import sys
p = sys.argv[1] + '/src/diag_families.inc'
s = open(p).read()
old = "            if (has_lo && has_hi) { lo_ok = hi_ok = false; break; }\n"
assert s.count(old) == 1, 'the two-sided guard is not where this expects'
open(p, 'w').write(s.replace(old, "", 1))
PY
[ $? -eq 0 ] || { echo "ARM PATCH FAILED"; exit 2; }

armed=$(build_and_run)
echo
echo "armed, two-sided guard deleted (the historical defect):"
echo "$armed"
echo

WANT="FAMILIES liverows=8 livecols=10 remrow=2/2/2/2 remcol=1/1/1/1 dualfix=7"
ok=1
case "$clean" in *"$WANT"*) ;; *) ok=0; echo "clean run does not match: want [$WANT]";; esac
case "$armed" in *"dualfix=8"*) ;; *) ok=0; echo "armed run did not move to 8";; esac
d1=${clean%%]*}; d2=${armed%%]*}
[ "$d1" != "$d2" ] || { ok=0; echo "both runs share one counter source"; }
if [ $ok -eq 1 ]; then
    echo "VERDICT: PASS. The dual-fixing arm counts its two candidate rules,"
    echo "refuses its three non-candidate rules, and the armed defect is caught."
else
    echo "VERDICT: FAIL"
fi
} 2>&1 | tee "$OUT"

rm -rf "$WORK"
