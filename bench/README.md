# bench — the acceptance gate

The gate is three Netlib sets. A fourth set is for the MIP tree, and three
PLATO sets exist for presolve measurements. MIPLIB 2017 is a MIP reading
with a work limit, not a gate.

| target | set | instances | time at `J=12` |
|---|---|---|---|
| `make netlib` | Netlib standard | 94 | ~85 s |
| `make netlib-kennington` | Kennington | 16 | ~8 min |
| `make netlib-infeas` | Netlib infeasible | 29 | ~10 s |
| `make miplib` | MIPLIB 3 | 24 | ~4 min |
| `make miplib2017` | MIPLIB 2017, the 30 smallest benchmark instances with an optimum | 30 | not a gate; ~6 min at `J=6` |
| `make plato-pds`, `plato-fome`, `plato-nug` | PLATO | 15 | not a gate |
| `make maros-meszaros` | Maros-Meszaros convex QP | 138 | not a gate; the QP reading |
| `make cblib` | CBLIB 2014, the continuous part | 29 | not a gate; the conic reading |

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
- Two cold solves in the same process give the same digest and the same work.
- Work units against the committed baseline: an instance past 2.0x is a
  regression, and every changed instance is listed in the result.

For the MIP set the point must be integral, feasible to the checker, at the
catalogue optimum, and the node count is part of the baseline.

For CBLIB the files stay gzipped CBF (`-x cbf.gz`), the checker is
`jaos_check_conic_solution` with every cone's dual, and the reference is
the library's own solution, except on nql and qssp, where the library's
solution is not the optimum: those eight carry the DIMACS value or the
value `bench/measurements/02-254/` certified (`bench/cblib.manifest` says
which and why).

## Baselines and results

`bench/<set>.baseline` is the committed record: per instance, the digest, the
work units, the iteration count. `make <set>` writes `bench/results/<set>.txt`
and prints the per-instance diff against the baseline. Seconds appear in the
run's output and never in a result file or a baseline.

Rewrite a baseline only with `make <set>-baseline`, after reading the diff.

## The other runners

Each writes `bench/results/<target>.txt`. None has a baseline or a verdict;
each reads one method against the dual simplex on the same instances.

- `make warm` and `make warm-kennington`: warm re-solve after one branching
  step per instance, against a cold solve. A ratio, not a verdict.
- `make primal` and `make primal-kennington`: the primal simplex, three-way
  split of agreement with the dual. No Kennington reading is committed.
- `make barrier` and `make barrier-infeas`: the barrier against the dual, in
  work units, with whether the checker accepts the point it publishes, on
  the standard set and on the infeasible one.
- `make pdlp` and `make pdlp-infeas`: the first-order method, the same way.
- `make concurrent`: the concurrent solve against the dual.
- `make plato-pds`, `make plato-fome`, `make plato-nug`: the PLATO sets,
  for presolve measurements (the table above).
- `make miplib2017`: the 30 smallest instances of the MIPLIB 2017
  benchmark set with a proven optimum, each solve stopped at a work limit
  (`MIPLIB2017_WORK`, 1e10 by default, the runner's `-L`). A line that
  stops at the limit carries the incumbent, the bound and the reference.
  A reading, not a gate: there is no baseline.
- `bench/compare/`: JAOS against HiGHS, SoPlex and Clp, in seconds.
- `bench/measurements/<id>/`: raw readings behind each refusal in
  `refusals.txt`. `make refusals` re-runs the ones that have a script.
