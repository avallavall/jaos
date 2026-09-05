#!/usr/bin/env bash
# D277's "before": HEAD's `long double` checker, same machine, same flags,
# same day as the after half.
#
# Method: save the candidate `src/check.c`, put HEAD's back, run the same
# instrument, restore. The candidate source and the after file are both
# checked back at the end rather than assumed. Same shape as 02-175's
# run-before.sh, which is D270's.
#
# The control is in the output itself: the header line
# `# check.c long double uses:` must read a nonzero count for the before
# half and 0 for the after. If both read the same, the two halves are the
# same binary and the comparison measures nothing.
#
# Not a gate tool. Run from anywhere:
#   bash bench/measurements/02-182/run-before.sh
set -u
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here/../../.." || exit 1

[ -s "$here/dual-cover.txt" ] || {
    echo "no after run to protect; run run-dual-cover.sh first"; exit 1; }

save=$(mktemp -d) || exit 1
cp "$here/dual-cover.txt" "$save/after.txt" || exit 1
cp src/check.c "$save/check.c" || exit 1

restore() {
    cp "$save/check.c" src/check.c
    cp "$save/after.txt" "$here/dual-cover.txt"
}
trap 'restore; rm -rf "$save"' EXIT

git show HEAD:src/check.c > src/check.c || exit 1
echo "before tree: $(grep -c 'long double' src/check.c) long double uses in src/check.c"

bash "$here/run-dual-cover.sh" "$here/before-dual-cover.txt" >/dev/null 2>&1
rc=$?

restore
echo
echo "restored, both checked:"
diff -q src/check.c "$save/check.c" && echo "  src/check.c is the candidate again"
diff -q "$here/dual-cover.txt" "$save/after.txt" && echo "  the after run is intact"
echo "  candidate long double uses: $(grep -c 'long double' src/check.c)"
echo
echo "--- BEFORE header ---"; grep '^# ' "$here/before-dual-cover.txt" | head -8
echo "--- AFTER header  ---"; grep '^# ' "$here/dual-cover.txt" | head -8
exit $rc
