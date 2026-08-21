# JAOS — Just Another Optimization Solver

A mathematical-programming solver written from scratch in C23. No external
dependencies, Apache 2.0, Linux and GCC only.

**Version 0.1.1.** JAOS reads an LP from disk, presolves it, solves it
with a revised dual simplex, and verifies the answer with an independent
checker. It does this on all 139 Netlib reference instances: 110 solved with
the objective inside the gate's tolerance and the checker green, 29 correctly
refused as infeasible.

Two qualifications, both measured, because "correctly" is a strong word.
**The published objective is exact** — the correctly rounded value of `c'x`
over the point it publishes, on 110 of 110, worst half an ulp. **The published
point is not always the optimal one.** Four of the 94 standard instances stop
measurably short: `pilot` by 2.31e-05, which is the one instance HiGHS,
SoPlex and Clp all beat. All four are the dual tolerance, and tightening it
repairs all four — three of them for less work — at the cost of six instances
crossing the gate's work bar. `TODO.md` carries that as a decision rather
than a defect, and `DECISIONS.md` D173 and D174 carry the measurements.

`make compare` measures JAOS against three other solvers, with every solver's
own presolve on and the dual simplex forced on both sides. The 2026-08-17 run
puts JAOS at **3.15x slower than HiGHS, 0.95x SoPlex (faster, for the first
time) and 2.57x slower than Clp**. One JAOS iteration costs 2.04x, 1.51x and
1.95x one of theirs, so the cost per iteration explains most of the gap. The
iteration counts are competitive. Milestone M2 targets the cost per
iteration.

The previous worst instance, `maros-r7` at 72.5x HiGHS, reads 1.33x after the
implied free column singleton (D106). The worst instance is now `stocfor3`
at 30.0x, and the difference is presolve: HiGHS reduces that model strongly
and JAOS barely touches it. `TODO.md` §5 carries that question.

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

- Reads fixed and free MPS, and a subset of the LP format.
- Presolve with six reduction families. Postsolve returns values, statuses
  and duals in terms of the caller's original problem.
- Curtis-Reid scaling.
- Sparse LU factorization with Forrest-Tomlin updates.
- Dual simplex with steepest-edge pricing and a Harris ratio test.
- An independent checker that verifies every answer against the original
  unscaled problem.

A loaded model can be modified: one bound, cost or coefficient at a time, or
whole rows and columns added or deleted. A re-solve then starts from the
basis of the previous solve instead of from scratch. A callback can watch a
running solve and stop it. A stopped solve keeps its basis, so raising the
limit and solving again continues from where it stopped.

`SPECS.md` lists every feature with its status: what exists, what does not,
and what is only partly there.

## What it does not do

JAOS cannot write files, and it can only be called from C. There is no
primal simplex, no barrier and no MILP.

The public API has forty-two functions. Seven of them configure something:
two tolerances, two budgets, where the output goes, how much of it there is,
and a callback that decides whether the solve continues. No function selects
a method. The solver decides which pricing rule runs, when it refactorizes,
and whether a sparse or a dense path is cheaper. Each such constant is
measured, fixed in the source, and not exposed as an option.

## Build and test

GCC 14 minimum. Linux only; on Windows use WSL.

```
make            # release static library, build/release/libjaos.a
make test       # unit suite
make sanitize   # unit suite under ASan + UBSan
make netlib     # the 94-instance acceptance gate (fetches instances first)
make pgo        # rebuild the library from a profile of it solving real models
```

`make netlib-kennington` and `make netlib-infeas` run the other two
reference sets. `bench/fetch.sh` downloads the instances and checks them
against pinned sha256 hashes. The instances never enter this repository.

### Compiler flags

`make` builds with `-O3 -flto -g -DNDEBUG`. There is no second "optimised"
target, because every flag that measured a gain is already in the default.
Each candidate flag ran over the whole standard set. A flag was only kept if
every verdict, iteration count and solution digest stayed the same. Timing:
minimum of three runs, geometric mean of per-instance ratios (D62):

| flag | vs the level below | verdict |
|---|---|---|
| `-O3` over `-O2` | 1.0055x | inside the noise |
| `-flto` | **1.0330x** | the only flag with a measured effect |
| `-march=native` | 1.0072x | inside the noise, and not portable |
| **PGO** | **1.1122x** | `make pgo` |

`make pgo` gains about three times as much as all the flags together. It
compiles the library instrumented, solves the standard set with it, and
compiles again from the recorded profile. Use it for anything you ship or
measure. It is not the default for two reasons. It takes minutes instead of
a second. It also needs the fetched instances, and a library that cannot
build before downloading 139 models cannot be packaged. `make pgo
PGO_LOAD="25fv47 maros-r7 pilot"` profiles on a subset when you want a
faster turnaround.

`make NATIVE=1` adds `-march=native -mtune=native`. Against plain LTO it
measured 1.0072x, inside the noise, so it gains nothing here. The binary
also fails with an illegal instruction on any CPU older than the build
machine, and that makes `libjaos.a` undistributable. Both reasons keep it
off by default. If you build for one known machine, measure it there before
trusting it.

`make LTO=0` drops `-flto`, for a toolchain whose binutils have no linker
plugin. This gives up the 3.3% gain.

`EXTRA_CFLAGS` is empty in every shipping build. It exists for one job:
sweeping a method constant over a range without editing the source between
runs. It is a development switch and it never selects a method. `make`
cannot see a flag change, so a sweep must run `make clean` between settings.
Without that, the sweep measures one binary several times.

The archive is built with `gcc-ar` instead of `ar`. An archive of LTO
objects keeps its symbols where only the linker plugin can read them, and
`gcc-ar` uses that plugin. A consumer who compiles without `-flto` still
links the archive correctly, and still gets the LTO gain, because the
objects stay in GIMPLE form.

`-g` stays in the shipping build. It costs nothing at run time, and a
profiler needs it. Profiling the shipping build found four of this
milestone's changelog entries. `-DNDEBUG` is what removes the assertions.
Removing the deterministic work counter and the wall-clock check was also
measured: 0.987x and 1.004x, both inside the noise. Both stay, because they
implement the public `jaos_work_units`, `jaos_set_work_limit` and
`jaos_set_time_limit`.

## Layout

```
include/jaos.h   the public header, the only one
src/             library sources
tests/           unit suite; tests/vendor/unity/ is the one vendored dependency
bench/           instance manifests, acceptance runner, results
bench/compare/   the harness that times JAOS against other solvers
docs/            formats, tolerances, scaling, work units, feature matrix
docs/research/   designs worked out on paper, with the literature checked
bench/measurements/  one directory per measured verdict, so it is re-derivable
```

## The documents, and which to read

- **`SPECS.md`** — every feature and where it stands.
- **`TODO.md`** — what is open, in the order it should happen.
- **`CHANGELOG.md`** — what landed and what it cost.
- **`DECISIONS.md`** — every closed decision, with the measurement that
  closed it.
- **`bench/README.md`** — the acceptance gate.
- **`bench/compare/README.md`** — how JAOS is compared against other
  solvers.
- **`docs/`** — the contracts behind every constant in the code: tolerances,
  work units, scaling, format support.
- **`bench/measurements/<id>/`** — the raw readings a verdict was taken
  from, one directory per verdict, each with its own `README.md`. Read the
  directory rather than any summary of it.

These documents record the design. Do not reconstruct it from the code.

**Start with `TODO.md`.** Its first section is a handover: the state of the
tree, what campaign is valid, what is open and what each open item needs
before any code.

## Licence

Apache 2.0. See `LICENSE`.
