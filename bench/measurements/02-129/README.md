# 02-129 — `can_move`'s units: the two sets 02-128 could not take, and a second arm

`TODO.md` section 0 stage 6. **Landed** (D214). Two scripts:
`run-arms.sh` is the campaign, `run-nopresolve.sh` is the one check the
review could not settle by reading. `review.md` is the numerics review that
added the second arm, taken before either ran.

## What 02-128 left open

02-128 measured `can_move`'s units on `netlib` and the forced-primal campaign
and found no verdict moved. Its own README names what was missing: `pds-20`
is a Kennington instance, D27's whole cautionary case is about `pds-20`, and
`make netlib-kennington` was never run. `netlib-infeas` was missing for the
same reason.

## The base is a run, not the committed file

`preflight.sh` reported `bench/results/netlib-kennington.txt` and
`netlib-infeas.txt` written **before 25 `src/` commits**. A diff against them
would credit those commits to this change. A run of clean HEAD (`2ee580f`) on
2026-08-28 reproduced all three records **byte for byte**, so the 25 commits
were no-ops on both sets and the committed record is a valid parent.

## Two arms, because the review found a third possibility

| arm | the last line of `can_move` |
|---|---|
| `rate` | `wrong_way > s->dual_tol` — D184's one-liner, the scaled space alone |
| `union` | `breached(s, v)` — the same question in both spaces |

Past `can_move`'s two earlier gates, `rate` is bit-for-bit the comparison
`dual_breach` makes. `union` accepts strictly more: every column `rate`
accepts, plus any breached only in the published space.

`review.md` finding 1 is why the second arm exists. With `rate`, a column
breached only in published space has no repair anywhere — `can_move` rejects
it, `arm_reentry`'s else branch is guarded by the scaled `dual_breach` so it
is not shifted either, and `wants_a_pivot` refuses it because its other bound
is finite — while `settled_dual_violation` still counts it.

## The campaign — `run-arms.txt`

All three sets, both arms, `J=12`. **`gate: PASS` and `baseline: 0 regressed,
0 improved, 0 new` on every set at every arm.**

| set | `rate` vs base | `union` vs base |
|---|---|---|
| `netlib` (94) | **94 bit-identical** | **94 bit-identical** |
| `netlib-infeas` (29) | **29 bit-identical** | **29 bit-identical** |
| `netlib-kennington` (16) | 14 bit-identical, 2 moved | the same 2, the same values |

The two arms are **byte-identical to each other on all three sets**. No
instance carries a column breached in the published space and not in the
scaled one, so the measurement cannot separate them. At `DUAL_TOL = 1e-9` that
window needs a column scale under `2^-10`.

## The two instances that moved, and both are `pds`

| instance | iterations | work | ratio |
|---|---|---|---|
| `pds-20` | 90938 → **44790** | 29627237041 → **5837911437** | **0.1970x** |
| `pds-06` | 9305 → 8769 | 237193725 → 196806834 | 0.8297x |

Work geometric mean over the 16 Kennington instances: **0.8930x**
(`geomean.py --metric work`; the 0.5116x ratio of totals is not the result,
D46). The other 14 are 1.0000x to the digit.

The set was run three times on this change — once in the `union` worktree,
once in the main tree, and once more to rewrite the baseline — and all
sixteen instance lines came back byte-identical every time.

Both publish the same objective as before — `pds-20` at 23821658640 — for a
different vertex: `digest` and `basis` both move, `col`, `row`, `rowrel` and
`gap` move with them. `checker=ok`, `cert=yes` and `dual=0` on both, and
`pds-20`'s `Q` **falls** from 4.22e-05 to 2.75e-05, its `rsub` from 1.77e-15
to 1.15e-15.

## What the review predicted, and what happened

`review.md` finding 2 predicted that dropping the flip would move `pds-20`'s
`dual=` off 0 and raise `Q`. **Neither happened.** `dual=` stays 0 and `Q`
falls. D27's cautionary case does not reproduce on this tree: the column D27
named still exists, the change still declines to flip it, and the instance
solves to the same objective in a fifth of the work.

## The reference build — `run-nopresolve.txt`

`-DJAOS_NO_PRESOLVE` is the build no reading of this change had used, and it
is the one where fixed **structural** columns survive to `can_move`. Three
arms, because the product differs from the union in two ways at once and only
a third arm separates them: `product -> rate` isolates the fixed column, whose
distance is exactly zero; `rate -> union` isolates the published space.

`netlib`, `J=12`. Both trees solve 92 of 94 and report `gate: NOT MET` against
the presolve baseline, which is the only baseline there is; the arms are
compared against each other and not against it.

**`np-rate` is byte-identical to `np-base`.** The units change alone moves
nothing here either, which settles the fixed-column question: the flip a
zero distance used to forbid is reached on no instance of this set, on the
build where fixed structural columns exist.

**`np-union` moves exactly one instance, and the published space is why.**

| `pilotnov` | `np-base` and `np-rate` | `np-union` |
|---|---|---|
| iterations | 3541 | 4182 |
| work | 129908011 | 162607997 (1.2517x) |
| `row` | 2.61e-07 | **5.17e-09** |
| `rowrel` | 4.12e-11 | **1.28e-12** |
| `Q` | 2.22e-08 | **2.13e-10** |
| `rsub` | 4.94e-12 | **4.74e-14** |
| objective | -4497.2761882188715 | -4497.2761882188706 |

The reference is -4497.2761882188715, so `np-union` publishes one ulp off it
and `np-base` publishes it exactly. Every residual it is judged on improves by
between one and two orders of magnitude, and the runner's own line says the
same thing from the other side: against the presolve baseline `np-base`
regresses on **the suboptimality bound**, 1.09e-13 to 4.94e-12, and `np-union`
regresses on **work** instead, 2.0x.

So the column the review said would lose its repair is real, it is on
`pilotnov`, and repairing it costs 25% more work on one instance of a build
JAOS does not ship and buys two orders of magnitude of certificate. That is
the fourth argument for `union`, and it is a measurement rather than a
deduction.

## Why `union` and not `rate`

The measurement cannot choose; both arms are byte-identical everywhere. The
argument chooses, and it has three parts.

1. **D92 says the two readings of a breach may not replace one another.**
   `rate` picks the scaled space and drops the other.
2. **`wants_a_pivot` (`src/simplex.c:2196`) already filters with `breached`**,
   over the complementary case — a column with no other real bound. `can_move`
   and `wants_a_pivot` partition the breached columns on `isfinite(other)`, so
   the two halves disagreeing about what counts as breached is a defect
   waiting for the instance that reaches it.
3. **The gap `rate` leaves is reachable through the public API.**
   `jaos_set_dual_tolerance(m, 1e-7)` (`include/jaos.h:328`) widens the window
   between the two spaces. D27 records `etamacro`'s column at a scaled breach
   of 4.89e-8 with a column scale of 1/32, published 1.56e-6: at that setting
   `rate` reads it as fine and the checker's `CHECK_TOL = 1e-6` does not.

`union` costs nothing measurable and gives up neither space.
