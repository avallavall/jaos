# 02-69 — The frozen-row window is scaled by what the comparison is made of, and presolve stops refusing a model the solver can solve

2026-08-20. `TODO.md`'s frozen-row scale item, opened by D155 once
`row_traffic` became a live quantity.

## The defect, and it is a wrong answer rather than a latent risk

The frozen-row feasibility test compares `min_act`/`max_act` against
`cur_ru[i]`/`cur_rl[i]`. Those two sides carry error from different places:
the activities are a sum over the surviving columns, and the bounds are
running differences every removed column shifted by its own `a*v`. The window
was `ps_round_tol(ps_bound_scale(cur_rl[i], cur_ru[i]))` — **the magnitude of
one operand**, which bounds neither.

`frozen-scale.c` is one shape at two gaps:

```
min x1  s.t.  R: 1e9*x0 + x1 + x2 <= 1e9
              x0 in [1,1]        fixed: cur_ru = 0, row_traffic = 1e9
              x2 in [0,1] cost 0, degree 1: relaxes R and FREEZES it
              x1 in [g, 10] cost 1
```

| g | before | after | reference build (the oracle) |
|---|---|---|---|
| 1e-10 | **INFEASIBLE** | OPTIMAL | **OPTIMAL** |
| 1e-4 | INFEASIBLE | INFEASIBLE | INFEASIBLE |

`1e-10` is about a thousandth of one ulp of 1e9 — an infeasibility the
arithmetic cannot represent. **Presolve refused a model the reference build
solves**, which is the shape this file calls the mirror-image catastrophe: a
solvable model refused, with nothing downstream to recover it.

## The measurement over the three sets, and why it could not find this

| | netlib (94) | infeas (29) | Kennington (16) |
|---|---|---|---|
| frozen rows tested | 16618 | 1894 | 602 |
| residue above zero | 0 | 4 | 0 |
| candidate exceeds shipped, shape A | 98 | 144 | 600 |
| worst A/shipped | 153 | 10.49 | 1.18 |
| candidate exceeds A, shape B | 4858 | 752 | 490 |
| worst B/shipped | 45930 | 168.2 | 92.2 |
| **verdicts either shape would flip** | **0** | **0** | **0** |

Both shapes were swept rather than the wider one assumed correct: `max(bound
scale, row_traffic)` alone, and that plus the activity traffic. **Neither
flips a verdict on any of the 139 instances**, so the campaign cannot see this
defect at all and only the constructed pair separates the two windows. The
four genuine infeasibilities in netlib-infeas stand at 5.63e14 times the
shipped window, so nothing in this range misses them.

**The ratio is between two windows and is NOT a measurement of the error.**
The first version of this record said "smaller than the error it is testing",
which the probe never computes — that would need a higher-precision
recomputation. Corrected after review.

## The absolute window, which no ratio answers

| | widest shipped | widest A | widest B | largest `rg.traffic` |
|---|---|---|---|---|
| netlib | 1.776e-08 | 1.776e-08 | 1.776e-08 | 1.61e6 |
| netlib-infeas | 7.011e-11 | 7.011e-11 | 7.011e-11 | 3.36e4 |
| Kennington | 7.254e-10 | 7.668e-10 | **6.494e-08** | 3.66e7 |

`PRIMAL_TOL` is 1e-7 and `CHECK_TOL` is 1e-6. The widest window this change
produces stays under both — **by a factor of 1.5, not by decades**. netlib and
netlib-infeas do not move in absolute terms at all. A set carrying a larger
`rg.traffic` than Kennington's 3.66e7 is where this stops being comfortable,
and that is the reopen condition. Asked for by `numerics-reviewer`, who
pointed out that every other figure here is a ratio.

## What the review found, and two of them changed the change

**The same defect is live at the activity pass, and this change does not touch
it.** `ps_row_tol(&rg)` is `8*eps*rg.traffic` alone, so the bound side's error
is uncovered there in mirror image. `activity-pass.c` case A is the model
above with one cost changed from 0 to 1, which stops the row freezing:

```
before, after, and ROUND_ULPS=1e12  ->  INFEASIBLE
reference build                     ->  OPTIMAL
```

A second live feasible-model-refused, in the shipping build. **The repair
there is not a copy of this one**: that `rtol` is shared with FORCING and
REDUNDANT, and widening the forcing window is what cost 02-04 a campaign. It
goes to `TODO.md` as its own change with its own measurement.

**The negative-half test could not fail.** Its first version used a model whose
frozen row keeps a live column, so the simplex refused it at `PRIMAL_TOL`
whatever the window did — at `ROUND_ULPS = 1e12` it still read INFEASIBLE. It
was reading the pipeline, not the window, so **the widening had no guard at
all**. Replaced with `activity-pass.c` case B, where the row is EMPTIED and
the simplex has no column left to refuse it with:

```
min x1  s.t.  R: 1e9*x0 + x1 == 1e9 + 100,  x0 in [1,1],  x1 in [0,3] cost 0
```

before / after / reference all INFEASIBLE, and `ROUND_ULPS = 1e12` **OPTIMAL**.
Validated by building the suite at that constant and watching the test fail.

**`8 * DBL_EPSILON` is a scale claim and not a bound.** `cur_rl[i] -= a*v` is a
plain running sum with no compensation, so after k removals the error goes with
`k * eps * scale`; eight ulps covers k of about three, and a netlib row with a
hundred removed columns is understated by roughly 12x. This file already says
so for the same quantity at `ps_verify_row_activities`, which multiplies by
`nnz - 1`. The direction is the loud one, so it is not a blocker; it is
`TODO.md`'s.

**The `rg.traffic` term has no test and structurally may not be able to have
one.** A frozen row with live columns survives into the reduced model where
the simplex re-tests the violation; the rows where this test is the last word
are the emptied ones, whose `rg.traffic` is zero. It is kept for coherence: at
the shipping constant `ps_round_tol(rg.traffic)` is exactly `ps_row_tol(&rg)`,
so the frozen-row window becomes a superset of the one the activity pass
applies to the same comparison.

**`max` rather than a sum.** The two errors are independent so the true bound
is their sum, and `sum <= 2*max`. The constant is 8 where the argument needs
about 1, so the factor of two is already paid, and switching would not close
the k gap above.

## The instrument was wrong, and it took three readings to see it

`bench/run -j N` forks children that share one stderr. `fprintf` with many
conversions issues several writes, so another child's output lands between
them and the line is torn. **`grep` still counts the line**, because the
fragment carries the tag, and `awk` then parses the fragment's fields only —
so the **sums come out low**.

Measured before fixing it: the same source, rebuilt and re-run, gave 4858,
4844 and 4798 for one counter with the line count intact at 188 every time.
Monotonically down, which is the signature.

A count that can only be undercounted is exactly the wrong shape for a probe
whose finding is `0 verdicts flip`. The fix is one `write(2)` per record;
verified across four optimisation levels and both `-j 1` and `-j 12`, six runs
identical.

**02-67 and 02-68 were re-verified with the fixed instrument**, because both
had already landed and both reported zeros:

```
02-67  shifts 320 / 30 / 8592, lost=0 destroyed=0   identical at -j 12 and -j 1
02-68  folds 8622 / 9750 / 81646, collapse=0        identical at -j 12 and -j 1
```

Both stand. There was no way to know that without checking.

## The cost on the gate: nothing

94, 29 and 16 instances bit-identical to the committed record, 0 digest
changes, `gate: PASS` with `0 regressed, 0 improved, 0 new` on each, and
`make configs` passes all five build configurations.

## Reproducing it

```
bash bench/measurements/02-69/run-frozen-window.sh
```

The probe applies to the tree BEFORE the change, so it builds a detached
worktree at `0ac44fd`. The three case programs are built against that tree,
the working tree, `-DJAOS_NO_PRESOLVE` and `-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=1e12`.
