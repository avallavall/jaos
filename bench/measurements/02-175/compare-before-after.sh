#!/usr/bin/env bash
# D270. What compensating the checker's primal walk bought, read against the
# exact arithmetic that judges both.
#
# before-exact-cover.txt is the same probe run against HEAD's uncompensated
# long double checker, on the same day and flags. The exact
# column is the same in both by construction -- jm_exact_evaluate did not
# change -- so any move is the checker's.
#
# Columns of exact-cover.txt:
#   instance status rows terms obj_ulps row_chk row_exact secs
set -u
cd "$(dirname "$0")/../../.." || exit 1
BEFORE=bench/measurements/02-175/before-exact-cover.txt
AFTER=bench/measurements/02-175/exact-cover.txt

for f in "$BEFORE" "$AFTER"; do
    [ -s "$f" ] || { echo "missing or empty: $f"; exit 1; }
done

echo "before: $(grep -m1 '^# tree:' $BEFORE)"
echo "after : $(grep -m1 '^# tree:' $AFTER)"
echo

join -j 1 \
  <(awk '!/^#/ && NF >= 7 && $2 == "optimal" {print $1, $5, $6, $7}' "$BEFORE" | sort) \
  <(awk '!/^#/ && NF >= 7 && $2 == "optimal" {print $1, $5, $6, $7}' "$AFTER" | sort) \
| awk '
{
    inst = $1
    obulps_b = $2 + 0; rchk_b = $3 + 0; rex_b = $4 + 0
    obulps_a = $5 + 0; rchk_a = $6 + 0; rex_a = $7 + 0
    n++
    if (rex_b != rex_a) { exactmoved++; exactlist = exactlist " " inst }
    if (obulps_b != 0) objdiff_b++
    if (obulps_a != 0) objdiff_a++
    if (rchk_b != rex_b) rowdiff_b++
    if (rchk_a != rex_a) rowdiff_a++
    if (rchk_b != rex_b && rchk_a == rex_a) { fixed++; fixedlist = fixedlist " " inst }
    if (rchk_b == rex_b && rchk_a != rex_a) { broke++; brokelist = brokelist " " inst }
    if (rchk_b != rchk_a) moved++
}
END {
    printf "instances compared (both optimal): %d\n\n", n
    printf "  exact column moved (must be 0)          : %d%s\n", exactmoved, exactlist
    printf "  objectives differing from exact, before : %d\n", objdiff_b
    printf "  objectives differing from exact, after  : %d\n", objdiff_a
    printf "  worst-row differing from exact, before  : %d\n", rowdiff_b
    printf "  worst-row differing from exact, after   : %d\n", rowdiff_a
    printf "  checker row figure moved at all         : %d\n", moved
    printf "\n  now agreeing that did not (%d):%s\n", fixed, fixedlist
    printf "  now disagreeing that did (%d):%s\n", broke, brokelist
}'
