# 02-185 — OBJNAME, and the arm that says the tests are about the choice

`docs/format-support.md` said:

> **`OBJNAME` is not supported yet** (rejected loudly); no target instance
> uses it.

The reader hit its `unsupported section` branch and refused the file. The
section is how an MPS file with more than one free row says which one is the
objective, and CPLEX writes it. Without it the reader takes the first `N`
row, which is the rule every file that omits the section is written to, and
that rule stays exactly as it was.

## What is here

| file | what it does |
|---|---|
| `validate-d280.sh` / `.txt` | the two golden tests watched going red, twice, for two different reasons |

There is no population run. No gate instance carries an `OBJNAME` section,
so a campaign can only say the change is a no-op, and all three sets say
that: `bench/results/` is byte-identical.

## The three arms, and the second is the one worth having

| arm | what it does | what must happen |
|---|---|---|
| 1 | HEAD's `src/mps.c`, which refuses the section | both tests fail |
| 2 | the candidate with the section still parsed and **the rule removed**, so the first `N` row wins again | both tests fail, **on the cost** |
| 3 | restored | both green |

Arm 1 alone is weak: any change that broke the reader would fail the same
way, and it fails at the first assertion, `jaos_read_mps` returning
`JAOS_ERR_INVALID_INPUT`.

Arm 2 is the real one. The section is read, the name is stored, and only the
one line in `rd_rows_line` that consults it is taken out. The tests still go
red, and the reading in the record shows where:

```
tests/test_mps.c:53:test_t3_objname_picks_the_second_free_row:FAIL: Expected 3 Was 10
```

Line 53 is the cost assertion. `3` is `PROFIT`'s coefficient on `X` and `10`
is `IGNORED`'s. So the test fails because the wrong row was chosen, not
because the file did not parse — which is what makes it a test of the
feature rather than of the syntax.

## What the golden model pins, and why it is two rows and not one

`tests/data/t3_objname.mps` has two free rows and names the second:

```
OBJNAME
    PROFIT
ROWS
 N IGNORED
 N PROFIT
 E BALANCE
```

Read with the first-`N`-row rule the same text is a **different model**:
`IGNORED` would be the objective at `10x + 20y`, and `PROFIT` would be a
free row. Both are legal models, which is why the test checks the costs
field by field instead of checking that the read succeeded.

`IGNORED` survives as an ordinary free row with both bounds infinite, which
is what the standard says an extra `N` row is. Dropping it would renumber
every row after it, and a caller reading `jaos_solution`'s row activities
would get a different vector than the file describes.

## The four refusals

| file | message |
|---|---|
| `e_objname_missing.mps` | `no free row by that name` |
| `e_objname_late.mps` | `OBJNAME after ROWS` |
| `e_objname_twice.mps` | `a second OBJNAME` |
| `e_objname_notfree.mps` | `no free row by that name` |

The last one is worth reading. `OBJNAME` naming a `G` row does not get its
own message: the objective has to be a free row, so a `G` row never matches,
and what the reader reports is that no free row carries the name. That is
true and it is the same sentence a name matching nothing gets. A separate
message would need the reader to remember non-`N` names for the sake of a
better error, and it does not.

The missing-name case is reported at `COLUMNS` rather than at `ENDATA`,
because `COLUMNS` is the first line at which every row is known and the
error should point near the mistake.
