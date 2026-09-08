# JAOS — Just Another Optimization Solver

JAOS solves linear and mixed-integer programs. It is written from scratch in
C23, links nothing but libc and libm, builds to one static library with GCC on
Linux, and is licensed under Apache 2.0.

Two properties hold on every commit. The answer is bit-identical on every
machine and every run: no clock decides anything, no iteration order depends
on an address, and floating-point contraction is off. And an answer counts
only when an independent checker, which shares no code with the solver,
accepts it against the model as the caller loaded it.

## Status

The last tagged release is 0.3.0. JAOS answers all 139 Netlib reference
instances correctly and solves 24 MIPLIB 3 instances to their catalogue
optima. It is slower than the established open-source LP solvers by a factor
that is measured and published in `bench/compare/`.

`SPECS.md` lists every feature with its status. `TODO.md` is what is being
built now.

## What it does

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

**Files.** Reads fixed and free MPS and the CPLEX-style core of the LP format
(`docs/format-support.md`). Reads and writes gzip with an inflate and deflate
written here. Writes MPS, LP, its own solution file, a point file and a duals
file, and reads them back. `jaos diff` says whether two files are the same
model; `jaos show` prints one row or column.

**Linear programs.** Presolve with six reduction families and a postsolve to
the caller's model. Curtis-Reid scaling. Sparse LU with Markowitz pivoting and
Forrest-Tomlin updates. Dual simplex with steepest-edge pricing, a Harris
ratio test with bound flipping, dual phase 1 by artificial bounds, Bland's
rule on a stall.

**Mixed-integer programs.** Branch and bound over the dual simplex, best bound
first, pseudocost branching. Gomory, knapsack cover and mixed-integer rounding
cuts at the root and below it. A rounding heuristic, a root dive and a
feasibility pump. A solution pool, a MIP start, a cutoff, a node limit and an
incumbent callback. Every default was set on the MIPLIB 3 set (`make miplib`).

**After the answer.** The independent checker. Sensitivity and ranging for
every cost and bound. Farkas certificates and unbounded rays, floating and
exact. An irreducible infeasible subsystem, written out as a model if asked,
and a feasibility relaxation. An exact rational proof that the basis is
optimal, the exact values, and a proof file that is checked from the model
alone. The checker and the prover also take a point or a basis another solver
produced.

**A model is not read-only.** Bounds, costs, coefficients, the objective's
sense and constant, rows and columns added or deleted, all reading back. A
re-solve starts from the previous basis. A stopped solve keeps its basis and
can resume, in-process or from a file.

**Command line.** `make cli` builds `jaos`. `jaos solve model.mps` prints one
fact per line; the exit code is the verdict. `convert`, `check`, `stats`,
`diff`, `show`, `iis`, `relax`, `verify`, `ranging`. Reference:
[`docs/cli.md`](docs/cli.md).

**Python.** The `jaos` package in `python/` over `libjaos.so`, standard
library only. Every C call is reachable, and a modeling layer sits on top.
`pip install .` builds the shared library and installs the package;
`python -m jaos solve model.mps` solves from the command line.

```python
p = jaos.Problem()
x = p.add_var(ub=4)
y = p.add_var(integer=True)
p.add(x + y <= 4)
p.maximize(x + 2*y)
p.solve()
```

## Build and test

GCC 14 or later, Linux. The same sources cross-compile for Windows with
mingw-w64 through the CMake package; see [`docs/build.md`](docs/build.md).

```
make              # build/release/libjaos.a
make test         # unit suite, the CLI's test, the install, CMake and Windows checks
make sanitize     # unit suite under ASan and UBSan
make cli          # build/cli/jaos
make shared       # build/release/libjaos.so, which the Python binding loads
make python-test  # the binding's own suite
make netlib       # the 94-instance gate (fetches the instances first)
make miplib       # the 24-instance MIP set
make compare      # time JAOS against HiGHS, SoPlex and Clp
make pgo          # rebuild from a profile of it solving real models
```

`make netlib-kennington` and `make netlib-infeas` run the other two reference
sets. Every set takes `J=N` to run N instances at a time. `bench/fetch.sh`
downloads the instances and checks them against pinned sha256 hashes.

```
make install                        # /usr/local
make install PREFIX=$HOME/.local
make install DESTDIR=/tmp/stage
make uninstall
```

Installs the header, both library forms, the tool and a pkg-config file:
`cc $(pkg-config --cflags jaos) prog.c $(pkg-config --libs jaos)`.

The same build under CMake, for projects that consume it with
`find_package`:

```
cmake -S . -B build/cmake -DCMAKE_C_COMPILER=gcc-14
cmake --build build/cmake --parallel
ctest --test-dir build/cmake
cmake --install build/cmake --prefix $HOME/.local
```

That installs the same files plus a package config, so a consumer writes
`find_package(jaos REQUIRED)` and links `jaos::jaos` (the archive),
`jaos::shared` or runs `jaos::cli`.

`make` builds with `-O3 -flto -g -DNDEBUG`. `make pgo` is worth about 1.1x on
top and needs the fetched instances. [`docs/build.md`](docs/build.md).

## Results

The gate is the Netlib collection: 94 standard instances, 16 Kennington, 29
infeasible. Every feasible instance solves to the published optimum within the
gate's tolerance, the checker accepts every answer, and the 29 infeasible
models are refused. `bench/README.md` says how it is run.

`make compare COMPARE_ARGS='-t P0'` times JAOS against HiGHS, SoPlex and Clp
with every solver's own presolve on and the dual simplex forced. The reading
in `bench/compare/results/P0.txt` (2026-08-30):

| vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|
| 3.60x slower per solve | 1.12x slower | 2.96x slower |

JAOS takes fewer iterations than SoPlex. The cost of one iteration is what
separates it from the field.

## Layout

```
include/jaos.h        the public header, the only one
src/                  library sources
tests/                unit suite; tests/vendor/unity/ is the one vendored dependency
python/               the jaos package, over ctypes and the standard library only
cli/                  the command-line tool, over the public header only
bench/                instance manifests, the gate runner, baselines, results
bench/compare/        the harness that times JAOS against other solvers
bench/measurements/   raw readings behind the refusals
docs/                 formats, tolerances, scaling, work units, the build, the CLI
docs/research/        designs worked out on paper
```

## The documents

- `SPECS.md` — every feature JAOS must have, with its status.
- `TODO.md` — the current milestone's backlog.
- `bench/refusals.txt` — ideas measured as worse, and what would reopen each.
- `docs/` — the constants behind the code and the formats it reads and writes.

A `D<n>` reference in `docs/` points at the retired decision log:
`git show 2d3c56b:DECISIONS.md`.

## Licence

Apache 2.0. See `LICENSE`.
