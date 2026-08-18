# The same reduced LP, two solves, both dual-feasible by the solver's own reading, 29% apart

Taken 2026-08-18, following `TODO.md` §5a. Four readings, three files. **It
corrects D119**, whose central sentence is refuted here by measurement.

Every probe is compiled into a COPY of the tree under `-DJAOS_DIAG`; `src/` is
read and never written. The state reached is only reachable through D118's
refused candidate, which is applied from `bench/measurements/02-27/candidate.diff`.

## What D119 got wrong

D119 said: *"Dual feasibility is an invariant the method maintains and nothing
re-reads it before the verdict is published."*

**That is false.** `dual_breach` (`src/simplex.c:2419`), `published_breach`
(`:2467`), `breached` (`:2490`), `arm_reentry` (`:2795`) and the whole re-entry
loop `reenter_after_settling` (`:3095`) all read it, and the loop keeps the
best point it finds by `settled_dual_violation`. D25 and D89 built that.

`reentry.txt` measures what the loop actually did on `pilotnov` under the
candidate:

```
REENTRY no-work rounds=6 dviol_now=0 dviol_best=0
        obj_now=-2115.3928900690385 obj_best=-2115.3928900690385
```

**Six rounds, and the solver's own dual violation reads zero.** The control,
`pilot-ja`, exits after one round with the same zero and the right answer. So
the solver checks, and its check says clean on a point the independent checker
rejects at 0.89. That is a disagreement between two computations of the same
quantity, which is a different defect from the one D119 named.

## Three hypotheses, all refuted by measurement

**1. The basics' reduced costs, assumed rather than computed.**
`compute_duals` assigns `s->d[v] = 0.0` to every basic variable
(`src/simplex.c:1086`) and never verifies it. `basic-redcost.txt` recomputes
`d = c - A'y` from the solver's own duals for **every** variable, unscaled:

| | worst basic | worst nonbasic, recomputed | worst nonbasic, carried |
|---|---|---|---|
| `pilotnov` | 3.82e-14 | 3.75e-14 | 3.75e-14 |
| `pilot-ja` | 1.14e-09 | 5.68e-14 | 5.68e-14 |
| `dfl001` | 1.11e-09 | 5.43e-10 | 5.43e-10 |

`pilotnov`'s are the **cleanest of the three**. `B'y = c_B` holds to 3.8e-14
and every nonbasic reduced cost has the right sign. The assumption is sound
here.

**2. A column resting on a bound dual phase 1 lent it.** Such a column is
interior in the caller's own box, so a nonzero reduced cost there is a
violation to the checker and legal to the solver — exactly the shape of a 0.89.
`lent-bounds.txt`:

```
LENT pilotnov  lent=72 resting=0 flagged_by_classify=0
LENT pilot-ja  lent=7  resting=0 flagged_by_classify=0
```

**No column rests on one.** 72 were lent and none is load-bearing at the exit.

**3. The reduced model differing between the two intervals.** Presolve runs
before the simplex and cannot depend on a simplex constant, but the sweep's
whole conclusion rests on it, so it was read rather than assumed.
`same-model.txt`:

```
64: 975/2172/13057 -> 867/1811/11676  fixed_col=230 empty_row=24 empty_col=0
                                      singleton_row=27 singleton_col=94 rounds=8
16: 975/2172/13057 -> 867/1811/11676  fixed_col=230 empty_row=24 empty_col=0
                                      singleton_row=27 singleton_col=94 rounds=8
```

**Identical, family by family.** Only the solve differs.

## What is left, and it is a contradiction

On one and the same reduced LP:

- the `REFACTOR_EVERY = 16` solve reaches an objective that postsolves to
  Koch's published optimum **to the last bit**;
- the `REFACTOR_EVERY = 64` solve stops at a point that is primal-feasible by
  its own test *verified against a fresh factorization* (D20), whose every
  reduced cost recomputed from its own duals is correctly signed to 3.8e-14,
  with no column on a lent bound — and whose postsolved objective is **29%
  worse**.

A basis that is primal-feasible and dual-feasible is optimal. Both readings
cannot be right, so **one of the solver's own optimality readings is measuring
something other than what it claims**, and it is reachable from a green tree by
one reordering in presolve.

## What has not been tested

Named so the next session does not redo the three above:

- **`s->status[v]` going stale.** After 156 stability rebuilds and whatever
  `repair_singular_basis` evicted, a variable's recorded status may not be the
  bound it is actually sitting at. Every reading above trusts `status` to say
  which side a reduced cost must fall on; if it lies, all of them agree with
  each other and with nothing else.
- **The primal test reading `s->xb` rather than a recomputation.** D20's second
  opinion refactorizes and re-reads the same carried vector; it does not
  compare `x_B` against an independent `B^-1(b - N x_N)`.
- **Postsolve.** It is the same code at both intervals and produces the right
  answer at 16, so it is not wrong on its own. It could still be faithfully
  reproducing a reduced point that was never optimal.

## The instruments

`trace.c` lives in `bench/measurements/02-28/` and is public-API only. The
three probes here are patches applied by the scripts that produced each file,
and none of them is in the tree.

Handed to `TODO.md` §5a.
