#!/bin/bash
# Are the two guards in src/inflate.c load-bearing?
#
# Both were added by reading the code rather than by a failing test, which is
# how a guard that protects nothing gets written. Each is removed in turn and
# its test must go red. A test that still passes with its guard gone is not
# evidence of anything.
#
# No `trap` here, deliberately. Bash runs an EXIT trap inside command
# substitution too, so a restore-on-exit trap fires on the first `$(...)`,
# deletes its own backup, and leaves the source patched. That happened once
# while this script was being written.
#
# Run from anywhere; writes guard-arms.txt beside this script.
set -u
cd "$(dirname "$0")/../../.." || exit 1
HERE=bench/measurements/02-152
OUT="$HERE/guard-arms.txt"
KEEP="$HERE/inflate.c.keep"

cp src/inflate.c "$KEEP" || exit 1

count_failures() {
    if ! make build/dev/test_inflate >/dev/null 2>&1; then
        echo "build-failed"
        return
    fi
    ./build/dev/test_inflate 2>&1 | grep -c ':FAIL'
}

{
git rev-parse --short HEAD > "$HERE/.head"
echo "tree: $(cat "$HERE/.head")"
rm -f "$HERE/.head"
echo

python3 - <<'PY'
s = open('src/inflate.c').read()
old = """    bool any_dist = false;
    for (int k = 0; k < ndist; k++)
        if (lengths[nlit + k] != 0)
            any_dist = true;
    if (any_dist) {
        if (!huff_build(&dist, lengths + nlit, ndist))
            BAD("compressed input: distance code set is not a Huffman code");
    } else {
        memset(&dist, 0, sizeof dist);
    }"""
new = """    if (!huff_build(&dist, lengths + nlit, ndist))
        BAD("compressed input: distance code set is not a Huffman code");"""
assert old in s, 'the distance-tree branch is not where this script expects'
open('src/inflate.c', 'w').write(s.replace(old, new))
PY
n1=$(count_failures)
cp "$KEEP" src/inflate.c

python3 - <<'PY'
s = open('src/inflate.c').read()
i = s.index('uint32_t head_crc')
j = s.index('}', s.index('the header");'))
open('src/inflate.c', 'w').write(s[:i] + s[j:])
PY
n2=$(count_failures)
cp "$KEEP" src/inflate.c

n0=$(count_failures)

echo "arm 1, a distance tree of all-zero lengths refused: $n1 test(s) fail"
echo "arm 2, the gzip header checksum ignored:            $n2 test(s) fail"
echo "restored, no arm applied:                           $n0 test(s) fail"
echo
if [ "$n1" = "1" ] && [ "$n2" = "1" ] && [ "$n0" = "0" ]; then
    echo "VERDICT: PASS, each guard is the only thing keeping its test green"
else
    echo "VERDICT: FAIL, an arm did not move exactly one test"
fi
} 2>&1 | tee "$OUT"

cp "$KEEP" src/inflate.c
rm -f "$KEEP"
