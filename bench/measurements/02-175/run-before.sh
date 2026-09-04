#!/usr/bin/env bash
# D270's "before", measured rather than quoted.
#
# 02-173 is D267's evidence and it is TRUNCATED: it stops at pds-06, holds
# 108 of the 110 instances that have an optimum, and carries no summary
# line. Its entry claims 110 evaluated and 75 differing, and neither figure
# is in it. So the before half of this comparison is taken here instead, on
# the same day, the same machine and the same build flags as the after half.
#
# Method: put HEAD's src/check.c back, run the same probe, restore. Both the
# after file and the candidate source are saved first and checked back at the
# end rather than assumed.
set -u
D=bench/measurements/02-175
cd /mnt/c/Users/vall-/Desktop/projectes/jaos || exit 1

[ -s "$D/exact-cover.txt" ] || { echo "no after run to protect; aborting"; exit 1; }
cp "$D/exact-cover.txt" /tmp/after-exact-cover.d270 || exit 1
cp src/check.c /tmp/check.c.d270 || exit 1

git show HEAD:src/check.c > src/check.c || exit 1
echo "tree under test: $(grep -c 'long double' src/check.c) long double uses (HEAD's checker)"

bash "$D/run-exact-cover.sh" >/dev/null 2>&1
mv "$D/exact-cover.txt" "$D/before-exact-cover.txt"

cp /tmp/check.c.d270 src/check.c
cp /tmp/after-exact-cover.d270 "$D/exact-cover.txt"

echo
echo "restored, both checked:"
diff -q src/check.c /tmp/check.c.d270 && echo "  src/check.c is the candidate again"
diff -q "$D/exact-cover.txt" /tmp/after-exact-cover.d270 && echo "  the after run is intact"
echo "  candidate long double uses: $(grep -c 'long double' src/check.c)"

echo
echo "--- BEFORE (HEAD's long double checker) ---"; tail -2 "$D/before-exact-cover.txt"
echo "--- AFTER  (compensated double) ---";         tail -2 "$D/exact-cover.txt"
