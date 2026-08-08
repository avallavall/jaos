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
- **The basis behind the answer**, not just the numbers: `jaos_basis` says
  which columns and which row activities rest on a bound and which are basic
  — that is, which constraints the optimum is actually held by, a question
  the published values cannot answer on their own.
- **An independent solution checker** that verifies a claimed solution against
  the original problem without consulting any solver state.
- **Two budgets**: a reproducible work limit counted in deterministic work
  units, and a wall-clock time limit. They are separate because only one of
  them means the same thing on two machines. What a work unit is, and where
  it is charged, is in [`docs/work-units.md`](docs/work-units.md).

Every tolerance a solve compares against, and the formulas the checker
judges with, are in [`docs/tolerances.md`](docs/tolerances.md). They were
drafts until the Netlib gate closed, and are now frozen: any change to one
is a changelog entry.

## How well it works today

Not a claim — a run. `make netlib` fetches the 94 instances of the Netlib
standard set, checksum-verified against a committed manifest, and judges
each solve against the published optimum, the independent checker, and a
second solve that has to agree bit for bit. The last result is committed at
[`bench/results/netlib.txt`](bench/results/netlib.txt).

| | |
|---|---|
| loads with the right shape | 94 / 94 |
| solved to optimal | 94 / 94 |
| objective within tolerance | 94 / 94 |
| independent checker green | 94 / 94 |
| identical across two solves | 94 / 94 |

**The gate is met.** Two further sets it asks for pass as well: the 16
**Kennington** problems on every condition, `ken-18` included at 105127 rows
by 154699 columns — an order of magnitude past anything in the table above —
and the 29 **infeasible** instances refused every time, with no false optimum
anywhere, which is the one thing that set exists to check.

Eight instances were refused at some point along the way, and **not one of
them was closed by widening a tolerance**. Each was a distinct defect with a
mechanism: a contribution the checker was silently dropping, a
bound-proximity test judged absolutely on a row whose terms cancel ten orders
of magnitude, a settled basis the method had never been handed back, a cycle
that had been read as a stall, a repair test weighing the wrong quantity in
the wrong space, a column with nowhere to rest, a residual of the basis solve
mistaken for a violated bound, and a clean-up loop taking one pivot where
twelve were waiting. `DECISIONS.md` carries what each one turned out to be
and `docs/research/netlib-campaign.md` the measurements, including the
readings that were wrong on the way — each of which is what pointed at the
next.

Two of those eight were the ones most readily explained as the limit of
double precision: `etamacro`, which defeats CPLEX at its defaults and SoPlex
at 1e-6, and `pilot87`, the worst-conditioned model in the set. Both were
defects.

The readers are the part that came out clean on the first run: every instance
loads with exactly the row and column counts two independent canonical
sources agree on — which is the one thing the checker structurally cannot
verify, since it reads the same stored matrix the solver does.

Those five numbers are a summary, and a summary is the wrong instrument for
noticing that a change broke something — now more than ever, since a gate
that passes is precisely the one whose totals cannot move. So `make netlib`
also diffs every instance against
[`bench/netlib.baseline`](bench/netlib.baseline), which records what each one
did last time, and fails on anything that got worse whatever the totals say.
See [`bench/README.md`](bench/README.md).

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
solved to published reference values. That gate is built, running and met on
all three instance sets — the numbers above are where it stands. What is left
of the milestone is bookkeeping rather than solver work; [`PLAN.md`](PLAN.md)
carries it, the staging, and the questions the next milestone opens on.

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
