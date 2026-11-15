# 02-458 — a second Newton finish, its active rows regularised less, for model 533

Taken on 2026-11-15 on the tree of dd54830 with this change to
`src/conic.c`, for TODO row J21. `read.sh` takes the sweep (`*-VALUE.txt`,
summarised in `sweep.txt`); `kkt533.py` computes the model's KKT points.

## Model 533

The second brute-force solve of 02-255 seed 4's model 533
(`tests/data/conic_finish_near_tangent.mps`) ended `numerical_error`: the
checker refused the finish's point, its rows 2.27e-9 of their traffic off
their sides. 6 columns, C4 and C6 fixed at -3, a ranged row R1, a convex
quadratic row R4 over C1, C3, C4 and C5, and a rotated cone 2·C1·C2 >= C5²
+ C6². R1 holds C5 at 2.196 or above, R4's ellipse keeps C1 near 0.009,
so the cone pushes C2 to 732 and the objective to 1.4e5.

TODO J21 asked which of R1, R4 and the cone the optimum leaves inactive.
None: `kkt533.py` solves the KKT conditions of each candidate active set
in 60-digit decimal arithmetic, and only {R1, R4, cone} is a KKT point,
feasible, with multipliers 1.968e7, 1.114e7 and 2.036e4 (C1 =
0.0094356723884169, C2 = 732.52539269604, C3 = -1.1574834962216, C5 =
2.1963012642515). The three hold at that point exactly.

What stops the finish is that R4 and the cone are nearly tangent there.
R4's derivative in C3 is -2e-8, since C3 sits where R4 lets C1 grow most,
and the cone's in C2 is 1.3e-4; with C4, C5 and C6 held, their two rows of
the Jacobian nearly coincide, along C1. The finish solves its
quasi-definite system with the active rows regularised by `CONIC_REG`
(1e-7) and refines against the system without it. Along that near-null
direction each refinement pass gains a factor of about 0.9, so ten passes
leave the step short of the rows: from the second step on, the residuals
of R1, R4, the cone and C4's fixed value stay at 1.6e-9 to 5.1e-9. Two
more Newton steps or ten more passes change nothing; the projection and
the dual refit that follow stall the same way (02-445 refined the
projection against the unregularised system and saw the same residual).

With the rows regularised by 1e-11 the finish reaches a point the checker
takes, at 140548.02061031791 against 140548.0206097062 from the KKT
point, a gap of 2.7e-12. Applied to every finish, that regularisation
broke `tests/data/qc_sqp_refused.mps`: its first finish holds 33
constraints that do not hold together, and the damped step of
`CONIC_REG` is what lets the second round, on 31, converge.

## The change

The finish (`cm_newton_rounds`) runs as before. When the checker refuses
its point, it runs again from the walk's point with the active rows
regularised by `CONIC_FINISH_REG` (1e-11) and the same refinement against
the unregularised system (`cm_fine_finish`), and its point stands only
if the checker takes it; otherwise the first finish's point goes on to
the settle, the quadratic programs and the flat rows as before. A tree's
node relaxation does not take the second finish, as it does not take the
settle.

## The sweep

`read.sh` with the value of `CONIC_FINISH_REG` read from the environment
(`finish-reg.patch`), against HEAD:

| set | HEAD | 1e-9 | 1e-11 | 1e-13 |
|---|---|---|---|---|
| 02-255, 8 seeds x 1000 | 1 unsure (seed 4) | 1 unsure | 0 | 0 |
| 02-326, 6 seeds x 1000 | 0 unsure | 0 | 0 | 0 |
| 02-253, 3 seeds x 1000 | 0 failed | 0 | 0 | 0 |

Every other count is the same in the four columns, the trees' work units
included (the brute-force solves are not in them), but for 02-326 seed
2: 16,662 units more in each column, where a continuous solve inside a
tree, refused, tries the second finish and keeps the first. 1e-11 is the largest
value that solves model 533.

## The gates

`make test` and `make sanitize` pass.
`test_a_finish_whose_active_gradients_nearly_meet_reaches_its_optimum`
solves model 533 to the KKT point's objective within 1e-9 and checks it;
with dd54830's `src/conic.c` it ends `numerical_error`.

02-432's `gate.sh`, dd54830 against the change:

- 02-255's mixed-integer conic models: its one `UNSURE` line gone, 588
  optima at seed 4 against 587; no other line moves.
- 02-326's shaped models: the same, but for seed 2's 16,662 work units.
- CBLIB's 80 mixed-integer instances at 1e10: the same status,
  incumbent and bound on all 80; 7 that end at the work limit or at the
  optimum move in work units, and `sssd-weak-20-8` takes 1,254 nodes
  against 1,253, at the limit.
- QPLIB's mixed-integer QCQPs: every line the same.
- 02-425's `warmcone.c`: P1, P2 and P3 all 0.
- 02-427's `warmqc.c`: 15 optima without duals under P2, against the 24
  that 02-452 records: the second finish gives nine of the solves that
  the flat rows held a checked point with duals. P1 and P3 0.
- `make cblib`: 29 of 29, 0 regressed.
