# JAOS — Just Another Optimization Solver

A mathematical-programming solver written from scratch in C23. No external
dependencies, Apache 2.0, Linux and GCC only.

**Version 0.1.0-dev.** It reads an LP from disk, solves it with a revised
dual simplex, and proves the answer right. It does that correctly on all 139
Netlib reference instances. It has never been timed against another solver.

## What it does today

```c
#include "jaos.h"

jaos_model *m;
jaos_model_new(&m);
jaos_read_mps(m, "model.mps");
jaos_solve(m);

if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
    double obj;
    jaos_objective(m, &obj);
    jaos_solution(m, x, row_activity, row_dual, col_dual);
}
jaos_model_free(m);
```

Fixed and free MPS, a subset of the LP format, Curtis-Reid scaling, a sparse
LU with Forrest-Tomlin updates, a dual simplex with steepest-edge pricing and
a Harris ratio test, and an independent checker that verifies every answer
against the original unscaled problem.

`SPECS.md` is the honest inventory: what exists, what does not, and what is
only partly there.

## What it does not do

There is no way to set a tolerance, choose an algorithm, modify a model after
loading it, re-solve warm, write a file, or call it from anything but C.
No presolve, no primal simplex, no barrier, no MILP. `SPECS.md` §4 is the
shortest summary: twenty-three public functions and not one configures
anything.

## Build and test

GCC 14 minimum. Linux only; on Windows use WSL.

```
make            # release static library, build/release/libjaos.a
make test       # unit suite
make sanitize   # unit suite under ASan + UBSan
make netlib     # the 94-instance acceptance gate (fetches instances first)
```

Also `make netlib-kennington` and `make netlib-infeas` for the other two
reference sets. Instances are fetched by `bench/fetch.sh` and pinned by
sha256; they never enter this repository.

## Layout

```
include/jaos.h   the public header, the only one
src/             library sources
tests/           unit suite; tests/vendor/unity/ is the one vendored dependency
bench/           instance manifests, acceptance runner, results
bench/compare/   the harness that times JAOS against other solvers
docs/            formats, tolerances, scaling, work units
```

## The documents, and which to read

- **`SPECS.md`** — what JAOS is built to be, and where every feature stands.
- **`PLAN.md`** — what is open, in the order it will be done.
- **`CHANGELOG.md`** — what landed and what it cost.
- **`DECISIONS.md`** — why. Every closed decision with the measurement that
  closed it.
- **`bench/README.md`** — the acceptance gate.
- **`bench/compare/README.md`** — how JAOS is compared against other solvers.

The design is written down. Do not reconstruct it from the code.

## Licence

Apache 2.0. See `LICENSE`.
