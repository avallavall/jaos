# 02-111 — phase 1 was under-billed by `nvar` per iteration, and the honest number costs two instances

2026-08-26. `TODO.md` §0's remainder list: "`primal_phase1_costs` bills `nrow`
units for an `nvar` memset".

## The defect

```c
memset(s->c1, 0, (size_t)s->nvar * sizeof *s->c1);
for (int64_t i = 0; i < s->nrow; i++) { ... }
jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
```

`docs/work-units.md`'s rule is one unit per variable looked at. The clear
writes every one of `nvar` doubles and was billed nothing; only the `nrow`
scan was charged. Against **336660 phase-1 iterations across the standard
set** (D197) that is not a rounding error.

It bills `nvar + nrow` now.

## What it cost, and every instance paid

`compare.txt`, per instance against the parent:

| | |
|---|---|
| instances `ok` on both sides | 53 |
| **bit-identical primal work** | **0 of 53** |
| work geometric mean, honest against under-billed | **1.0625** |
| worst | `standata` **1.1759** |
| best | `grow22` 1.0007 |

The 29 that disagree on both sides moved too, worst `stocfor3` at 1.1586.

**Nothing on the gate moved.** All three sets `gate: PASS`, `0 regressed, 0
improved, 0 new`, every file in `bench/results/` byte-identical.
`primal_phase1_costs` is reachable only through `run_primal`, and only
`cfg.force_primal` reaches that.

## Four instances changed category, and they are not regressions

| instance | before | after |
|---|---|---|
| `bnl2` | DISAGREE | overrun |
| `tuff` | DISAGREE | overrun |
| `pilot-ja` | ok | **DISAGREE** |
| `standmps` | ok | **DISAGREE** |

Campaign totals: measured **55 → 53**, overrun **7 → 9**, disagreed 31, errors
1. Work geometric mean against the dual **3.9023 → 4.0039**.

**The solver does exactly the same arithmetic on all four.** The budget is
`10x` the dual's work units, so billing honestly exhausts it sooner: phase 1 or
the dual's re-entry now stops where it previously ran on. `tuff`'s phase 1 goes
from 843 iterations to 805 for that reason, and `wood1p`'s from 3820 to 3764.
**The work was always being done; the counter was not reporting it.**

`pilot-ja` and `standmps` end `NUMERICAL_ERROR` rather than `WORK_LIMIT`
because the budget runs out inside the dual's settling re-entry, leaving the
point dual infeasible for the D146 guard to refuse — the message-less refusal
D194 localised to `src/simplex.c:5496`.

Iterations by method move with it: phase 1 336660 → 325776 (39.5% → 38.8%),
phase 2 97 → 95 (0.0%), dual re-entry 515522 → 513203 (60.5% → 61.2%).

## What this exposes, and it is worth more than the fix

**The memset is `O(nvar)` per iteration to clear at most `nrow` entries.** Only
the basics that violate a bound are ever set, so at most `nrow` of `nvar`
positions are non-zero, and `nvar` is `ncol + nrow`. Clearing only what the
previous call set would make it `O(nrow)` and recover most of the 6.25% this
entry just charged.

That was invisible while the sweep was free. It is `TODO.md` §0's next item.

## Four records that described a solver that no longer exists

Landed with the same change, because they mislead a reader immediately and
this session has twice paid for a claim that outlived its code.

`run_primal`'s header opened `The primal simplex, phase 2 only`, and said
`There is no primal phase 1 yet` and `a cold start never gets here`. All three
have been false since phase 1 landed, and D195 measured the exact opposite of
the last: **0 of 94 skip phase 1**.

In `bench/primal.c`, `PRIMAL_UNREACHED`'s comment explained it as the primal
declining to start because there is no phase 1, the summary's `all_ok` comment
cited "while §0 stage 4 is open", and the message the runner **prints** said
the same wrong thing out loud. It now says phase 1 could not repair the point
it was given, which is what the verdict means, and it reads 0 on all 94.
