# pilotnov answers a different question: 67 of its costs never come back

Taken 2026-08-18, following `TODO.md` §5a. Six probes, six files. **It corrects
D119**, whose central sentence is refuted here by measurement, and it resolves
the contradiction D120 recorded.

The short form: the solve borrows costs to keep dual feasibility, the loans on
`pilotnov` reach **1.6e+32** on a cost of order one, and the round trip that
repays them leaves 67 costs permanently wrong — one by **55.11**. Every
optimality reading the solver takes is then correct about an objective nobody
asked for.

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

**4. The carried `x_B` is not the gap either.** D20's second opinion
refactorizes and re-reads the primal test, and it runs **before** the re-entry
loop; the loop then flips nonbasics between bounds and updates `x_B`
incrementally (`s->xb[i] -= s->col[i]`, `src/simplex.c:2367`) with nothing
re-verifying it. `carried-xb.txt` recomputes `x_B` from the factorization at
the exit and compares:

| | worst absolute | worst relative | carried basics out of bounds |
|---|---|---|---|
| `pilotnov` | 7.22e-10 | 1.63e-11 | 0 |
| `pilot-ja` | 0 | 0 | 0 |
| `dfl001` | 0 | 0 | 0 |

`pilotnov` is the only one that drifts at all, and 7.2e-10 is four orders too
small to be a 29% objective error.

## What it is: the solver was answering a different question

**5. `s->cost` is not the model's cost any more.** `cost-drift.txt` compares
`s->cost` at the exit against the vector the scaling pass built:

| | costs moved | shift still pending | worst absolute | worst relative |
|---|---|---|---|---|
| `pilotnov` | **67** | 0 | **55.110016** | **1.0** |
| `pilot-ja` | 0 | 0 | 0 | 0 |
| `dfl001` | 236 | 0 | 3.64e-12 | 2.22e-16 |

Sixty-seven of `pilotnov`'s costs are permanently different from the ones the
solve was given, one of them by **55.11 on a cost of magnitude at most one**,
and every shift record reads zero. `dfl001` moves 236 costs and stays at
2.2e-16, which is what rounding looks like.

**6. The books balance and the arithmetic does not.** `loan-balance.txt`
tallies every lend (`shift_to_feasible`) and every repayment (`repay_shifts`
and `primal_cleanup`) per variable:

```
pilotnov  worst_drift=55.110016 at v=1050:
              lent=1.61113965389807e+32  repaid=1.61113965389807e+32  shift=0
          unbalanced=186  worst_balance=256 (v=2172)
pilot-ja  worst_drift=0   unbalanced=0
dfl001    worst_drift=-3.64e-12 at v=995: lent=repaid=4.18e-09  unbalanced=0
```

Two separate things, and the first is the answer:

- **The round trip is not bit-exact.** `s->cost[v] += need` then
  `s->cost[v] -= shift` is `x += d; x -= d`, which does not restore `x`. On
  v=1050 the loans total **1.6e+32** against a cost of order one. A cost of
  order one plus 1e32 *is* 1e32; subtracting 1e32 back leaves nothing of it.
  55.11 is what came out.
- **And 186 variables never balanced at all**, the worst by 256, so loans are
  being lost as well as rounded away.

**That resolves the contradiction.** Every optimality reading in probes 1 to 4
is correct — about the objective `s->cost` holds, which is no longer the one
the caller asked about. The solver is dual-feasible for its own perturbed
problem; the checker judges the model's true costs and sees 0.89. The 29% is
the difference between the two objectives.

`settled_objective` computes `(s->cost[v] - s->shift[v]) * x` and its comment
calls the subtraction "belt and braces rather than arithmetic that matters".
With `shift` at zero and `cost` off by 55.11 it is neither: it reports the
perturbed objective.

## What this is not

**It is not a defect the 139 instances reach.** At HEAD `pilotnov` runs 1042
weight restarts and 0 stability rebuilds and answers correctly; the candidate's
trajectory is what drives the loans to 1e32. Nothing here proposes a change,
and no repair is costed.

**And it is not a tolerance.** No threshold in the file decides any of it.

## The instruments

`trace.c` lives in `bench/measurements/02-28/` and is public-API only. The
three probes here are patches applied by the scripts that produced each file,
and none of them is in the tree.

Handed to `TODO.md` §5a.
