# bench — the acceptance gate

The gate is three Netlib sets and MIPLIB 3, the set for the MIP tree. The
other targets are readings. They are not part of the gate.

| target | set | instances | time |
|---|---|---|---|
| `make netlib` | Netlib standard | 94 | ~85 s at `J=12` |
| `make netlib-kennington` | Kennington | 16 | ~8 min at `J=12` |
| `make netlib-infeas` | Netlib infeasible | 29 | ~10 s at `J=12` |
| `make miplib` | MIPLIB 3 | 24 | ~4 min at `J=12` |
| `make miplib2017` | MIPLIB 2017, the 30 smallest benchmark instances with an optimum | 30 | not a gate; ~6 min at `J=6` |
| `make plato-pds`, `plato-fome`, `plato-nug` | PLATO | 8, 4 and 3 | not a gate |
| `make maros-meszaros` | Maros-Meszaros convex QP | 138 | not a gate; the QP reading |
| `make cblib` | CBLIB 2014, the continuous part | 29 | not a gate; the conic reading |

Run the gate this way:

```
make netlib netlib-infeas J=12
make netlib-kennington J=2
make miplib J=2              # when src/mip.c changed
```

Kennington and MIPLIB 3 run at `J=2`, each in its own job, because a
MIPLIB tree once grew past 8 GB at a higher `J`.

**Always pass `J=N`.** Without it the set runs one instance at a time.

`bench/fetch.sh` downloads the instances into `instances*/` and checks each
against the sha256 in its `.manifest`. The instances never enter the
repository. Netlib's compressed format is expanded with `emps`, fetched and
checksummed the same way.

## What each instance is judged on

- The status is what the reference says: optimal, or infeasible for the
  infeasible set.
- The objective matches the Koch reference within `1e-6 * max(|ref|, 1)`.
- The independent checker accepts the answer against the original model.
- The relative suboptimality bound the checker reports is at most `1e-6`.
- Two cold solves in the same process give the same digest and the same work.
- Against the committed baseline, an instance regresses when a verdict
  goes from yes to no, when its work passes 2.0x, or when its
  suboptimality bound rises past 2.0x (a bound at or under `1e-16` never
  counts). Every regression, every verdict change and every node-count
  change is listed in the result.

For the MIP set the point must be integral, feasible to the checker, at the
catalogue optimum, and the node count is part of the baseline.

For CBLIB the files stay gzipped CBF (`-x cbf.gz`), the checker is
`jaos_check_conic_solution` with every cone's dual, and the reference is
the library's own solution, except on nql and qssp, where the library's
solution is not the optimum: those eight carry the DIMACS value or the
value `bench/measurements/02-254/` certified (`bench/cblib.manifest` says
which and why).

## Baselines and results

`bench/<set>.baseline` is the committed record: per instance, the status,
the five verdicts, the iteration count, the work units and the
suboptimality bound, and for the MIP set the node count. `make <set>`
writes `bench/results/<set>.txt` and prints the per-instance diff against
the baseline. Seconds appear in the run's output and never in a result file
or a baseline.

Rewrite a baseline only with `make <set>-baseline`, after reading the diff.

## The readings

Each writes `bench/results/<target>.txt`. Four compare against a committed
baseline the way the gate does: `make maros-meszaros`, `make cblib`, `make
plato-pds` and `make plato-fome`. The others have no baseline. Re-take
`make primal barrier pdlp concurrent warm J=12` when the dual or the
primal simplex, presolve or the crossover changes.

- `make warm` and `make warm-kennington`: warm re-solve after one branching
  step per instance, against a cold solve. No baseline.
- `make primal` and `make primal-kennington`: the primal simplex, three-way
  split of agreement with the dual. No baseline. No Kennington reading is
  committed.
- `make barrier` and `make barrier-infeas`: the barrier against the dual, in
  work units, with whether the checker accepts the point it publishes, on
  the standard set and on the infeasible one. No baseline.
- `make pdlp` and `make pdlp-infeas`: the first-order method, the same way.
  No baseline.
- `make concurrent`: the concurrent solve against the dual. No baseline.
- `make maros-meszaros`: the QP reading, against
  `bench/maros-meszaros.baseline`.
- `make cblib`: the conic reading, against `bench/cblib.baseline`.
- `make miplib2017`: the 30 smallest instances of the MIPLIB 2017
  benchmark set with a proven optimum, each solve stopped at a work limit
  (`MIPLIB2017_WORK`, 1e10 by default, the runner's `-L`). A line that
  stops at the limit carries the incumbent, the bound and the reference.
  No baseline.
- `make plato-pds`, `make plato-fome` and `make plato-nug`: the PLATO
  sets, for presolve measurements. `make plato` runs the first two.
  plato-pds and plato-fome compare against their baselines. plato-nug has
  none and runs only when named. Since 2026-09-25 it stops each solve at
  `PLATO_NUG_WORK` work units (1e12, the runner's `-L`), so its file is
  always written: nug08-3rd solves in 3.9e11, and nug20 and nug30 stop at
  the limit after 132292 and 37376 iterations, in 28 minutes at `J=3` and
  2.3 GB. Without it they had run past 2 h 15 min and 4.9 GB, and the
  runner, which writes its file only when every instance ends, wrote
  nothing. The runner took `-L` and `-O` for MIP sets only until the same
  day; it now applies both to every set. The PLATO readings
  and baselines date from 2026-08 and have not been re-taken since. Their
  baseline headers name `make netlib-baseline`; the targets that rewrite
  them are `make plato-pds-baseline` and `make plato-fome-baseline`.
- `make compare-solvers`, `make compare COMPARE_ARGS='-t P0'` and
  `bench/compare/run-mip.sh`: JAOS timed against other solvers, in
  seconds, on LP and MIP only. A QP rung and a conic rung are a row in
  `TODO.md`. `bench/compare/README.md` describes the harness.
- `bench/measurements/<id>/`: raw readings behind each refusal in
  `refusals.txt`. `make refusals` re-runs the ones that have a script.
