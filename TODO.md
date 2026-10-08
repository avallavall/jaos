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

J3. **Conic models with no accepted optimum.** QPLIB_2456, QPLIB_3105 and
QPLIB_2468 end `NUMERICAL_ERROR`: the walk's point is exact on the primal
side, and its duals miss by 3.5e-6 to 3.6e-5. Some ball rows sit 2e-5 to
4e-5 off their side with duals over 1e-7, and neither settle rule picks an
active set that holds (`bench/measurements/02-319/`, `02-327/`). Small
cases of the same failure: `bench/measurements/02-371/qcgen.py`, 48 of 150
models of 80 columns end `NUMERICAL_ERROR`. Next: a finish that moves the
columns and the duals together while the active set changes, such as a
semismooth Newton step on the KKT system, read first on those small
models. Also: a
certificate for an infeasibility whose free column with no curvature needs
its coefficient to vanish exactly. Verify with `make cblib` and 02-319's
`conread.sh`.

## Tier 3: the largest performance gaps

J7. **MIP: MIPLIB 2017, 5 of 30 at 1e10 work units, 28 with an
incumbent** (`bench/results/miplib2017.txt`); HiGHS and SCIP solve 8 each
in 20 s. Open, largest first:

- No incumbent: `ic97_potential` and `csched008`. HiGHS finds
  `csched008`'s first point by a sub-MIP at node 533.
- Weak roots on the timetabling models: `timtab1` 427178 against HiGHS's
  569441, `ic97_potential` 3868 against SCIP's 3896. Their cuts come from
  rows summed around cycles of the event graph; the simple form was refused
  (`cycle-cuts-two-potentials`). Next: a separation that aims at the
  cycle's rounding gap.
- `beasleyC3`: HiGHS's presolve removes 597 of its 1750 rows, JAOS's 161.
  Next: the MIP presolve's other reductions.
- Node cost: on `neos-911970` a JAOS node takes about 200 LP iterations
  against HiGHS's 72, and a strong-branching probe is a full LP solve.
  Next: probes as a few dual steps on the node's factors.
- `binkar10_1`'s bound stays 0.4% short; cuts at nodes did not pay
  (`node-pool-scan`, `bench/measurements/02-340/`).
- MIPLIB 3: `l152lav` and `bell3a` trees are far larger than HiGHS's; the
  Gomory family is JAOS's weakest (`bench/measurements/02-351/`, `02-364/`).

Verify with `make miplib J=2`, `make miplib2017` and
`bench/measurements/02-328/m17sum.py`.

J8. **LP: time per iteration and presolve.** `stocfor3` takes 11.2x
HiGHS. Half of that is presolve: HiGHS leaves 8259 rows, JAOS 13305. In the
simplex the largest item left is the dense FTRAN of the steepest-edge `tau`
(14% of `stocfor3`'s instructions). In a MIP node, Curtis-Reid scaling of
the node LP is 22% of `bell5`'s instructions (`bench/measurements/02-353/`,
`02-354/`); the tree's LP scale carried to the node was refused
(`node-scale-from-parent`). Verify with `tools/icount.sh` and
`make compare COMPARE_ARGS='-t P0'` on a quiet machine; the gates
byte-identical or re-based.

J9. **The large QPs and MIQPs.** 7 of QPLIB's 8 largest convex QPs reach
1e11 work units, and QPLIB_9008's factor does not fit in memory even with
nested dissection (about 45 GB). Next: a solve that does not factor the
whole system, or one that uses the time steps' structure. 13 of 17 convex
MIQPs and 37 of CBLIB's 80 mixed-integer instances stop at the work limit.

## Tier 4: the other performance rows

J10. **The primal's six overruns and the crossover's fourteen**
(`bench/results/primal.txt`, `bench/results/barrier.txt`). Seven primal
remedies are refused; read `bench/refusals.txt` first.

J11. **MIP switches that are off by measurement**: strong branching,
zero-half and lifted cover cuts, local branching, restarts outside network
mode, bound propagation and probing. The last reading had none land
(`bench/measurements/02-361/`). Each needs a form that pays on MIPLIB 3 and
the 2017 set.

J12. **Parallel.** A parallel simplex; a round of nodes cheap enough to be
the default where a node takes a few pivots (`bench/measurements/02-290/`).
The round size cannot follow the thread count, because the answer must not
depend on the machine.

## Tier 5: the features SPECS still lists

J13. **Cones and quadratic rows.** Cuts and warm starts in the conic tree;
a quadratic row over more than `CONIC_QC_DENSE` columns; QPLIB's
mixed-integer QCQPs.

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

J19. **Exact solving with no tolerances.** Numbers past the limbs, MIPs (an
exact branch and bound), and data read as exact decimals
(`bench/measurements/02-358/`).
