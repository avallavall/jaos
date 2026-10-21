# 02-401 — a push proximal term that follows each column's curvature

Taken on 2026-10-21 on the tree of a90c64c. `always.sh`, `stall.sh` and
`broad.sh` take the readings; the records are in `always/`, `stall/` and
`broad/`. No code changed.

## The idea

02-400 found the push stalling on circulation seed 1282: free columns with
curvatures near 1e-9 keep reduced costs, because the push's proximal term,
`QP_PUSH_REG` (1e-6), is larger than their curvature, so each round moves
them about a thousandth of the way. `always.py` gives each free column a
proximal term of the smaller of `QP_PUSH_REG` and K times its curvature.
`stall.py` does the same only from the first round in which the push
stalls.

## The readings

02-365's circulations, against the tree (`always.sh`, `stall.sh`):

| set | the tree | always, K = 1e-3 | always, K = 0.1 | at the stall, K = 1e-3 or 0.1 |
|---|---|---|---|---|
| 12 nodes, seeds 1 to 400 | 13 `numerical_error` | 13: 25 and 201 solve, 102 and 163 fail; work 0.931x | 13: 201 solves, 163 fails; 0.930x | 13, the same; 1.003x and 1.004x |
| 12 nodes, seeds 1001 to 1400 | 6 | 6; 1380 ends `unbounded`; 0.944x | the same; 0.950x | 6; 1.008x |
| 40 nodes, seeds 1 to 120 | 2 | 1: 9 solves; 0.882x | the same; 0.928x | 1: 9 solves; 1.004x and 1.007x |
| 40 nodes, seeds 1001 to 1200 | 2 | 1: 1076 solves; 0.890x | the same; 0.856x | 2; 1.006x and 1.007x |

The always-on term at K = 0.1 on the other QP sets (`broad.sh`): 02-248's
6000 generated QPs and Maros-Meszaros read the same; of QPLIB's 11
continuous convex QPs, QPLIB_9002 takes 1.225x the work.

On seed 163 the always-on term makes a free column's weight in the push's
normal matrix about 1e10, and the full step then leaves rows off by up to
0.17 against a tolerance of 0.0068: the push frees six pins, pins them
again and repeats for 40 rounds. On seed 1380 the push fails the same way,
the conic walk reports an improving ray, and the tool publishes
`unbounded` with a ray the checker takes. Every arc there has a positive
curvature, so the model is bounded; the ray test reads the cycle's
curvature as zero.

The stall-triggered form changes almost nothing and costs 0.3% to 0.8%.
Neither lands (`push-reg-by-curvature`).

## The augmented system

On the augmented system a small curvature is a small pivot, not a large
weight in a sum, so the push's rows might keep their accuracy there.
`aug.py` makes the barrier take the augmented system for a QP, the walk
and the push both; `aug.sh` reads the 14 circulations above under the
tree, the augmented system, the augmented system with the always-on term
at K = 0.1, and that term alone (`aug.txt`). The augmented system solves
none of the failing seeds, takes up to 2.7x the work on the others, and
sends seed 1380 to the same false `unbounded`. With the always-on term it
keeps seed 163, which the term alone breaks, but breaks seed 1308.
