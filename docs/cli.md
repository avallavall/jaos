# The command-line tool

`jaos` solves a model from a file and converts a model between the formats
JAOS reads and writes. It also runs the library's five analyses on a model:
the independent checker, the infeasible subsystem, the feasibility
relaxation, the exact verifier and ranging. It links `libjaos.a` through
`include/jaos.h` like any other consumer, so it can do what the library can
do. `make cli` builds it as `build/cli/jaos`, and `make test` compiles it
and runs its test, `tests/cli.sh`.

## Usage

```
jaos solve FILE [--solution OUT] [--start SOLUTION] [--work-limit N]
                [--mip-start SOLUTION] [--partial-start POINT]
                [--cutoff V]
                [--basis BAS] [--write-basis BAS]
                [--write-point PT] [--write-duals D] [--pool-out PRE]
                [--proof PATH] [--exact]
                [--time-limit SECONDS] [--primal-tol T] [--dual-tol T]
                [--threads N]
                [--cut-rounds N] [--cover-rounds N] [--cut-depth D]
                [--clique-rounds N] [--zero-half-rounds N]
                [--flow-cover-rounds N] [--hull-rounds N]
                [--node-cut-cap K] [--cut-stall F] [--node-cut-stall F]
                [--root-cut-drop | --no-root-cut-drop]
                [--cover-lift | --no-cover-lift] [--mir-rounds N]
                [--dive] [--dive-child RULE] [--dive-backtrack N]
                [--dive-gap F] [--node-mir | --no-node-mir]
                [--mir-aggregate N] [--dive-heuristic N]
                [--dive-heuristic-depth D] [--rins N]
                [--local-branching K]
                [--tree-batch N]
                [--dive-degrade F] [--feaspump N]
                [--pump-general 0|1] [--pump-obj F]
                [--pump-always | --no-pump-always]
                [--rcfix | --no-rcfix] [--tighten | --no-tighten]
                [--mip-presolve | --no-mip-presolve]
                [--restart | --no-restart]
                [--probing | --no-probing] [--probing-cap M]
                [--clique-fix | --no-clique-fix]
                [--conflicts | --no-conflicts]
                [--symmetry | --no-symmetry] [--orbital | --no-orbital]
                [--propagate N]
                [--propagate-depth D]
                [--algorithm dual|primal|barrier|pdlp|concurrent]
                [--no-heuristics]
                [--opt NAME=VALUE]... [--params FILE]
                [--node-limit N] [--branching RULE]
                [--node-select RULE]
                [--reliability N] [--probe-cap M] [--probe-depth D]
                [--no-cut-drop] [--pool-size K] [--log LEVEL]
                [--check] [--quiet]
jaos convert IN OUT [--positional]
jaos check FILE SOLUTION [--tol T]
jaos check FILE --proof PROOF
jaos check FILE --point POINT [--duals DUALS] [--tol T]
jaos stats FILE
jaos options [--opt NAME=VALUE]... [--params FILE]
jaos diff A B
jaos show FILE (--row NAME | --col NAME)
jaos iis FILE [--write OUT] [--positional] [--work-limit N]
jaos relax FILE [--rows | --cols] [--apply OUT] [--positional]
             [--work-limit N]
jaos verify FILE [--values] [--proof PATH] [--basis BAS]
             [--work-limit N]
jaos ranging FILE [--work-limit N]
jaos STUB -AMPL [NAME=VALUE]...
jaos --version
jaos --commit
jaos --help [COMMAND]
```

Options take their value as the next argument: `--work-limit 1000`, not
`--work-limit=1000`.

Every command prints one fact per line on stdout, as `key value`. Rows and
columns carry the names the file gives them. A row the file left unnamed is
`R<I+1>`, counting from 1, and such a column is `C<J+1>`. The exception is
`at_row` and `at_col` in `check --proof` and `verify`, which print the
0-based index. Numbers have 17 significant digits, so they read back as the
same double. An infinite bound reads `inf` or `-inf`. Everything that is
not a fact about the model goes to stderr.

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
  `time_limit`, `node_limit`, `numerical_error` or `interrupted`. On
  `numerical_error` the library's reason goes to stderr as
  `jaos: FILE ends numerical_error: REASON`. A point whose objective
  overflows a double ends `numerical_error` with that reason.
- `objective` is printed only for an optimum, because the library gives no
  objective for any other outcome.
- `objective_exact` follows `objective` under `--exact` when the optimum
  was proved over the rationals. It is an integer or a ratio of two
  integers.
- `iterations` and `work_units` are the solve's own counts.
- `time` is the solve's wall-clock seconds. It is the last line of the
  solve's own report. The lines of `--check`, `--pool-out` and `--proof`
  come after it.

Where presolve fired, four more lines follow the counts. `presolve_rows`,
`presolve_columns` and `presolve_nonzeros` give the size of the model the
simplex ran on, and `presolve_rounds` counts the presolve rounds. A model
presolve does not touch prints none of the four, and neither does a build
with presolve compiled out.

A branch and bound that solved at least one node prints these lines, in
this order: `nodes`, `cuts`, `heuristic_points`, `first_incumbent`,
`fixed_cols`, `tightened`, `symmetry_generators`, `symmetry_orbits` and
`bound`, then `incumbent`, `start_accepted` and `pool_points` when they
apply. An LP prints none of them.

- `nodes` counts the relaxations the tree solved.
- `cuts` counts the rows the cut rounds added, at the root and at the nodes
  down to `--cut-depth`.
- `heuristic_points` counts the incumbents a heuristic found.
- `first_incumbent` is the node at which the first incumbent appeared, 0
  when none did.
- `fixed_cols` counts the bounds `--rcfix` moved, and `tightened` the
  bounds `--propagate` moved. Each reads 0 when its switch is off.
- `bound` is the best objective an open node could still reach.
- `incumbent` follows when the tree holds an integer point, so a tree
  stopped by a limit says what it found. At `optimal` the `objective`,
  `bound` and `incumbent` lines print the same number.
- `start_accepted` follows when `--mip-start` or `--partial-start` gave a
  start: 1 when the start, or the point that completed it, was taken, and 0
  when it was not.
- `pool_points` is described under `--pool-size`.

In a branch and bound, a node whose relaxation fails is set aside with its
bound and the tree goes on. The tree then ends `optimal` only when its
incumbent is within the gap of every bound set aside. Otherwise it ends
`numerical_error` with the first such node's reason.

### What is reproducible

Every line but `time` is byte-identical between two runs of the same file
with the same options, on any machine. `grep -v '^time '` removes the
`time` line before a diff. `head -n -1` removes it only when `--check`,
`--pool-out` and `--proof` are absent, because their lines follow it.

A run that stops on `--time-limit` or on Ctrl-C stops where the clock
says, so its `iterations` and `work_units` lines can differ between runs. A
run that stops on `--work-limit` is reproducible.

In the library, a stopped solve can go on from where it stopped (see
`jaos_solve` in `docs/api.md`). The tool cannot resume, because its
process ends with the solve. `--start` and `--basis` are its warm starts.

The solver's log, when `--log` asks for one, goes to stderr. Logging does
not change an answer.

### Options

| option | what it does |
|---|---|
| `--solution OUT` | writes the solution file to `OUT`: the optimum, or the certificate of an infeasible or unbounded model. A MIP has a certificate only when its root relaxation is infeasible. A solve stopped by a limit or an interrupt writes no file, and stderr says why. `docs/format-support.md` describes the format. |
| `--start SOLUTION` | warm-starts from the basis in `SOLUTION`, an optimum's or a certificate's file that `solve --solution` wrote for this model. A file for another model, or one with no basis, is refused with the library's message and exit 5. |
| `--basis BAS` | warm-starts from the basis in `BAS`, an MPS basis file, which may come from another solver. Giving both `--basis` and `--start` is a usage error. A file whose names or count do not fit the model is refused with the library's message and exit 5. |
| `--write-basis BAS` | writes the basis the solve stopped on to `BAS`, as an MPS basis file. An optimum, an infeasible or unbounded answer and a stop on a limit leave a basis. A solve that never ran, one that ended on a numerical error, and a verdict presolve reached without a simplex leave none. Then stderr says so and the exit code stays the answer's. |
| `--write-point PT` | writes the optimum's point to `PT`, one `NAME VALUE` line per column, the shape `check --point` reads. Any other outcome writes no file, and stderr says why. |
| `--write-duals D` | writes the optimum's row duals to `D` in the same shape, for `check --point P --duals D`. |
| `--pool-out PRE` | writes one point file per solution pool entry, best first: `PRE-0.pt`, `PRE-1.pt`, and so on. Prints `pool_files N`, the number of files written. An LP has no pool, so nothing is written and stderr says so. The exit code does not change. |
| `--check` | runs the independent checker on an optimum and prints the eighteen-field report of `check FILE SOLUTION`, then a `check_ok` line. `check_ok` is `yes` when the point is primal feasible and the duals are feasible or were not checked. On a model with cones, `max_cone_violation` follows the eighteen fields. The tolerance is always 1e-7, whatever `--primal-tol` says. The exit code stays the solve's. For any other outcome stderr says there is nothing to check. |
| `--proof PATH` | writes the answer's exact proof to `PATH`, always as plain text, and prints `proof_file PATH`. For an optimum the tool runs `jaos verify` first and writes nothing unless the verdict is `optimal`. Where `jaos verify` refuses the model (an optimal MIP, a quadratic objective, cones or quadratic rows), the tool exits 5 after printing its report. For an infeasible or unbounded answer the tool derives the exact ray. When it cannot, it writes the published doubles if they pass the exact check. Otherwise it writes nothing, says so on stderr and exits 5. `jaos check FILE --proof PATH` judges the file. |
| `--exact` | proves an LP's answer over the rationals and repairs a basis the proof breaks by exact pivots (`jaos_set_exact` in `docs/api.md`; measured in `bench/measurements/02-358/`). A proved optimum prints `objective_exact`, and its published values are the exact ones rounded to the nearest double. A model that turns out infeasible over the rationals ends `infeasible` with an exact certificate. An answer that cannot be proved ends `numerical_error` with the reason. A MIP, a quadratic objective or a model with cones is a usage error. |
| `--work-limit N` | stops the solve after `N` deterministic work units, as `work_limit`. `N` must be a positive integer. In a MIP every sub-solve runs under what is left of `N`. |
| `--time-limit SECONDS` | stops the solve after that many wall-clock seconds, as `time_limit`. Must be positive; fractions are fine. It is the one input that breaks bit-identity. |
| `--primal-tol T` | how far a variable may sit outside its bounds and still count as feasible. Default 1e-7. |
| `--dual-tol T` | how far a reduced cost may sit on the wrong side of zero. Default 1e-9, `DUAL_TOL` in `docs/tolerances.md`. |
| `--algorithm A` | the method for a continuous model: `dual`, the default, `primal`, `barrier`, `pdlp` or `concurrent`. [Which method solves what](#which-method-solves-what) says which models each one takes. |
| `--threads N` | the thread count, `1` by default; `0` takes every core. More threads speed up `--algorithm concurrent`, the barrier's Cholesky factor, and a tree under a `--tree-batch` above 1. The answer, the tree and the `work_units` line are the same at any count, and only `time` changes (`bench/measurements/02-264/`, `bench/measurements/02-288/`). `docs/api.md`, "Threads", has the details. A negative count is a usage error. |
| `--log LEVEL` | prints the solver's log on stderr. `LEVEL` is `off`, `summary`, `progress` or `detail`. Default `off`. |
| `--quiet` | prints the `status` line and none of the rest of the solve's report. The lines of `--check`, `--pool-out` and `--proof` still print. |
| `--opt NAME=VALUE` | any option by name, repeatable; the same setters the flags above reach. Names: `work_limit`, `time_limit`, `threads`, `primal_tolerance`, `dual_tolerance`, `algorithm`, `log_level`, `mip_gap`, `mip_node_limit`, `mip_tree_batch`, `mip_branching`, `mip_reliability`, `mip_probe_cap`, `mip_probe_depth`, `mip_cut_rounds`, `mip_cut_depth`, `mip_cut_drop`, `mip_node_cut_cap`, `mip_cover_rounds`, `mip_cut_stall`, `mip_node_cut_stall`, `mip_root_cut_drop`, `mip_cover_lift`, `mip_mir_rounds`, `mip_node_mir`, `mip_mir_aggregate`, `mip_dive`, `mip_dive_child`, `mip_dive_backtrack`, `mip_dive_gap`, `mip_dive_degrade`, `mip_dive_heuristic`, `mip_dive_heuristic_depth`, `mip_rins`, `mip_feaspump`, `mip_pump_general`, `mip_pump_obj`, `mip_pump_always`, `mip_rcfix`, `mip_tighten`, `mip_presolve`, `mip_probing`, `mip_probing_cap`, `mip_clique_fix`, `mip_conflicts`, `mip_symmetry`, `mip_orbital`, `mip_propagate`, `mip_propagate_depth`, `mip_heuristics`, `mip_pool_size`, `mip_cutoff`, `mip_clique_rounds`, `mip_zero_half_rounds`, `mip_flow_cover_rounds`, `mip_local_branching`, `mip_node_select`, `mip_restart`, `mip_gap_rule`, `mip_hull_rounds`, `exact`. A name matches in any case. Booleans take `true`/`false`, `on`/`off`, `yes`/`no` or `1`/`0`, also in any case. `mip_node_select` takes an integer: 0 for `bound`, 1 for `estimate`. `mip_gap` is the gap at which a branch and bound stops as optimal; it has no flag of its own, its default is 1e-6, and 0 means zero. `mip_gap_rule` takes `shifted`, the default, which measures the gap against `1 + |incumbent|`, or `relative`, which measures it against `|incumbent|` as SCIP and HiGHS do. `log_level` takes effect only through `--log`: set by `--opt` or `--params` it prints nothing, because the tool installs its log callback only for `--log`. An unknown name or a value of the wrong kind is a usage error. |
| `--params FILE` | options from a file: one `name value`, `name = value` or `name: value` per line, `#` to end of line a comment. Read before the `--opt` flags, so `--opt` overrides the file. Every other flag overrides both, except `--threads`, `--work-limit`, `--time-limit` and `--reliability`: the tool applies those four first, so the file and `--opt` override them. |

Both tolerances act in the scaled space the solver works in;
`docs/tolerances.md` says what that means. A value the library refuses,
such as a negative tolerance, is reported on stderr and the tool exits 5
before reading the file.

Ctrl-C during a solve stops it at the next point the solver checks. The
tool then prints `status interrupted` and exits 3.

### Branch-and-bound options

These options steer the branch and bound, and none of them changes an LP.
Each one sets the option named like the flag with `mip_` in front and
underscores for dashes: `--cut-rounds` sets `mip_cut_rounds`, and
`--mip-presolve` sets `mip_presolve`. The setter of that option in
`docs/api.md` describes it in full. Some defaults differ in network mode,
which `jaos_set_mip_mir_rounds` in `docs/api.md` defines.

Each default is the setting that won its measurement. A switch that is off
by default was measured and refused. It stays so the refusal can be tested
again: `bench/refusals.txt` says what would reopen each one, and
`make refusals` runs the ones that have a script.

| option | what it does |
|---|---|
| `--mip-start SOLUTION` | gives the tree the integer point in `SOLUTION`, a file this model's `solve --solution` wrote. The root judges it like any heuristic point, and a point that is not integral or breaks a bound or a row is refused. An accepted start lets the tree prune from node 1. |
| `--partial-start POINT` | gives the tree the values of the columns a point file names (`#` starts a comment). At the root, a copy of the model with every given integer column fixed at its rounded value is solved as a tree of at most `MIP_START_NODES` nodes, and its best point is judged as `--mip-start` judges one. The given values of continuous columns are not used. A name the model does not have, or a value that is not finite, is a usage error. |
| `--cutoff V` | prunes every node that cannot beat objective `V`, in the model's own sense, from node 1. No point that fails to beat `V` becomes the incumbent, so a cutoff better than the optimum ends `infeasible` with exit 1. |
| `--node-limit N` | stops the tree when its node count reaches `N`, as `node_limit`, keeping its incumbent. `N` must be a positive integer. |
| `--cut-rounds N` | rounds of Gomory mixed-integer cuts at the root, kept for the whole tree. Default 1; `0` turns them off. A negative count is a usage error. |
| `--cut-depth D` | one round of Gomory cuts at every node down to depth `D`; each cut holds in its node's subtree. Default 3; `0` keeps the cuts at the root. A negative depth is a usage error. |
| `--node-cut-cap K` | keeps at most `K` cuts per node below the root, the most efficacious. Default 4; `0` is no cap. A negative count is a usage error. |
| `--no-cut-drop` | keeps a node's cut in every node under it. By default a cut that does not bind at a node leaves the relaxation under it. |
| `--root-cut-drop` | lets a root cut leave the relaxation below a node where it does not bind. On by default; `--no-root-cut-drop` keeps every root cut in every node. |
| `--cut-stall F` | ends the root's cut rounds after one that moved the bound by less than `F` of (1 + \|bound\|). Default 0, never. |
| `--node-cut-stall F` | gives no more cut rounds to a node, or to anything under it, once its own round moved its bound by less than `F` of (1 + \|bound\|). Default 0, never. |
| `--cover-rounds N` | rounds of knapsack cover cuts at the root, from the all-binary rows. Default 4; `0` turns them off. A negative count is a usage error. |
| `--cover-lift` | lifts each cover cut with Balas's coefficients. Off by default; `--no-cover-lift` keeps the default, which extends a cover by every heavier item. |
| `--clique-rounds N` | rounds of clique cuts at the root, from the conflict graph of the all-binary rows. The graph is searched in a fixed order. Default 4; `0` turns them off. |
| `--zero-half-rounds N` | rounds of zero-half cuts at the root, from rows with integer data taken alone, in pairs and in triples, at most fifty cuts per round. Default 0, off. |
| `--flow-cover-rounds N` | rounds of flow cover cuts at the root (after Padberg, Van Roy and Wolsey), from single-node flow sets whose columns have variable upper bounds on binaries. Default 5 (`bench/measurements/02-317/`); `0` turns them off. |
| `--hull-rounds N` | rounds of hull cuts at the root: for a row of at most eight integer columns with at most 1024 integer points, the valid inequality the point violates most (`MIP_HULL_ROUNDS` in `docs/tolerances.md`). Default 20; `0` turns them off. |
| `--mir-rounds N` | rounds of mixed-integer rounding cuts on the model's rows at the root. Default 6. Left at the default, the rounds go on up to 20 while each lifts the root bound by at least 1e-4 of itself (`MIP_MIR_MORE`, `MIP_MIR_MORE_STALL`). A count given here runs as given. `0` turns them off. |
| `--mir-aggregate N` | lets an MIR row absorb up to `N` other rows before it is rounded. Aggregated root cuts are kept only when they lift the bound by `MIP_MIR_AGG_GAIN` of itself. Default 6; `0` is the single-row form, which a model with a quadratic objective always uses. |
| `--node-mir` | adds MIR cuts over a node's own bounds to its Gomory round. Off by default; `--no-node-mir` keeps the default. |
| `--dive` | dives from each selected node: the child `--dive-child` picks is solved next and its sibling joins the open set, until a node is pruned or integral. Off by default (D289-dive in `bench/refusals.txt`). Without `--dive`, a tree with no incumbent after 1000 nodes dives on its own until it finds one (`MIP_NOINC_DIVE_AFTER` in `docs/tolerances.md`). |
| `--dive-child RULE` | which child a dive solves first: `nearer`, the default, `up`, `down` or `pseudocost`. An unknown rule is a usage error. Only matters with `--dive`. |
| `--dive-backtrack N` | lets a dive resume from the deepest sibling it left, up to `N` times. Default 0. Only matters with `--dive`. |
| `--dive-gap F` | resumes a dive only while the waiting sibling's bound is within `F` of (1 + \|best open bound\|). Default 0, no condition. Only matters with `--dive`. |
| `--dive-degrade F` | goes on into a child only while its bound is within `F` of (1 + \|its parent's\|). Default 0, no condition. Only matters with `--dive`. |
| `--dive-heuristic N` | at the root, on a copy of the relaxation, fixes the integer column nearest an integer and solves again, up to `N` times. Default 50; `0` turns it off. |
| `--dive-heuristic-depth D` | runs that dive at every node down to depth `D`, the root being 0. Default 0, the root alone. |
| `--rins N` | fixes the integer columns on which an incumbent and a node's relaxation agree and dives on the rest, up to `N` solves per incumbent (after Danna, Rothberg and Le Pape). Default 50 (`bench/measurements/02-325/`), and 0 on a model with a quadratic objective unless it is set. |
| `--local-branching K` | for each incumbent, solves a tree of at most `MIP_LOCAL_BRANCHING_NODES` nodes that keeps the binary columns within `K` flips of it (after Fischetti and Lodi). Default 0, off (`bench/measurements/02-286/`). |
| `--feaspump N` | rounds of the feasibility pump at the root, while there is no incumbent. Default 20; `0` turns it off. |
| `--pump-general 0\|1` | gives the pump an auxiliary column and two rows per general integer column (after Bertacco, Fischetti and Lodi). Off by default. |
| `--pump-obj F` | blends the objective into the pump at a weight that starts at 1 and is multiplied by `F` each round (after Achterberg and Berthold). Default 0.5; `0` is the plain pump; `1` or more is a usage error. |
| `--pump-always` | runs the pump at the root even when an incumbent exists. Off by default; `--no-pump-always` keeps the default. |
| `--no-heuristics` | turns off the rounding heuristics: the rounding of each fractional node's point and, at the root of a linear model, lock rounding and the feasibility jump. In a model with cones or quadratic rows it turns off the root's rounding. |
| `--rcfix` | reduced-cost fixing at the root once an incumbent exists; every node inherits the bounds. On by default (`bench/measurements/02-335/`); `--no-rcfix` turns it off. |
| `--tighten` | coefficient tightening of binary columns in one-sided rows, and the parity step over equality rows, at the root. On by default; `--no-tighten` turns both off. |
| `--mip-presolve` | before the tree, merges two continuous columns that an equality row makes equal. On by default; `--no-mip-presolve` turns it off. |
| `--probing` | after the root solve, tries each fractional binary at 0 and at 1 and keeps what the propagation proves. Off by default; `--no-probing` keeps the default. |
| `--probing-cap M` | stops probing at `M` times the work of the root solve. Default 1; `0` is no cap; a negative value is a usage error. |
| `--clique-fix` | at each node, fixes every literal the root's clique table puts in conflict with a fixed binary. On by default (`bench/measurements/02-325/`); `--no-clique-fix` turns it off. |
| `--conflicts` | at an infeasible node whose proof needs at most 32 binaries fixed on the path, adds a row that forbids that combination. On by default; `--no-conflicts` turns it off. |
| `--symmetry` | detects the model's symmetries at the root, under a work cap of `MIP_SYMMETRY_WORK` (250) times the model's size. Off by default. `--orbital`, which is on by default, runs the same search, so this switch matters only with `--no-orbital`. |
| `--orbital` | orbital branching and fixing with the root's symmetries. On by default; `--no-orbital` turns it off. |
| `--propagate N` | passes of bound propagation at each node before its relaxation is solved. Default 0, and 4 on a model with a quadratic objective (`MIP_QUAD_PROPAGATE`). `jaos options` prints the unset value as -1. |
| `--propagate-depth D` | the deepest node at which propagation runs, the root being 0. Negative, the default, is every node. |
| `--restart` | starts the tree again from the root when the root's reduced costs fix at least `MIP_RESTART_FRAC` of the integer columns. Off by default and on in network mode (`bench/measurements/02-345/`); `--no-restart` turns it off. |
| `--branching RULE` | `pseudocost`, the default, scores each column by the gain its branches have shown so far; `most-fractional` takes the column farthest from an integer. |
| `--node-select RULE` | which open node the tree takes next: `estimate`, the default, the lowest pseudocost estimate with the lowest bound every fifth pick, or `bound`, the lowest bound (`bench/measurements/02-286/`). An unknown rule is a usage error. |
| `--reliability N` | strong branching on up to eight candidates per node until a column has `N` branches each way. Default 0, never. Only matters under pseudocost branching. |
| `--probe-cap M` | stops each strong-branching child solve at `M` times the work of the node's own relaxation. Default 0, no cap; a negative value is a usage error. Only matters with `--reliability`. |
| `--probe-depth D` | runs strong branching only at nodes down to depth `D`. By default every depth. A negative depth is a usage error. Only matters with `--reliability`. |
| `--tree-batch N` | how many open nodes the tree takes per round (`N >= 1`, default 1; more than 64 reads as 64). A round runs on up to `--threads` threads and its answers are taken in order, so the answer does not depend on the thread count. Above 1 the search itself changes (`bench/measurements/02-264/`, `bench/measurements/02-290/`). |
| `--pool-size K` | keeps the `K` best distinct integer points, best first. `K` must be a positive integer. Default 1, the incumbent alone. Two points are distinct when they differ on an integer column, or anywhere when the model has no integer column. Prints `pool_points N`, how many the pool holds, whenever `--pool-size` is on the command line. A size set by `--opt` or `--params` prints no line. |

### Which method solves what

`jaos_solve` in `docs/api.md` gives the same rules for the library.

- `dual`, the default, and `primal` are the two simplex methods, and they
  give the same answer. A MIP's relaxations run on the primal under
  `primal` and on the dual under every other setting.
- `barrier` is Mehrotra's predictor-corrector with a sparse Cholesky
  factor (`src/chol.c`), then a crossover to a basis that the primal or the
  dual simplex finishes. It certifies neither infeasibility nor
  unboundedness: a run that diverges or reaches `BARRIER_MAX_ITER` hands
  the model to the dual simplex. `bench/results/barrier.txt` holds its
  reading.
- `pdlp` is primal-dual hybrid gradient (after Applegate et al., 2021) to a
  relative tolerance of `PDLP_TOL`, then the same crossover and hand-off.
  `bench/results/pdlp.txt` holds its reading.
- `concurrent` runs the dual, the primal and the barrier on three copies,
  in that order, under work budgets that start at `CONCURRENT_SLICE` and
  grow by `CONCURRENT_GROWTH`. The first answer wins. The work is the sum
  over the three, and no clock decides the winner.
- A MIP under `barrier`, `pdlp` or `concurrent` runs its relaxations on the
  dual.
- A quadratic objective (`QUADOBJ` in MPS, a `[ ... ] / 2` block in LP) is
  solved by the barrier under `dual` or `barrier`, without presolve or
  crossover. `--log summary` says which linear system it took. When the
  barrier fails, or the checker refuses its point, the conic interior point
  solves the model again, and the limits cover both. When the checker
  refuses both, the solve ends `numerical_error`.
- A MIP with a quadratic objective solves its root and every node by the
  barrier, without Gomory cuts. While no incumbent is known, its root
  rounds the integer columns and solves with them fixed, then dives up to
  `--dive-heuristic N` solves. `--no-heuristics` or `--dive-heuristic 0`
  turns both off.
- Second-order cones or quadratic rows are solved by the conic interior
  point (`src/conic.c`) under `dual` or `barrier`. Outside a tree node, an
  optimum the checker refuses is repaired and checked again, and the solve
  ends `numerical_error` when it still fails. `unbounded` comes only with a
  direction the ray checker takes. `infeasible` comes with a certificate
  the checker takes, except on a model with a quadratic row. A cone whose
  head is fixed at 0, or one that can never bind, is left out of the walk.
  When every cone is left out and no row is quadratic, the model is solved
  as if it had no cones.
- With integer columns as well, the conic branch and bound
  (`src/conictree.c`) solves the model, with the conic interior point at
  every node. It reads `mip_gap`, `--branching`, `--node-limit`,
  `--tree-batch`, `--cutoff`, `--mip-start`, `--no-heuristics`,
  `--dive-heuristic` and the work and time limits, and no other
  branch-and-bound option. SOS sets, semi-continuous columns and indicator
  rows beside cones exit 5.
- `primal`, `pdlp` and `concurrent` refuse a quadratic objective, cones and
  quadratic rows, and the tool exits 5.

## `options`

`jaos options` prints every option with its value, one `name value` per
line: the defaults, unless `--opt NAME=VALUE` or `--params FILE` on the
same command line changed one. The output is what `--params` reads, so
`jaos options --opt ... > run.txt` saves a run's settings and
`jaos solve model.mps --params run.txt` replays them. `mip_propagate`
prints -1 while it is unset, which leaves the choice to the tree, and a
replayed -1 keeps that choice. Exit 0 unless an option is unknown or its
value is of the wrong kind, which is a usage error.

## `stats`

`jaos stats FILE` reads the model and prints what it is, one `key value`
per line. It solves nothing, so the exit code is 0 unless the file cannot
be read.

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
semicontinuous_columns 0
sos_sets 0
indicator_rows 0
quadratic_columns 0
quadratic_rows 0
cones 0
objective_nonzeros 12
min_abs 0.109
max_abs 2.386
objective_min_abs 0.32
objective_max_abs 10
```

The four row counts partition the rows and the four column counts
partition the columns. A **ranged** row or column has two finite bounds
that differ, a **fixed** one has two that are equal, and a **free** one has
neither. A **binary** column is an integer column whose bounds, rounded
inward to integers, are exactly 0 and 1. The magnitude pairs range over
the nonzeros and over the nonzero costs. `quadratic_rows` counts the rows
with a quadratic part and `cones` the second-order cones.

## `convert`

`convert` reads `IN` and writes `OUT`. `OUT`'s extension chooses the
format: `.mps` writes free-format MPS, `.lp` CPLEX-style LP, `.nl` AMPL's
text `.nl` with the names in `.col` and `.row` beside it, `.qplib` the
QPLIB text format, `.osil` OSiL XML, and `.cbf` the Conic Benchmark Format
without names. Any other extension is a usage error. The output name is
checked before the input is read.

The `.nl` writer lists the integer columns last, as the format does, and
refuses SOS sets, semi-continuous columns, indicator rows, cones, quadratic
rows and a quadratic objective. Write MPS, LP, QPLIB or OSiL for a
quadratic objective, and MPS for cones or quadratic rows.

What JAOS writes, JAOS reads back as the same model, names included. A row
or column the input did not name is written by its position, `R<I+1>`,
`C<J+1>`, `COST`. A name the LP dialect cannot spell (one holding a `-`,
starting with a digit, or spelling a keyword) is written to LP as
`c<j+1>`, `r<i+1>` or `obj`, and a `\ column c2 was x-1` comment at the top
of the file keeps the original. MPS takes every name.

`--positional` takes every name off the model before writing, so the file
comes out with `R1`, `C1` and `COST`. Only the names are lost
(`bench/measurements/02-274/`).

Every path this tool writes to is compressed when it ends in `.gz`, except
the proof file of `solve --proof` and `verify --proof`, which is always
plain text. Every path the tool reads may be compressed.
`docs/format-support.md`, "Compressed output", has the rule.

A write the format cannot express is refused. The tool prints the
library's message, which names the row or column, exits 5, and leaves no
file behind. `docs/format-support.md` lists what each format cannot
express.

## `check`

`check FILE SOLUTION` reads the model from `FILE` and `SOLUTION`, a file
that `solve --solution` wrote, and judges what the file holds with the
library's independent checkers. The checkers work on the model as loaded,
in its original units, and share no code with the solver. The first line
says which kind of answer the file claims: `status optimal`,
`status infeasible` or `status unbounded`.

For an optimum the checker judges the column values and row duals. The
output is its report, with the field names of `jaos_check_report` in
`include/jaos.h`:

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

`docs/api.md`, under `jaos_check_solution`, says what each number means,
and `docs/tolerances.md`, "The checker's tolerance", how `tol` is applied.
The two fields that decide are `primal_feasible` and
`dual_feasible`. The exit code is 0 when both are `yes` and 1 otherwise.

On a model with cones the file must carry the `cone` records that
`solve --solution` writes, and the check reads them as the cones' duals.
The report then ends with `max_cone_violation`. A file without the records
is refused with exit 5. A quadratic row needs nothing extra: the checker
takes the row's gradient `a + Qx` at the point.

On a model with integer columns, SOS sets, semi-continuous columns or
indicator rows the dual half does not run and `checked_duals` reads `no`,
because LP duality does not close a MIP's gap. The primal half decides,
and it counts the integrality violation and the SOS nonzeros.

For a certificate the file carries one `ray` record per row when the model
was proved infeasible, or per column when it was proved unbounded. The
report is `jaos_certificate_report`'s fields for an infeasible file and
`jaos_ray_report`'s for an unbounded one, then `certified`:

```
status infeasible
sup_columns 4
inf_rows 7
gap 3
certified yes
```

For an unbounded quadratic model the ray must also be flat: `curvature` is
`d'Qd` along the ray, and `certified` needs it near zero. On a linear model
it reads 0. The exit code is 0 when `certified` is `yes` and 1 otherwise.

`--tol T` is the checker's tolerance. It defaults to 1e-7, the solver's own
feasibility tolerance.

The solution file must be for this model. The library refuses a file whose
row or column count differs from the model's, or whose records carry other
names, and the tool exits 5 with the library's message. The checker reads
the values and the row duals and recomputes everything else from the
model.

### `check FILE --point POINT [--duals DUALS]`

A point file holds another solver's answer: one `NAME VALUE` line per
column, in any order, `#` to end of line for a comment. `check --point`
runs the same checker on it and prints the same report, with
`status point` as its first line.

```
$ jaos check model.mps --point other-solver.txt
status point
max_col_violation 0
...
primal_feasible yes
dual_feasible no
checked_duals no
gap_certified no
```

`--duals DUALS` brings the row multipliers, in a file of the same shape
over the row names. Without it the dual half does not run and the exit code
comes from the primal half alone. With it the verdict is
`primal_feasible && dual_feasible`, as for a solution file.

Every column must appear exactly once. A column the file does not name is
refused with its name and exit 5.

### `check FILE --proof PROOF`

This form judges an exact proof file, the one `solve --proof` or
`verify --proof` wrote. Every comparison is over the rationals, so this
path has no tolerance. The file carries no basis. For an optimum the
checker tests the three conditions that together prove a point optimal:
the point is inside every bound and row, every reduced cost points into the
model from the side its column rests on, and anything strictly inside its
bounds carries a zero multiplier. The checker refuses a model with integer
columns, SOS sets, semi-continuous columns or a quadratic objective, and
exits 5.

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

`claims` says which answer the file asserts. The `primal`, `dual` and
`objective` lines are printed for an optimum only. For a certificate the
verdict is the `proof` line alone, with `at_row` or `at_col` giving the
0-based index where it failed. `terms` is how many exact products the check
formed.

The exit code is 0 when the proof holds and 1 when it does not. A file that
is not a proof for this model exits 5. A number that outgrows the exact
arithmetic's limb budget exits 4, which means "cannot judge".

A certificate is judged more strictly here than by `check FILE SOLUTION`,
which ignores a term below the rounding level of its sum. A multiplier one
rounding away from zero, on a column with no finite bound on the side it
points at, passes there and fails here.

## `diff`

`diff A B` reads both files and says whether they describe the same model.
It prints one line per difference, then a `differences` count. Exit 0 when
the two are the same model, 1 when they are not.

```
$ jaos convert model.mps model.lp
$ jaos diff model.mps model.lp
differences 0
$ jaos diff model.mps other.mps
nonzeros 5 6
differences 1
```

Values are compared exactly, and names as the model gives them, so a row
nobody named is `R<i+1>` on both sides. A size that differs stops the
comparison. The lines are:

- `rows`, `columns` and `nonzeros` with the two sizes; `sense` and `offset`
  with the two values.
- `col_name` and `row_name` with the index and the two names.
- `cost`, `integer` and `col_entries` with the column's name and the two
  values.
- `col_bounds` and `row_bounds` with the name and four bounds: `A`'s lower
  and upper, then `B`'s.
- `entry` with the column's name, the row's name and the two coefficients.
- `semicontinuous`; `quadratic`, `quadratic_pair` and `quadratic_nz` for
  `Q`; `sos_sets`, `sos` and `sos_member`; `indicator`; `cones`, `cone` and
  `cone_member`; `row_quadratic_nz` and `row_quadratic` for the rows'
  quadratic parts.

## `show`

`show FILE --row NAME` prints one row:

```
$ jaos show model.mps --row DEMAND
row DEMAND
index 0
lower 10
upper inf
entries 3
term X1 1
term X2 1
term X3 1
```

`--col NAME` prints a column the same way, with its `cost` and `integer`
lines and its terms named by row. Exactly one of the two is required. A row
with a quadratic part prints a `quadratic_entries` count after the terms,
then one `qterm COLUMN COLUMN VALUE` line per entry of the lower triangle.
The row is `a'x + ½ x'Qx`, so `qterm x x 2` is `x²`.

It solves nothing, and a positional name works where the model named
nothing. Exit 0, or 5 when no row or column carries the name.

## `iis`

`iis FILE` solves the model. When the answer is infeasible, it finds one
irreducible infeasible subsystem: a set of bound sides that has no feasible
point, and that becomes feasible when any one of them is dropped. The
output is the status line, one line per bound side, then the counts:

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

A row with both sides in the subsystem appears twice. Rows come first, then
columns, each in index order. `candidates` is how many sides the deletion
filter started from, `solves` how many re-solves it ran on a private copy,
and `work_units` what they cost. `from_certificate` says whether the
candidates came from the infeasibility certificate. The subsystem found is
the same on every machine and every run.

The subsystem covers row and column bounds only. Integer and
semi-continuous columns, SOS sets, indicator rows and a quadratic objective
are dropped before the search, so the answer explains the linear
relaxation. When that relaxation is feasible, the tool says so on stderr
and exits 5.

Exit 0 when a subsystem was printed. When the model is optimal or unbounded
the tool prints its status line, says on stderr that there is nothing to
find, and exits 1. Exit 5 on an error, including a re-solve that could not
decide its side. `--work-limit N` stops the solve after N work units; a
first solve that stops early ends the command with exit 5.

### `iis FILE --write OUT`

`--write OUT` writes the subsystem as a model, in the format `OUT`'s
extension names (the six `convert` writes, with `.gz` to compress).

```
$ jaos iis model.mps --write sub.mps
status infeasible
row LIM2 upper
row EQ1 lower
col X1 lower
col X2 lower
members 4
...
subsystem_rows 2
subsystem_columns 3
subsystem_file sub.mps
$ jaos solve sub.mps
status infeasible
```

The member sides keep their values. Every other side becomes infinite,
every row with no member side and every column with no entries and no
member bound is dropped, and every cost is zeroed. Solving the file reads
`infeasible` (`bench/measurements/02-218/`). Names survive and indices do
not. A model that is not infeasible writes no file. `--positional` takes
every name off the subsystem before writing, as `convert --positional`
does.

## `relax`

`relax FILE` finds the smallest total move of bounds that makes the model
feasible. It prints one line per bound that has to move, signed and named,
then the totals:

```
row LIM2 upper 2.5
total 2.5
rows_moved 1
cols_moved 0
largest 2.5
work_units 8890
```

A negative move lowers the bound's lower side by that much, and a positive
move raises its upper side. Add every move to the bound it names and the
model has a feasible point, and no other set of moves has a smaller total.
"Smallest" means the smallest sum of the sizes. A feasible model prints no
move line and a total of 0.

| option | what it does |
|---|---|
| `--rows` | only row bounds may move |
| `--cols` | only column bounds may move |
| `--apply OUT` | write the model with every move applied, in the format `OUT`'s extension names (the six `convert` writes); the extension is checked before the input is read |
| `--positional` | take every name off before writing `OUT` |
| `--work-limit N` | stop the elastic copy after N deterministic work units |

Without `--rows` or `--cols`, both may move, at the same price per unit.

The model itself is never solved. An elastic copy is, and `work_units` is
what it cost. The copy carries the integer marks, the SOS sets, the
semi-continuous columns and the indicator rows. A semi-continuous column's
lower bound never moves.

A freed integer column is held in a box: its own bounds widened by `M` on
each side. `M` starts at twice the answer of the copy without integer
marks, and doubles until the answer fits inside the box. `work_units` is
the sum over the rounds. `tests/data/relax_rounds.mps` needs four rounds.
The search stops after `RELAX_BOX_ROUNDS` widenings, or when a round passes
`RELAX_ROUND_WORK` times the first round's work
(`bench/measurements/02-297/`). It then tries to prove from the equality
rows, or from one ranged row over integer columns, that no box holds an
integer point. When it can, the message adds "none in any box" and names
the reason (`bench/measurements/02-362/`, `02-369/`). The
tool says on stderr how wide the last box was and exits 5.

Exit 0 with an answer. Exit 5 when the copy did not finish, or when the
model has no relaxation at all: a row or column whose lower bound is above
its upper bound cannot be opened by moving its two ends.

## `verify`

`verify FILE` solves the model and runs whatever exact arithmetic the
answer allows. When the answer is optimal, it proves or refuses to prove,
in exact arithmetic with no tolerance, that the published basis certifies
it. The verdict is `optimal` when every basic value lies inside its bounds
and every reduced cost points into the model.

A MIP (integer columns, SOS sets or semi-continuous columns, as
`jaos_model_has_integer` says) is refused before the solve with exit 5,
because the basis behind its answer is the last node's. A quadratic
objective, cones and quadratic rows are refused after the solve: the tool
prints the status line and exits 5. `--basis` refuses all of them with no
solve.

`--work-limit N` stops the solve after N work units. There is then no
answer to prove, so the tool prints its status line and exits 5. With
`--basis` there is no solve, so the option does nothing.

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

- `proof` is `optimal`, `broken` or `refused`. `refused` means a number
  outgrew the arithmetic's limb budget. `bound_bits` is a worst-case
  estimate of the size the proof needs, reported only, and `capacity_bits`
  is the size the arithmetic holds.
- `stage` says which check a `broken` verdict came from: `rank`, `primal`
  or `dual`. It is `none` otherwise.
- On `broken`, `at_row` or `at_col` gives the 0-based index of the row or
  column that breaks the proof, when one does, and `violation` says how far
  out it is.
- `blocks`, `largest_block`, `bytes_held` and `terms` describe the work.
  The work is reported and not billed to the work counter. A block of `k`
  rows costs about `k` cubed products of large integers
  (`bench/measurements/02-275/`).

When the answer is infeasible, `verify` derives the Farkas multipliers
exactly from the basis the solve stopped on:

```
status infeasible
certificate exact
bound_bits 76
capacity_bits 4096
blocks 17
largest_block 2
at_row 13
bytes_held 3168
terms 46
```

- `certificate` is `exact` or `refused`. `refused` means a number outgrew
  the limb budget.
- `at_row` is the 0-based index of the row whose own logical the ray
  leaves the basis on. It is absent when a structural column holds that
  position.
- `--values` prints the derived multipliers as `multiplier NAME V`.
- `--proof PATH` writes them. When the derivation was refused, it writes
  the published doubles if they pass the exact check, and otherwise writes
  nothing and exits 5. No `proof_file` line is printed here.

When the answer is unbounded, the direction is derived the same way. It
prints `ray exact` or `ray refused` and the same cost lines. `--values`
prints `direction NAME V` per column, and `--proof` writes the direction,
with the same fallback.

`jaos check FILE --proof PATH` says whether derived multipliers or a
derived direction certify. It shares only the exact arithmetic with the
derivation.

After a `proof optimal`, `--values` prints one `x NAME VALUE` line per
column, one `y NAME DUAL` line per row, then `objective_exact VALUE`. Every
value is an exact rational, such as `4`, `1/3` or `-7/2`. The
`objective_exact` line is absent when the objective outgrew the arithmetic
while the values fitted. Nothing is printed for a `broken` or `refused`
proof.

```
x X1 4
x X2 3
x X3 3
y DEMAND 4
y CAP1 -2
y CAP2 -1
objective_exact 29
```

`--proof PATH` writes the proof to a file only when the verdict is
`optimal`, and then prints `proof_file PATH`. On `broken` or `refused` it
says so on stderr. `docs/format-support.md` describes the file.

The exit code is the verdict: 0 proved, 1 broken, 3 refused. For an
infeasible or unbounded model it is 0 when the certificate or ray is
derived and 4 when it is refused. A solve that stopped on a limit has
nothing to prove: the tool prints its status line, says so on stderr, and
exits 5.

### `verify FILE --basis BAS`

`--basis BAS` proves the basis in an MPS basis file instead, and the model
is never solved. `BAS` may come from another solver, and the answer says,
over the rationals, whether that basis is an optimal basis of `FILE`.

```
$ jaos verify model.mps --basis other-solver.bas
proof optimal
stage none
bound_bits 1169
capacity_bits 4096
blocks 24
largest_block 4
bytes_held 10560
terms 221
```

There is no `status` line, because nothing was solved. The verdicts and the
exit codes are the same as above, and `--values` and `--proof` work the
same way. A file whose names or basic count do not fit the model is refused
with the library's message and exit 5.

## `ranging`

`ranging FILE` solves the model. When the answer is optimal, it prints how
far every cost, every row bound and every column bound may move, everything
else held, before the basis behind the optimum stops being optimal. Every
interval contains the number's current value. An end with no limit reads
`inf` or `-inf`.

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

Rows and columns come in index order. The intervals are about the basis.
Another optimal basis may carry the same optimum further, and that union is
not computed. A degenerate basis can report an interval of zero width.

Exit 0 when the intervals were printed. A solve that is not optimal has no
basis to range: the tool prints its status line, says so on stderr, and
exits 5. `--work-limit N` stops the solve after N work units, with the same
result. A quadratic objective is refused the same way, because a QP
optimum carries no basis. A MIP is refused before the solve, from
`jaos_model_has_integer`, because the basis behind its answer is the last
node's and carries that node's branching bounds.

## `STUB -AMPL`

`jaos STUB -AMPL [NAME=VALUE]...` is how AMPL, Pyomo's `asl:` interface
and JuMP's AmplNLWriter call a solver. The tool reads `STUB.nl` (a `STUB`
given with its `.nl` is the same stub), solves it and writes `STUB.sol`. It
prints nothing.

The options come first from the environment variable `jaos_options`, then
from the words after `-AMPL`, so the command line wins. Each is a name and
a value, joined by `=` or white space. The names are those `jaos options`
prints, in any case: `work_limit=1e9`, `MIP_GAP 1e-4`. JuMP passes options
on the command line and Pyomo through `jaos_options`
(`bench/measurements/02-257/`).

`STUB.sol` holds a message, a blank line, `Options` and the option values
of `STUB.nl`'s first line, the counts of rows, of row duals, of columns and
of column values, the duals, the values, and a last line `objno 0 CODE`.
The message is `JAOS 0.5.0: optimal; objective -7` or the like, or the
reason the solve did not run. The duals are there for a continuous optimum,
in the signs `solve --solution` prints. The values are there for an
optimum, or for a limit or a callback stop that left a point. `CODE` is
AMPL's:

| CODE | |
|---|---|
| 0 | optimal |
| 200 | infeasible |
| 300 | unbounded |
| 400 | stopped by a limit or a callback, with a point |
| 401 | stopped the same way, with none |
| 500 | failed: a numerical error, a file the `.nl` reader refuses, an unknown option |

The exit code is 0 whenever `STUB.sol` was written, because AMPL reads the
file only after a 0, and 5 when it could not be written. The `.nl` reader
takes bodies of degree two or less. A body of any other kind ends with
code 500 and the reader's message.

## Which reader is used

The input file's name chooses the reader. A name ending in `.lp`, `.nl`,
`.qplib`, `.osil` or `.cbf`, with or without `.gz` after it, goes to that
format's reader. Every other name goes to the MPS reader, because MPS files
come as `.mps`, `.MPS`, `.sif` or with no extension. The comparison is
case-sensitive.

Every reader looks at the first two bytes of the file and inflates a gzip
file itself, so `model.mps.gz` and `model.mps` read the same way. Writing
goes by the name: a path ending in `.gz` is compressed and one that does
not is not. `docs/format-support.md`, "Compressed input", has the rule.

A file that cannot be read is reported on stderr with the library's
message, which names the offending line, and the tool exits 5.

## `help`

`jaos --help` prints the whole usage text, and `jaos help COMMAND` prints
one command's synopsis lines, its own description and the footer.
`jaos help` and `jaos -h` are the same command as `jaos --help`, and
`jaos version` is the same as `jaos --version`. `jaos --commit` prints the
git commit the library was built from, 12 hex digits, or an empty line when
the build ran outside a git checkout.

```
$ jaos help convert
Usage:
  jaos convert IN OUT [--positional]

convert reads IN and writes OUT in the format OUT's extension names,
  .mps, .lp, .nl (the names beside it in .col and .row), .qplib, ...
```

A word that is not a command is a usage error with exit 5, and so is more
than one.

## Exit codes

The exit code is the verdict, so a script can branch on it without parsing
stdout. What each code means depends on the command.

| code | `solve` | `check` | `check --proof` | `iis` | `verify` | `ranging` | `relax` |
|---|---|---|---|---|---|---|---|
| 0 | optimal | primal and dual feasible | the proof holds | a subsystem was printed | proved, or the exact certificate or ray derived | intervals printed | the relaxation was printed |
| 1 | infeasible | not feasible | the proof is broken | the model is not infeasible | the basis does not certify the answer | | |
| 2 | unbounded | | | | | | |
| 3 | stopped by a limit or Ctrl-C | | | | refused: the numbers do not fit | | |
| 4 | numerical error | | cannot judge: the numbers do not fit | | the exact certificate or ray refused | | |
| 5 | usage or I/O error | usage or I/O error | not a proof of this model | usage or I/O error | usage or I/O error | usage or I/O error | no relaxation, or an error |

`check --point` exits as `check` does. `diff` exits 0 when the two files
are the same model, 1 when they differ and 5 on an error. `convert`,
`stats`, `show` and `options` exit 0 when they did their work and 5
otherwise. `STUB -AMPL` exits 0 whenever it wrote `STUB.sol`, whose last
line carries the verdict, and 5 otherwise.

Every command exits 5 on a usage error, an unreadable input, an unwritable
output, a refused write, or a refused solution file. `iis`, `verify` and
`ranging` also exit 5 when their solve ends `work_limit`, `interrupted` or
`numerical_error`, because such a solve decides nothing about the model.
`ranging` also exits 5 when its solve is infeasible or unbounded.

These three commands print the status line and nothing else before the
report, and none of them prints a time, so two runs of the same file give
byte-identical output.
