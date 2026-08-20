# 02-75 — keeping the residue instead of covering it, and the first change in the class that moves the gate

D165. Closes what D162, D163 and D164 were all working around.

## What the four entries were doing

`cur_rl[i]` and `cur_ru[i]` are the row bounds presolve carries while it
removes columns, and they were plain running subtractions with no
compensation — the only ones left in `src/presolve.c`, while `ps_row_range` has
used a Neumaier accumulator for activities since 02-04.

| | |
|---|---|
| D162 | widened three windows to COVER the error |
| D163 | found a fourth window and widened it too |
| D164 | **refused** the only repair a window can offer for the part that arrives inside a folded value — it published two rows violated by 7.5 times `CHECK_TOL` |

This removes the error. `ps_bound_shift` keeps the residue, `cur_rl[i]` still
holds `sum + comp` after every update, and **no read site changed** — there are
about fifteen and a value compensated at some of them and not others would be
worse than no compensation at all.

## 1. What it would change, measured before building — `compensate.txt`

| | netlib (94) | netlib-infeas (29) | Kennington (16) |
|---|---|---|---|
| window reads | 581826 | 82726 | 2775394 |
| with a non-zero correction | 5260 | 250 | **0** |
| worst correction, absolute | 2.526e-12 | 5.258e-13 | 0 |
| worst correction / row traffic | 2.223e-15 | 2.927e-13 | 0 |
| **window verdicts spared** | **0** | **0** | **0** |
| **window verdicts newly refused** | **0** | **0** | **0** |
| folds | 8622 | 9750 | 81646 |
| **folds whose value moves** | **2** | **0** | **0** |
| worst move in a folded value | 2.22e-16 | 0 | 0 |

**Kennington's correction is exactly zero across 2.7 million reads.** That is
worth putting beside 02-74, which reported a worst inherited error of 2.08e-11
on the same set: 02-74 measured the WINDOW's bound on the error and this
measures the error. A bound of 2e-11 over an error of 0 is not a contradiction,
it is the gap between the two, and it says the shift-count windows D162 and
D163 built are far wider than anything these instances need.

**Both directions were measured.** A compensated bound can refuse a row the
uncompensated one accepts as well as the other way round, and only one of those
is safe. Neither happens: 0 and 0 on all three sets.

## 2. What it actually changed, and the probe under-predicted it

**The probe measured window verdicts and folded values. It did not measure the
reduced model's row bounds**, and that is the channel that carried almost all
of the movement: `p->reduced.row_lower[ri2] = cur_rl[i]` hands the compensated
bound straight to the simplex, so all 5260 corrected rows reach it, not just
the 2 that moved a fold.

Predicted: 2 instances might move. Measured: **15 moved, 14 digests changed.**
The prediction was not wrong about what it measured; it was measuring the wrong
channel. A probe over a value that is later COPIED somewhere has to follow the
copy.

| | netlib | netlib-infeas | Kennington |
|---|---|---|---|
| bit-identical | 79 | 29 | 16 |
| moved | **15** | 0 | 0 |
| digest changes | **14** | 0 | 0 |

`gate: PASS` on all three, `baseline: 0 regressed, 0 improved, 0 new` on all
three, and `record_diff.py` reads **no regression** on all three.

**Work: geometric mean 1.0000x on every set**, best `capri` 0.9993x, worst
`bandm` 1.0000x. Iteration counts are IDENTICAL on all 15 moved instances and
so are the presolve reduction counts — the trajectory is the same length and
removes the same rows and columns, it just lands on different bits.

**Every moved instance keeps `objective=ok`, `checker=ok`, `det=ok`.** Four
objectives moved in their last digits, each against its Koch reference on the
same line:

| | was | now | Koch |
|---|---|---|---|
| `bandm` | -158.62801845012069 | -158.62801845012066 | -158.62801845012063 |
| `pilot87` | 301.71034743753643 | 301.71034743753637 | 301.71034733311052 |
| `ship08s` | 1920098.2105346196 | 1920098.2105346199 | 1920098.2105346196 |
| `pilot` | -557.48970616840484 | -557.48970616840506 | -557.48972928406818 |

`bandm` and `pilot87` move toward the reference, `ship08s` and `pilot` away, in
every case by one or two ulps against gaps that are unchanged in every
significant digit. `basis=` changed on `bandm`, `capri`, `czprob` and `finnis`;
the gate has covered the basis since D150 and reads `det=ok` on all four.

## 3. The chained model — `chain-after.txt`

02-74's model, where a fold fixes a column at a value carrying another row's
error:

| tree | status | objective | row S residual | row R residual |
|---|---|---|---|---|
| D164, the parent | **infeasible** | — | — | — |
| D164's refused window repair | optimal | 0 | 7.629e-06 | 7.51e-06 |
| **this tree** | **optimal** | **1.1920928955078125e-07** | **0** | **0** |
| reference build, the oracle | optimal | 1.1920928955078125e-07 | 0 | 0 |

**The compensated tree and the oracle agree to the last bit** — same `x1`, same
`w1`, same objective, both rows at residual zero. The pinned change-detector
D164 left behind fired on the first build, `Expected 2 Was 1`, and now asserts
the answer instead of the defect.

## 4. What is NOT done here

**The shift counts are now redundant and they still ship.** `row_shifts`,
`ps_shift_excess` and `ps_end_scale` widen four windows to cover an error that
no longer exists. Removing them NARROWS those windows, which is the direction
that refuses feasible models, so it is a separate change with its own
measurement — and D162's and D163's own tests are the cases it has to keep
passing. `TODO.md` has it.

Nothing here says the windows are wrong. It says they are covering something
that has been removed underneath them.
