# 02-70 — Clause 1 of the activity pass gets its own window, and the other two keep theirs

2026-08-20. D159's defect at the site that is not frozen, found by
`numerics-reviewer` while reviewing D159.

## The defect

The activity pass computed `rtol = ps_row_tol(&rg)` once — `8*eps*rg.traffic`,
the ACTIVITY half — and used it for all three clauses. Clause 1 compares
`min_act` against `cur_ru[i]`, and `cur_ru[i]` is a running difference every
removed column shifted by its own `a*v`. Nothing covered that side.

`bench/measurements/02-69/activity-pass.c` case A is D159's model with one cost
changed from 0 to 1, which stops the cost-0 singleton family relaxing the row,
so it stays live and this pass judges it instead:

```
min x1 + x2  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
                   x0 in [1,1]        fixed: cur_ru = 0, row_traffic = 1e9
                   x1 in [1e-10, 10] cost 1
                   x2 in [0, 1]      cost 1
```

| | before | after | reference build (the oracle) |
|---|---|---|---|
| A | **INFEASIBLE** | OPTIMAL | **OPTIMAL** |
| B — D159's guard | INFEASIBLE | INFEASIBLE | INFEASIBLE |

A second solvable model refused, in the shipping build. B is D159's
emptied-row model, carried here to show this change does not move it.

## Only clause 1 can take a wider window

The three clauses share `rtol` and the direction is not shared:

| clause | test | a wider window |
|---|---|---|
| 1 INFEASIBLE | `min_act > ru + rtol` | fires **less** — can only stop a refusal |
| 2 FORCING | `min_act >= ru - rtol` | fires **more** — pins columns |
| 3 REDUNDANT | its mirror | fires **more** — drops rows |

Widening the forcing window is the change that pinned `pilot` column 3554 and
cost 02-04 a campaign, and the comment beside `PRESOLVE_ROUND_ULPS` still
records it. So clause 1 gets `8 * DBL_EPSILON * max(1, rg.traffic,
row_traffic[i])` and clauses 2 and 3 are untouched.

**"A wider window fires less, so it can only stop a refusal" is the
incomplete half that let the first version's defect through.** Every row this
window rescues arrives at FORCING with its condition already true, by
construction: rescued on the upper side means `min_act > ru + rtol`, and
clause 2 tests `min_act >= ru - rtol`. Clause 3 cannot be reached, since it
needs `max_act <= ru + rtol` and `max_act >= min_act`. So a rescued row is
**pinned and deleted**, and D159's safety argument does not transfer — there a
frozen row survives for the simplex to re-test, here the row and its columns
are dead (`numerics-reviewer`).

## The measurement

| | netlib | infeas | Kennington |
|---|---|---|---|
| rows at the activity pass | 554828 | 70832 | 2681996 |
| clause 1 fires, shipped | 0 | 8 | 0 |
| clause 1 fires, candidate | 0 | 8 | 0 |
| **verdicts it would flip** | **0** | **0** | **0** |
| rows where it is wider | 57812 | 13714 | 9850 |
| worst candidate/shipped | 7.39e6 | 3.02e7 | 9.37e4 |
| widest ABSOLUTE, shipped | 4.038e-07 | 1.908e-06 | 5.223e-09 |
| widest ABSOLUTE, candidate | **4.038e-07** | **1.908e-06** | **5.223e-09** |

3307656 rows, **0 flips**, and the eight genuine infeasibilities in
netlib-infeas fire under both windows.

**The absolute window does not grow at all**, on any set. The change lifts
rows whose window is tiny and never reaches a magnitude the shipped one did
not already reach — which is a stronger statement than the ratio, and the one
D159's review asked for. (Those maxima are above `PRIMAL_TOL` on two of the
three sets, and that is the shipped window's own doing, unchanged here.)

## The cost on the gate: nothing

94, 29 and 16 instances bit-identical, 0 digest changes, `gate: PASS` with
`0 regressed, 0 improved, 0 new` on each, `make configs` passes all five, 225
tests on the plain build and 224 on the reference build.

## What was refuted — the first version published a wrong answer

`bound-scale.c`. The first version put `ps_bound_scale(rl, ru)` in the window
"for symmetry with D159". It is not padding, it is a defect:

```
min x0 + x1  s.t.  -1e12 <= x0 + x1 <= 0,  x0 in [1e-3, 1],  x1 in [0, 1]
```

Infeasible by exactly 1e-3, and nothing is ever removed, so `row_traffic` is 0
and `rg.traffic` is 2.001. `ps_bound_scale(-1e12, 0)` is 1e12, giving a window
of 1.78e-3 — **entirely from the row's LOWER bound, for a test on the UPPER
side**. It published `optimal` with an objective of 0.001 where the reference
build reports INFEASIBLE, and the row was then pinned by FORCING and deleted,
so the simplex never saw it either.

| build | C |
|---|---|
| before the diff | INFEASIBLE |
| first version of the diff | **optimal, obj 0.001** |
| the term dropped | INFEASIBLE |
| reference build | INFEASIBLE |

`ps_bound_scale`'s own comment says it is the window a comparison between two
BOUNDS uses; clause 1 compares a computed activity against one bound. And
there is no third error term for it to cover at any size: `min_act` carries
`eps * rg.traffic`, `ru` carries `eps * row_traffic[i]`, and both are in the
max. **Dropped outright rather than narrowed**, which is the same wrong
quantity in a smaller amount.

**And `ps_round_tol` put clause 1 on the `EXTRA_CFLAGS` sweep hook**, which
`ps_row_tol`'s own comment and `docs/tolerances.md` both forbid for the
activity-range readings — 02-09 did it for a few hours and review caught it,
so this is the second occurrence. It was behavioural, not tidy: `itol` could
fall BELOW `rtol` at a lower setting and invert the clause ordering, and case
C flipped between `ULPS=8` and `ULPS=4`. A literal 8 is immune at 1, 4, 8 and
64.

Both found by `numerics-reviewer` and both reproduced here on four builds
before being accepted.

## A third live wrong answer, at D159's own site, and it predates D159

`-1e12 <= x0 + x1 <= 0` with x0 and x1 both cost 0 in [1e-4, 1]. Both are
cost-0 bounded singletons, so both relax and freeze the row, which is then
emptied. Infeasible by 2e-4.

| build | verdict |
|---|---|
| pre-D159 (`0ac44fd`) | **optimal**, x = {1e-4, 1e-4} |
| with D159 landed | **optimal** |
| the frozen-row window with `ps_bound_scale` dropped | INFEASIBLE |
| reference build | INFEASIBLE |
| the same model with `rl = -INFINITY` | INFEASIBLE on all four |

The same shape, the same term, and the control with an infinite lower bound
confirms it is the irrelevant end supplying the number. D159's safety argument
does not cover it: an emptied frozen row is deleted with everything else, so
nothing re-tests it. It predates D159 and D159 did not fix it. Carried to
`TODO.md` as its own change rather than folded in here, because the campaign
this record reports was not run for it.

## The test's guard, and it is weaker than D159's

`test_the_activity_pass_is_not_refused_below_its_own_traffic` asserts OPTIMAL
on both builds. Raising `PRESOLVE_ROUND_ULPS` does not falsify it, because the
reference build has no window at all — so unlike D159's emptied-row test, this
one cannot be failed by moving the constant. What it does pin is the
disagreement between the two builds, which is what the defect was.

## Reproducing it

```
bash bench/measurements/02-70/run-activity.sh
```
