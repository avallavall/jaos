# 02-183 — the LP reader folds a constant instead of refusing it (D278)

`src/lpfmt.c` refused a bare number inside a constraint expression:

```
line 4: constant term in a constraint; fold it into the right-hand side
```

It tells the caller to do arithmetic the reader can do itself. `3x + 5 <= 10`
and `3x <= 5` are the same constraint, and nothing about the first is
ambiguous. The reader folds it now, and both ends of a two-sided row shift
by it.

## What is here

| file | what it does |
|---|---|
| `run-lpcover.sh` / `lpcover.txt` | 02-138's coverage instrument, unchanged, over all 139 gate instances on the tree with the change |
| `validate-d278.sh` / `.txt` | the new test watched going red without the change, and the six refusals watched staying refused |

## The two questions, and both are answered

**Did the round trip move?** No, and it should not have.

| | round-tripped | refused | differed |
|---|---|---|---|
| D276 (`02-181`) | 138 | 1 | 0 |
| **D278 (here)** | **138** | **1** | **0** |

Same single refusal, a free row, and the same breakdown by first cause:
0 ranged, 1 free row, 0 empty row, 0 orphan column, 0 other. The writer
never emits a constant inside a constraint, so nothing it produces takes the
new path. A change to the reader that moved this table would have been the
finding.

**Is the test evidence?** `validate-d278.sh` puts HEAD's `src/lpfmt.c` back
and the new test fails there — HEAD refuses the file at its first
constraint, so `jaos_read_lp` returns `JAOS_ERR_INVALID_INPUT` and the first
assertion goes. Restored, it passes again.

**And is the change narrow?** That is the second arm and it is the one worth
having. A reader that started accepting a constant by loosening the term
rule would take other refusals with it, so both rejection suites must stay
green on the candidate: `el_int`, `el_rangedir`, `el_unkbound`,
`el_badchar` and `el_noend` are still refused, each with its own message and
its own line number. They are.

## What the test file covers, and why four shapes

`tests/data/g_const.lp` has one constraint per route through the parser:

| | | |
|---|---|---|
| `c1` | `x + 5 <= 10` | after the terms, one-sided |
| `c2` | `-3 + 2 y >= 7` | before them, and negative |
| `c3` | `3 <= x + y + 1 <= 8` | inside a range |
| `c4` | `x + 2 + 3 = 10` | two of them, on an equality |

`c2` is the one worth reading. A signed number at the head of a constraint
is a left-hand bound only when a relation follows it. Here a `+` follows, so
the parser pushes the number back with its sign folded in and it arrives as
an ordinary constant term — the same path `3 x + y >= 2` uses to keep its
`3` as a coefficient.

`c3` is the other. The constant sits between the two ends of the range, so
both ends shift by it. Shifting one end only would silently widen or narrow
the row, and the row would still look plausible.

The objective's own constants are unchanged and still land in `obj_offset`.
The test asserts that in the same breath, because it is what keeps the two
paths apart.
