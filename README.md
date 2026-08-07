# JAOS — Just Another Optimization Solver

A mathematical programming solver written from scratch in C23, with no
dependencies beyond the C standard library.

**Status: early. Linear programs are solved end to end; everything above them
is scheduled, not built.** The version number says `0.1.0-dev` and means it.

---

## What works today

```c
#include <jaos.h>

jaos_model *m;
jaos_model_new(&m);
jaos_read_mps(m, "problem.mps");
jaos_solve(m);

if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
    double obj;
    jaos_objective(m, &obj);

    double *x = malloc(jaos_num_col(m) * sizeof *x);
    jaos_solution(m, x, NULL, NULL, NULL);
    /* ... */
}
jaos_model_free(m);
```

- **Readers** for MPS (fixed and free layout) and CPLEX-style LP format,
  including ranged rows, all continuous bound types, objective constants and
  Fortran `D` exponents. Every rejection carries a line number and a reason.
- **A dual simplex** with bounds and dual steepest-edge pricing, solving
  minimisation and maximisation, equalities, ranged rows and free variables —
  reporting optimal, infeasible and unbounded outcomes.
- **Sparse LU factorization** with Markowitz threshold pivoting and
  Forrest-Tomlin updates, so a basis change costs work proportional to the
  change rather than a refactorization.
- **Curtis-Reid scaling**, with geometric-mean equilibration as an option.
- **An independent solution checker** that verifies a claimed solution against
  the original problem without consulting any solver state.
- **Two budgets**: a reproducible work limit counted in deterministic work
  units, and a wall-clock time limit. They are separate because only one of
  them means the same thing on two machines. What a work unit is, and where
  it is charged, is in [`docs/work-units.md`](docs/work-units.md).

Every tolerance a solve compares against, and the formulas the checker
judges with, are in [`docs/tolerances.md`](docs/tolerances.md). They are
drafts until the Netlib gate closes.

## How well it works today

Not a claim — a run. `make netlib` fetches the 94 instances of the Netlib
standard set, checksum-verified against a committed manifest, and judges
each solve against the published optimum, the independent checker, and a
second solve that has to agree bit for bit. The last result is committed at
[`bench/results/netlib.txt`](bench/results/netlib.txt).

| | |
|---|---|
| loads with the right shape | 94 / 94 |
| solved to optimal | 93 / 94 |
| objective within tolerance | 91 / 94 |
| independent checker green | 86 / 94 |
| identical across two solves | 93 / 94 |

**The gate is not met**, and that is the honest summary of where JAOS is.
One instance does not terminate. Seven return an answer the checker
rejects — mostly on the dual conditions, two of them by margins far too
large to be rounding. What each failure is, and which open question it
belongs to, is in [`PLAN.md`](PLAN.md).

The readers are the part that came out clean: every instance in the set
loads with exactly the row and column counts two independent canonical
sources agree on — which is the one thing the checker structurally cannot
verify, since it reads the same stored matrix the solver does.

Those five numbers are a summary, and a summary is the wrong instrument for
noticing that a change broke something: while the gate is unmet it reports
`NOT MET` either way, and the counts can stay put while one instance starts
solving and another stops. So `make netlib` also diffs every instance
against [`bench/netlib.baseline`](bench/netlib.baseline), which records what
each one did last time, and fails on anything that got worse whatever the
totals say. See [`bench/README.md`](bench/README.md).

No **performance** claim appears here or anywhere else in this repository.
Speed needs a controlled machine before any number about it means anything,
and that host does not exist yet. Nothing above is a timing.

## Building

Linux, GNU make, GCC 14 or newer. On Windows, build under WSL.

```sh
make            # static library in build/release/libjaos.a
make test       # unit tests
make sanitize   # the same tests under AddressSanitizer + UndefinedBehaviorSanitizer
make netlib     # fetch the Netlib set, run the acceptance gate, diff against the baseline
make clean
```

`make netlib` needs a network connection the first time; the instances are
verified against a committed manifest and never enter the repository. See
[`bench/README.md`](bench/README.md).

Link against `build/release/libjaos.a` with `-lm`. The public interface is the
single header `include/jaos.h`.

## Design commitments

These are not preferences; they constrain every line and are recorded with
their reasoning in [`DECISIONS.md`](DECISIONS.md).

- **C23, GCC, Linux.** GCC 14 is the minimum: JAOS uses `constexpr`, checked
  integer arithmetic from `<stdckdint.h>`, and C23's removal of implicit
  function declarations — which turns a class of silent link-time failure into
  a compile error.
- **No external dependencies.** Nothing beyond libc and libm. Where a third
  party would normally be pulled in, JAOS implements it. This costs less than
  it looks: serious solvers already hand-write the parts that matter, because
  generic dense libraries do not fit the data structures.
- **Deterministic by default.** Same input, same result and same search path,
  on every machine. No algorithmic decision reads the clock — the clock
  reports, it never decides. This is why the two budgets are separate.
- **CPU only.** Not deferred, out of scope. The simplex is inherently
  sequential; the methods that suit GPUs want a different data layout, and
  mixing both degrades each.
- **Implemented from published literature, never from another solver's
  source.** Algorithms and papers are free; a specific implementation is
  copyrighted. This is stricter than the law requires, deliberately.
- **No claim without a run.** No statement about correctness or speed is made
  without executing the relevant reference set.

## Where this is going

Linear programming is the foundation, not the destination. The declared scope,
in build order: LP; mixed-integer LP; quadratic and mixed-integer quadratic;
quadratically constrained; second-order cone; nonlinear; mixed-integer
nonlinear, convex and global; and specialised network algorithms. Constraint
programming is explicitly out of scope — a different paradigm, not a later
phase.

The current milestone is LP correctness, whose gate is the Netlib test set
solved to published reference values. That gate is built and running, and
not yet met — the numbers above are where it stands. [`PLAN.md`](PLAN.md)
carries the staging, the failures still open, and the questions behind them.

## Repository

| | |
|---|---|
| [`DECISIONS.md`](DECISIONS.md) | Closed design decisions and the reasoning that closed them |
| [`PLAN.md`](PLAN.md) | Build order, current milestone in detail, open questions |
| [`CHANGELOG.md`](CHANGELOG.md) | What changed, for someone using this |
| [`docs/`](docs/) | Format dialects, scaling, tolerances, work units; `research/` for what was worked out but not committed to |
| [`bench/`](bench/) | The Netlib acceptance gate: pinned manifest, per-instance baseline, fetch script, runner, results |
| `include/jaos.h` | The public interface, and the only public header |

## Licence

Apache License 2.0. See [`LICENSE`](LICENSE).
