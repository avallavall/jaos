# The pair sums to one only by accident, and the repair is a swap rather than a status

Taken 2026-08-18, counting the branches D133 said had to be counted before
anything is proposed. Closed as D134.

## `SINGLETON_ROW` decides two things with two independent tests

It restores one row, so it is worth exactly one basic variable. It writes two
statuses, by tests that never look at each other
(`src/presolve.c`, the `JM_PS_SINGLETON_ROW` case):

- **the column** is set `BASIC` only when `!zero_works && this_row_owns`;
- **the row** is `AT_LOWER`/`AT_UPPER` when its activity lands exactly on a
  bound, `BASIC` otherwise.

So `drift = (row BASIC) + (column set BASIC) − 1`, and only two of the four
combinations are right.

| combination | drift each | netlib | Kennington |
|---|---|---|---|
| row at a bound, column not set | **−1** | 2524 | 3886 |
| row at a bound, column set BASIC | 0 | 4200 | 48586 |
| row BASIC, column not set | 0 | 1372 | 116 |
| row BASIC, column set BASIC | **+1** | 526 | 29058 |
| | **net** | **−1998** | **+25172** |

**The sign flip between the sets is which wrong combination dominates.**
netlib is mostly "row at a bound, column not set"; Kennington is mostly "row
BASIC, column BASIC". A repair that only handles one direction fixes one set
and worsens the other.

6726 of netlib's 8622 firings are already right, and 48702 of Kennington's
81646.

## `SINGLETON_COL` is wrong every time it publishes BASIC

```c
orig->sol_col_status[j] =
    (xv == rec->lo) ? JAOS_BASIS_AT_LOWER :
    (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;
```

**Its row survives**, so nothing is restored to pay for a basis position.

| | netlib | Kennington |
|---|---|---|
| value at `rec->lo` or `rec->hi`, published nonbasic | 11332 | 120 |
| otherwise, published **BASIC** | **5902 (+5902)** | **482 (+482)** |

34% of netlib's firings and 80% of Kennington's.

## The canary closes

netlib `+3904`, Kennington `+25654` — identical to D133's per-set sums, which
were themselves identical to the published error. Every basic in the wrong
place is now traced to a named branch of a named family.

## Why the repair is not "stop writing BASIC"

**A strictly interior variable cannot be nonbasic.** `AT_LOWER` and `AT_UPPER`
are claims that the variable rests on that bound, and `SINGLETON_COL`'s
`BASIC` branch is reached precisely when it rests on neither. So the status is
not the error — the error is that the basis has one member too many, and
correcting it means taking a different variable *out*.

That makes this a swap, not a status choice: mark the interior column basic
**and** move some row's logical out of the basis, chosen so the resulting set
is still a basis. Nothing in postsolve does that today, and nothing here
proposes how.

The same argument applies to `SINGLETON_ROW`'s "row BASIC, column BASIC"
combination, which is 29058 of Kennington's firings. Its "row at a bound,
column not set" combination is the opposite problem — one member too few —
and needs a variable brought *in*.

## What is left open

The repair, and it now has: two families, four named branches, an exact count
per branch per set, and a closing sum for any candidate to be checked against.
Nothing is costed and no source file was touched.

## Reproducing it

`run-singleton-branches.sh`, beside this file. `src/` is read and never
written; the patch is applied in a throwaway worktree.
