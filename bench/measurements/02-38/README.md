# A quarter of netlib and over half of Kennington lose the warm start outright

Taken 2026-08-18, refreshing the `warm` records and then explaining what they
say. Closed as D129.

## Why the records were rewritten

`bench/results/warm.txt` and its Kennington sibling were last written at
`44c0ef6`, an 01-03 commit, **21 `src/` commits ago**. `TODO.md` carried that
as a standing debt: a diff against them reports the whole of presolve and
cannot isolate a later change. The gate was green and the tree clean, so the
precondition for rewriting was met.

## The headline got much worse, and that is the finding

| | old record (pre-presolve) | new record |
|---|---|---|
| netlib, work units warm/cold, geometric mean | 0.0164 | **0.0696** |
| netlib, worst instance | `afiro` 0.5768 | `80bau3b` **1.0000** |
| Kennington, work units warm/cold | 0.0041 | **0.0873** |
| Kennington, worst instance | `pds-06` 0.0329 | `cre-a` **1.0000** |

A ratio of exactly 1.0000 means the warm re-solve did **bit-identical** work
to the cold one: `80bau3b` at 3511 iterations and 64249140 units either way,
`dfl001` at 21985 and 2744690896. No instance was at 1.0000 in the old record.

**26 of netlib's 92 measured instances read exactly 1.0000, and 6 of
Kennington's 11.** Three of netlib's 26 are branches that take zero iterations
on both sides and are identical for a legitimate reason — `pilotnov` 0/1021,
`scrs8` 0/137, `share1b` 0/1.

## Where the warm start goes

The driver solves three times per instance: the anchor, then warm, then cold
after `jaos_clear_basis`. So calls 0 and 2 are expected to find no basis, and
**call 1 is the whole question**. `run-warm-fallback.sh` reports which of
`build_warm_basis`'s two refusals fires on each.

| set | call 1 accepted | **count mismatch** | no basis stored |
|---|---|---|---|
| netlib | 66 | **23** | 0 |
| Kennington | 5 | **6** | 0 |

**23 and 23, 6 and 6.** The count mismatches equal the non-trivial 1.0000
instances exactly, on both sets. Attribution is not an inference here.

`nbasic != nrow` is `TODO.md`'s standing debt, and the debt says its cost is
"a lost warm start" with no number. The number is **25% of the standard set
and 55% of Kennington**. Nothing is stored-and-rejected for any other reason:
`no-basis` never fires on the warm solve.

**The mismatch is usually by one and is not always short.** Thirteen of
netlib's 23 are `nbasic = nrow - 1`. Three publish *more* basic variables than
rows — short by −10, −11 and −2 — which is a different shape from a missing
one. Kennington's worst is `nrow=3074 nbasic=3051`, short by 23.

## It is the defect, not presolve shrinking the denominator

Both explanations predict a worse ratio, so the two were separated by
computing the geometric mean over the instances that kept their warm start:

| | all | kept the warm start | lost it |
|---|---|---|---|
| netlib | 0.0696 | **0.0244** (66) | 1.0000 (26) |
| Kennington | 0.0873 | **0.0047** (5) | 1.0000 (6) |

**Kennington's survivors read 0.0047 against the old record's 0.0041** —
unchanged for practical purposes. netlib's read 0.0244 against 0.0164, worse
but the same order. The jump to 0.0696 and 0.0873 is the lost warm starts and
almost nothing else.

## What this does not say

**It is not a wrong answer and it is not a gate regression.** The fallback is
correct by construction: `build_warm_basis` refuses a basis it cannot trust
and the cold start is always right. No checker or digest reads a starting
status, which is exactly why 21 commits passed with nobody noticing. The three
gate sets are unaffected.

**And no repair is proposed or costed here.** `TODO.md` already carries the
defect with a named minimum case and a pinned test; this gives it a price.

## Reproducing it

`run-warm-fallback.sh`, beside this file, for the attribution. For the
records: `make warm J=12` and `make warm-kennington J=12`. `src/` is read and
never written by the probe.
