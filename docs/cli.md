# The command-line tool

`jaos` solves a model from a file, converts one between the formats JAOS
reads and writes, and runs the library's four analyses on a model: the
independent checker, the infeasible subsystem, the exact verifier and
ranging. It is a thin program over the public API: it links `libjaos.a`
through `include/jaos.h` like any other consumer, and it can do exactly what
the library can do. `make cli` builds it as `build/cli/jaos`, and `make test`
compiles it and runs its test, `tests/cli.sh`.

## Usage

```
jaos solve FILE [--solution OUT] [--start SOLUTION] [--proof PATH]
                [--work-limit N] [--time-limit SECONDS]
                [--primal-tol T] [--dual-tol T]
                [--mip-start SOLUTION] [--cutoff V]
                [--cut-rounds N] [--cover-rounds N] [--cut-depth D]
                [--node-cut-cap K] [--cut-stall F] [--node-cut-stall F]
                [--root-cut-drop | --no-root-cut-drop]
                [--cover-lift | --no-cover-lift] [--mir-rounds N]
                [--mir-aggregate N] [--node-mir | --no-node-mir]
                [--dive] [--dive-child RULE] [--dive-backtrack N]
                [--dive-gap F] [--dive-degrade F]
                [--dive-heuristic N] [--dive-heuristic-depth D]
                [--rins N] [--feaspump N] [--pump-general 0|1]
                [--pump-obj F] [--pump-always | --no-pump-always]
                [--rcfix | --no-rcfix]
                [--propagate N] [--propagate-depth D]
                [--no-heuristics] [--node-limit N] [--branching RULE]
                [--reliability N] [--probe-cap M] [--probe-depth D]
                [--no-cut-drop] [--pool-size K]
                [--log LEVEL] [--quiet]
jaos convert IN OUT
jaos check FILE SOLUTION [--tol T]
jaos check FILE --proof PROOF
jaos stats FILE
jaos iis FILE
jaos verify FILE [--values] [--proof PATH]
jaos ranging FILE
jaos --version
jaos --help
```

**The branch-and-bound switches are the long half of that list, and every
one of them has a measurement behind it.** A default is a setting that was
swept and won; a switch that is off is one that was built, measured and
refused, and it stays reachable so the refusal can be re-tested.
`bench/refusals.txt` names what would reopen each, and `make refusals`
runs the ones with a script. None of them changes an LP.

Options take their value as the next argument: `--work-limit 1000`, not
`--work-limit=1000`.

Every command prints one fact per line on stdout, as `key value`. Rows and
columns are named as the file names them; a constraint an LP file left
unlabelled is called by its position, `R<I+1>` counting from 1, and a
column `C<J+1>` (D284). Numbers are printed with 17 significant digits, so
they read back as the same double; an infinite bound reads `inf` or `-inf`.
Everything that is not a fact about the model goes to stderr.

## `solve`

`solve` reads `FILE`, solves it, and prints one fact per line on stdout:

```
status optimal
objective 29
iterations 3
work_units 412
time 0.000087
```

- `status` is one word: `optimal`, `infeasible`, `unbounded`, `work_limit`,
  `time_limit`, `node_limit`, `numerical_error` or `interrupted`.
- `objective` is printed only when the solve found an optimum. The library
  refuses to give an objective for any other outcome, because a number
  cannot be told apart from a genuine objective of zero, and the tool
  respects that refusal. The value is printed with 17 significant digits, so
  it reads back as the same double.
- `iterations` and `work_units` are the solve's own counts.
- `nodes`, `cuts`, `heuristic_points`, `first_incumbent` and `bound` are
  printed for a mixed-integer model only (D288, D289, D290): the
  relaxations the tree solved, the rows the root cuts added, the incumbents
  the rounding heuristic found, the node at which the first incumbent
  appeared (0 when none), and the best objective any open node could still
  reach, which is the optimum when the status is `optimal`.
- `time` is the solve's wall-clock seconds. It is always the last line.

`--quiet` prints the `status` line only.

**Where presolve fired, four more lines follow the counts** (D329):
`presolve_rows`, `presolve_columns` and `presolve_nonzeros` are the model
the simplex actually ran on, and `presolve_rounds` is the cascading loop's
own count. A model presolve does not touch prints none of the four, and
neither does a build with presolve compiled out.

**A mixed-integer solve prints its own lines**: `nodes`, `cuts`,
`heuristic_points`, `first_incumbent`, `bound`, and `fixed_cols` and
`tightened` where `--rcfix` and `--propagate` moved a bound. An LP prints
none of them.

### What is reproducible

Everything above the `time` line is byte-identical between two runs of the
same file with the same options, on any machine (D8). The `time` line is the
one number JAOS reports that is not reproducible, and it is printed last so
that `head -n -1`, or `grep -v '^time '`, removes it before a diff. Do not
put the `time` line in a file you diff against later.

One exception: a run that stops on `--time-limit` or on Ctrl-C is cut by a
clock, and where a clock cuts is not reproducible. Its `iterations` and
`work_units` lines can differ between runs. A run that stops on
`--work-limit` is reproducible, because the work counter is deterministic.

The solver's log, when `--log` asks for one, goes to stderr and never to
stdout. Logging never changes an answer: a model solved at `--log detail`
prints the same facts as the same model solved silently.

### Options

| option | what it does |
|---|---|
| `--solution OUT` | writes the solution file to `OUT`: the optimum when the solve found one, and the certificate when it proved the model infeasible or unbounded (D285). A solve that stopped on a budget or an interrupt has no answer to write; no file is written and stderr says why. The file format is JAOS's own; `docs/format-support.md` describes it. |
| `--start SOLUTION` | warm-starts from the basis in `SOLUTION`, a file `solve --solution` wrote for this model's optimum: the statuses are read and handed to the model before the solve, so re-solving a model from its own answer costs no iteration. A file for another model, or one holding a certificate, is refused with the library's message and exit 5. |
| `--proof PATH` | writes the answer's exact proof to `PATH` (D325, D328). An optimum's proof is its coordinates, so the tool runs `jaos verify` first and writes nothing when that refuses, saying so on stderr; an infeasible or unbounded answer's proof is the certificate the solve already published, which needs no verify because every double in it is already an exact rational. `jaos check FILE --proof PATH` judges any of the three from the model alone. Prints `proof_file PATH` when it wrote one. |
| `--mip-start SOLUTION` | hands the branch and bound the integer point in `SOLUTION`, a file this model's `solve --solution` wrote, before it runs (D326). It is checked at the root by the same acceptance every heuristic point gets, so a point that is not integral, or that sits outside a bound or a row, is refused and the search runs as if none had been given -- a starting point the caller got wrong is never published as an answer. What it buys is the pruning: the tree has a bound from node 1. No effect on an LP. |
| `--cutoff V` | drops every node whose relaxation cannot beat objective `V`, from node 1 and with no incumbent needed (D326). It also gates what may become the incumbent, so a cutoff tighter than the true optimum ends the search `infeasible` and exits 1 -- the honest answer to "is there a solution better than this?", not a defect. `V` is in the model's own sense. No effect on an LP. |
| `--work-limit N` | stops the solve after `N` deterministic work units. `N` must be a positive integer. The outcome is `work_limit`. |
| `--time-limit SECONDS` | stops the solve after that many wall-clock seconds. Must be positive; fractions are fine. The outcome is `time_limit`. |
| `--primal-tol T` | how far a variable may sit outside its bounds and still count as feasible. Default 1e-7. |
| `--dual-tol T` | how far a reduced cost may sit on the wrong side of zero. Default 1e-7. |
| `--cut-rounds N` | rounds of Gomory mixed-integer cuts at the root of a mixed-integer model (D289): one cut per fractional integer column of the relaxation's basis per round, kept for the whole tree. Default 1, the setting that measured 0.660x the plain tree's work with no instance past 2x (D289); `0` turns them off. A negative count is a usage error. No effect on an LP. |
| `--cover-rounds N` | rounds of knapsack cover cuts at the root, beside the Gomory rounds (D300): every all-binary row, each finite side read as a knapsack over literals, gives the greedy cover the point violates, extended by every heavier item. Default 4, the setting that measured 0.745x the work of the Gomory round alone over the MIP set with no instance past 2x (D300); `0` turns them off. A negative count is a usage error. No effect on an LP. |
| `--cut-depth D` | one round of Gomory cuts at every node whose depth is at most `D`, each valid in its node's subtree and in the relaxation for exactly the nodes under it (D296). Default 3, with `--node-cut-cap`'s four cuts per node, the pair that measured 0.835x the work of root-only cuts over the MIP set with no instance past 2x (D301); `0` is the root only. A negative depth is a usage error. No effect on an LP. |
| `--node-cut-cap K` | at most `K` cuts per node below the root, the most efficacious kept, violation over the cut's norm (D301). Default 4; `0` is no cap, and the root's rounds are never capped. A negative count is a usage error. Only matters with `--cut-depth`. |
| `--no-cut-drop` | carries a node's cut to every node under it even once its slack is basic there; by default a cut that does not bind at a node leaves the relaxation under it, and two nodes holding the same cuts share the rows (D297). Only matters with `--cut-depth`. |
| `--dive` | dives from each selected node of a branch and bound: the child on the nearer side of the fraction is solved next and its sibling joins the open set, until a node is pruned or integral. Off by default, because it measured 1.125x the work of the plain best-bound order over the MIP set (D289). No effect on an LP. |
| `--dive-child RULE` | which child the dive solves first, with `--dive` (D295): `nearer`, the default, is the side the fraction is closer to; `up` and `down` are fixed; `pseudocost` is the direction whose expected objective loss is the smaller. An unknown rule is a usage error. No effect on an LP or without `--dive`. |
| `--cut-stall F` | ends the root's cut rounds after one that moved the bound by less than `F` of (1 + \|bound\|). Default 0, never: every fraction read at or above 1.007x over the MIP set (D304, refused). |
| `--node-cut-stall F` | gives no cut round to a node whose own round moved its bound by less than `F` of (1 + \|bound\|), and none to anything under it. Default 0, never: alone it reads 0.816x, and beside the root-cut drop every fraction leaves `bell5` at the cap (D305, refused). |
| `--root-cut-drop` | lets a root cut leave the relaxation below a node where its slack is basic, like a node's own cut. On by default: 0.799x the work over the MIP set, 13 better and 2 worse, none past 2x (D306). `--no-root-cut-drop` keeps every root cut in every node. |
| `--cover-lift` | lifts each cover cut with Balas's coefficients instead of extending it by every heavier item. Off by default: 1.005x with `l152lav` past 2x (D307, refused). `--no-cover-lift` is the default. |
| `--mir-rounds N` | rounds of mixed-integer rounding cuts on the model's rows at the root, beside the Gomory and cover rounds (D309, after Marchand and Wolsey): every row and finite side shifted to the bounds nearer the point, scaled by a few candidates, the most violated kept. Default 6, which measured 0.719x the work over the MIP set with none past 2x; `0` turns them off. |
| `--mir-aggregate N` | lets a MIR row absorb `N` others, substituting a continuous column out each time, before it is rounded. Default 0, the single-row form: the aggregate measured 1.140x at its best step count with `gen` past 2x (D312, refused). Only matters with `--mir-rounds`. |
| `--node-mir` | adds MIR cuts over a node's own bounds to its Gomory round, under the same cap. Off by default: 0.991x with two instances past 2x, and the same shape at every cap and depth (D310, refused). `--no-node-mir` is the default. |
| `--dive-backtrack N` | lets a dive resume from the deepest sibling it left, up to `N` times per dive. Default 0, every sibling to the open set at once: 0.992x at sixteen with two past 2x, 1.170x unbounded (D308, refused). Only matters with `--dive`. |
| `--dive-gap F` | resumes only while the waiting sibling's bound is within `F` of (1 + \|best open bound\|). Default 0, no bound on the resume: 1.067x at its tightest fraction and worse above (D311, refused). Only matters with `--dive`. |
| `--dive-degrade F` | goes on into a child only while the node's own bound is within `F` of (1 + \|its parent's\|), read before the node's own cut round. Default 0, no bound: 1.048x at best against the plain dive (D316, refused). Only matters with `--dive`. |
| `--dive-heuristic N` | at the root, on a copy of the relaxation as the cuts left it, fixes the integer column nearest an integer and solves again, up to `N` times, and judges an integral point like any heuristic point. Default 50: 1.032x the work with the first incumbent earlier on 6 of 24 and later on none (D313). `0` turns it off. |
| `--dive-heuristic-depth D` | runs that dive at every node down to depth `D`, the root being 0. Default 0, the root alone: depth 1 reads 1.049x and depth 4 1.356x with four instances past 2x, a worse rate than the heuristics already on (D314, refused). |
| `--rins N` | fixes the integer columns an incumbent and a node's relaxation already agree on and runs the dive on the rest, up to `N` solves, once per distinct incumbent (after Danna, Rothberg and Le Pape). Default 0, off: it found a point on one instance of 24 and moved no first incumbent, because the root dive reaches them first (D315, refused). |
| `--feaspump N` | rounds of the feasibility pump at the root (D318, after Fischetti, Glover and Lodi): the relaxation's point is rounded, the copy is re-solved for the point nearest that rounding in L1, and the pair repeats. Default 20: 1.026x the work with the first incumbent at node 1 on six of 24 and later on none. `0` turns it off. It runs only while nothing has an answer yet. |
| `--pump-general 0\|1` | gives the pump an auxiliary column and two rows per general integer column, so such a column's distance to its rounding counts wherever the rounding sits (after Bertacco, Fischetti and Lodi). Off by default: 1.007x alone with `gt2` the only instance it moves, and the objective pump reaches `gt2` without it (D320, refused). |
| `--pump-obj F` | blends the model's own objective into each of the pump's rounds at a weight that multiplies by `F` per round from 1 (after Achterberg and Berthold), so early rounds pull toward good points and late ones toward feasible. Default 0.5: 0.984x the plain pump's work, 2 better and 0 worse, `gt2`'s first incumbent from node 382 to 1. `0` is the plain pump; `1` or more is a usage error. |
| `--pump-always` | runs the pump at the root even where something already holds an incumbent. Off by default: 1.051x, `gen` at 3.083x, and **the first incumbent moved on none of the 24** -- the guard is skipping a pump that would have found nothing (D322, refused). `--no-pump-always` is the default. |
| `--rcfix` | at the root, once an incumbent exists, pulls an integer column's far bound in to the furthest integer its reduced cost still allows, and every node inherits it. Off by default: 1.010x, `p0282` 0.519x against `gt2` 2.492x out of the same deduction (D323, refused). `--no-rcfix` is the default. |
| `--propagate N` | passes of bound propagation at each node before its relaxation is solved: each reads the model's rows over the node's own bounds, proves the node infeasible with no solve where a row admits no point, and pulls in the integer bounds the rows imply. Default 0, off: 1.093x at one pass and 1.074x at four, with `bell5` unfinished at the cap (D324, refused). |
| `--propagate-depth D` | the deepest node propagation runs at, the root being 0; negative, the default, is every node. Depth 0 is not less propagation but the free half of it, since the root's deductions hold for the whole tree: 1.051x, and it moves a bound on 6 of 24 while the other 18 read exactly 1.000x (D324). Only matters with `--propagate`. |
| `--no-heuristics` | turns the rounding heuristic off: by default every fractional node's relaxation is rounded to the nearest integers and kept as the incumbent when it is inside every bound and row (D290). No effect on an LP. |
| `--node-limit N` | stops a branch and bound before its `N`-th node past the limit, as `node_limit`, keeping the incumbent it has; `N` must be a positive integer (D291). No effect on an LP. |
| `--branching RULE` | which column a fractional node branches on: `pseudocost`, the default, scores each column by the objective gain a unit move in each direction has cost so far in the tree; `most-fractional` takes the column farthest from an integer (D292). No effect on an LP. |
| `--reliability N` | how many branches in each direction a column needs before its pseudocost is trusted; below it, at most eight candidates per node have their children solved on the spot and the gains initialise the pseudocosts. Default 0, never: over the MIP set the probes cost more work than the smaller trees saved at every setting from 1 to 8 (D293, refused, `bench/refusals.txt`). No effect on an LP or under most-fractional branching. |
| `--probe-cap M` | stops each strong-branching child solve at `M` times the work the node's own relaxation took (D294); a probe that reaches it teaches the pseudocost nothing. `0` is no cap; a negative value is a usage error. No effect without `--reliability`. |
| `--probe-depth D` | strong branching probes at nodes down to depth `D` only, the root being 0 (D298); by default every depth. A negative depth is a usage error. No effect without `--reliability`. |
| `--pool-size K` | keeps the `K` best distinct integer points a branch and bound finds, best first (D299), and prints `pool_points N`, how many it holds; `K` must be a positive integer. Default 1, the incumbent alone, which prints no line. |
| `--log LEVEL` | prints the solver's log on stderr. `LEVEL` is `off`, `summary`, `progress` or `detail`. Default `off`. |
| `--quiet` | prints the `status` line only. |

Both tolerances act in the scaled space the solver works in;
`docs/tolerances.md` says what that means. A value the library refuses, such
as a negative tolerance, is reported on stderr and the tool exits 5 before
reading the file.

Ctrl-C during a solve stops it at the next point the solver checks, and the
tool prints `status interrupted` and exits 3. It does not kill the process
mid-way.

## `stats`

`jaos stats FILE` reads the model and prints what it is, one `key value`
per line. It solves nothing, which makes it the one analysis subcommand
with no verdict: the exit code is 0 unless the file cannot be read.

```
$ jaos stats model.mps
rows 27
columns 32
nonzeros 83
equality_rows 8
ranged_rows 0
one_sided_rows 19
free_rows 0
empty_rows 0
fixed_columns 0
ranged_columns 4
one_sided_columns 28
free_columns 0
empty_columns 0
integer_columns 0
binary_columns 0
objective_nonzeros 12
min_abs 0.109
max_abs 2.386
objective_min_abs 0.32
objective_max_abs 10
```

The four row counts partition the rows and the four column counts
partition the columns, so each set sums to its total; that is what the
tests check, because a count that is merely printed is not evidence that
the walk saw every row. A **ranged** row or column has two finite bounds
that differ, a **fixed** one has two that are equal, and a **free** one has
neither. **Binary** is what the branch and bound would see: the bounds
rounded inward to integers being exactly 0 and 1 (D292's rule), not a pair
that happens to read 0 and 1 before rounding. The magnitude pairs are over
the nonzeros and over the nonzero costs, and their ratio is what scaling
exists to shrink (D327).

## `convert`

`convert` reads `IN` and writes `OUT`. The output format is chosen by
`OUT`'s extension: `.mps` writes free-format MPS, `.lp` writes CPLEX-style
LP, and any other extension is a usage error. The output name is checked
before the input is read.

What JAOS writes, JAOS reads back as the same model, names included: the
input's names are written out, and a row or column the input did not name
is written by its position, `R<I+1>`, `C<J+1>`, `COST` (D284). A name the
LP dialect cannot spell -- one holding a `-`, starting with a digit, or
spelling a keyword -- is refused by name when converting to LP, with the
message pointing at MPS, which takes every name.

A write the format cannot express is refused: the tool prints the library's
message, which names the row or column, exits 5, and leaves no file behind.
`docs/format-support.md` lists what each format cannot express. The LP
dialect is the narrower one; a free row, for example, has no spelling in it.
When the LP writer refuses a model, converting it to `.mps` instead works.

## `check`

`check FILE SOLUTION` reads the model from `FILE`, reads `SOLUTION`, a file
that `solve --solution` wrote, and judges what it holds with the library's
independent checkers. The checkers work on the model as loaded, in its
original units, and share no code with the solver. The first line says
which kind of answer the file claims, `status optimal`, `status
infeasible` or `status unbounded`, and that decides which checker runs and
which report follows.

For an optimum the checker judges the column values and row duals, and the
output is its report, one field per line, with the field names of
`jaos_check_report` in `include/jaos.h`:

```
status optimal
max_col_violation 0
max_row_violation 0
max_row_violation_relative 0
max_dual_violation 0
primal_objective 29
dual_objective 29
objective_gap 0
gap_positive 0
gap_negative 0
max_dropped_multiplier 0
dropped_terms 0
certified_suboptimality 0
unquantified_rays 0
relative_suboptimality 0
primal_feasible yes
dual_feasible yes
checked_duals yes
gap_certified yes
```

The header comment on that struct says what each number means and why most
of them decide nothing on their own. The two that decide are
`primal_feasible` and `dual_feasible`. The exit code is 0 when both are
`yes` and 1 otherwise.

For a certificate (D285) the file carries one `ray` record per row when the
model was proved infeasible, or per column when it was proved unbounded,
and the matching checker judges it from the model alone. The report is
`jaos_certificate_report`'s fields for an infeasible file and
`jaos_ray_report`'s for an unbounded one, then `certified`:

```
status infeasible
sup_columns 4
inf_rows 7
gap 3
certified yes
```

The exit code is 0 when `certified` is `yes` and 1 otherwise. A
certificate whose numbers were changed still reads, because the reader
judges the format and not the mathematics; it is the checker that refuses
it.

`--tol T` is the checker's tolerance. It defaults to 1e-7, the solver's own
feasibility tolerance. `docs/tolerances.md` says how the checker applies it.

The solution file must be for this model. The library refuses a file whose
row or column count differs from the model's, or whose records carry names
other than the model's own, and the tool exits 5 with the library's
message. From an optimum it reads the values and the row duals; the reduced
costs, activities and basis statuses in the file are not used, because the
checker recomputes what it needs from the model.

### `check FILE --proof PROOF`

The other half of `check` judges an **exact proof file**, the one `solve
--proof` or `verify --proof` wrote (D325, D328). It is not the solution
file's checker with a tighter bar: it has no bar at all. Every comparison
is over the rationals, the model's own doubles being exact rationals
themselves, so there is no tolerance in this path and no near miss.

It also reads **no basis**. The file carries none. For an optimum the
checker re-derives the three conditions that make a point optimal — the
point is inside every bound and every row, every reduced cost points into
the model from the side its column rests on, and anything strictly inside
its bounds carries a zero multiplier — and those three together are
sufficient, so a file that passes is proved optimal rather than consistent
with somebody else's basis.

```
$ jaos solve model.mps --proof model.proof
...
proof_file model.proof
$ jaos check model.mps --proof model.proof
claims optimal
primal ok
dual ok
objective ok
terms 148
proof holds
```

`claims` says which of the three the file asserts. The `primal`, `dual`
and `objective` lines belong to an optimum and are printed for one only;
for a certificate the verdict is the `proof` line alone, with `at_row` or
`at_col` naming where it failed. `terms` is how many exact products the
check formed, which is what its cost scales with.

The exit code is 0 when the proof holds and 1 when it does not. A file
that is not a proof for this model is a usage error, exit 5. A product or
a sum that outgrows the exact arithmetic's limb budget exits 4: that is
"cannot judge" and not a verdict.

**The two certificate kinds need no `verify` step**, because the vector the
solve publishes is already exact and what is uncertain is only whether it
certifies. **They are also judged more strictly than `check FILE
SOLUTION`**, which ignores a term below its own traffic because a sum of
doubles cannot place a zero more finely. Over the 29 pinned infeasibles
the tolerance checker certifies 28 and this one certifies 18; every one of
the eleven fails on a single column whose multiplier is a rounding away
from zero with no finite bound on the side it points at. Both numbers are
right and they answer different questions
(`bench/measurements/02-211/`, D328).

## `iis`

`iis FILE` solves the model. When the answer is infeasible, it finds one
irreducible infeasible subsystem: a set of bound sides that has no feasible
point on its own, and that becomes feasible when any one of them is dropped.
The output is the status line, one line per bound side in the subsystem,
then the counts from the report:

```
status infeasible
row LIM2 upper
row EQ1 lower
col X1 lower
col X2 lower
members 4
candidates 4
solves 5
work_units 41450
from_certificate yes
```

A row's two bounds are two constraints, and so are a column's, so a row
whose both sides are in the subsystem appears twice. The number of side
lines equals `members`. Rows come first, then columns, each in index order
and each under its name.

`candidates`, `solves` and `work_units` are the cost: how many sides the
deletion filter started from, how many re-solves it ran, and what they cost
in the same unit `solve` reports. The re-solves run on a private copy, so
nothing is billed to the model. `from_certificate` says whether the
candidates came from the infeasibility certificate or the filter had to
start from every finite side.

A model may have several such subsystems. This finds one, the same one on
every machine and every run, so the output is reproducible.

Exit 0 when a subsystem was printed. When the model is optimal or unbounded
the tool prints its status line, says on stderr that there is nothing to
find, and exits 1. Exit 5 on an error, including a re-solve that could not
decide its side.

## `verify`

`verify FILE` solves the model. When the answer is optimal, it proves, or
refuses to prove, that the published basis certifies it, in exact
arithmetic with no tolerance anywhere. The basis is rebuilt over the
integers and eliminated exactly; the verdict is `optimal` when every basic
value lies inside its bounds and every reduced cost points into the model.

```
status optimal
proof optimal
stage none
bound_bits 2
capacity_bits 4096
blocks 3
largest_block 1
bytes_held 0
terms 10
```

- `proof` is `optimal`, `broken` or `refused`. `refused` is not a failure.
  It is the answer when the numbers the proof would hold do not fit in the
  arithmetic; `bound_bits` is what the proof needs and `capacity_bits` what
  the arithmetic holds.
- `stage` says which check a `broken` verdict came from: `rank`, `primal`
  or `dual`. It is `none` otherwise.
- On `broken`, `at_row` and `at_col` name the row or column that breaks the
  proof, when one does, and `violation` says how far out it is.
- `blocks`, `largest_block`, `bytes_held` and `terms` describe the work.

`--values` prints, after a `proof optimal`, what the proof proved (D286):
one `x NAME VALUE` line per column, one `y NAME DUAL` line per row, then
`objective_exact VALUE`, every value an exact rational -- `4`, `1/3`,
`-7/2` -- with no rounding anywhere. A basic column's value is what the
exact elimination solved, a nonbasic one's is the bound its status names,
and the objective is summed from those. The `objective_exact` line is
absent when that sum outgrew the arithmetic on a model whose values fitted.
Nothing is printed for a `broken` or `refused` proof.

```
x X1 4
x X2 3
x X3 3
y DEMAND 4
y CAP1 -2
y CAP2 -1
objective_exact 29
```

The exit code is the verdict: 0 proved, 1 broken, 3 refused. A model whose
solve is not optimal has no basis to prove; the tool prints its status line,
says so on stderr, and exits 5.

The cost is stated, not billed to the work counter, and it is not small.
Eliminating a block of `k` rows forms about `k` cubed products of large
integers, so on a model with a big block the proof takes seconds where the
solve took milliseconds. `SPECS.md` section 5 says how many of the
reference bases it proves and how many it refuses. The output is
reproducible bit for bit.

**`--proof PATH` writes the proof to a file** instead of printing it
(D325). It writes one only when the verdict is `optimal`; on `broken` or
`refused` it says so on stderr and the exit code is the verdict's.
`jaos check FILE --proof PATH` is what judges it back, and
`docs/format-support.md` describes the file.

## `ranging`

`ranging FILE` solves the model. When the answer is optimal, it prints how
far every cost, every row bound and every column bound may move, everything
else held, before the basis behind the optimum stops being optimal. Every
interval contains the number's current value; an end that is not limited
reads `inf` or `-inf`.

```
status optimal
cost X1 -inf 4
cost X2 -inf 4
cost X3 3 inf
rhs DEMAND 7 107 10 inf
rhs CAP1 -inf 4 0 7
rhs CAP2 -inf 3 0 6
bound X1 -inf 4 4 inf
bound X2 -inf 3 3 inf
bound X3 -inf 3 3 inf
```

- `cost NAME LOWER UPPER`: the interval the cost of column `NAME` may take.
- `rhs NAME LOWER_LO LOWER_HI UPPER_LO UPPER_HI`: for row `NAME`, the
  interval its lower bound may take, then the interval its upper bound may
  take.
- `bound NAME LOWER_LO LOWER_HI UPPER_LO UPPER_HI`: the same for the two
  bounds of column `NAME`.

Rows and columns come in index order, each under its name.

The intervals are about the basis, not about the answer. A model with more
than one optimal basis may carry the same optimum further along another
basis, and that union is not computed. A degenerate basis reports an
interval of zero width on the side a tie closes, which is the true answer
for that basis.

Exit 0 when the intervals were printed. A model whose solve is not optimal
has no basis to range; the tool prints its status line, says so on stderr,
and exits 5.

## Which reader is used

The reader is chosen by the input file's name. A name ending in `.lp` or
`.lp.gz` goes to the LP reader. Every other name goes to the MPS reader,
because an MPS file has been called `.mps`, `.MPS`, `.sif` and nothing at
all. The comparison is case-sensitive.

Compression is not decided by the name. Both readers look at the first two
bytes of the file and inflate a gzip file themselves, so `model.mps.gz` and
`model.mps` read the same way. `docs/format-support.md`, "Compressed input",
has the rule.

A file that cannot be read is reported on stderr with the library's message,
which names the offending line, and the tool exits 5.

## Exit codes

The exit code is the verdict, so a script can branch on it without parsing
stdout. What each code means depends on the command.

| code | `solve` | `check` | `iis` | `verify` | `ranging` |
|---|---|---|---|---|---|
| 0 | optimal | primal and dual feasible | a subsystem was printed | proved | intervals printed |
| 1 | infeasible | not feasible | the model is not infeasible | the basis does not certify the answer | |
| 2 | unbounded | | | | |
| 3 | stopped by a limit or Ctrl-C | | | refused: the numbers do not fit | |
| 5 | usage or I/O error | | | | |

`convert` exits 0 when the file was written and 5 otherwise. Code 4, a
numerical failure, is `solve`'s alone.

Every command exits 5 on a usage error, an unreadable input, an unwritable
output, a refused write, or a refused solution file. The three commands
that solve first (`iis`, `verify`, `ranging`) also exit 5 when that solve
does not finish, that is, when it ends `interrupted` or `numerical_error`,
because a solve that stopped decides nothing about the model and no verdict
code fits. `verify` and `ranging` exit 5 as well on a model whose solve is
infeasible or unbounded, since they need an optimum and codes 1 and 3 are
taken by their own verdicts.

The three analysis commands that solve first print the status line and
nothing else before the report, so the output of two runs of the same file
is byte-identical: none of them prints a time.
