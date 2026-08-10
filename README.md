# JAOS — Just Another Optimization Solver

A mathematical-programming solver written from scratch in C23. No external
dependencies, Apache 2.0, Linux and GCC only.

**Version 0.1.0-dev.** It reads an LP from disk, solves it with a revised
dual simplex, and proves the answer right. It does that correctly on all 139
Netlib reference instances.

Against the field, with presolve off and the dual simplex forced on both
sides, it is **3.8x slower than HiGHS and 1.4x slower than SoPlex** — on
**0.70x** SoPlex's iteration count, so the search is competitive and each
iteration costs about twice what it should. `make compare` reproduces that.

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

A loaded model can be changed one bound, cost or coefficient at a time, and
re-solving starts from the basis the previous solve reached rather than from
scratch.

`SPECS.md` is the honest inventory: what exists, what does not, and what is
only partly there.

## What it does not do

There is no way to add or delete a row or a column after loading, write a
file, or call it from anything but C. No presolve, no primal simplex, no
barrier, no MILP. Thirty-three public functions, and the six that configure
anything set the contract — two tolerances, two budgets, and where the output
goes and how much of it there is — never the method.

## Build and test

GCC 14 minimum. Linux only; on Windows use WSL.

```
make            # release static library, build/release/libjaos.a
make test       # unit suite
make sanitize   # unit suite under ASan + UBSan
make netlib     # the 94-instance acceptance gate (fetches instances first)
make pgo        # rebuild the library from a profile of it solving real models
```

Also `make netlib-kennington` and `make netlib-infeas` for the other two
reference sets. Instances are fetched by `bench/fetch.sh` and pinned by
sha256; they never enter this repository.

### There is one set of shipping flags, and it was measured

`make` builds `-O3 -flto -g -DNDEBUG`. There is no second "optimised" target,
because there is nothing to put in it. Each candidate was run over the whole
standard set with **every verdict, iteration count and solution digest
unmoved** — none of them trades an answer for a second — and then timed,
minimum of three runs, geometric mean of per-instance ratios (D62):

| | vs the rung below | |
|---|---|---|
| `-O3` over `-O2` | 1.0055x | noise |
| `-flto` | **1.0330x** | the only flag that does anything |
| `-march=native` | 1.0072x | noise, **and not portable** |
| **PGO** | **1.1122x** | `make pgo` |

**`make pgo` is worth three times every flag put together.** It compiles the
library instrumented, solves the standard set with it, and compiles again
from what that recorded. Use it for anything you intend to ship or measure.
It is not what `make` does, for two reasons: it takes minutes rather than a
second, and it cannot run until the instances have been downloaded — a
library that will not build without fetching 139 models from netlib is a
library nobody can package. `make pgo PGO_LOAD="25fv47 maros-r7 pilot"` uses
a subset when you want the turnaround.

**`NATIVE=1` exists and you probably do not want it.** `make NATIVE=1` adds
`-march=native -mtune=native`. Measured against plain LTO it is **1.0072x —
inside the measurement noise**, so it buys nothing here, and the binary it
produces **dies with an illegal instruction on any CPU older than the one
that built it**. That makes `libjaos.a` undistributable, which is why it is
not the default. If you are building for one known machine and want to try
it, measure it there rather than assuming; on this one it did not pay.

**`LTO=0`** drops `-flto` for a toolchain whose binutils have no linker
plugin. You lose the 3.3%.

The archive is built with `gcc-ar`, not `ar`, because an archive of LTO
objects keeps its symbols where only the plugin can see them. A consumer
compiling **without** `-flto` links it fine — and gets the benefit of it
anyway, since the objects are still GIMPLE for the plugin to work on.

**`-g` stays in the shipping build.** It costs nothing at run time, it is
what a profiler reads, and profiling the build that ships is how four of this
milestone's entries were found. `-DNDEBUG` is what removes the assertions.
Removing the deterministic work counter and the wall-clock check was measured
too, at 0.987x and 1.004x — inside the noise in both directions — so both
stay: they are the public `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit`, and they are free.

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
