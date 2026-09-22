# 02-293 — the convex QP reading of row C4, and three QP fixes

Taken on 2026-09-22 for TODO rows C4 and C6, on the tree of 8876091 and
then on the fixes below.

## The reading on 8876091

`make maros-meszaros J=4`: 138 instances, 137 solved, 133 objective ok,
136 checker ok, 1 failed (values, refused as not convex). Three answers
passed the checker but not the runner's suboptimality ceiling of 1e-6:
aug2dcqp at 1.7, aug2dqp at 1.64, aug3dqp at 1.03e-3. Four missed the
reference: hues-mod and huestis by 6.5e-6, liswet2 by 1.1e-6 and liswet8
by a factor of ten. The checker refused qgrow22 (duals off by 2.98e-6).
`make cblib J=4`: 29 of 29 solved, objective ok and taken by the checker.

## Outside references

HiGHS 1.15.1, primal and dual tolerances 1e-9: liswet2 24.998076103,
liswet8 714.47005905. On hues-mod and huestis it stops at its null-space
limit of 4000 and finds no answer in 600 s at a limit of 20000.

Clp 1.17.11, `-barrier`: liswet2 24.99807612, liswet8 714.470059,
hues-mod 34824463.87, huestis 3.482446387e11.

JAOS's hues-mod point is feasible to 1e-11 with a certified bound of
7.96e-13 relative, so the table's 3.4824690e7 is not the minimum of this
file, and the same holds for huestis. liswet8's 7144.7006 is 714.47006
with the decimal point moved. `bench/maros-meszaros.manifest` now takes
3.4824464e7, 3.4824464e11 and 714.47006 for these three and names this
folder as their source. liswet2's reference stands: HiGHS and Clp agree
with it, and JAOS was the one off.

## The checker charges a curved column by its curvature

On aug2dcqp, 772 columns carried reduced costs of -1.8e-15 to -4.4e-13
and no upper bound. The checker charged each against the upper bound the
rows imply, 3.6e10 to 3.6e18, for a positive half of 1.1e7 whose largest
term was 7.0e5. Each of these columns has a diagonal entry of `Q` and no
entry off it, so the term is `w²/(2q)` instead (`docs/tolerances.md`):

| instance | bound before | bound after | certified |
|---|---|---|---|
| aug2dcqp | 1.7 | 2.2e-15 | yes |
| aug2dqp | 1.64 | 2.3e-15 | no, 252 columns with no `Q` entry drop their term |
| aug3dqp | 1.03e-3 | 1.03e-3 | yes |

aug3dqp keeps its bound: 114 columns with no quadratic term sit at about
1.3 above a lower bound of 0 with reduced costs of -4e-13 to -8e-13, and
the rows imply upper bounds of 2.8e10 to 8.6e10 on them.

## The push's polish

On liswet2 the push settles in one round with 9998 of the 10000
second-difference rows active. Its eight refinement passes took the rows'
residual from 0.2926 to 0.2872 of their tolerance, 0.3% a pass, so the
rows stayed 2.9e-10 off and the objective 1.1e-6 under the optimum. The
rows' matrix `E Θ E'` has eigenvalues far under the regularisation of
1e-8, and refinement shrinks a residual along those by only `λ/(λ + δ)`.

Conjugate gradients on the same matrix, preconditioned by the push's
factor, reach the rows in 86 steps (0.2865 to 0.0052 of the tolerance).
The polish runs once, when the push has settled, and keeps its step only
if the rows come closer, no free variable leaves its box and the push's
sign test still passes. liswet2 then ends at 24.998076102879509, rows 5e-12
off, taken by the checker.

The first version ran the same steps inside every round whose refinement
stalled. liswet8 then did not settle and the checker refused its point
(duals off by 3.1e-4), and liswet8, liswet10 and liswet11 took 2.2x, 2.8x
and 2.1x the work (`qp-push-cg-rounds` in `bench/refusals.txt`).

The polish's reading, `make maros-meszaros-baseline J=4` with the new
manifest: 138 instances, 137 solved, 137 objective ok, 136 checker ok,
1 failed (values), and aug3dqp alone over the ceiling. The polish ran on
two instances. liswet2 took 37978994 work units where it took 18819512.
stadat1's rows went from 2.7e-9 to 9.1e-10 off for 18347084 work units
where it took 18153166. Every other instance reads main's work to the
unit. The baseline also takes up work that earlier commits changed and
the results already read: cont-100, cont-200, cont-201, cont-300, ksip,
laser and ubh1.

## qgrow22 and QPLIB_9002 to the conic interior point

`qp-stall-to-conic.diff` hands a quadratic model to the conic interior
point when the push does not settle and the walk cannot reach
`BARRIER_TOL_QP`, as after a failed barrier. qgrow22 ends `OPTIMAL` at
-149628953.47 with duals off by 7.95 (the barrier's point: 2.98e-6) and
143091710 work units (67023693). QPLIB_9002 ends `NUMERICAL_ERROR` at
an infeasibility certificate the checker does not confirm, where the
barrier's point ends `OPTIMAL` with duals off by 2.1e4. Refused
(`qp-stall-to-conic`).

## The MIQP set

`miqpall.sh ARM` runs QPLIB's 17 convex mixed-integer QPs (types CBL, CML
and DML) at 1e10 work units, three at a time; `bound` adds
`--node-select bound`. `miqp-estimate.txt` is the default order since
ae25a70, `miqp-bound.txt` the best bound. The incumbents are the same but
on QPLIB_3547, where the default finds the reference's -0.56001 and the
best bound -0.2007. The best-bound order ends with a higher bound on eight
files, QPLIB_3871 the most (113.2 against 106.1), and a lower one on
QPLIB_10050. The default stays. The polish leaves all 17 to the bit.

`miqp-nodes-without-push.diff` skips the push at every node of the tree.
`miqp-nopush.txt`: QPLIB_3980 gets its first incumbent, 7.6 against a
reference of 6.325; QPLIB_3547 stops after one node with -0.5170 where the
push found -0.5600; QPLIB_5577 loses its root bound. Refused
(`miqp-nodes-without-push`).
