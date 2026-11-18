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

J24. **QPs whose optimum lies far out.** 02-365's `netqp.py` circulations
carry cycles of arcs bounded only above, with curvatures down to 1e-11,
so their optimum can carry flows of 1e11 to 1e13. 10 of seeds 1 to 400
and 18 of seeds 1001 to 1800 at 12 nodes end `numerical_error`, and 1 of
seeds 1 to 120 and 1 of seeds 1001 to 1200 at 40 nodes
(`bench/measurements/02-448/`). Seed 1611 has ended optimal since the
checker judges a reduced cost against its terms. Of the 31 before it
(`02-400/`, `02-413/`, `02-416/`), 16 stop on the barrier's growth stop,
3 at its iteration cap,
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

J27. **Walks that break at a cone's boundary, and optima no point
reaches. Parked on 2026-11-15 by the maintainer.** Quadratic rows and
cones with no interior are held (`bench/measurements/02-446/`,
`02-452/`). In 02-431's permutation reading 49 of 200,000 continuous
models and 13 of 198,391 mixed ones still end with an answer in one
order and `numerical_error` in the other, and 2 mixed ones at two optima
(`02-452/`). Of 02-452's 71 pairs, 19 have room in every piece
(`02-453/`): in 10 no point attains the optimum and the walk's direction
stops being finite; 9 are walks that break near the end, because a
cone's slack and dual both near its boundary, the NT scaling widens to
|w| ~ 1e5, and the slack direction, recovered through W², grows past
1e+150. 11 of the 41 pairs with a flat cone are left: a cone whose probe
fails because another piece has no interior, a walk that ends with no
answer, and two infeasible models. Next: the slack direction recovered
from the linearised primal rows, read on 02-453's 8 models, then on
every conic gate, since it moves every conic iterate. Verify with
02-452's `read.sh`.

## Tier 3: the largest performance gaps

J7. **MIP: MIPLIB 2017, 9 of 30 at 1e10 work units, 29 with an
incumbent** (`bench/results/miplib2017.txt`); in 20 s JAOS solves 5 and
HiGHS and SCIP 8 each, the three JAOS leaves open being `beasleyC3`,
`binkar10_1` and `neos-911970` (`bench/compare/README.md`). Open, largest
first:

- No incumbent: `ic97_potential`. `csched008` has one at its root from
  J32's search over the starts (`bench/measurements/02-472/`): 175, its
  optimum 173. Its bound stays at 171 after 96 nodes. Next for it: the
  node cost (about 6e7 work units a node, 02-466).
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
- The seven at 5e10, five times the budget (02-465): `binkar10_1` closes
  with node MIR (02-474, below), and `pk1` and `mas76` close inside 1e10
  (`bench/measurements/02-475/`). `pk1`'s objective is a whole number at
  every integer point, which the objective step now reads through
  continuous columns held as differences; with node relaxations that stop
  at the incumbent and reduced-cost fixing at every node it closes at
  7.5e9 in 174,316 nodes, where SCIP takes 353,721. `mas76` closes at
  5.6e9 in 379,655 nodes against SCIP's 216,712. SCIP's ablation on
  `mas76` puts its conflicts from nodes that pass the incumbent at 1.54x
  its nodes and its node cuts at 1.46x; JAOS has neither on `mas76`'s
  rows, which all hold its continuous column.
- The other four lack bound per node: `gen-ip002`, `gen-ip054`, `neos5`
  and `assign1-5-8`. In 20 s JAOS takes 1.5 to 9 times the nodes of
  HiGHS and SCIP on `gen-ip002`, `gen-ip054` and `assign1-5-8` (02-465);
  on `gen-ip002` and `gen-ip054` JAOS's bound per node leads, and neither
  HiGHS nor SCIP closes them in 300 s (02-476). Next: a conflict from a
  node whose relaxation stopped at the incumbent, its proof the stopped
  duals over the path's bounds, read on `mas76`, `mas74` and the four,
  since JAOS draws conflicts from infeasible nodes only (02-475).
- `gen-ip054` closes at 2.8e9 work units in 75,989 nodes on an
  LLL-reduced basis of the improving region (`--lattice`, 02-476), where
  the columns' tree is open at 1e11. The tree first runs 20,000 nodes over
  the columns for an incumbent: the reduced tree finds few points of its
  own, and with 1000 first `gen-ip002` lost its optimum (found by the
  columns' tree at node 10,326) and ended at -4754.42. `gen-ip002` holds
  the optimum and its bound at 1e10 moves from -4804.34 to -4802.43; best
  bound first on the columns reads -4801.21. Rounding each node's point in
  the columns costs 4% to 9% of the work and finds better points late
  (`lattice-round-columns`). Next for `gen-ip002`: a heuristic for the
  reduced tree whose points hold the bound rows `V y >= 0`, so that the
  first phase can be short. With 02-475's fixing at every node the first
  20,000 nodes no longer reached the optimum, and `gen-ip002` ended 0.0025
  away; with Devex at the nodes (02-479) it holds the optimum again, bound
  -4805.67. Verify with `gen-ip002` holding its optimum and closing inside
  1e10.
- `neos5` and `assign1-5-8` are out of reach at 1e10
  (`bench/measurements/02-477/`, `02-479/`, `02-480/`). HiGHS closes
  `neos5` in 206,031 nodes and SCIP has not after 2.48M. A node
  relaxation took 34 iterations against HiGHS's 12.6: its steepest-edge
  weights started at 1 and restarted at 13.7 of its pivots, so it priced
  by the largest infeasibility; SCIP under partial pricing takes 35.8.
  Below the root the dual now prices by Devex (`bench/measurements/02-479/`):
  18 iterations and 8.6e4 work units a node against 3.1e5, 116,376 nodes
  and bound 14.25 at 1e10 against 32,557 and 14. Closing in HiGHS's node
  count needs 4.9e4 a node. Of the node work, 63% is in up children
  (`x_j = 1` on its covering rows) solved away from their parent, 34
  iterations each; with node presolve kept on every node it reaches
  150,788 nodes, but MIPLIB 3 reads 1.225x (`node-presolve-always`).
  Neither HiGHS nor SCIP closes `assign1-5-8` in 600 s (200.0 and 205.0).
  Its gap is the tree's: the LP reads 183.36, the products of its columns
  with the triangle rows 184.53, its 26 cost columns alone made integer
  193, the optimum 212. A second symmetry search before the first branch
  finds the exchange of the four groups, and the objective step takes the
  26 cost columns as whole (02-477); the Gomory rounds now read those
  columns, and the slacks of their rows, as integer (02-480): the root
  goes from 185.26 to 189.80 and the bound at 1e10 from 195.9–196.3 to
  197.6–198.1 over five null readings, some 0.6 under SCIP's at the same
  nodes. JAOS's bound gains 0.9 a doubling of the nodes (200.25 at
  500,000), SCIP's 1.3; past 211 takes about 2e9 nodes at JAOS's rate and
  2.5e7 at SCIP's, against 40,000 at 1e10. Of SCIP's 199.05 at 50,000
  nodes symmetry is 6.1, reliability branching 4.2 and node cuts 2.3.
  Next for `assign1-5-8`: integral slacks on the all-integer equality rows
  with Gomory rounds past the first, 198.7–199.0 at 1e10, in a form that
  keeps `bell5`'s tree (`gomory-integral-slacks`, `gomory-rounds-implied`);
  then reliability branching's probes. Next for `neos5`: the up children's
  relaxations, and a node presolve rule read per node, since
  `MIP_NODE_PRESOLVE_ITERS` was set on iteration counts of the old
  pricing. Verify at 1e10 on both and with `make miplib J=2`.
- Strong branching is on by default for a linear model: reliability 4,
  the children on the node's kept factors, each capped at 1.5 times the
  node's work (02-461 to 02-464). The 2017 gap sum is 13.52 against
  13.67, and the dual side 6.515 against 6.596. Of the seven, `pk1`'s dual
  gap narrows from 0.256 to 0.203 and `mas76`'s from 0.0073 to 0.0057, and
  none closes. Cap 1 reads 13.36 on 2017 but takes `misc03` to 2.9x,
  whose tree grows from the capped probes' weak bounds (02-462). On
  `neos-911970` a JAOS node takes about 200 LP iterations against
  HiGHS's 72. Next: what makes cap 1 cheaper on 2017 without growing
  `misc03`, read probe by probe.
- `binkar10_1` closes at 5.2e9 work units in 6,280 nodes with MIR over
  each node's bounds at every depth, on the rows whose columns are all
  integer, with c-MIR's complementation and divisor halving, stopped
  once they lift under a quarter of the branching gain
  (`bench/measurements/02-474/`; HiGHS 4,066 nodes, SCIP 1,859). The
  same steps at the root reach SCIP's 6710.3 but grow `gt2` and `p0201`
  (`mir-root-cmir-steps`). Next: the root's steps on the rows where they
  lift, read on `gt2` and `p0201`.
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
`bench/measurements/02-328/m17sum.py`. Read the sum against 02-474's
spread: six readings of one code, with only the root's work moved,
span 10.85 to 11.88, so a change is judged instance by instance.

J32. **MIP: a library of primal heuristics, at least 50.** JAOS has 19:
- for linear MIPs:
  - node rounding, lock rounding;
  - the search over the starts of a time-indexed schedule, the
    feasibility jump, the exchanges between assignment rows, threshold
    accepting over the items of partitioning rows, the same priced by an
    LP over the columns that link rows, moves over the disjunctions of
    big-M rows;
  - the root dive, the feasibility pump, a RENS sub-MIP, a RINS sub-MIP,
    the RINS dive;
  - local branching (off), and the dive from node 1000 with no
    incumbent;
- for MIQPs: the relaxation rounded, and a halving dive;
- for the conic tree: rounding and a dive.

The LP, QP and conic solvers need none: they solve to optimality, and their
start rules (crash basis, initial basis) are not primal heuristics. SCIP
ships about 50. Step 0 is a scheduler that every heuristic plugs into. Each
heuristic has a detector of where it applies and a share of the work, and
the scheduler keeps the success of each and picks the next by it, with a
seeded rule, so the portfolio stays deterministic. Its first piece is in:
at a root with no incumbent, the dive and the pump each get half the
root's work, or a floor by the model's size (`MIP_HEUR_ROOT`,
`MIP_HEUR_FLOOR`, 02-466). MIPLIB 3 reads its baseline, and the 2017 gap
sum goes from 13.52 to 13.36. Then, by what each reaches on the open
instances:

- Made for a structure. 02-463's `census.py` sorts the open MIPLIB 2017
  instances into families, each with a heuristic in the literature for its
  shape. The first, exchanges between assignment rows, closes `enigma` at
  the root (895 nodes to 1, `bench/measurements/02-463/`). The second,
  threshold accepting over the items of partitioning rows, takes the 2017
  gap sum from 13.36 to 13.06 (`mad` 0.30 to 0.09 of the optimum away,
  `neos-911970` 0.030 to 0.004, `bench/measurements/02-469/`). The third,
  moves over the disjunctions of big-M rows, takes it to 12.04 (`glass4`
  1.0 to 0.42, `graphdraw-domain` 0.44 to 0.06, `neos-3754480-nidda` to
  its optimum, `bench/measurements/02-470/`). The fourth,
  a search over the starts of a time-indexed schedule, takes it to
  10.91 (`csched008` from no incumbent to 0.012 away, `csched007`
  0.32 to 0.13, `bench/measurements/02-472/`). The fifth, the same items
  search priced by an LP over the columns that link rows, takes it to
  10.85 (`glass4` 0.50 to 0.25 and its dual gap 0.33 to 0.25,
  `bench/measurements/02-471/`). The rest, largest first, follow below.
- Generic, by family (Berthold's survey of MIP primal heuristics, 2006 and
  2014; Fischetti and Lodi, 2011):
  - rounding: shifting, ZI round, seeded randomized rounding,
    shift-and-propagate (Berthold and Hendel);
  - diving: fractional, coefficient, guided toward the incumbent,
    pseudocost, vector-length (for covering and partitioning rows),
    line-search, Farkas, conflict;
  - neighbourhoods: crossover and mutation over the pool, DINS, a
    zero-objective sub-MIP, RENS at nodes, variable neighbourhood search
    over local branching; proximity search was refused on its own budget
    (`proximity-search`, 02-467) until the scheduler can give it work only
    while it improves;
  - improvement: 2-opt, polishing by evolution over the pool (Rothberg);
    1-opt with the continuous columns held was refused (`one-opt`,
    02-468) until an LP over them follows the moves;
  - local search: tabu and weighted moves past one flip (lift and
    breakthrough moves), simulated annealing over the jump.
- For MIQP and the conic tree, which have two each: the jump, the pump,
  RINS and RENS carried to quadratic and conic rows.

The structures, largest first:

- Partitioning rows with items whose continuous columns link rows:
  02-471's search prices each move by an LP, about 56,000 units on
  `glass4`, 18,000 of them the setup of each solve, and takes most of its
  budget there; `glass4` ends 0.25 away and `graphdraw-domain` 0.10.
  Next: an LP that keeps its factors between moves, read on those two.
- Big-M placement: 02-470's moves change one disjunction at a time, with
  kicks of up to four. Alone, `glass4` stops at 1.70e9, 0.42 away, and its
  last ten runs find nothing; with 02-471's search it ends 0.25 away. `supportcase26` ends 0.018 away against 0.011
  before: a job there moves past several of the other chain's jobs at
  once, and a single flip between them closes a cycle. Next: a move that
  frees every disjunction of one object (a rectangle, a job) and solves
  them as a sub-MIP with the rest held, read on those two.
- Periodic timetables: the modulo network simplex (`ic97_potential` has no
  incumbent, `timtab1` is 0.68 away).
- Scheduling with one start per job, the objective after the first
  point: 02-472's search places the jobs by their earliest start, ties by
  the relaxation's mean start, and sees the objective only through the
  bound it propagates. On `csched008`'s makespan that bound drops the late
  starts (175, its optimum 173). On `csched007`'s total tardiness it drops
  none, so the search stops at its first point, 407, and the RINS sub-MIP
  takes it to 397 at 1e10 (optimum 351). Next: a neighbourhood over the
  starts that frees the jobs of one time window and searches them again
  under the bound, read on `csched007`.
- Fixed-charge networks: dynamic slope scaling, then closing and opening
  arcs (`beasleyC3` 0.10 away).
- Market split and general integer equality systems: lattice
  reformulations (`markshare2`, `gen-ip002`, `gen-ip054`, 02-394's 34
  models with no point).
- Shapes the census marks, outside the open set: set covering by a
  Lagrangian greedy (Beasley; Caprara, Fischetti and Toth), knapsack
  systems by a core greedy (`mas74` 0.08 away), lot sizing by
  relax-and-fix and fix-and-optimize (`tr12-30`), bin packing by first fit
  decreasing, colouring by DSATUR, facility location by opening and
  closing sites.

Each one runs only where its detector finds its shape. Verify with the
primal side of `m17sum.py`, the first incumbent's node, `make miplib
J=2`, and 02-394's set for the general integer family.

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

J11. **MIP switches that are off by measurement**: zero-half and lifted
cover cuts, local branching, restarts outside network
mode, bound propagation and probing. The last reading had none land
(`bench/measurements/02-361/`); propagation runs only in a tree with no
incumbent past node 1000 (`02-407/`). Each needs a form that pays on
MIPLIB 3 and the 2017 set.

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
