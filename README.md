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
- **A dual simplex** with bounds, solving minimisation and maximisation,
  equalities, ranged rows and free variables — reporting optimal, infeasible
  and unbounded outcomes.
- **Sparse LU factorization** with Markowitz threshold pivoting and
  Forrest-Tomlin updates, so a basis change costs work proportional to the
  change rather than a refactorization.
- **Curtis-Reid scaling**, with geometric-mean equilibration as an option.
- **An independent solution checker** that verifies a claimed solution against
  the original problem without consulting any solver state.
- **Two budgets**: a reproducible work limit counted in deterministic work
  units, and a wall-clock time limit. They are separate because only one of
  them means the same thing on two machines.

No performance claims appear here, or anywhere else in this repository, until
they have been measured on a controlled machine. The benchmark campaign that
would produce them has not run yet.

## Building

Linux, GNU make, GCC 14 or newer. On Windows, build under WSL.

```sh
make            # static library in build/release/libjaos.a
make test       # unit tests
make sanitize   # the same tests under AddressSanitizer + UndefinedBehaviorSanitizer
make clean
```

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
solved to published reference values. [`PLAN.md`](PLAN.md) carries the staging
and the open questions.

## Repository

| | |
|---|---|
| [`DECISIONS.md`](DECISIONS.md) | Closed design decisions and the reasoning that closed them |
| [`PLAN.md`](PLAN.md) | Build order, current milestone in detail, open questions |
| [`CHANGELOG.md`](CHANGELOG.md) | What changed, for someone using this |
| [`docs/`](docs/) | Format dialects, scaling |
| `include/jaos.h` | The public interface, and the only public header |

## Licence

Apache License 2.0. See [`LICENSE`](LICENSE).
