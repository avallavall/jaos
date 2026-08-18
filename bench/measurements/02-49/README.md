# The 80 are an exact degenerate tie the recovery division rounds off, and the swap's guard reads a status another family rewrites

Taken 2026-08-18, the re-measure `TODO.md` ordered before believing the 80
declines are a second defect. Closed as D140.

## The question

D135 counted 80 netlib firings whose row logical is "already nonbasic" when
the singleton column comes back interior, which contradicts the swap's own
derivation. Its probe read the PUBLISHED status and assumed in a comment that
"that row survives into the reduced model" without checking either. This
probe reads, at the exact moment `ps_singleton_col_swap`'s guard runs: did
the row survive, what status the reduced solve gave it, whether an earlier
swap of this same family already took the logical, and — after the first
pass failed its canary — what the column's status was at its OWN replay
write, not just what the guard reads.

## The first probe failed its canary, and the failure is the second finding

`run-reduced-status.sh` read 6132 records with a BASIC column against
D134/D135's 5902. The 230 extra are not singleton-column recoveries:
**`JM_PS_SINGLETON_ROW`'s replay writes `sol_col_status[j] = BASIC` for its
own column (`src/presolve.c:2132`), and that column can be one a
`SINGLETON_COL` record restored earlier in the same LIFO walk.** The swap's
guard (`src/presolve.c:1881`) then reads the rewrite, not the recovery.

`run-two-point.sh` stores the status at the replay write and classifies both
populations separately. Predictions stated in the script before the run:
netlib true=5902, phantom=230, lost=0; Kennington 482/0/0. All held exactly.

## The clean classification, netlib (both solves of each instance counted)

| of the 5902 true firings | count |
|---|---|
| swapped — the exchange fired | 5670 |
| declined: partner in the basis, row activity interior | 152 |
| declined: row did not survive (D135's unchecked assumption) | **0** |
| declined: an earlier swap of this family took the logical | 0 |
| declined: survived, reduced BASIC, overwritten by someone | 0 |
| declined: **the reduced solve left the logical nonbasic** | **80** |

Kennington: 482 of 482 swapped, every other class zero, phantom zero.

The sum closes: 5902 − 5670 = 232 unpaid, plus D136's 40 singleton rows
= +272, which is 02-48's published netlib SUM exactly.

## What the 80 are

All 80 survived, so D135's survival assumption was right; what it could not
see is why the logical is out. The reduced solve rests it **exactly on the
widened row bound** — 58 on the widened lower with status `AT_LOWER`, 22 on
the widened upper with status `AT_UPPER`, matching one for one. That is a
legitimate degenerate vertex of the reduced model.

At that vertex the exact recovery is the column AT its own bound: activity
exactly at `rl − a·hi` means `(rl − rest)/a = hi` in exact arithmetic. The
replay's division rounds it a few ulps inside, the interior test then calls
the column BASIC, and the basis gains a member nothing pays for. After the
full replay, 74 of the 80 rows land exactly ON their original bound — the
row is binding and its nonbasic logical is correct; the interior `xv` is
rounding noise from the recovery, not a second defect in the derivation.

## The 152, and why D135 said 108

D135 classified tightness after the replay loop but **before the final
carry fold** (`sol_row[i] = ps_published(sol_row[i] + rowc[i])` runs after
the point its probe instrumented), so 44 rows it called tight are loose on
the folded activity the shipped swap actually reads. 5714 − 44 = 5670
swapped, 108 + 44 = 152 loose. This is the fourth partial-activity misread
in five probes; the swap itself reads the folded value and is right.

## The phantom 230

On 230 netlib records the column published nonbasic at its own replay (202
`AT_LOWER`, 28 `AT_UPPER`) and the guard read `BASIC` because
`SINGLETON_ROW` rewrote it. **The swap fired on zero of them today**, so
this costs nothing on the current sets — but the guard's premise ("this
record's column came back interior") is not what it reads, and a firing
would take out a logical whose basis slot `SINGLETON_ROW` already paid with
its own restored row. The value test `sol_col[j] == rec->lo || == rec->hi`
reproduces the replay's own decision bit for bit (IEEE `==` ignores zero
sign, and `ps_published` only normalises −0.0) and cannot be rewritten.

## What is left open, handed to `TODO.md`

- The 80: a repair candidate exists — at the singleton-col replay the
  surviving row's status still holds the reduced solve's answer (nothing
  writes a surviving row's status during the replay), so a nonbasic status
  names the exact bound and the exact `xv`. Snapping to it moves published
  values by ulps and digests with them; it is a value change and gets the
  full gate, `numerics-reviewer` and `jaos-measurer`, not a status edit.
- The 152: reduced solve has the logical basic and the row interior; no
  local exchange exists. Unrepaired and now the largest single class.
- The guard hardening (status → value test): behaviour-identical today by
  this measurement, worth landing so the premise and the read agree.

## The hardening landed, same day

The value guard replaced the status read in `ps_singleton_col_swap`, with
two asserts enforcing that a recorded nonbasic status still matches the
value. Judged before landing: `make test` and `make sanitize` green;
`numerics-reviewer` delivered (first time in this run) with no correctness
findings and two low ones, both fixed — a stale 5714/108 count in the
comment above the guard, and the asserts themselves, which it proposed; all
three sets bit-identical to the committed records (94 + 29 + 16 instances,
110 digests and 29 verdicts unmoved); and the 02-48 published-count probe on
the candidate reading its committed numbers exactly (`guard-count.txt`,
beside this file).

## Reproducing it

`run-reduced-status.sh` (the canary failure that found the rewrite) and
`run-two-point.sh` (the clean two-point read), both beside this file with
their outputs. `src/` is read and never written; each applies its patch in a
throwaway worktree.
