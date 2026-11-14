# 02-452 — a cone with no interior

Taken on 2026-11-15 on the tree of cf507d1 with this change to
`src/conic.c`, for TODO row J27. `read.sh` takes the reading: 02-431's
`permcone.c` on HEAD's library dumps the continuous disagreements of
seeds 1 to 10, `interior.c` probes their pieces, `kinds.py` sorts them,
the change's tool solves each dump (`pairs.txt`), and 02-431's
permutation reading runs on the change's library (`continuous.txt`,
`mixed.txt`). 02-432's `gate.sh` takes the gate.

## The 71 disagreements by kind

`interior.c` solves each model, then for each cone and each quadratic
row the largest `t`, at most 1, by which it can be tightened over the
rest of the model with the objective set aside: `t` taken off a cone's
head (both heads of a rotated cone), or a row's finite side moved in by
`t`. A piece whose `t` is 0 has no point strictly inside. `kinds.py`
takes a pair's kind from both orders (`kinds.txt`):

| kind | pairs | what |
|---|---|---|
| cone | 41 | a cone's `t` is 0 in either order: at its tip in 14, on one ray in 26, and one infeasible model |
| row | 3 | a quadratic row's `t` is 0 and no cone's |
| failed | 7 | a probe ends with no answer and no piece is flat |
| interior | 20 | every piece has room |

Two of the cone kind and one of the row kind are infeasible models: one
order proves it, the other ends `numerical_error`, and a probe's `t` is
negative or within 4e-7 of 0 (`s9/t3738`: -1). The interior kind is not
J27's: on the
three read (`s3/t5565a`, `s6/t11013a`, `s5/t17371b`) the point passes
the primal test and its duals miss by 1.4e-7 to 1.9e-6 of their terms,
or the walk ends far from any point.

## The change

`cm_flat_rows` runs, as before, when the checker refuses an optimum of a
continuous model after the settle and the quadratic programs, now also
for a model with cones and no quadratic rows. It first holds the
quadratic rows of 02-446 and solves the model again, as before. When
that holds nothing or does not stand, and the refused point passes the
primal test, it looks at each cone on its boundary at that point, up to
`CONIC_FLAT_CONES` (8):

1. A copy of the model sets the objective aside and maximises `t`, at
   most 1, with the cone's head replaced by a new column at the head
   less `t` (both heads of a rotated cone) (`cm_flat_cone_probe`).
2. A cone whose `t` is 0 has no point strictly inside: every point of
   the model puts its columns on one ray of the cone's boundary, or at
   its tip. The probe's point says which (`cm_flat_cone_hold`). When
   every column of the cone is within the primal tolerance of 0 there,
   and `t` within the primal tolerance of 0, the columns are fixed at 0.
   Otherwise, with `t` within the dual tolerance of 0, each member is
   tied to the head by a row along the probe's point, the ray scaled to
   lie on the boundary (a rotated cone's smaller head tied to its larger
   by the members' square norm over two), the head kept at 0 or more,
   and the cone dropped.
3. The model with those cones held, and the rows held before, is solved
   again. Its point stands when the checker passes it on the model as
   loaded and its objective is within `sqrt(CONIC_TOL_ROUGH)` of the
   refused point's, relative. The duals are published as NaN, as for a
   held row.

`tests/data/cone_no_interior_ray.mps` (seed 3's model 3290, the order
that failed) is a cone on one ray, and `cone_no_interior_tip.mps` (seed
5's model 17341) a cone whose only point is its tip; both end
`numerical_error` on cf507d1 and optimal at 0 without duals on the
change (`test_a_cone_with_no_interior_ends_optimal_without_duals`).

## What was tried first, and why it is not the change

The first forms held more, and the full permutation reading showed what
that costs: answers that disagree between orders by far more than the
tolerance.

- **Probes on the model with the pieces found so far held, a joint probe
  that tightens every piece whose own probe failed by one `t`, and a
  row probe that takes a point the checker refuses.** 39 of the 41
  cone pairs ended the same, but the reading had 9 continuous and 6
  mixed pairs of optima that differ, against 0 and 2 on cf507d1: seed
  10's mixed model 16368 ended at 5 where its optimum is 2, and seed 8's
  model 12277 at -2.03 where it is 0. A held row fixes its curved
  columns at the probe's point, and at a degenerate optimum that point
  is good to the square root of the probe's accuracy only: model 12277's
  row fixed C1 at 1.9e-4 and C3 at 4.5e-8, and its rotated cone
  `2·C3·C7 ≥ C1²` then forced C7 to 0.41. A joint probe's point and a
  refused probe's point are no better placed.
- **The same holds where the walk ends with no answer and the feasibility
  walk finds a point.** There is no refused point to judge the held
  optimum by: seed 9's continuous model 19432, whose optimum is -4,
  ended at -4.0066 and -4.0169 in its two orders, each holding its row
  at C4 = 2.4e-6 and 1.6e-5 where it is 0.
- **A guard at `CONIC_TOL_ROUGH` itself.** It refused good holds: seed
  7's model 14615 holds to 14/3, and its refused point stood at
  4.66664726.
- **A ray held at `t` within the primal tolerance, and the rows' hold
  dropped when the cones' did not stand.** Seed 4's model 238 and seed
  6's model 12075 have cones with `t` of 5.8e-8 and 6.5e-8 that are not
  flat; holding them moved model 12075's optimum from -2 to 0, the guard
  refused it, and the row hold that cf507d1 published went with it.

## The reading

The 71 pairs, each file solved by the change's tool (`pairs.txt`):

| kind | pairs | the same status in both orders on the change |
|---|---|---|
| cone | 41 | 30 |
| row | 3 | 0 |
| failed | 7 | 0 |
| interior | 20 | 1 |

Of the 142 files, the change holds one cone in 26, two in 1, a row and
one or two cones in 3, and a row alone in 9, as cf507d1 does. Of the 11
cone pairs left, 2 are the infeasible models, 3 are walks that end with
no answer (`s2/t1714a`, `s7/t12481b`, `s8/t4925b`), and 6 have a cone
whose probe ends with no answer, because another piece has no interior
either (`s1/t7608b`, `s2/t646b`); on 2 of those another cone is held and
the held model ends with no answer. Where both orders now end optimal,
their objectives agree within 3.3e-7 of `1 + |objective|`.

02-431's permutation reading, against 02-449's on 32f2863, whose
`src/conic.c` is cf507d1's:

| mode | models | STATUS | OBJ | no answer either way |
|---|---|---|---|---|
| continuous, 32f2863 | 200,000 | 71 | 0 | 450 |
| continuous, the change | 200,000 | 49 | 0 | 382 |
| mixed, 32f2863 | 198,391 | 17 | 2 | 594 |
| mixed, the change | 198,391 | 13 | 2 | 584 |

40 continuous and 12 mixed STATUS lines are 02-449's. The 9 continuous
and 1 mixed STATUS lines that are new are models both orders failed on
cf507d1, one order of which now ends optimal; each was dumped and solved
by both tools. Of the OBJ lines, seed 7's mixed model 6441 is 02-449's.
Seed 5's mixed model 2650 is new: both orders end at 1.998261 on
cf507d1, and at 1.999879 and 1.999872 on the change. Its optimum is 2:
with C5 at 0 the rotated cone forces C2 and C3 to 0, and R2 then forces
C7 to 0. cf507d1's point puts C3 at 1.5e-6, inside the tolerance on the
cone, which lets C7 reach 0.0017; the change's leaf solves hold a cone
and land nearer 2, each order at its own point within tolerance.

## The gates

`make test`, `make sanitize`, `make python-test`, `julia-test`,
`dotnet-test`, `java-test` and `r-test` pass. 02-432's gate, cf507d1
against the change:

- 02-255, 8 seeds x 1000, and 02-326, 6 seeds x 1000: every line the
  same, work units included (`mip-*.txt`, `dis-*.txt`).
- CBLIB's 80 mixed-integer instances at 1e10: every line the same but
  turbine07_lowb's work, 10,000,324,640 against 10,000,097,669; both stop
  at the work limit with the same incumbent, bound and 990 nodes
  (`cblib-mip-*.txt`).
- QPLIB's mixed-integer QCQPs: every line the same.
- 02-425's `warmcone.c`, misocp, seeds 1 to 3: P1, P2 and P3 all 0.
- `make cblib`: 29 of 29 checked, 0 regressed against the baseline.

The gate runs 02-427's `warmqc.c` as it is, which counts an optimum
without duals under P2; 02-446's `read.sh` reads it through `warmqc.py`
on both libraries (`warmqc-*.txt`): every line the same, with 24 optima
without duals in qcp mode and P1, P2 and P3 all 0. Its models hold no
cones.
