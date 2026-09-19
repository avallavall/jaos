# 02-255 — integer columns with cones, and an unbounded relaxation

Until this reading a model with cones or quadratic rows and integer
columns was refused. `src/conictree.c` now solves it with a branch and
bound of its own: best bound first, the most fractional integer column
branched, the conic interior point at every node, and a node whose
relaxation is integral closed by a solve with those columns fixed at
their rounded values, whose answer is the one published, with its cones'
duals. The MIP gap, node limit, cutoff, MIP start and incumbent callback
apply to it; cuts do not, and its two heuristics came with the second
reading below. SOS sets,
semi-continuous columns, indicator rows and a node callback beside cones
are refused by name.

## The models

`misocp.c`: 3 to 8 columns and a planted point whose 1 to 3 integer
columns are integral, each integer column boxed to its planted value
plus or minus 1 or 2, the continuous columns in boxes of four shapes (box,
free, open above, open below). 0 to 2 cones, quadratic or rotated, whose
head columns are continuous and moved so the point sits inside; 0 to 4
linear rows and 0 to 2 convex quadratic rows, strictly satisfied at the
point, their sides on a 2^-20 grid; a third of the models carry a convex
diagonal objective. Every fifth model gets a row that holds one integer
column between two integers, so no integer point is feasible while the
relaxation is. Brute force fixes the integer columns at every value of
their boxes and solves each continuous model. The tree has to end
`INFEASIBLE` when no value is feasible, `UNBOUNDED` when a feasible value
is unbounded, and otherwise `OPTIMAL` at the best value within 1e-6
relative, with a point the checker takes at 1e-6 and integer columns
exactly integral. `conic.c` of 02-253 prints a digest of the answers in
the same way.

`misocp.sh [RUNS] [SEED] [OUT]` builds `misocp.c` against
`build/release/libjaos.a` and runs it; a model that fails a check is
written to `OUT/fNNNNN.mps`.

## What the first run found, 100 models at seeds 1 to 3

- **The linear tree called an integer-infeasible model unbounded.** A
  model the generator drew with no cone and no quadratic row is a plain
  MILP or MIQP and goes to `src/mip.c`, which ended `UNBOUNDED` whenever
  a relaxation was unbounded. The trap rows made 5 of 300 such models
  infeasible over the integers with an unbounded relaxation. For rational
  data an unbounded relaxation makes a model with an integer point
  unbounded, so both trees now settle it with one more solve: the model
  with its objective cleared. An integer point ends the tree `UNBOUNDED`,
  none ends it `INFEASIBLE`. `tests/test_mip.c` holds the case
  (`2x = 1` against `2x = 2`, with a ray in `y`). The four gates read
  byte-identical after the change: none of their relaxations is
  unbounded.
- **The conic tree gave up at the first node its relaxation failed on.**
  On CBLIB (below) 11 of the 32 instances the first run reached ended
  that way, each at a node whose walk stopped within 1e-6 of an optimum
  the checker did not pass. A node
  relaxation now keeps such a point, marked rough (`conic_rough`, set
  only under `node_solve`): the tree branches on it but never prunes by
  its objective, so its children keep the parent's bound, and every
  answer the tree publishes still comes from a fixed solve the checker
  takes. A node whose relaxation fails outright is split at the middle of
  its widest integer column; only a node with every integer column fixed
  that still fails ends the tree `NUMERICAL_ERROR`.
- **Best bound first found no integer point** on the weak formulations
  of CBLIB's sssd and on turbine07_lowb: 30000 nodes and 1e11 work units
  without an incumbent. A plunge down the child the value rounds to did
  not help, since each dive ended at an infeasible node and the search
  went back to the shallow nodes. The tree now searches depth first, with
  backtracking to the latest open node, until its first incumbent, then
  best bound first with a plunge after each branching. sssd-weak-15-4
  finds its first incumbent at node 33. On the generated models the
  answers are the same to the bit in every order tried.

## The generated reading, 1000 models at seeds 1 to 3

1784 optima, every one within 2.5e-16 of brute force and taken by the
checker with its integer columns exactly integral; 598 infeasible and 609
unbounded verdicts, all as brute force says; no failure. In 9 models
brute force itself met a numerical error on one of its continuous solves,
and those are not scored. 7311 nodes over the 3000 models. Before the
rough nodes and the splits, one root relaxation ended the tree as a
numerical error (seed 3, model 358); it now splits and solves.

## CBLIB 2014, the 80 mixed-integer instances

`cblib.sh` at a work limit of 1e11 units per instance. The references are
CBLIB's `stat.set-cblib2014.csv`: MOSEK's points, most of them claimed
optimal.

| outcome | instances |
|---|---|
| `OPTIMAL`, taken by the checker | 36 |
| work limit, with an incumbent | 27 |
| work limit, no incumbent | 17 |
| numerical error or wrong verdict | 0 |

- **The optima.** 17 end below the reference, by up to 6.2e-7 relative,
  4 equal it, and 12 end above it by at most 2.6e-7. The other three are
  sssd-strong-20-4, -25-4 and -30-4, at 3.7e-6, 6.3e-6 and 3.1e-6 above.
- **The sssd answers stand.** On sssd-strong-20-4 a gap of 1e-12 gives
  the same point, and a cutoff of 287809.5, between the reference and the
  answer, ends `INFEASIBLE` in 809 nodes. Every node relaxation there
  ends with its two objectives within 4e-10. The checker refuses the
  duals of 4 of them (a violation of 1.2 to 2.7), and node 697 of those,
  solved alone with the stall stop off, ends at the same value to 1.2e-9.
  CBLIB's point is integral to about 1e-5 only, MOSEK's default: the
  tree's answer with every integer column loosened by 1e-6 solves to
  287809.90, loosened by 1e-5 to 287804.78, and the reference 287809.41
  lies between. The weak and the strong file of one sssd model carry
  references up to 6.1e-6 apart, in both directions.
- **27 end at the work limit with an incumbent.** Four are within 1e-4 of
  the reference: classical_50_3, pp-n100-d10000 (below it), pp-n100-d10
  and pp-n1000-d10. Seven are within 1.3e-2: sssd-strong-15-4, the four
  sssd-strong-*-8, robust_100_2 and robust_100_3. The other 16 are 17% to
  340% above the reference, and their bounds 5% to 65% below it: the
  eight sssd-weak files, six uflquad-nopsc files over 20 and 30
  facilities, and turbine07_lowb with its aniso variant.
- **17 end at the work limit with no incumbent**: shortfall_50_3, the
  six shortfall files over 100 and 200 assets, the three classical_200,
  robust_100_1, the three robust_200, pp-n1000-d10000 and the two
  pp-n100000.
- **Two pp references are wrong.** The pp files (Ziegler 1982) minimise
  `sum c_j x_j + e_j / x_j` over one row `sum a_j x_j <= b`, so their
  relaxation has a closed form (`pp.py`). On pp-n100000-d10000 it is
  21957042.36, the tree's bound to 1e-9, and CBLIB's reference
  18347954.04 is 16% below it; the library gives that point a
  feasibility error of 5.0e-3. pp-n100000-d10 has no reference (error
  1.41, objective 0); its closed form is 746996.16, again the tree's
  bound. `pp.py` also rounds the relaxation's point to integers that keep
  the row: 21957096.92 and 748470.84 on those two, and a point below the
  reference on pp-n1000-d10000, pp-n100-d10000 and pp-n1000-d10. The
  tree has no rounding heuristic, so it finds none of these.

## The rounding and the dive, a second reading

The tree above had no incumbent on 17 CBLIB instances, and `pp.py` found
points on four of them with a plain rounding. The tree now tries two
heuristics at the root. Both fix the integer columns and solve for the
rest:

- the relaxation's integer columns rounded to the nearest integer
  (`--no-heuristics` turns it off);
- while no incumbent is known after that, a dive: each solve fixes the
  half of the fractional integer columns nearest an integer, until none
  is fractional or `--dive-heuristic` solves are spent (50 by default,
  the linear tree's cap), and its last point is rounded.

**Three readings chose that.** A first version also rounded at nodes 2,
4, 8 and on. Over the whole CBLIB set it gave the outcomes of the table
below. Then each heuristic alone, on 18 instances (`cblib.sh` with
`--dive-heuristic 0` or `--no-heuristics`): the dive earns the portfolio
gains (robust_50_1 at 0.12x the plain tree's work against 0.90x with the
rounding alone, shortfall_50_1 at 0.069x against 1.00x) and alone finds
shortfall_50_3's incumbent; the rounding alone finds the first incumbent
at node 1 on uflquad-psc-20-100 and pp-n10-d10000, and on the
pp-n100000 files it is one solve where a dive over 100000 columns needs
17. Then the root-only rounding, on the 35 instances where the first
version moved something: the same incumbents but one, robust_200_2,
which it improves from 3.1e-3 to 1.2e-3 above the reference, and 0.88x
to 1.00x the first version's work on the trees of 7 to 31 nodes; over the
whole set it gives the same outcomes at 0.989x the first version's work
on the 37 optima. The later roundings are refused
(`conic-rounding-schedule`).

On the generated models every answer is the same to the bit: the three
digests do not move. Nodes fall from 7311 to 7299 and the work rises to
1.093x, the price of the extra solves on trees of 2.4 nodes on average.

On CBLIB at the same work limit of 1e11:

| outcome | before | after |
|---|---|---|
| `OPTIMAL`, taken by the checker | 36 | 37 |
| work limit, with an incumbent | 27 | 43 |
| work limit, no incumbent | 17 | 0 |

- robust_100_3 now ends `OPTIMAL`, and robust_100_2's incumbent moves
  from 5.0e-3 above the reference to 2.4e-9.
- Over the 36 instances optimal both times the work is 0.749x (geometric
  mean 0.761): 0.069x on shortfall_50_1, 0.09x to 0.12x on the three
  robust_50, and up to 1.41x on turbine54, whose tree has 7 nodes.
- The 17 without an incumbent now end with one: shortfall_50_3 within
  1.7e-7 of the reference and pp-n1000-d10000 3.8e-8 below it; the 100-
  and 200-asset portfolio files within 5.2e-4 to 6.1e-3 above it;
  pp-n100000-d10000 at 21957096.97 against its bound of 21957042.36, and
  pp-n100000-d10 at 748506.60 against 746996.16.

## Files

- `misocp.c`, `misocp.sh`: the generated reading.
- `cblib.sh`: the CBLIB reading.
- `pp.py`: the closed form of a pp file's relaxation, and its rounding.
