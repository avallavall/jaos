# 02-283 — a position hint for the pivot row's values, refused

Taken on 2026-09-21 on aaceb4c (TODO B15). `compact_pivot_row` finds each
value of the pivot row by scanning the column for the pivot row, 3.5% of
pilot87's instructions (`../02-281/`). The candidate
(`position-hint-candidate.diff`) stores, beside each entry of a row's
pattern, the position its value took in the column when it was added.
Columns only move entries left when they drop some and put fill-in at the
end, so the value sits at or before the hint unless the row was refilled
after a drop; the search goes back from the hint and then forward past it,
and finds the one entry the old scan found. The four gates' instance lines
are byte-identical, work units included.

`tools/icount.sh -r aaceb4c`:

| instance | aaceb4c | candidate | ratio |
|---|---|---|---|
| maros-r7 | 11873354553 | 12040529689 | 1.01408 |
| pilot87 | 141783127741 | 140907141070 | 0.99382 |

Writing and growing the second array at every push costs more on maros-r7
than the shorter search saves, so the bar (down by 0.3% on both) fails.
