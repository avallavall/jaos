# 02-181 — the LP writer, once an empty row is a zero term (D276)

`SPECS.md` listed `jaos_write_lp` as partial with this missing: "a free row
and a row with no coefficients — each refused by name". D265 measured the
cost: **104 of the 139 gate instances round-tripped, 35 were refused, 34 of
them for an empty row** (`02-172/lpcover.txt`). The row said neither closes
with a reader change, "LP has no way to write a constraint with no terms".

That sentence is true and the conclusion drawn from it was not. LP has no
form for an empty constraint BODY. It has an ordinary form for a term whose
coefficient is zero, and `src/model.c` drops explicit zeros on load, so

    R1693: 0 C1 = 5

reads back as the empty row it was written from.

## What was run

```
bash bench/measurements/02-181/run-lpcover.sh
```

02-138's instrument, unchanged, on the tree with the writer change: write
every gate instance to LP, read it back, compare every field with `==`.
02-172's reading is D265's and is not overwritten — one file cannot carry two
trees.

## The answer

| | round-tripped | refused | differed |
|---|---|---|---|
| D265 (`02-172`) | 104 | 35 | 0 |
| **D276 (here)** | **138** | **1** | **0** |

The one left is a free row, which LP genuinely cannot say: a constraint with
no bound on either side is not a constraint, and the two-sided form takes
numbers rather than `inf`. `jaos_write_mps` has neither limit and every
refusal message points at it.

**`0 differed` is the figure that matters.** The instrument reads each file
back and compares every field exactly, so a writer that produced a valid file
saying something else would show here. It does not: the 34 instances that
moved from refused to written all came back byte for byte the model they went
out as.

## What this does not change

The zero term is written against column 0 every time, which is a fixed rule
and not a choice the data can influence (D8). A model with a row and no
columns at all is still refused, because there is no variable to hang the
term on, and `tests/test_write.c` has that case.

## Files

- `run-lpcover.sh` — the runner, linking release through `objs.sh`
- `lpcover.txt` — the reading
