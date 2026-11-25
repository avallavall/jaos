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
so their optimum can carry flows of 1e11 to 1e13. 4 of seeds 1 to 400
and 11 of seeds 1001 to 1800 at 12 nodes end `numerical_error`, and none
of seeds 1 to 120 and 1 of seeds 1001 to 1200 at 40 nodes, from 10, 18,
1 and 1 before the walk lifted a column off the bound that closes rows
only the regularisation moves, stopped reading a growing dual over rows
within 1e-2 as divergence, and kept its second leg's last point within
1e-8 (`bench/measurements/02-484/`). Of the 16 left, seven stop on the
growth stop: 288, 1411 and 1537 on a primal iterate past 1e10 of the
data, and 110, 1163, 1514 and 1746 on a dual one while their rows are
off by 1.6e-2 to 2.4. Seeds 127 and 1535 reach the iteration cap. On
195, 1104, 1282, 1458, 1504, 1624 and 40-node 1076 the walk converges
and no push or polish gives a point the checker passes. On all 16 the conic
walk then reports an improving direction the ray checker refuses. 02-417
found that on 195 and 1624 only the reduced-cost test refused a point
whose gap is certified, with a column off its bound by a reduced cost as
large as its terms. HiGHS calls such models optimal at points from which
the objective still falls along a cycle, so it cannot judge this family
(`02-398/`). On 02-398's seven, now four solved, one refinement step on
the normal equations finishes 1104 and 1282 as well, and with the growth
stop lifted all seven (`02-484/`, `r398-knobs.txt`); alone, before the
lift, it finished only 1357 (`barrier-far-qp-switches`). A push proximal
term that follows each column's curvature broke two
(`push-reg-by-curvature`). Next: that refinement step in a QP walk,
read on every QP set; then a push solve that keeps the rows exact when
its weights span 1e-6 to 1e10, for the seven that converge. The checker's
ray test stays as it is: on 2026-11-18 the maintainer chose to keep its
contract and push the solver instead. Verify with `netqp_read.py`
on the same seeds and 02-398's `read.sh`.

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

J36. **MIP: 96 of MIPLIB 2017's 240 benchmark instances never leave the
root in 300 s.** JAOS solves 20 of the 240 at 300 s on one thread, where
Mittelmann's table at the same time reads serial HiGHS 82 and SCIP 73;
97 end with no incumbent (`bench/measurements/02-491/`, the reading of
2026-11-18). Split by piece at 300 s (`bench/measurements/02-501/`), the
LP leads on 74 of 94: the root LP on 40 (32% of the work, 45% of the
seconds) and the cut rounds' LP re-solves on 34 (45%, 31%); the root dive
leads on most of the 17 heuristic ones. The dual simplex's solves no
longer pass over every row and the symmetry refinement sorts each colour
class alone, with every answer and work unit unchanged: the root LP runs
1.23x as fast on the 31 sampled models of 90,000 rows or more (up to
1.96x) and 1.01x on the rest, and no instance changes category. The root's
first solve now hands pricing to dual Devex when its exact weights drift,
as a lone LP does (`bench/measurements/02-503/`). `mzzv11`'s root LP ends
in its lone 63,258 iterations, `dano3_3`'s in 45,213 from 280,779 and
`qap10`'s in 22,721 from 379,018. Of the 33 whose 300 s ran out in the
root LP, 4 now end it: `mzzv11`, `uccase9`, `uccase12` and
`neos-3988577-wolgan`. At 300 s `qap10` is now optimal, and `uccase12`
and `dano3_3` leave the root. `ns1830653`'s re-solve after cut round 9
no longer settles for most of 300 s: a dual run's rounds stop once they
cost more than its walk (`bench/measurements/02-504/`), and its root
reaches bound 11162 from 11130, its ten cut rounds' re-solves taking
99.9% of the work. U's columns sit in one block in the dense forward
solve's order (`bench/measurements/02-505/`), every answer and work unit
the same: on 27 of the other 29 the root LP reaches 1.14x the iterations
in 300 s (geometric; up to 1.42x), and `rail01`'s ends. The
elimination's pass over a column no longer branches on each row where the
pivot column is long (`bench/measurements/02-506/`), every answer and work
unit the same: a factorization in the solver takes 0.70x the cycles on
`mzzv11`, 0.77x on `dano3_3` and 0.75x on `ex9`, and on the 26 of the 27
that run out of 300 s the root LP reaches 1.019x the iterations
(geometric; up to 1.21x).
Row positions kept exact cost the pass more than `compact_pivot_row`'s
search (`lu-exact-row-positions`). On `mzzv11`'s root LP the factorization
takes 22% of the seconds (14 ms, one every 65 iterations). Fewer updates
between factorizations does not pay as read (`bench/measurements/02-507/`,
`refactor-interval-and-fill`): every other interval and the rule on U's
fill move the walk more than its cost, and `wood1p`'s suboptimality bound
with it. Taking it up again needs the interval's cost apart from the walk,
one walk's bases and entering columns replayed through the LU at each
interval. Next: the cut rounds' re-solves. Verify with 02-506's `root.sh`
and 02-501's `pair.sh` on 02-505's `root27.txt`.

J7. **MIP: MIPLIB 2017, 11 of 30 at 1e10 work units, 29 with an
incumbent** (`bench/results/miplib2017.txt`); in 20 s JAOS solves 5 and
HiGHS and SCIP 8 each, the three JAOS leaves open being `beasleyC3`,
`binkar10_1` and `neos-911970` (`bench/compare/README.md`). Open, largest
first:

- `markshare2` holds bound 0 at 1e10. `markshare_4_0`, of its family,
  closes at 2.0e9 work units in 378,496 nodes under the dual proofs of
  node relaxations stopped at the incumbent (`bench/measurements/02-500/`).
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
  7.5e9 in 174,316 nodes, where SCIP takes 353,721, and under 02-500's
  dual proofs at 6.6e9 in 152,344. `mas76` closes at 5.6e9 in 379,655
  nodes against SCIP's 216,712, and under the proofs at 5.1e9 in 315,175;
  its null readings spread 1.5x on either side of the change. SCIP's
  ablation on `mas76` puts its conflicts from nodes that pass the
  incumbent at 1.54x its nodes and its node cuts at 1.46x; JAOS has no
  node cuts on `mas76`'s rows, which all hold its continuous column.
- Of the four that lacked bound per node, `gen-ip054` closes on a reduced
  basis (below) and `neos5` under the dual proofs (below); `gen-ip002` and
  `assign1-5-8` lack it still. In 20 s JAOS takes 1.5 to 9 times the nodes
  of HiGHS and SCIP on `gen-ip002`, `gen-ip054` and `assign1-5-8`
  (02-465); on `gen-ip002` and `gen-ip054` JAOS's bound per node leads,
  and neither HiGHS nor SCIP closes them in 300 s (02-476). Neither of the
  two makes a dual proof: their first 200 stopped relaxations give none
  that passes the checks, `gen-ip002`'s also with no cuts below the root,
  and the pool stops (02-500).
- `gen-ip054` closes at 1.4e9 work units on an LLL-reduced basis of the
  improving region taken after 1000 nodes (`--lattice`, 02-476, 02-478),
  where the columns' tree is open at 1e11. `gen-ip002` holds its optimum
  at 1e10 with its bound at -4803.89 (`bench/measurements/02-478/`): the
  reduced tree matches the columns' bound a node at twice the work a node,
  so once it has spent the first tree's work without passing its bound the
  tree goes back to the columns, which find the optimum. No rounding of
  the reduced tree's own points reaches a point past the first tree's
  incumbent (`lattice-node-rounding`). The check is a knife edge where the
  two trees' bounds a unit of work are alike: on `gen-ip002` it fails by
  0.95 after 1000 nodes and passes by 0.24 after 5000, and by 1.00 under
  `--node-select bound`, which then end at -4771.08 and -4737.98. Closing
  `gen-ip002` needs the bound at -4783.73: with the optimum given from the
  root, best bound first reads -4800.75 on the columns and -4800.93 on the
  reduced basis at 1e10, and SCIP -4801.29 after 2.27M nodes in 300 s.
  Under 02-500's change the 200 stopped relaxations before the proof pool
  stops move the work that check reads, the tree stays on the reduced
  basis, and `gen-ip002` ends at 1e10 with bound -4806.13 and incumbent
  -4774.65.
  Next for `gen-ip002`: a relaxation that lifts the bound per node, since
  node order and the basis move it by under 5. Verify with `gen-ip002`
  closing inside 1e10.
- `neos5` closes at 7.4e9 work units in 165,501 nodes (02-500), HiGHS's
  110,582 to 206,031, from the record's 1.91e11 in 2,977,069: up to 600
  dual proofs of node relaxations stopped at the incumbent cut and tighten
  the nodes before their relaxations, and since its symmetries move 79%
  of its integer columns its cuts stay at the root (`MIP_SYMMETRY_CUTS`).
  The proofs alone close it at 2.5e10, the root's cuts alone at 7.7e10.
  The proofs' tightened bounds had taught the pseudocosts gains for
  columns the nodes did not branch on; written ahead of the branching
  bound they shrink `neos5`'s tree from 222,931 nodes to 186,781 and
  `pk1`'s from 202,147 to 159,769. Orbital fixing at a node writes its
  bounds after the branching bound the same way. Next: the same order for
  orbital fixing, read on the models with symmetries.
- Neither HiGHS nor SCIP closes `assign1-5-8` in 600 s (200.0 and 205.0).
  Its gap is the tree's: the LP reads 183.36, the products of its columns
  with the triangle rows 184.53, its 26 cost columns alone made integer
  193, the optimum 212. A second symmetry search before the first branch
  finds the exchange of the four groups, and the objective step takes the
  26 cost columns as whole (02-477); the Gomory rounds read those
  columns, and the slacks of their rows, as integer (02-480), and on such
  a model the root's rounds go past the first while they lift the bound
  and a round's cuts hold under ten times the model's nonzeros (02-483):
  the root goes from 189.80 to 192.30 after eight rounds and the bound at
  1e10 from 197.32-197.52 to 199.04-199.36 over five null readings, in
  26,000 to 40,000 nodes against 58,000. At 20,000 nodes it reads
  198.5-199.0 against SCIP's 197.12, at 50,000 199.71 against 199.05.
  JAOS's bound still gains 0.9 a doubling of the nodes (202.93 at
  570,000), SCIP's 1.3; past 211 takes about 2e8 nodes at JAOS's rate and
  2.5e7 at SCIP's, against 30,000 at 1e10. Of SCIP's 199.05 at 50,000
  nodes symmetry is 6.1, reliability branching 4.2 and node cuts 2.3.
  The equality rows' integral slacks only rewrite a cut, since the slack
  is fixed, and grow `misc07` (`gomory-equality-slacks`); `bell5` grew
  under rounds past the first, not under integral slacks, and has no
  implied integers. The root now spends 2.2e9 of the 1e10 against 3.6e8,
  its heuristics' share with it. Under 02-500 its symmetries move 92% of
  its integer columns and its cuts stay at the root: bound 200 at 1e10 in
  35,814 nodes, and 200 in 32,762 nodes under `--reliability 16`. Next for
  `assign1-5-8`: reliability branching's probes.
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

J32. **MIP: a library of primal heuristics, at least 50.** JAOS has 23:
- for linear MIPs:
  - node rounding, ZI rounding at every node until 500 runs find nothing
    (`bench/measurements/02-499/`),
    lock rounding;
  - the search over the starts of a time-indexed schedule, the
    feasibility jump, the exchanges between assignment rows, threshold
    accepting over the items of partitioning rows, the same priced by an
    LP over the columns that link rows, moves over the disjunctions of
    big-M rows;
  - the root dive, the feasibility pump, a RENS sub-MIP, a RINS sub-MIP,
    the RINS dive;
  - crossover over the pool, mutation of the incumbent and DINS, the last
    two picked by their incumbents per run (`bench/measurements/02-488/`);
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
sum goes from 13.52 to 13.36. The second: in the tree, mutation and DINS
share a slot, and the one with the higher score of incumbents per run
runs (`MIP_GH_REWARD`, 02-488). Next: the same pick over every tree
heuristic, RINS among them, scored by work as well as runs, which
02-490's dives would need. Then, by what each reaches on the open
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
  - rounding: shifting, seeded randomized rounding, shift-and-propagate
    (Berthold and Hendel); ZI round is in (02-489) and stops after 500
    runs in a row without an incumbent (02-499). Against ZI off it reads
    1.013x geometric and 0.970x in total on MIPLIB 3, from 1.043x and
    1.061x with the old stop. Its own work is 0.25% of the set's; the
    rest is the path its units send the shares that read the tree's work
    down, strong branching's and the sub-MIPs'. `misc06`'s 1.43x is
    02-488's crossover at the root. On MIPLIB 2017 it keeps its points on
    `beasleyC3`, `gen-ip002` and `gen-ip054`, none of them a last
    incumbent, and ZI off reads 3.94 on the primal side against 5.23,
    `markshare_4_0` and `timtab1` by path. Next: shares that read the
    node relaxations' work alone, then a reading of what ZI's points are
    worth without the path;
  - diving: Farkas, conflict; the coefficient, pseudocost, guided,
    vector-length and line-search dives in the tree were refused
    (`tree-dives`, 02-490) until a dive costs less than its share or the
    scheduler gives the dives work only while they find points;
  - neighbourhoods: a zero-objective sub-MIP, RENS at nodes, variable
    neighbourhood search over local branching; crossover, mutation and
    DINS are in (02-488); proximity search was refused on its own budget
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
the default where a node takes a few pivots. The round size cannot follow
the thread count, because the answer must not depend on the machine.
Under `--tree-batch N` the linear tree solves each node once, on one of N
copies of its relaxation kept for the tree, and takes the rest of each
node in round order (stage 1 of `bench/measurements/02-495/`): rounds of
8 read 1.026x MIPLIB 3's work on one thread and the same records at 1, 2,
4 and 12 threads. Next: 02-495's `speedup.sh` on a quiet machine (load
under 1), for its item 8: at least 1.8x geometric at 8 threads, 1.5x at
4, peak RSS at most 1.5x batch 1; under a load of 5 it read 1.52x and
1.47x. Then stage 2: rounds of 8 by default where a tree's node
relaxations are expensive, by the rule in work units of 02-495's section
4, verified with its items 9 to 11.

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

J16. **Links.** GAMS, which needs a link library of its own.

J18. **The certified bound on suboptimality alone cannot separate a wrong
vertex from a right one.**

J19. **Exact solving with no tolerances.** Large dense blocks, and MIPs
(an exact branch and bound). With decimal data and numbers whose storage
follows their size, 91 of Netlib's 94 prove; `d2q06c`, `pilot` and
`pilot87` hold a block of 892 to 1487 rows whose fraction-free elimination
passes `VERIFY_BLOCK_WORK` after 370 to 450 seconds, and `dfl001` takes 36
minutes, since each of its 6 exact pivots sweeps its block of 3129 rows
again (`bench/measurements/02-358/`, `02-418/`, `02-485/`). Next: an
elimination that neither holds nor sweeps the whole block, such as Dixon's
p-adic lifting, and one factorization of a block for all its solves; read
on those four and on the 91 against 02-485's seconds.
