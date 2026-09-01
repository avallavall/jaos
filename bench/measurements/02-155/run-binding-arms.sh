#!/bin/bash
# Do the Python binding's tests catch a binding bug?
#
# All 27 passed the first time they were run, which is when a suite is least
# trustworthy. The binding owns no arithmetic: everything it can get wrong is
# an argument in the wrong slot, an array of the wrong length, or a status
# code dropped on the floor. So each of those three is introduced on purpose
# and the suite has to go red.
#
# Arm 3 is the one worth reading. Dropping the status check does not make a
# call fail — it makes a FAILED call look like a successful one, which is the
# defect a binding is most likely to ship with and the least likely to be
# noticed.
#
# No `trap`: bash runs an EXIT trap inside command substitution too (02-152).
#
# Run from anywhere; writes binding-arms.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 2
HERE=bench/measurements/02-155
OUT="$HERE/binding-arms.txt"
KEEP="$HERE/jaos.py.keep"

cp python/jaos.py "$KEEP" || exit 2
make shared >/dev/null 2>&1 || { echo "shared library failed to build"; exit 2; }

# The digest is printed with every arm on purpose. An arm that silently ran
# against the previous arm's source reports that arm's failures, and the two
# results then look like agreement. That happened while this script was
# being written: arm 2 reported arm 1's six failures.
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
old = "            cost, cl, cu, rl, ru, num_nz, starts, idx, vals))"
assert old in s, 'the load call is not where this script expects'
open('python/jaos.py', 'w').write(
    s.replace(old, "            cost, cu, cl, rl, ru, num_nz, starts, idx, vals))", 1))
PY
a1=$(reds)
cp "$KEEP" python/jaos.py

# Arm 2 flips the sign of every row dual and changes nothing else. The
# arrays keep their lengths and their slots, so only an assertion on a
# VALUE can catch it. A first version of this arm swapped the two array
# arguments instead; that overflows a buffer by one double and six
# unrelated tests fell over, which caught the arm without testing anything.
python3 - <<'PY'
s = open('python/jaos.py').read()
old = """        return Solution(list(cv[:nc]), list(ra[:nr]),
                        list(rd[:nr]), list(cd[:nc]))"""
assert old in s, 'the solution return is not where this script expects'
open('python/jaos.py', 'w').write(
    s.replace(old, """        return Solution(list(cv[:nc]), list(ra[:nr]),
                        [-v for v in rd[:nr]], list(cd[:nc]))""", 1))
PY
a2=$(reds)
cp "$KEEP" python/jaos.py

python3 - <<'PY'
s = open('python/jaos.py').read()
old = """    def _check(self, rc):
        if rc != Status.OK:
            raise JaosError(rc, self.error)
        return None"""
assert old in s, '_check is not where this script expects'
open('python/jaos.py', 'w').write(
    s.replace(old, """    def _check(self, rc):
        return None   # ARM 3: every status dropped""", 1))
PY
a3=$(reds)
cp "$KEEP" python/jaos.py

a0=$(reds)

echo "arm 1, col_lower and col_upper swapped:"; echo "$a1"
echo
echo "arm 2, every row dual's sign flipped:"; echo "$a2"
echo
echo "arm 3, every status code dropped:"; echo "$a3"
echo
echo "no arm applied:"; echo "$a0"
echo
# Each line is "[digest] failing tests", so the verdict reads past the
# digest. Four distinct digests are also required: two arms sharing one is
# two arms that ran the same source.
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
