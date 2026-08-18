# The 186 loans were never lost: 02-29 measured its own tally

Taken 2026-08-18, following `TODO.md` §5a item 1. One probe, one file. **It
corrects 02-29**, whose `unbalanced=186` was read as loans going missing, and
it closes the item. Closed as D124.

## The question, and why 02-29 could not answer it

02-29 tallied every lend against every repayment, per variable, on `pilotnov`
under D118's refused presolve candidate:

```
unbalanced=186  worst_balance=256 (v=2172)
```

`TODO.md` carried it as *"186 loans go missing, and nothing explains it"*. The
tally has a defect available to it that looks exactly the same:

- `g_lent[v]` is **one** accumulator over every loan of the whole solve.
- `s->shift[v]` is reset to zero at every repayment, so what a repayment hands
  to `g_repaid[v]` is a **partial** sum, and `g_repaid[v]` is the sum of those
  partial sums.

Adding *k* terms in one accumulator and adding them in segments are not the
same number in floating point. So `lent != repaid` is what a lost loan looks
like **and** what re-association looks like, and 02-29 could not tell them
apart.

## The discriminator

A third tally, `g_lent_bysegs[v]`: the same loans, accumulated the way the
repayments are — per segment, then added up. And a fourth counter for the one
thing reading the source cannot settle, that `s->shift` is written at the
three patched sites and nowhere else: at every repayment the segment
accumulator must equal `s->shift[v]` exactly.

`pilotnov`, under the candidate, at HEAD's repayment code:

| tally | unbalanced | worst |
|---|---|---|
| one accumulator (02-29's) | **221** | −0.5 at v=1791 |
| by segment | **0** | 0 |

```
shift written elsewhere: 0 mismatch(es)
loans still outstanding:  0
worst one-accumulator v=1791:
    lent=4369593160644541.5  repaid=4369593160644542
    lends=125  repays=6  ulp(lent)=0.5
```

**Every loan is repaid, bit for bit.** The worst disagreement in the whole
solve is **exactly one ulp** of the total: 125 loans added in one accumulator
against the same 125 added in six segments, on a total of 4.37e15 whose ulp is
0.5. A lost loan of 0.5 would be a coincidence; one ulp is the signature of
re-association.

The count is 221 rather than 02-29's 186 because D122 changed what a repayment
does, which changes the trajectory. The shape did not change.

`pilot-ja` and `dfl001` are balanced on both tallies, on both binaries, and at
HEAD `pilotnov` is balanced on both as well: no loan on the shipping
trajectory grows to a magnitude where the re-association is visible at all.

## The negative control

An instrument that finds nothing is worth nothing until it is shown able to
find something. `-DJAOS_NEGCTL` drops one variable's loan on the floor the
moment it is lent — the cost keeps the money and the record forgets it, which
is what a lost loan is. Both counters see it:

```
one-accumulator: unbalanced=222  worst=5.0117784483782649e+30 (v=7)
by-segment:      unbalanced=1    worst=5.0117784483782649e+30 (v=7)
shift written elsewhere: 11 mismatch(es)
worst one-accumulator v=7: lent=5.0117784483782649e+30 repaid=0
                           lends=269 repays=11
```

## The number D122 borrowed from the wrong line

02-29's `cost-drift.txt` reports `moved=67 shift_still_pending=0`: 67 columns
whose cost moved while the record came back to zero. Its `loan-balance.txt`
reports `unbalanced=186`, which is a different property of a different set of
columns. D122's entry and two source comments cited **186** for the
cost-moved-record-zero shape. **The number for that shape is 67.** The
comments carry 67 now and say where the wrong one came from; D122's text
stands, corrected here.

## What is left open

`TODO.md` §5a's remaining item, unchanged and untouched by this: nothing
bounds a loan relative to the cost it lands on.

## Reproducing it

`run-loan-attribution.sh`, beside this file. Three binaries with distinct
md5s, printed, because a probe measured twice looks like a clean result (D82).
`src/` is read and never written; the patch is applied in a throwaway
worktree.
