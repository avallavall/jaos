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

*JAOS's column was last checked against `src/` and `cli/` on 2026-09-05. The other
columns were last checked against their published documentation on
2026-09-04, and that pass moved six cells — two of them corrections rather
than news, because HiGHS's and SCIP's IIS both predate the previous check.
Versions: JAOS 0.2.0 · HiGHS 1.15.1 · SoPlex 8.0.3 · Clp 1.17.11 ·
SCIP 10.0.3 · Gurobi 13.0.3 · Hexaly 15.0. No rival's major version moved
since 2026-08-13; only patch levels did.*

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
| Primal simplex | ◐ | ● | ● | ● | ● | ● | ? |
| Barrier / interior point | ○ | ● | ○ | ● | ● | ● | ? |
| Crossover to a basic solution | ○ | ● | — | ◐ | ● | ● | ? |
| First-order method (PDLP / PDHG) | ○ | ● | ○ | ○ | ○ | ● | ○ |
| GPU acceleration | ○ | ● | ○ | ○ | ○ | ● | ○ |
| Concurrent solve (race several methods) | ○ | ◐ | ○ | ○ | ● | ● | ? |

JAOS's dual simplex has steepest-edge pricing, a Harris two-pass ratio test with
bound flipping, dual phase 1 by artificial bounds and a Bland fallback.

**The primal simplex reads ◐ rather than ○ since 2026-08-31**, and the reason it
is not ● is that no caller can select it. `run_primal` and `run_primal_phase1`
are in `src/simplex.c`, the tests reach them and `make primal` measures them.
The only route in is `cfg.force_primal` in `src/jaos_internal.h`, a development
switch and not public API (D64, D188). Devex pricing is what is missing
(`TODO.md` §0 stage 5, blocked on a paywalled source), and it is Devex alone
that still blocks crossover. Stage 7, the unboundedness verdict, landed on
2026-09-01: the primal declares a ray it meets in phase 2 on the same D19
proof the dual already used (D241), and the shared lent-bound verdict also
proves a ray that needs several columns at once (D247).

**HiGHS's GPU row was ◐ on the claim that the PDLP work was "in progress
rather than released". It is released.** HiGHS ships cuPDLP-C and a native
first-order solver of its own, HiPDLP; `solver` accepts `"pdlp"` and
`"hipdlp"`, and both run on an NVIDIA GPU under Linux and Windows. The GPU is
the point of the row rather than a bonus: the documentation says that on a CPU
these are "unlikely to be competitive with the HiGHS interior point or simplex
solvers". Neither generates a basic solution and neither has a crossover, so
the crossover row above is about the interior point solvers only.

**Gurobi's GPU row was `?` and is ● .** Gurobi 13.0 added PDHG with GPU
acceleration: `Method` takes `GRB_METHOD_PDHG`, `PDHGGPU=1` selects the GPU,
and there is a documented GPU-enabled build. For a MIP it applies to the root
relaxation only.

## 3. Preparing and handling the model

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Presolve | ◐ | ● | ● | ● | ● | ● | ● |
| Postsolve back to original indices | ● | ● | ● | ● | ● | ● | ● |
| Scaling | ● | ● | ● | ● | ● | ● | ? |
| Sparse LU with update (Forrest-Tomlin or similar) | ● | ● | ● | ● | ● | ● | ? |
| Hyper-sparse triangular solves | ◐ | ● | ● | ● | ● | ● | ? |
| Modify a loaded model (bounds, costs, coefficients) | ● | ● | ● | ● | ● | ● | ● |
| Add and delete rows and columns | ● | ● | ● | ● | ● | ● | ● |
| Warm start from a previous basis | ● | ● | ● | ● | ● | ● | ? |
| Read and write a starting basis | ● | ● | ● | ● | ● | ● | ? |
| Resume after a work or time limit | ● | ● | ? | ? | ● | ● | ● |

JAOS's presolve rows are ◐ as of phase 2: the reduced-model machinery, the
postsolve stack and six reduction families have landed, and what is left is
counted rather than guessed — duplicate rows, duplicate columns and dominated
columns are deferred at 0.15% of these 139 models (D101), doubleton
equalities and 99.7% of them sit behind the bound tightening D97 refused, and
the implied free column singleton reaches equality rows only, which is a
third of what its counter reads (D106). `TODO.md` §1 and §3 own the
remainder. Two later measurements belong here: `make refusals` re-runs D101's
reopen condition and finds zero removable rows and columns on all 15 plato
instances (D242), and dual fixing was measured and refused at 0.67% of
netlib's and 1.09% of fome's live columns against a 5% bar (D246). Before
phase 2 both rows were ○.

**Postsolve moved from ◐ to ● on 2026-09-04, and it was a bookkeeping error
rather than a change.** The cell was ◐ with no gap named anywhere, which the
legend above forbids. It was echoing presolve. The one postsolve defect the
record ever carried was D167's broken row-count promise on 46 of netlib's 188
solves, and D257 closed it: every postsolve status is decided from the
reduction's structure, and 188 of 188 netlib and 32 of 32 Kennington solves
publish exactly `num_row` basics. Postsolve covers every reduction JAOS
performs, so the row is complete for what it has to undo.

Hyper-sparsity is ◐ because **FTRAN's passes still traverse every slot**.
Both solves report their pattern, and since D253 both of BTRAN's triangular
passes compute only their reachable slots. FTRAN's skip the arithmetic of a
zero slot and bill per nonzero, so the work counter cannot see the traversal
at all — only an instruction count can, which is why this row does not close
on a work-unit measurement.

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
| Machine-checkable certificate of the result | ◐ | ○ | ◐ | ○ | ● | ○ | ○ |
| Certified bound on suboptimality | ◐ | ○ | ● | ○ | ● | ○ | ○ |
| Infeasibility / unboundedness certificate | ● | ◐ | ◐ | ◐ | ● | ● | ? |
| Irreducible infeasible subsystem (IIS) | ● | ● | ○ | ○ | ● | ● | ? |

Three rows carry most of the meaning.

**The machine-checkable certificate moved from ○ to ◐ on 2026-09-05
(D285).** The solution file now carries the Farkas certificate of an
infeasible answer and the ray of an unbounded one, and `jaos check` judges
either from the model and the file alone, so a verdict can leave the
process that found it and be checked by another. It is ◐ and not ● because
the optimum's proof is still the tolerance-judged checker report: the
exact rational proof `jaos_verify` computes is not written to a file, and
the point and duals a solution file carries certify optimality only to a
tolerance.

**The IIS, and this row was wrong until 2026-09-04.** It read "among the open
solvers here only Gurobi documents the feature", with HiGHS and SCIP at ○.
Both have it, and both had it before this page was first written. HiGHS ships
`Highs::getIis` with documented options (`iis_strategy`, whose value 8 is
"Find true IIS", and `iis_time_limit`), and HiGHS 1.15.0's release notes fix
bugs in it. SCIP 10.0 ships an IIS Finder, section 3.9 of the Suite 10.0
paper. The honest claim left is narrower: JAOS has one, and it is not
distinctive.

What JAOS's is: `jaos_iis` names, for an INFEASIBLE answer, a set of bound
sides (a row's or a column's lower or upper bound) that is infeasible on its
own and becomes feasible when any one of them is dropped. Chinneck and
Dravnieks's sensitivity filter over the published certificate, then their
deletion filter, one warm re-solve per candidate on a private copy of the
model. The solver itself is the oracle: on **28** of the 29 reference
infeasibles the members alone re-solve INFEASIBLE and each one dropped
re-solves OPTIMAL, and all 29 reproduce. The 29th is `cplex2`, infeasible by
less than the feasibility tolerance, which keeps three of its 232 members a cold
re-solve does not need; the fixpoint pass that would drop them is refused on
cost (D264, `bench/measurements/02-171/`).

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
`SPECS.md` lists exact rational verification as **partial** — the arithmetic
landed at D266 and `jm_exact_evaluate` walks a published point with no
rounding (D267) — and the verifier is what is still missing. This is what
that line is missing against.

## 7. Input and output

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Read MPS (fixed and free) | ● | ● | ● | ● | ● | ● | ● |
| Read LP format | ◐ | ● | ● | ● | ● | ● | ● |
| Read compressed input | ● | ● | ● | ● | ● | ● | ? |
| Direct load from arrays | ● | ● | ● | ● | ● | ● | ● |
| Write MPS | ● | ● | ● | ● | ● | ● | ? |
| Write LP | ◐ | ● | ● | ● | ● | ● | ? |
| Write a solution file | ● | ● | ● | ● | ● | ● | ● |
| Reject unsupported constructs with a line number | ● | ? | ? | ? | ? | ? | ? |

JAOS's LP reader covers a CPLEX-style core; the exact subset is in
`docs/format-support.md`.

The three writer rows moved from ○ on 2026-08-31 (D226). Write LP is ◐ because
the dialect is narrower than a model: a free row is refused by name, and the
message points at `jaos_write_mps`, which has no such limit. A ranged row is
**not** refused — D239 writes it as the two-sided form and reads it back as
one row with two ends — and a row with no coefficients is written as a zero
term and read back as the empty row it was (D276). What JAOS writes it reads
back as the same model, checked field by field and name by name: 139 of 139
gate instances through MPS, and **104 of 139 through LP with 35 refused and
0 differing** (D284, `bench/measurements/02-188/lpcover.txt`). 34 of the 35
are a name the LP scanner cannot read back -- Netlib names start with digits
and hold `*` and `-` -- and the writer refuses them by name rather than
rename them, pointing at MPS. It was 138 and 1 while the writer printed
positional names (D276, `02-181/`), which no scanner refuses.

Write MPS reads ● and still has three refusals, which is not a contradiction:
two of them are shapes the format itself has no syntax for, and the third is a
ranged row whose two bounds no RANGES entry reconstructs exactly. All three
are refused with the row named. Nothing on the gate reaches any of them. The
comparison this row invites is what the other solvers do with the same input,
and this page has not measured that.

## 8. Using it from another language

| | JAOS | HiGHS | SoPlex | Clp | SCIP | Gurobi | Hexaly |
|---|---|---|---|---|---|---|---|
| Command-line tool | ● | ● | ● | ● | ● | ● | ? |
| C or C++ | ● | ● | ● | ● | ● | ● | ● |
| Python | ● | ● | ● | ● | ● | ● | ● |
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
| Sensitivity analysis and ranging | ● | ● | ○ | ● | ○ | ● | ? |

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
at 0.2.0 and it is not a criticism. What matters is whether the empty cells are
the right ones to be empty. JAOS is present in eight of the ten sections; the
two it is absent from entirely are mixed-integer machinery and parallelism.

**Three things JAOS has that the field mostly does not.** Bit-identical results
across machines, which Gurobi explicitly does not promise. An independent
checker of the solver's own answer, shipped with the solver, which none of the
others ship — SCIP ships `viprchk`, but that verifies a certificate SCIP
emits, which is a different object. A budget counted in reproducible work
units rather than seconds. All three come from the same decision, and it is
the project's actual distinguishing feature.

**And one thing this page claimed and had wrong until 2026-09-04.** The IIS
was listed as Gurobi's alone among the open solvers. HiGHS and SCIP both have
one, both before this page was written. It is off the distinctive list.

**One place where JAOS is behind where it thought it was ahead.** Verification
is JAOS's own subject, and SoPlex and SCIP are further along it: exact rational
LP solutions, exact MILP solving with no tolerances, and certificates an
external checker verifies. JAOS's checker is a good floating-point checker. It
is not a proof. `SPECS.md` lists exact rational verification as partial — the
arithmetic is here since D266 and the verifier is not — and this page says
what it is missing against.

**The current milestone will barely move this page, and that is by design.**
M2 is about speed, not features. Of everything on this page, only the presolve
and postsolve rows change when M2 closes, and they are at ◐ already rather
than at ○. Nor do they reach ●: the families left are deferred or refused with
a measurement — duplicate rows and columns and dominated columns deferred
(D101, re-tested at D242), bound tightening refused (D97), dual fixing
measured and refused (D246).
The measured gap at rung P0 is 3.60x HiGHS, 1.12x SoPlex and 2.96x Clp
(`bench/compare/results/P0.txt`, 2026-08-30), and closing it moves no cell
here at all. P0 is the rung to read: T0 was taken before JAOS had a
presolve, so against a presolving JAOS it puts presolve on one side only.

That is the answer to "is the plan improving the features as phases close": for
this milestone, almost not. If the feature matrix is the thing that should be
improving, the roadmap after M2 has to say so, because M2's own success
criterion is a time ratio and nothing else.

**What would move the most cells for the least work**, if features rather than
speed became the goal: Python bindings, then sensitivity and ranging. The three
writer rows were the other half of that answer and they landed on 2026-08-31.
That did not close section 7: Read LP and Write LP are both still ◐, on the
dialect's own limits. Neither of the two left is on the speed path. The
barrier method and the MIP section are large pieces of work and are correctly
not scheduled yet.

**That answer was taken up and all three cells moved.**
Compressed input is present (D240) and so is Python (D243), both under the
premise that keeps every dependency out: the inflate is written here and the
binding is ctypes over the standard library. **Sensitivity and ranging landed
on 2026-09-03 (D258)**, and it was not the small job this paragraph implied — it
needs the basis and a factorization of it. The published basis has the
promised count on every gate solve since D257, and D258 ranges over that
basis refactored on the model as loaded, so the presolve half this paragraph
feared does not exist.

---

## Sources

Cells for HiGHS, SoPlex, Clp, SCIP, Gurobi and Hexaly were taken from public
documentation on the date above. JAOS's own cells come from `SPECS.md`,
`DECISIONS.md` and the measured results in `bench/`.

- HiGHS solver capabilities and parallelism: <https://ergo-code.github.io/HiGHS/dev/solvers/> and <https://ergo-code.github.io/HiGHS/stable/parallel/>
- HiGHS 2026 development, QP and GPU PDLP: <https://highs.dev/assets/HiGHS_Newsletter_26_0.pdf>
- Gurobi 13.0 problem classes and new methods: <https://www.gurobi.com/resources/reports/what-s-new-in-gurobi-13-0>
- Gurobi determinism across machines: <https://support.gurobi.com/hc/en-us/articles/360031636051-Is-Gurobi-deterministic> and <https://support.gurobi.com/hc/en-us/articles/360045849232-Why-does-Gurobi-perform-differently-on-different-machines>. The explicit negative is stated by Gurobi staff here — "running Gurobi on different hardware may lead to different optimal solutions being returned", "deterministic behavior is only guaranteed when repeating runs on an identical setup": <https://support.gurobi.com/hc/en-us/community/posts/23910882878993-Deterministic-behaviour-in-different-machines>
- HiGHS IIS options (`iis_strategy`, `iis_time_limit`): <https://ergo-code.github.io/HiGHS/stable/options/definitions/>
- HiGHS first-order solvers and GPU (cuPDLP-C, HiPDLP): <https://ergo-code.github.io/HiGHS/dev/solvers/> and the 1.15.0 release notes <https://github.com/ERGO-Code/HiGHS/releases/tag/v1.15.0>
- Gurobi PDHG and GPU: <https://www.gurobi.com/product/whats-new>, <https://docs.gurobi.com/projects/optimizer/en/current/reference/parameters.html> and <https://support.gurobi.com/hc/en-us/articles/43498824105873-Installing-and-Running-GPU-enabled-Gurobi>
- SoPlex reads and writes LP format: its own description, "a standalone solver reading MPS or LP format files via a command line interface", plus the `--writefile` option — <https://github.com/scipopt/soplex> and <https://soplex.zib.de/doc/html/FAQ.php>. zib.de returned HTTP 429 on 2026-09-04; the claim was taken from SoPlex's own repository description and release notes, and is worth one more pass when the site is reachable
- Release histories used for the version line: <https://github.com/ERGO-Code/HiGHS/releases>, <https://github.com/scipopt/soplex/releases>, <https://github.com/coin-or/Clp/releases>, <https://www.zib.de/news/scip-optimization-suite-1000-released>, <https://support.gurobi.com/hc/en-us/articles/360048138771-Gurobi-release-and-support-history>, <https://www.hexaly.com/announcements/hexaly-optimizer-15-0>
- SoPlex as an exact LP solver: <https://soplex.zib.de/doc/html/EXACT.php>
- SCIP Optimization Suite 10.0, exact solving and certificates: <https://arxiv.org/pdf/2511.18580>
- VIPR certificate format: <https://github.com/scipopt/vipr>
- Hexaly Optimizer model types: <https://www.hexaly.com/hexaly-optimizer> and <https://www.hexaly.com/docs/last/modelingprinciples/index.html>

No solver's source code was read to produce this page. That rule (D2, D11, D12,
D15) applies to implementation, and it is respected here: everything above comes
from documentation a user can read.
