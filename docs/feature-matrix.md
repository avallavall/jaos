# Feature matrix — JAOS against the field

`SPECS.md` says what JAOS is built to be and how far it has got. It does not say
what a serious solver is expected to have, so it cannot tell you whether the
target is the right target. This page is the other half: the feature list a
mathematical-programming solver is measured against, and where each solver
stands on it.

The list was written from what the field offers, not from what JAOS has. Most
of it is therefore empty for JAOS, which is the point. A matrix drawn the other
way round would flatter the project and teach nobody anything.

**How to read it**

| symbol | meaning |
|---|---|
| ● | present and complete |
| ◐ | present but partial — the gap is named in the notes |
| ○ | absent |
| — | not applicable to that solver's scope |
| ? | not verified for this page; treat as unknown, not as absent |

**The solvers.** The first three are the ones JAOS is benchmarked against, so
their columns can be checked by running `make compare`. SCIP, Gurobi and Hexaly
are there for reference and their entries come from public documentation, not
from measurement here. CPLEX, Xpress, COPT and Mosek are in the same class as
Gurobi and are left out only to keep the table readable.

*Last checked: 2026-08-13. Versions: JAOS 0.1.0-dev · HiGHS 1.15 · SoPlex 8.0.3
· Clp 1.17.11 · SCIP 10.0 · Gurobi 13.0 · Hexaly 15.0.*

---

## 1. Problem classes

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Linear programming (LP) | ● | ● | ● | ● | ● | ● | ● |
| Quadratic programming (QP) | ○ | ● | ○ | ○ | ● | ● | ● |
| Quadratically constrained (QCP, SOCP) | ○ | ○ | ○ | ○ | ● | ● | ● |
| Mixed-integer linear (MILP) | ○ | ● | — | — | ● | ● | ● |
| Mixed-integer quadratic (MIQP, MIQCP) | ○ | ○ | — | — | ● | ● | ● |
| Nonlinear (NLP) | ○ | ○ | — | — | ● | ● | ● |
| Mixed-integer nonlinear (MINLP) | ○ | ○ | — | — | ● | ● | ● |
| Constraint programming | ○ | ○ | — | — | ◐ | ○ | ● |
| Black-box / simulation optimization | ○ | ○ | — | — | ○ | ○ | ● |

SoPlex and Clp are LP solvers by design; their integer side is SCIP and CBC
respectively, which is why those rows read "—" and not "○".

## 2. LP algorithms

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Dual simplex | ● | ● | ● | ● | ● | ● | ? |
| Primal simplex | ○ | ● | ● | ● | ● | ● | ? |
| Barrier / interior point | ○ | ● | ○ | ● | ● | ● | ? |
| Crossover to a basic solution | ○ | ● | — | ◐ | ● | ● | ? |
| First-order method (PDLP / PDHG) | ○ | ● | ○ | ○ | ○ | ● | ○ |
| GPU acceleration | ○ | ◐ | ○ | ○ | ○ | ? | ○ |
| Concurrent solve (race several methods) | ○ | ◐ | ○ | ○ | ● | ● | ? |

JAOS's dual simplex has steepest-edge pricing, a Harris two-pass ratio test with
bound flipping, dual phase 1 by artificial bounds and a Bland fallback. The
missing primal simplex is what blocks crossover; it was measured and is not a
speed argument on its own (D81, D85).

HiGHS's GPU entry is ◐ because the GPU PDLP work is in progress rather than
released.

## 3. Preparing and handling the model

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Presolve | ◐ | ● | ● | ● | ● | ● | ● |
| Postsolve back to original indices | ◐ | ● | ● | ● | ● | ● | ● |
| Scaling | ● | ● | ● | ● | ● | ● | ? |
| Sparse LU with update (Forrest-Tomlin or similar) | ● | ● | ● | ● | ● | ● | ? |
| Hyper-sparse triangular solves | ◐ | ● | ● | ● | ● | ● | ? |
| Modify a loaded model (bounds, costs, coefficients) | ● | ● | ● | ● | ● | ● | ● |
| Add and delete rows and columns | ● | ● | ● | ● | ● | ● | ● |
| Warm start from a previous basis | ● | ● | ● | ● | ● | ● | ? |
| Read and write a starting basis | ● | ● | ● | ● | ● | ● | ? |
| Resume after a work or time limit | ● | ● | ? | ? | ● | ● | ● |

JAOS's presolve rows are ◐ as of phase 2: the reduced-model machinery, the
postsolve stack and the first reduction families have landed, the rest are in
progress. Before phase 2 both rows were ○. Hyper-sparsity is ◐ because both
triangular solves report their pattern but not every billed pass is reduced.

## 4. Mixed-integer machinery

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Branch and bound | ○ | ● | — | — | ● | ● | ● |
| Cutting planes | ○ | ● | — | — | ● | ● | ● |
| Primal heuristics | ○ | ● | — | — | ● | ● | ● |
| Solution pool | ○ | ○ | — | — | ● | ● | ● |
| Deterministic parallel tree search | ○ | ◐ | — | — | ● | ● | ? |

This whole section is out of scope for the current milestone and is not
scheduled. It is here because it is most of what separates an LP solver from a
solver people buy.

## 5. Parallelism

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Parallel LP solve | ○ | ● | ○ | ○ | ? | ● | ? |
| Parallel MIP solve | ○ | ◐ | — | — | ● | ● | ● |
| Deterministic under parallelism | — | ? | — | — | ? | ● | ? |

JAOS is single-threaded per model by design. The "—" on the determinism row
means the question does not arise, not that it fails. The benchmark runner's
`-j N` is process-level concurrency, one instance per process, not threads.

## 6. Correctness and verification

This is the section JAOS was built around, so it is worth reading carefully —
including the row where the field is ahead.

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| **Bit-identical results across different machines** | ● | ? | ? | ? | ? | ○ | ? |
| Deterministic on the same machine and version | ● | ● | ● | ● | ● | ● | ● |
| Independent checker shipped with the solver | ● | ○ | ○ | ○ | ○ | ○ | ○ |
| Exact rational LP solutions | ○ | ○ | ● | ○ | ● | ○ | ○ |
| Exact solving with no numerical tolerances | ○ | ○ | ● | ○ | ● | ○ | ○ |
| Machine-checkable certificate of the result | ○ | ○ | ◐ | ○ | ● | ○ | ○ |
| Certified bound on suboptimality | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Infeasibility / unboundedness certificate | ○ | ◐ | ◐ | ◐ | ● | ● | ? |
| Irreducible infeasible subsystem (IIS) | ○ | ○ | ○ | ○ | ○ | ● | ? |

Two rows carry most of the meaning.

**Cross-machine bit-identity.** Gurobi's own documentation states it is
deterministic on the same machine but not between different machines, and that
an LP with several optima can return a different one on different hardware.
JAOS gives the stronger guarantee: the same bits on every machine and every run.
The other columns are marked `?` because none of those projects makes an
explicit cross-machine claim either way, and absence of a claim is not evidence
of failure.

**Exact arithmetic and certificates.** JAOS ships an independent checker that no
other solver here does, and that is a real difference. But it is a
floating-point checker judging against tolerances. SoPlex has solved LPs exactly
over the rationals since version 2.1 and added precision boosting in 6.0; SCIP
10.0 solves MILPs with no numerical tolerances at all and can emit a VIPR
certificate that an external program verifies in exact rational arithmetic.
`SPECS.md` lists exact rational verification as missing, and this is what that
line is missing against.

## 7. Input and output

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Read MPS (fixed and free) | ● | ● | ● | ● | ● | ● | ● |
| Read LP format | ◐ | ● | ○ | ● | ● | ● | ● |
| Read compressed input | ○ | ● | ● | ● | ● | ● | ? |
| Direct load from arrays | ● | ● | ● | ● | ● | ● | ● |
| Write MPS | ○ | ● | ● | ● | ● | ● | ? |
| Write LP | ○ | ● | ○ | ● | ● | ● | ? |
| Write a solution file | ○ | ● | ● | ● | ● | ● | ● |
| Reject unsupported constructs with a line number | ● | ? | ? | ? | ? | ? | ? |

JAOS's LP reader covers a CPLEX-style core; the exact subset is in
`docs/format-support.md`.

## 8. Using it from another language

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| C or C++ | ● | ● | ● | ● | ● | ● | ● |
| Python | ○ | ● | ● | ● | ● | ● | ● |
| Julia | ○ | ● | ● | ● | ● | ● | ? |
| Java, .NET | ○ | ◐ | ○ | ○ | ◐ | ● | ● |
| MATLAB, R | ○ | ◐ | ○ | ● | ● | ● | ? |
| AMPL, GAMS and similar modelling systems | ○ | ● | ○ | ● | ● | ● | ● |

## 9. Controlling a solve

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Time limit | ● | ● | ● | ● | ● | ● | ● |
| Deterministic work limit | ● | ◐ | ? | ? | ● | ● | ? |
| Set primal and dual tolerances | ● | ● | ● | ● | ● | ● | ? |
| Logging with verbosity levels | ● | ● | ● | ● | ● | ● | ● |
| Progress callback that can stop the solve | ● | ● | ? | ● | ● | ● | ● |
| Callbacks that steer the search | ○ | ◐ | ○ | ○ | ● | ● | ○ |
| Choose the algorithm | ○ | ● | ● | ● | ● | ● | — |
| Sensitivity analysis and ranging | ○ | ● | ○ | ● | ○ | ● | ? |

JAOS's "choose the algorithm" is ○ by decision, not by omission: D64 draws the
line at what depends on the caller's data against what depends on the method,
and the method side is the solver's. That is a defensible position for a library
with one algorithm and becomes harder to hold once there are several.

JAOS's deterministic work limit is worth noting as a ● where most of the field
is weaker: the budget is counted in reproducible work units, so the same model
stops at the same point on any machine.

## 10. Licence and distribution

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Open source | ● | ● | ● | ● | ● | ○ | ○ |
| Free for commercial use | ● | ● | ◐ | ● | ◐ | ○ | ○ |
| No external dependencies | ● | ○ | ○ | ○ | ○ | — | — |

JAOS is Apache 2.0 with no dependencies at all. That is unusual and it is a
deliberate constraint, not an accident of youth.

---

## What the matrix says

**JAOS is an LP solver and most of this page is empty for it.** That is expected
at version 0.1.0-dev and it is not a criticism. What matters is whether the
empty cells are the right ones to be empty.

**Three things JAOS has that the field mostly does not.** Bit-identical results
across machines, which Gurobi explicitly does not promise. An independent
checker shipped with the solver, which none of the others ship. A budget counted
in reproducible work units rather than seconds. All three come from the same
decision, and it is the project's actual distinguishing feature.

**One place where JAOS is behind where it thought it was ahead.** Verification
is JAOS's own subject, and SoPlex and SCIP are further along it: exact rational
LP solutions, exact MILP solving with no tolerances, and certificates an
external checker verifies. JAOS's checker is a good floating-point checker. It
is not a proof. `SPECS.md` already lists exact rational verification as missing;
this page says what it is missing against.

**The current milestone will barely move this page, and that is by design.**
M2 is about speed, not features. Of everything on this page, only the presolve
rows change when M2 closes — and they change from ○ to ●, which is one line.
The measured gap at tier T0 is 3.72x HiGHS, 1.34x SoPlex, 3.77x Clp, and closing
it moves no cell here at all.

That is the answer to "is the plan improving the features as phases close": for
this milestone, almost not. If the feature matrix is the thing that should be
improving, the roadmap after M2 has to say so, because M2's own success
criterion is a time ratio and nothing else.

**What would move the most cells for the least work**, if features rather than
speed became the goal: writing MPS and LP files, a solution file, and Python
bindings. None is on the speed path, all four are currently in the backlog, and
together they close most of sections 7 and 8. The barrier method and the MIP
section are large pieces of work and are correctly not scheduled yet.

---

## Sources

Cells for HiGHS, SoPlex, Clp, SCIP, Gurobi and Hexaly were taken from public
documentation on the date above. JAOS's own cells come from `SPECS.md`,
`DECISIONS.md` and the measured results in `bench/`.

- HiGHS solver capabilities and parallelism: <https://ergo-code.github.io/HiGHS/dev/solvers/> and <https://ergo-code.github.io/HiGHS/stable/parallel/>
- HiGHS 2026 development, QP and GPU PDLP: <https://highs.dev/assets/HiGHS_Newsletter_26_0.pdf>
- Gurobi 13.0 problem classes and new methods: <https://www.gurobi.com/resources/reports/what-s-new-in-gurobi-13-0>
- Gurobi determinism across machines: <https://support.gurobi.com/hc/en-us/articles/360031636051-Is-Gurobi-deterministic> and <https://support.gurobi.com/hc/en-us/articles/360045849232-Why-does-Gurobi-perform-differently-on-different-machines>
- SoPlex as an exact LP solver: <https://soplex.zib.de/doc/html/EXACT.php>
- SCIP Optimization Suite 10.0, exact solving and certificates: <https://arxiv.org/pdf/2511.18580>
- VIPR certificate format: <https://github.com/scipopt/vipr>
- Hexaly Optimizer model types: <https://www.hexaly.com/hexaly-optimizer> and <https://www.hexaly.com/docs/last/modelingprinciples/index.html>

No solver's source code was read to produce this page. That rule (D2, D11, D12,
D15) applies to implementation, and it is respected here: everything above comes
from documentation a user can read.
