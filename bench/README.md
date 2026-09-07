# bench — the acceptance gate

The gate is three Netlib sets. A fourth set is for the MIP tree, and three
PLATO sets exist for presolve measurements.

| target | set | instances | time at `J=12` |
|---|---|---|---|
| `make netlib` | Netlib standard | 94 | ~85 s |
| `make netlib-kennington` | Kennington | 16 | ~8 min |
| `make netlib-infeas` | Netlib infeasible | 29 | ~10 s |
| `make miplib` | MIPLIB 3 | 24 | ~4 min |
| `make plato-pds`, `plato-fome`, `plato-nug` | PLATO | 15 | not a gate |

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

## Baselines and results

`bench/<set>.baseline` is the committed record: per instance, the digest, the
work units, the iteration count. `make <set>` writes `bench/results/<set>.txt`
and prints the per-instance diff against the baseline. Seconds appear in the
run's output and never in a result file or a baseline.

Rewrite a baseline only with `make <set>-baseline`, after reading the diff.

## The other runners

- `make warm` and `make warm-kennington`: warm re-solve after one branching
  step per instance, against a cold solve. A ratio, not a verdict.
- `make primal`: the primal simplex on the standard set, three-way split of
  agreement with the dual.
- `bench/compare/`: JAOS against HiGHS, SoPlex and Clp, in seconds.
- `bench/measurements/<id>/`: raw readings behind each refusal in
  `refusals.txt`. `make refusals` re-runs the ones that have a script.
