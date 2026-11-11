# TODO — the current milestone

Rows from `SPECS.md`, and defects found by reading the code and the
readings. A row says what is wrong, the next step and how to verify it.
What was tried and refused is in `bench/refusals.txt` and the
`bench/measurements/` folders a row names. A row leaves this file in the
commit that lands it. When the file is empty, pick the next rows from SPECS.

Milestone J, in priority order. Tier 1 is defects: answers the checker
refuses, or models with an answer that end with none. Tiers 3 and 4 are the
performance gaps, largest first. Tier 5 is the features SPECS still lists.
Where JAOS stands against other solvers is in `bench/compare/README.md`.

## Tier 1: defects

J21. **A cone model with quadratic rows whose optimum sits at a large
scale.** The second brute-force solve of 02-255 seed 4's model 533 ends
`numerical_error`: its columns, rows, cones and duals pass the checker
and its gap is 8e-7. The settle's projection moves the fixed column C4
5e-9 off its value and stalls, the active rows stay 1.7e-9 to 3.2e-9 off
their sides, and their multipliers are near 2e7
(`bench/measurements/02-445/`). Next: a projection that holds a fixed
column out of the system instead of as a constraint in it. Verify with
02-432's `gate.sh`: no `UNSURE` line on 02-255 seed 4.

J24. **QPs whose optimum lies far out.** 02-365's `netqp.py` circulations
carry cycles of arcs bounded only above, with curvatures down to 1e-11,
so their optimum can carry flows of 1e11 to 1e13. 10 of seeds 1 to 400,
6 of seeds 1001 to 1400 and 13 of seeds 1401 to 1800 at 12 nodes end
`numerical_error`, and 1 of seeds 1 to 120 and 1 of seeds 1001 to 1200
at 40 nodes (`bench/measurements/02-400/`, `02-413/`, `02-416/`). Of
those 31, 16 stop on the barrier's growth stop, 3 at its iteration cap,
and 12 converge but no push gives a point the checker passes; the conic
walk then reports, on 30, an improving direction the ray checker refuses
(`02-414/`). On six of the 12, only the reduced-cost test refuses a
point whose gap is certified. On five of those, a column sits 4e-5 to 80
off a bound with a reduced cost of 1e4 to 5e4 toward it, as large as its
terms, so a relative test would refuse them too (`02-417/`). HiGHS calls seven of them optimal at
points from which the objective still falls along a cycle, so it cannot
judge this family (`02-398/`). The six of seeds 1001 to 1400 stop in the
barrier on `BARRIER_DIVERGE_QP`. With that stop lifted, seed 1156's walk
is within 1e-5 by iteration 40; then a set of rows closed off by arcs at
their bounds keeps a residual near 1, and the walk drives those rows'
duals to 1e13. On seed 1282 the walk then converges and the push stalls,
because its proximal term, `QP_PUSH_REG`, is larger than the curvatures
of the free columns near 1e-9 (`02-400/`). A proximal term that follows
each column's curvature solves four more and breaks two
(`push-reg-by-curvature`); with it, seed 1380 reaches the conic walk,
which publishes a ray the checker takes, though every arc's curvature is
positive and the model is bounded: the ray test reads 1e-11 as zero
(`02-401/`). Seven barrier switches each finish seed 1340 or 1357 and no
other (`barrier-far-qp-switches`), and the augmented system solves none
(`02-401/`). Next: a walk that lifts an arc off its bound when a closed
set of rows keeps a residual, since all six stop in the walk before any
push;
then a push solve that keeps the rows exact when its weights span 1e-6
to 1e10. A ray test that weighs a
ray's curvature against its slope changes the checker's contract and is
the maintainer's decision. Verify with `netqp_read.py` on the same seeds
and 02-398's `read.sh`.

J27. **A quadratic row that only touches the other constraints.** On
02-427's continuous reading, 16 of 64,000 edited models end
`numerical_error`: the point passes the checker, its duals miss by 0.07
to 35. On the eight with one quadratic row, the row's smallest value
over the other rows and bounds equals its side, so no point lies
strictly inside it and finite duals need not exist. Moving each quadratic
side out by 1e-4 solves all 16 (`bench/measurements/02-427/`). In
02-431's permutation reading, 155 of 200,000 continuous models end with
an answer in one order and `numerical_error` in the other, and 22 of
198,380 mixed ones, after 02-432 and 02-433. Of seed 1's first five continuous ones,
four have a quadratic row or cone with no point strictly inside, and
with each side or head moved out by 1e-4 both orders agree. The fifth
has the dual's form of the same defect: its optimum is a limit that no
point reaches. Of the 36 mixed ones 02-432 took apart, 35 agree once
moved out, and one has a limit for an optimum. On such a row a point
within tolerance moves the objective by about the square root of the
tolerance, so two orders can also end at optima 9e-5 apart
(`bench/measurements/02-431/`, `02-432/`). Next:
duals of small violation and large size from the walk's last iterates,
since the checker's dual test is absolute; an optimum with no finite
duals changes the checker's contract and is the maintainer's decision.
Verify with 02-427's `warmqc.c` in qcp mode on seeds 1 to 8.

## Tier 3: the largest performance gaps

J7. **MIP: MIPLIB 2017, 5 of 30 at 1e10 work units, 28 with an
incumbent** (`bench/results/miplib2017.txt`); HiGHS and SCIP solve 8 each
in 20 s. Open, largest first:

- No incumbent: `ic97_potential` and `csched008`. HiGHS finds
  `csched008`'s first point by a sub-MIP at node 533; JAOS reaches 57
  nodes in 1e10, its root's ten LP solves in the cut loop taking 2.7e9 of
  it, so the node cost below decides it.
- Weak roots on the timetabling models: `timtab1` 427178 against HiGHS's
  569441, `ic97_potential` 3868 against SCIP's 3896. Their cuts come from
  rows summed around cycles of the event graph; the simple form was refused
  (`cycle-cuts-two-potentials`). Next: a separation that aims at the
  cycle's rounding gap.
- `beasleyC3`: HiGHS's presolve removes 597 of its 1750 rows, JAOS's 161.
  HiGHS drops 398 whole arcs (flow, binary, capacity row) and 199 node
  rows: leaves with no demand, whose two arcs form a cycle of two, and the
  leaves their removal leaves (`bench/measurements/02-387/`). JAOS on
  HiGHS's presolved model reaches bound 745 and incumbent 851 at 1e10,
  against 742 and 830, so presolve is not what keeps it open. Next: the
  root bound and the incumbent.
- Node cost: on `neos-911970` a JAOS node takes about 200 LP iterations
  against HiGHS's 72, and a strong-branching probe is a full LP solve.
  Next: probes as a few dual steps on the node's factors.
- `binkar10_1`'s bound stays 0.4% short; cuts at nodes did not pay
  (`node-pool-scan`, `bench/measurements/02-340/`).
- MIPLIB 3: `l152lav` and `bell3a` trees are far larger than HiGHS's; the
  Gomory family is JAOS's weakest (`bench/measurements/02-351/`, `02-364/`).
- General integers with equality rows: of 02-394's 700 generated MILPs,
  JAOS stops at 2e9 work units on 34 that HiGHS proves, 33 of them in that
  family. At 5e8 work units the default finds a point on 22 of the 34 and
  `--dive` on 16, so no switch fixes it (`bench/measurements/02-394/`).
  The root's feasibility jump finds none on the 33 even without its cap
  (`02-399/`), and the root restart never fires on them (`02-406/`).
  Node propagation from node 1000 while no incumbent is known brings the
  34 from 23 points to 27 at 5e8 (`02-407/`). Rounding on the lattice of
  the all-integer equality rows finds none (`02-422/`): the rows that block
  a first point carry continuous columns. Next: a heuristic whose first
  point satisfies such rows, read on those models and on MIPLIB.

Verify with `make miplib J=2`, `make miplib2017` and
`bench/measurements/02-328/m17sum.py`.

J8. **LP: time per iteration and presolve.** `stocfor3` takes 11.2x
HiGHS. Half of that is presolve: HiGHS leaves 8259 rows, JAOS 13305. In the
simplex the largest item left is the dense FTRAN of the steepest-edge `tau`
(14% of `stocfor3`'s instructions); `tau` is dense there, so a density
estimate of its own changes nothing (`bench/measurements/02-423/`). In a MIP node, Curtis-Reid scaling of
the node LP is 22% of `bell5`'s instructions (`bench/measurements/02-353/`,
`02-354/`); the tree's LP scale carried to the node was refused
(`node-scale-from-parent`). Verify with `tools/icount.sh` and
`make compare COMPARE_ARGS='-t P0'` on a quiet machine; the gates
byte-identical or re-based.

J9. **The large QPs and MIQPs.** 7 of QPLIB's 8 largest convex QPs reach
1e11 work units, and QPLIB_9008's factor does not fit in memory even with
nested dissection (about 45 GB). Next: a solve that does not factor the
whole system, or one that uses the time steps' structure. 15 of 17 convex
MIQPs and 37 of CBLIB's 80 mixed-integer instances stop at the work limit,
and `QPLIB_5543` ends `numerical_error` at its root
(`bench/measurements/02-415/`).
QPLIB_3312, a continuous QCQP, stops at 1e11 work units: its factor takes
7.5 times nql90's operations at a similar size, because of its 12-entry
rows; the disc rows and dense nodes are ruled out
(`bench/measurements/02-375/`). Next for it: an ordering that beats
minimum degree on that system, or a smaller system.

## Tier 4: the other performance rows

J10. **The primal's six overruns and the crossover's fourteen**
(`bench/results/primal.txt`, `bench/results/barrier.txt`). Seven primal
remedies are refused; read `bench/refusals.txt` first.

J11. **MIP switches that are off by measurement**: strong branching,
zero-half and lifted cover cuts, local branching, restarts outside network
mode, bound propagation and probing. The last reading had none land
(`bench/measurements/02-361/`); propagation runs only in a tree with no
incumbent past node 1000 (`02-407/`). Each needs a form that pays on
MIPLIB 3 and the 2017 set.

J29. **A MIP's tree depends on the signs of its integer columns.** With
columns mirrored and rows negated, all 24 MIPLIB 3 models take another
path. `bell5` mirrored is still open after 185,084 nodes at 2e9 work
units, where the model as it is ends at 20,089. A mirrored integer
column swaps which child is up and which is down, and a column mirrored
apart from its twins no longer matches them in the symmetry search
(`bench/measurements/02-440/`, `02-441/`). Next: on `bell5`, find the
first node where the two trees part and the rule that reads a sign
there. Verify with 02-440's `read.sh` on MIPLIB 3: `bell5` under 2e9
work units both ways.

J12. **Parallel.** A parallel simplex; a round of nodes cheap enough to be
the default where a node takes a few pivots (`bench/measurements/02-290/`).
The round size cannot follow the thread count, because the answer must not
depend on the machine.

## Tier 5: the features SPECS still lists

J13. **Cones and quadratic rows.** Cuts and warm starts in the conic tree;
a quadratic row over more than `CONIC_QC_DENSE` columns that matter;
QPLIB's mixed-integer QCQPs, where all 14 stop at 1e11 work units and 9
have no incumbent (`bench/measurements/02-376/`, `02-379/`). A bound point
at the root and a backtracking dive were refused (`conic-tree-bound-point`,
`conic-dive-backtrack`). Next for them: a first
point that keeps a cardinality row such as QPLIB_10010's, and a dive that
does not cost a full solve per step.

J14. **The barrier, PDLP and the concurrent solve.** A rule for an LP to
choose the augmented system; a crossover cheaper than a crash; for PDLP, a
set too large to factor and feasibility polishing; for the concurrent
solve, a set where the barrier wins.

J15. **Presolve.** Duplicate columns, dominated columns, bound tightening,
dual fixing, and the substitution of a column the equality does not imply
free. Each waits for a set where it removes 5% of the rows or columns
(D101, D246, D97 in `bench/refusals.txt`; `bench/measurements/02-312/`).

J16. **Links.** GAMS, which needs a link library of its own; `.nl` bodies
above degree two.

J17. **The feasibility relaxation's proof that no box holds a point, where
several inequality rows together leave no integer point.** The equality
rows' case and the single ranged row's case are proved
(`bench/measurements/02-362/`, `02-369/`).

J18. **The certified bound on suboptimality alone cannot separate a wrong
vertex from a right one.**

J19. **Exact solving with no tolerances.** Numbers past the limbs, and MIPs
(an exact branch and bound). With decimal data, 20 of Netlib's 94 still
outgrow the limbs; builds with 512 or 1024 limbs prove four of them
(`bench/measurements/02-358/`, `02-418/`, `02-419/`). Next: numbers whose
storage follows their size.
