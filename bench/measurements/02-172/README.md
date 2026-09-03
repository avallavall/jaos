# 02-172 — what jaos_write_lp covers at HEAD, after D239

D265. The same instrument as 02-138's (`../02-138/lpcover.c`, unchanged) on
a later tree. D239 taught `jaos_read_lp` the two-sided form, so the writer
stopped refusing a ranged row. Nothing else about the writer moved.

## Why this is not a re-run of 02-138

02-138's `lpcover.txt` is D226's reading. Re-running the script in place
would replace the evidence for the decision that directory closed, and the
file would then carry two trees with no label saying so. This directory
carries the D239-and-later reading; 02-138 keeps its own.

## What is here

| file | what it does |
|---|---|
| `run-lpcover.sh` | builds `../02-138/lpcover.c` against the dev objects and runs it over all 139 gate instances. Writes `lpcover.txt` |
| `lpcover.txt` | one line per instance: `ok`, or `refused` with the class and the writer's own message |

## The reading

**104 of the 139 gate instances round-trip through the LP writer and 35 are
refused: 34 for an empty row, 1 for a free row, 0 for a ranged row.** Of the
104 that round-trip, 0 differed on read-back.

Before D239 the same instrument read 102 / 37 with 2 ranged
(`../02-138/lpcover.txt`). The two ranged instances are the whole of the
change, and they are the whole of what D239 predicted would close.

## What this settles

`SPECS.md` and `TODO.md` both carried the count. `SPECS.md` had 104 / 35 and
cited a re-run whose output was never committed; `TODO.md` and
`docs/format-support.md` still had 102 / 37 and still said the reader
rejects a ranged row. The number has one owner now and it is this file.

## What this does NOT say

It does not say the remaining 35 could be made to round-trip. A free row and
a row with no coefficients have no spelling in the dialect, which is a
statement about the format and not about this writer.
