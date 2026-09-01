#!/bin/bash
# Does the modeling layer's suite catch a layer bug?
#
# The 34 tests added with the layer passed the first time they were run,
# which is when a suite is least trustworthy (02-155 says the same about the
# binding's 27). The layer owns no arithmetic either, but it owns three
# things the binding did not: a change-tracking path that applies deltas to
# a loaded model, the marshalling of a 18-field report struct, and the
# folding of an expression's constant into a constraint's bounds. Each is
# broken on purpose and the suite has to go red.
#
# Arm 1 is the one worth reading. A dirty row bound that is never applied
# does not fail — the solve runs and answers the PREVIOUS question, which is
# the defect a warm re-solve path is most likely to ship with. The test that
# catches it judges the re-solve against a fresh build of the changed state,
# so agreeing with the stale model cannot agree with the fresh one.
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
#
# Run from anywhere; writes layer-arms.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-158
OUT="$HERE/layer-arms.txt"
KEEP="$HERE/jaos.py.keep"

cp python/jaos.py "$KEEP" || exit 2
make shared >/dev/null 2>&1 || { echo "shared library failed to build"; exit 2; }

# The digest is printed with every arm; 02-155 says why: two arms that ran
# the same source once reported the same failures and looked like agreement.
reds() {
    echo "  [$(md5sum python/jaos.py | cut -c1-8)] $(JAOS_LIBRARY="$PWD/build/release/libjaos.so" \
        python3 -m unittest discover -s python 2>&1 | \
        grep -E '^(FAIL|ERROR): ' | sed 's/^[A-Z]*: //; s/ .*//' | \
        sort | tr '\n' ' ')"
}

{
echo "tree: $(git rev-parse --short HEAD)"
echo

python3 - <<'PY'
s = open('python/jaos.py').read()
old = "                self._m.set_row_bounds(i, c._lo, c._hi)"
assert s.count(old) == 1, 'the delta application is not where this expects'
open('python/jaos.py', 'w').write(
    s.replace(old, "                pass  # ARM 1: the dirty bound is lost", 1))
PY
a1=$(reds)
cp "$KEEP" python/jaos.py

python3 - <<'PY'
s = open('python/jaos.py').read()
old = """        return CheckReport(*(getattr(rep, f)
                             for f, _ in _CheckReport._fields_))"""
assert s.count(old) == 1, 'the report marshalling is not where this expects'
open('python/jaos.py', 'w').write(
    s.replace(old, """        return CheckReport(*(getattr(rep, f)
                             for f, _ in reversed(_CheckReport._fields_)))""", 1))
PY
a2=$(reds)
cp "$KEEP" python/jaos.py

python3 - <<'PY'
s = open('python/jaos.py').read()
old = "        hi = -d._c if upper else INFINITY"
assert s.count(old) == 1, 'the comparison fold is not where this expects'
open('python/jaos.py', 'w').write(
    s.replace(old, "        hi = d._c if upper else INFINITY", 1))
PY
a3=$(reds)
cp "$KEEP" python/jaos.py

a0=$(reds)

echo "arm 1, a dirty row bound never applied on the warm path:"; echo "$a1"
echo
echo "arm 2, the check report's eighteen fields reversed:"; echo "$a2"
echo
echo "arm 3, a comparison's constant not folded into the bound:"; echo "$a3"
echo
echo "no arm applied:"; echo "$a0"
echo
fails() { printf '%s' "$1" | sed 's/^.*\] *//'; }
digest() { printf '%s' "$1" | sed 's/^ *\[\([0-9a-f]*\)\].*/\1/'; }

ok=1
[ -n "$(fails "$a1")" ] || ok=0
[ -n "$(fails "$a2")" ] || ok=0
[ -n "$(fails "$a3")" ] || ok=0
[ -z "$(fails "$a0")" ] || ok=0
uniq_digests=$(printf '%s\n%s\n%s\n%s\n' "$(digest "$a1")" "$(digest "$a2")" \
    "$(digest "$a3")" "$(digest "$a0")" | sort -u | wc -l)
[ "$uniq_digests" -eq 4 ] || ok=0
echo "distinct sources across the four runs: $uniq_digests of 4"
echo
if [ $ok -eq 1 ]; then
    echo "VERDICT: PASS, all three defect shapes are caught and a clean tree"
    echo "is green."
else
    echo "VERDICT: FAIL, an arm passed the suite or the clean tree did not"
fi
} 2>&1 | tee "$OUT"

cp "$KEEP" python/jaos.py
rm -f "$KEEP"
