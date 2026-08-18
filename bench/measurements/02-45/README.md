# The exchange the reduction suggests is available and valid on 97% of firings

Taken 2026-08-18, testing the swap D134 said the repair has to be. Closed as
D135.

## The question

D134 established that `JM_PS_SINGLETON_COL` publishing a `BASIC` column is not
a status error but a basis one member too large: the column lands strictly
inside its own bounds, so it cannot rest on a bound and cannot be nonbasic,
and its row **survives**, so nothing is restored to pay for the position. 5902
of netlib's 17234 firings, 482 of Kennington's 602.

The exchange the reduction itself suggests is: **the column enters, and the
logical of the row it was substituted out of leaves.** That is also what
`JM_PS_IMPLIED_FREE_COL` already does — restore a row and a column, column
`BASIC`, row `AT_LOWER` — and that family reads drift 0 (D133).

Two things have to hold for it:

- **the partner has to be in the basis**, or there is nothing to take out;
- **the row has to rest on a bound**, or making its logical nonbasic claims a
  bound the row is not on.

## Both hold, nearly always

| | netlib | Kennington |
|---|---|---|
| firings publishing a `BASIC` column | 5902 | 482 |
| row logical `BASIC` — **partner available** | 5822 (98.6%) | **482 (100%)** |
| of those, row activity exactly on a bound — **swap valid** | **5714 (96.8%)** | **482 (100%)** |
| partner available, row not on a bound | 108 | 0 |
| row logical already at a bound — no partner | 80 | 0 |

**The rule works on 5714 of netlib's 5902 and on all 482 of Kennington's.**
The remainder is 188 netlib firings, 3.2%, in two named shapes: 108 where the
partner is in the basis but the row is not on a bound, and 80 where the
logical is already out.

The canary closes: 5902 and 482 are D134's counts exactly, so this is
measuring the same firings.

## The reading that had to be redone

The first pass judged tightness **inside** the replay and read 0 rows on a
bound, which would have said the swap is never valid. **A row's activity is
not final until every record touching it has replayed** — `ps_row_add`
accumulates into it — so that pass was reading partial sums. The probe now
tallies per row during the replay and classifies afterwards, with every
activity final. The number went from 0 to 5714.

## What is left open

**The 188.** A repair that handles the 5714 and leaves the rest is still
wrong by 188 on netlib, so the closing sum does not reach zero. Those two
shapes need their own answer before anything lands.

**And `SINGLETON_ROW`, untouched here.** D134's four combinations stand:
netlib is dominated by "row at a bound, column not set" (2524, one member too
few) and Kennington by "row BASIC, column BASIC" (29058, one too many). The
exchange for a family that is short a member is the mirror of this one — a
variable brought *in* — and no candidate has been measured for it.

Nothing is costed and no source file was touched.

## Reproducing it

`run-swap-candidate.sh`, beside this file. `src/` is read and never written;
the patch is applied in a throwaway worktree.
