# 02-81 — compensating the refinement residual, which was refused on an argument and overturned by the measurement

D171. One source change in `src/simplex.c`: the residual
`subtract_basis_times` forms for the iterative-refinement step is accumulated
with Neumaier compensation.

## The refusal this overturns, and why the argument was not enough

D168 compensated `compute_primal`'s right-hand side and left two sums of the
same shape alone. `numerics-reviewer` refused them from the terms: these sum
products of `x_B`, which is an FTRAN output already carrying the
factorization's error, **so an accumulator cannot reach an error that is
already inside a term**.

That is sound about the terms and wrong about the consequence. The residual is
what the correction is computed from, so a term lost there leaves a correction
that is short, and the point that gets published is the one the correction
lands on. The reopen condition D168 wrote down was *"a model where the
refinement residual loses a term that changes a published value"*. There is no
constructed model, and there did not need to be: the sets have three.

## The control, and why it comes first

`run-refinement-residual.sh` builds three trees with the **same** flags —
control, primal side, both sides. The first version of this measurement had no
control and read "all three sets differ", which was the `-b` footer and not the
change.

**The control's 94 netlib instance lines are byte-identical to the committed
record.** The only difference is the two-line baseline footer, because the
probe runs without `-b`. That is also the evidence that `-O2` here and the
Makefile's `-O3 -flto -march=native` produce the same bits.

## netlib — the counts move and the worst case does not

88 of 94 moved, 88 digest changes, **work geometric mean 1.0000x**.

| figure | better | worse | worst over the set |
|---|---|---|---|
| `rowrel` | **76** | 7 | 6.36e-12 → 6.36e-12 |
| `N` | 71 | 15 | 2.89e-05 → 2.89e-05 |
| `row` | 59 | 20 | 8.44e-07 → 8.44e-07 |
| `rsub` | 53 | 14 | 6.91e-05 → 6.91e-05 |
| `Q` | 52 | 15 | 0.0386 → 0.0386 |
| `col` | 17 | 8 | 4.92e-08 → 4.92e-08 |
| `sub` | 31 | 30 | 1.3e-13 → 1.3e-13 |
| **`gap`** | 31 | **51** | 2.21e-10 → 2.21e-10 |
| `dual` | 0 | 0 | — |

76 against 7 is not a coin, so the change is systematically more accurate here.
**On this set alone that would not be enough to land it**, because no worst
case moves and 88 digests is a large churn for a few ulps. Reading only netlib
is what nearly refused this.

## Kennington — the worst case moves, and by four orders of magnitude

11 of 16 moved, work geometric mean 1.0000x.

| figure | better | worse | **worst over the set** |
|---|---|---|---|
| `rowrel` | 9 | **0** | **8.81e-13 → 8.88e-17** |
| `col` | 7 | **0** | **8.81e-13 → 2.07e-14** |
| `N` | 8 | 1 | **1.35e-07 → 8.12e-08** |
| `Q` | 5 | **0** | 0.00219 → 0.00219 |
| `row` | 8 | 1 | 5.9e-12 → 5.85e-12 |
| `rsub` | 5 | 1 | 4.18e-14 → 4.18e-14 |
| `gap` | 5 | 5 | 2.09e-14 → 2.09e-14 |

Per instance, on the `pds` family, and `col` is a breach of a column bound in
the model's own units:

| | `col` before | after | `rowrel` before | after |
|---|---|---|---|---|
| `pds-06` | 4.26e-14 | **1.58e-30** | 4.26e-14 | **1.58e-30** |
| `pds-10` | 2.84e-14 | **2.52e-29** | 2.84e-14 | **5.89e-17** |
| `pds-20` | **8.81e-13** | **5.05e-28** | **8.81e-13** | **8.42e-17** |

Those are not a few ulps. A published value that sat outside its own declared
bound by 8.81e-13 now sits 5.05e-28 outside it, which is to say inside it. That
is the reopen condition met three times over, and it is what lands this.

## netlib-infeas

14 instances move by a handful of work units, **0 digest changes**, all 29
still correctly refused.

## The published basis moves on nine netlib instances

`bandm`, `bnl2`, `cycle`, `czprob`, `dfl001`, `etamacro`, `fit1p`, `ganges`
and `scrs8`. Kennington: none. **The first version of this record counted
digests and stopped there** (`numerics-reviewer`), and `bench/run.c` says the
basis is part of the answer.

The mechanism is the one the change is for: `refine = true` runs at the refresh
that verifies optimality, a more accurate `x_B` moves a marginal feasibility
test, and the solve takes a different last pivot. Nothing else moved on those
nine — `cert`, `dual`, `drop` and `rays` are identical on all of them.

## The symmetric change is refused

D29 says *"a point read off an accurate x_B and an inaccurate y is not more
consistent than one read off neither"*, so compensating `compute_duals`'
refinement dot product as well is the obvious next move. It was built and
measured in the same run, and **it changes nothing at all** — no direction
count, on either set. netlib reads 89 moved instead of 88 at work 1.0001x with
every better/worse pair identical; Kennington reads 14 instead of 11 with the
same 11 digest changes and the same counts. The dual sum runs over one column's
nonzeros while the primal one runs over every basic column touching a row, so
there is nothing there to recover.

## The `gap` column, and the argument that was withdrawn

`gap` goes the wrong way on 51 netlib instances. **The first version of this
record argued that if it were D29's primal-dual inconsistency the symmetric
change would have moved it, and it did not.** That argument has no power and is
withdrawn: the symmetric change moved *nothing at all*, so it cannot
discriminate between hypotheses (`numerics-reviewer`).

**The direct evidence is in the two committed records and it is stronger.**
`dual`, `cert`, `drop` and `rays` are unchanged on all 94 netlib and all 16
Kennington instances. A primal that had gone inconsistent with its dual would
show in `dual`, and `dual` does not move anywhere.

What is left is that `gap = |pos_model - neg_model| / scale` is a difference of
two halves much larger than itself, so at these magnitudes it is what
cancellation leaves rather than a number. The 82 netlib movers span
**1.49e-19 to 4.12e-13** — this record first said 1e-19 to 1e-16 and was wrong
by three orders at the top — against a set worst of 2.21e-10 that does not
move. The relative swings are tens of percent, up to 64x on `fit1p`. A quantity
that moves like that under a one-ulp change of its inputs has no sign to read.

## What it cost, and the gate could not see it — the published basis

`bench/run.c`'s `basis=` is a hash: it detects a change and never reports a
count, and the count exists only when 02-48's probe is run by hand. D167 is the
entry that says a figure with no owner drifts. Nine basis hashes moved here, so
it was run — at three trees, and the attribution is exact.

| tree | exact | **WRONG** | worst over | sum |
|---|---|---|---|---|
| `4747f29`, D171's parent | 142 | **46** | +18 | +248 |
| `39a49f6`, **D171** | 140 | **48** | **+21** | +272 |
| `311d73b`, D172 | 140 | 48 | +21 | +272 |

Kennington reads `exact=32 WRONG=0` at all three. D172 changes nothing here,
which is what a diff that writes only `m->objective` predicts.

**So D171 cost two netlib solves their valid published basis**, and pushed the
worst over-count from +21 and the sum by 24. By `TODO.md`'s own stated measure
for that item — the count of solves publishing a wrong basis — **that is a
regression**, and the gate reported `0 regressed` because it cannot see a
basis.

**It is recorded rather than traded away quietly, and the change still
stands.** What it bought is three published column values that sat outside
their own declared bounds and no longer do, worst 8.81e-13 on `pds-20`. What it
cost is two more solves on an item that was already wrong on 46 and whose
stated price is a lost warm start rather than a wrong answer. The two are not
the same kind of thing, and this record says both.

## The seconds — `timing.txt`

`jm_work_add` is unchanged, so the same units are billed for about four times
the arithmetic per nonzero plus two dense `O(nrow)` passes that bill nothing.
D171's first version had no time ratio and `CLAUDE.md` asks for one exactly
where the units are blind (`numerics-reviewer`).

**The noise floor here is unusually good.** The whole `ken` family and `pds-02`
come back bit-identical under D171 — same iterations, same work, same digest —
so their ratio is the added arithmetic and nothing else, and `ken-13` is 747
million work units of it.

| instance | parent s | D171 s | ratio | |
|---|---|---|---|---|
| `ken-07` | 0.0362 | 0.0374 | 1.0345x | bit-identical |
| `ken-11` | 1.4469 | 1.4996 | 1.0364x | bit-identical |
| `ken-13` | 8.7286 | 8.3348 | **0.9549x** | bit-identical |
| `pds-02` | 0.0609 | 0.0611 | 1.0033x | bit-identical |
| `dfl001` | 10.4785 | 10.3915 | 0.9917x | moved |
| `pds-20` | 31.2641 | 31.0214 | 0.9922x | moved |
| **geometric mean, the four bit-identical** | | | **1.0067x** | the arithmetic alone |

Four instances doing byte for byte the same work span 0.9549x to 1.0364x, which
is this host's 6.27% repeatability (D93). **The added arithmetic is not
measurable here**, which is what `subtract_basis_times` running only on the
handful of `refine = true` refreshes predicts.

## What this does not close

- **There is no constructed model.** The evidence here is three instances on
  Kennington and a control. `bench/measurements/02-72/`'s shape does not carry
  over: on that model every product is exact, which is why compensating the
  accumulation recovered all of it, and here the terms are products of an FTRAN
  output. If a model can be built it would turn this into a pinned test and it
  is worth a session.
- **`apply_flips` is the third sum of this shape** and is untouched. It
  accumulates over a bound-flip batch into `s->col` and subtracts the FTRAN'd
  result from `x_B`, so mid-solve `x_B` loses terms again after every batch.
  The final `refine = true` refresh rebuilds `x_B` from scratch, so what it
  loses does not reach the answer.
  ~~It would need a third `[nrow]` array; `s->rhsc` and `s->resc` are both live
  inside the call this runs in.~~ **That liveness claim is false**
  (`numerics-reviewer`): `apply_flips` is called from `dual_ratio_test`
  mid-iteration, with `compute_primal` nowhere on the stack, so both arrays are
  dead then. The true contract is more useful — each is `memset` at the entry
  of its single reader and dead at its exit, so neither carries a producer
  contract, and a borrower following the same discipline is safe. What is not
  safe is a borrower needing its value to survive a call to
  `compute_primal(s, true)`.

- **The probe's `KEYS` list is narrower than the record it reads.**
  `run-refinement-residual.sh` parses `col`, `row`, `rowrel`, `gap`, `Q`, `N`,
  `sub` and `rsub`, and never `dual`, `drop`, `cert`, `rays` or `basis`, all of
  which `bench/run.c` prints. Its "moved" count compares only what it parsed,
  so an instance whose only change was `basis` counted as not moved — which is
  how the nine above were missed. **`record_diff.py` is the honest instrument
  here**; this probe is the narrow one and is kept for the direction counts it
  does compute.
