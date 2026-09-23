# JAOS — Just Another Optimization Solver

JAOS solves linear and mixed-integer linear programs. It also solves convex
quadratic programs, and second-order cone and convex quadratically
constrained programs, with integer columns or without; `SPECS.md` marks
those classes partial and says what each still lacks. It is written from
scratch in C23, links nothing but libc and libm (and `-pthread`, which
glibc 2.34 and later keep inside libc), builds to one static
library with GCC on Linux, and is licensed under Apache 2.0.

Two properties hold on every commit. The answer is bit-identical on every
machine and every run: no clock decides anything, no iteration order depends
on an address, and floating-point contraction is off. And an answer counts
only when an independent checker, which shares no algorithm with the
solver, accepts it against the model as the caller loaded it.

## Status

The last tagged release is 0.4.0. JAOS answers all 139 Netlib reference
instances correctly and solves 24 MIPLIB 3 instances to their catalogue
optima. On the Netlib set it is about 2x slower per solve than HiGHS and
Clp, and faster than SoPlex. `bench/compare/` measures and publishes the
factors.

`SPECS.md` lists every feature with its status. `TODO.md` holds the current
milestone's backlog, and it is empty between milestones.

## What it does

```c
#include <stdio.h>
#include <stdlib.h>
#include "jaos.h"

int main(void)
{
    jaos_model *m;
    if (jaos_model_new(&m) != JAOS_OK)
        return 1;
    if (jaos_read_mps(m, "model.mps") != JAOS_OK || jaos_solve(m) != JAOS_OK) {
        fprintf(stderr, "%s\n", jaos_model_error(m));
        jaos_model_free(m);
        return 1;
    }
    if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
        double obj;
        double *x = malloc((size_t)jaos_num_col(m) * sizeof *x);
        if (x != nullptr && jaos_objective(m, &obj) == JAOS_OK &&
            jaos_solution(m, x, nullptr, nullptr, nullptr) == JAOS_OK)
            printf("objective %.17g\n", obj);
        free(x);
    }
    jaos_model_free(m);
    return 0;
}
```

**Files.** Reads fixed and free MPS and writes free MPS. Reads and writes the
CPLEX-style core of the LP format, AMPL's text `.nl`, QPLIB, OSiL and the Conic
Benchmark Format (CBF) (`docs/format-support.md`). Reads and writes gzip with an inflate and deflate
written here. Writes its own solution file, a point file and a duals file, and
reads them back. `jaos diff` says whether two files are the same
model; `jaos show` prints one row or column. `jaos STUB -AMPL` answers
AMPL's solver protocol; Pyomo calls it that way on a linear or quadratic
model, and so can JuMP through AmplNLWriter.

**Linear programs.** Presolve with seven reduction families, the last one
substituting an implied free column out of a short equation, and a
postsolve to the caller's model. Curtis-Reid scaling. Sparse LU with Markowitz pivoting and
Forrest-Tomlin updates. Dual simplex with steepest-edge pricing that falls
back to dual Devex when a weight drifts, a Harris
ratio test with bound flipping, dual phase 1 by artificial bounds, a cost
perturbation on a stall and Bland's rule after it. The dual is the default;
`--algorithm` also takes `primal`, `barrier` (Mehrotra's predictor-corrector
with a crossover to a basis), `pdlp` (the first-order method) and
`concurrent` (the dual, the primal and the barrier raced under a work
budget, the same winner on every machine).

**Quadratic and conic programs.** A convex quadratic objective is solved by
the barrier, whose point is finished by a push onto its active set.
Second-order cones, rotated or not, and convex quadratic rows are solved by
a homogeneous self-dual interior point with Nesterov-Todd scaling and a
Newton finish, and the checker judges the answer with the cones' duals.
Integer columns beside a quadratic objective go to the branch and bound
below, over barrier relaxations; beside cones or quadratic rows they go to
a branch and bound of its own, which can take its open nodes in rounds
solved on several threads with the same answer at any thread count
(`--tree-batch`).

**Mixed-integer programs.** Branch and bound over the dual simplex, best
estimate first with the best bound every fifth pick, pseudocost branching.
Gomory, knapsack cover, mixed-integer rounding and clique cuts at the root,
and Gomory cuts to depth 3. A rounding heuristic, a root dive and a
feasibility pump. A solution pool of distinct integer assignments, a MIP
start, a cutoff, a node limit, an
incumbent callback and a node callback that adds lazy constraints and user
cuts and picks the branching column. Conflict analysis at infeasible nodes.
Symmetry detection at the root and orbital branching on its orbits. The
tree can take its open nodes in rounds solved on several threads, with the
same answer at any thread count (`--tree-batch`, off by default).
Every default was set on the MIPLIB 3 set (`make miplib`), and the node
order on the MIPLIB 2017 reading as well (`bench/measurements/02-286/`).

**After the answer.** The independent checker. Sensitivity and ranging for
every cost and bound. Farkas certificates and unbounded rays, floating and
exact. An irreducible infeasible subsystem, written out as a model if asked,
and a feasibility relaxation. An exact rational proof that the basis is
optimal, the exact values, and a proof file that is checked from the model
alone. The checker and the prover also take a point or a basis another solver
produced.

**A model is not read-only.** Bounds, costs, coefficients, the objective's
sense and constant, rows and columns added or deleted, all reading back. A
re-solve starts from the previous basis. A solve stopped on a work limit, a
time limit or a callback parks its whole state on the model and the next
solve goes on from exactly where it stopped, so an LP stopped and solved on
ends on the uninterrupted answer to the bit; a stopped solve also writes its
basis, which is the warm start from a file.

**Command line.** `make cli` builds `jaos`. `jaos solve model.mps` prints one
fact per line; the exit code is the verdict. `convert`, `check`, `stats`,
`options`, `diff`, `show`, `iis`, `relax`, `verify`, `ranging`, and
`jaos STUB -AMPL` for AMPL's solver protocol. Reference:
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

**Julia.** The `JAOS` package in `julia/JAOS` over `libjaos.so`: the C
calls through `ccall`, and `JAOS.Optimizer`, a MathOptInterface optimizer,
so JuMP uses it directly. It passes MathOptInterface's own conformance
suite, with four tests left out because JAOS refuses what they ask (a
non-convex quadratic row, and an IIS that keeps integrality). A change
after a solve goes to the loaded model, so JuMP re-solves warm, and lazy
constraints and user cuts run in the tree. The `JAOS.` functions reach
every C call. `make shared` builds the library; `Pkg.develop(path="julia/JAOS")`
adds the package.

```julia
using JuMP, JAOS
model = Model(JAOS.Optimizer)
@variable(model, x <= 4)
@variable(model, y, Int)
@constraint(model, x + y <= 4)
@objective(model, Max, x + 2y)
optimize!(model)
```

**.NET, Java and R.** `dotnet/Jaos` (.NET 8, P/Invoke), `java/src`
(Java 22 or later, the foreign-function API, no glue code) and `R/jaos`
(an R package over `.Call`) reach every C call Python reaches. .NET and
Java call all 200 directly. R calls 142 of them and reaches the other 58,
the option setters and getters, through the option names. The callbacks,
the checkers, the exact proofs, the IIS and the files are all there. The
.NET and Java packages add a modelling layer (`Problem`, `Var`, `Expr`)
that re-solves warm after a bound, cost or sense changes; R has
`jaos_solve_lp` over a dense or sparse matrix.

```csharp
using var p = new Problem();
var x = p.AddVar(ub: 4);
var y = p.AddVar(integer: true);
p.AddLe(x + y, 4);
p.Maximize(x + 2 * y);
p.Solve();
```

## Build and test

GCC 14 or later, Linux. The same sources cross-compile for Windows with
mingw-w64 through the CMake package, and the tool and the Python binding
run natively on Windows with Linux's answers byte for byte
(`tests/windows.sh`). CI also builds them with clang-cl on Windows and
with GCC 14 on macOS; see [`docs/build.md`](docs/build.md).

```
make              # build/release/libjaos.a
make test         # unit suite, the CLI's test, the install, CMake and Windows checks
make sanitize     # unit suite under ASan and UBSan
make configs      # the above over all five build configurations, clean between each
make cli          # build/cli/jaos
make shared       # build/release/libjaos.so, which the Python and Julia bindings load
make python-test  # the binding's own suite
make julia-test   # the Julia package's suite, MathOptInterface's conformance tests included
make dotnet-test  # the .NET binding's checks
make java-test    # the Java binding's checks; `make java` builds build/java/jaos.jar
make r-test       # the R package, installed into build/R, and its checks
make netlib       # the 94-instance gate (fetches the instances first)
make miplib       # the 24-instance MIP set
make compare-solvers               # build HiGHS, SoPlex and Clp, once
make compare COMPARE_ARGS='-t P0'  # time JAOS against them on Netlib
make pgo          # rebuild from a profile of it solving real models
```

`make netlib-kennington` and `make netlib-infeas` run the other two reference
sets, `make maros-meszaros` the 138 QPs of Maros and Meszaros (137 of them
convex), and
`make cblib` the 29 continuous instances of CBLIB 2014. Every set takes
`J=N` to run N instances at a time. `bench/fetch.sh` downloads the
instances and checks them against pinned sha256 hashes.

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
top (1.1122x in the table of [`docs/build.md`](docs/build.md)) and needs
the fetched instances. It profiles `libjaos.a`, and the tool gets the
profile when `make cli` links it again; `libjaos.so`, the Python wheels
and the other bindings carry no profile.

## Results

The gate is the Netlib collection: 94 standard instances, 16 Kennington, 29
infeasible. Every feasible instance solves to the published optimum within the
gate's tolerance, the checker accepts every answer, and the 29 infeasible
models are refused. `bench/README.md` says how it is run.

`make compare COMPARE_ARGS='-t P0'` times JAOS against HiGHS, SoPlex and Clp
with every solver's own presolve on and the dual simplex forced. It times
SoPlex and Clp only after `make compare-solvers` has built them. The reading
in `bench/compare/results/P0.txt` (2026-09-22, tree 7311fa3):

| vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|
| 2.03x slower per solve | 0.66x, faster | 1.93x slower |

JAOS takes about as many iterations as HiGHS and Clp (1.14x and 1.06x) and
fewer than SoPlex (0.45x). One iteration costs 1.5x to 1.8x what it costs
each rival, and that is what separates JAOS from the field.

On MIP, `bench/compare/run-mip.sh` gives each solver 20 s per instance, one
thread and a relative gap of 1e-6 (2026-09-21, tree 3086162). On MIPLIB 3
JAOS solves 23 of 24, HiGHS 1.15.1 and SCIP 10.0 all 24, and JAOS's shifted
mean time is 1.43x HiGHS's and 1.48x SCIP's. On the 30 MIPLIB 2017
instances of `make miplib2017` JAOS solves none, HiGHS 8 and SCIP 7. These
MIP numbers were taken at tree 3086162, before the current MIP defaults
(the best-estimate node order, ae25a70), and a re-take is due.

JAOS is timed against other solvers on LP and MIP only. A QP rung and a
conic rung are a row in `TODO.md`.

## Layout

```
include/jaos.h        the public header, the only one
src/                  library sources
tests/                unit suite; tests/vendor/unity/ is the one vendored dependency
python/               the jaos package, over ctypes and the standard library only
julia/JAOS/           the Julia package: ccall and a MathOptInterface optimizer
dotnet/               the .NET binding (Jaos) and its checks (Jaos.Check)
java/                 the Java binding (src/org/jaos) and its checks (check/)
R/                    the R package (jaos) and its checks (check.R)
cli/                  the command-line tool, over the public header only
bench/                instance manifests, the gate runner, baselines, results
bench/compare/        the harness that times JAOS against other solvers
bench/measurements/   raw readings behind every measured verdict
docs/                 the API, the CLI, formats, tolerances, scaling, work units, the build, the feature matrix
docs/research/        designs worked out on paper
```

## The documents

- `SPECS.md` — every feature JAOS must have, with its status.
- `TODO.md` — the current milestone's backlog.
- `bench/refusals.txt` — ideas measured as worse, and what would reopen each.
- `docs/` — the API, the tool, the formats, the constants behind the code;
  [`docs/README.md`](docs/README.md) lists every page.
- `CONTRIBUTING.md` — how to build, test and send a change, and the rules
  it must hold.
- `SECURITY.md` — how to report a vulnerability.

A `D<n>` reference in `docs/` points at the retired decision log:
`git show 2d3c56b:DECISIONS.md`.

## Licence

Apache 2.0. See `LICENSE`.
