# Context

Running notes from the DOC-typed sources, keyed by topic. Six documents:
`docs/tolerances.md` and `docs/work-units.md` and `docs/scaling.md` and
`docs/format-support.md` (precedence 3), `bench/README.md` and
`bench/compare/README.md` (precedence 4), `README.md` (precedence 5).

The parts of these documents that are hard constraints rather than background
are additionally carried in `constraints.md`, with the same source paths. What
is here is the reasoning and the measurement behind them, which is what a
downstream reader needs in order not to re-derive a closed question.

Source paths are relative to the repository root.

---

## Topic: why the gate is judged twice
- source: bench/README.md
- The gate is all-or-nothing and reported `NOT MET` for almost the whole of M1, which makes it useless for the question actually asked of every change on the way — *did this make anything worse?* A run that fixed one instance and broke two scored exactly like the run before it, and the summary counts can come out identical when gains and losses cancel.
- "That is not hypothetical. It is how ten commits reached `main` in August 2026 carrying a wrong answer on `pilot-we`, a checker rejection on `pilotnov` and a seventy-sevenfold slowdown on `grow22`, under a summary line that never moved."
- All three sets now report `PASS`, "which does not retire the problem — it is the state in which the summary line is *guaranteed* to say nothing."
- A baseline that only ever tightens is one nobody remembers to loosen, so improvements are printed too.

## Topic: what the seven non-judging record columns are for
- source: bench/README.md
- `rowrel` is the worst row residue as a fraction of what that row carries; D24 refused to let the primal test become relative and keeps the measurement here instead.
- `Q` and `N` are the two sums the gap is the difference of, so a small gap can be told apart from two large halves cancelling. On 22 of the 94 instances that reach an optimum, `Q` is more than twice `|Q − N|` — the count was 35 of 93 when first measured and falls as the answers get more accurate.
- `drop` is the largest multiplier whose term the duality identity could not take; `cert` is whether any were dropped. 54 of 110 accepted answers are now certified, against 12 before the checker began bounding unbounded variables by what the rows imply, and the largest term the identity still cannot take is 3e-08.
- `rsub` is that bound as a fraction of the objective: 6.9e-05 where `pilot` is right and 5.02e-03 where it is wrong.
- `sub` is a certified lower bound on suboptimality and `rays` counts directions that could not be quantified; `sub` is sound and, on this evidence, uninformative.

## Topic: instances, emps, and why Koch rather than netlib
- source: bench/README.md
- The standard set is Koch's plain-MPS mirror, published with ZIB-Report 03-05 — the instances and the reference optima come from one source rather than two that have to be reconciled.
- Kennington and the infeasible set are served packed, so `fetch.sh` expands them with netlib's own `emps`: downloaded, verified against a pinned sha256, built into a temporary directory, never stored in the repository. `emps.c` carries no licence, no copyright notice and no public-domain declaration, so redistributing it is not something an Apache-2.0 project can do cleanly; using it as a dev-time tool is.
- Koch recomputed netlib's MINOS 5.3 optima in exact rational arithmetic and found some published ones wrong. The two sources disagree by more than the gate's tolerance on eight instances: `80bau3b`, `ganges`, `greenbea`, `greenbeb`, `nesm`, `pilot`, `pilot-we`, `scrs8`.
- `maros-r7` and `pilot87` used to fall back to the netlib readme because Koch's exact rationals were published at a path that no longer resolves. The report's PostScript recovers them: `koch-verify.py` reproduces 82 references exactly with none in disagreement, eighty of which were pinned independently beforehand — "which is what validates the decoding rather than assuming it".
- `pilot87` was previously judged against a reference 1.26e-6 outside tolerance of the exact optimum. No verdict moves; what changes is that the gate is now honest about what it measures against.
- Cross-checks run once on the way in: Koch's row count is the canonical netlib count minus the objective row on every instance but `boeing1`, and his column counts match on all of them.

## Topic: what `make warm` measures and what it skips
- source: bench/README.md
- The gate solves each instance once from a fresh load, which is precisely the case warm re-solve does not touch. So the gate can prove warm starting broke nothing and can never say what it is worth.
- A branch rather than an arbitrary nudge, "because its size comes from the model's own numbers and not from a constant chosen here, and because it is the workload phase 7 will run millions of times".
- Two things make the ratios mean anything: the cold number is checked against the gate's own iteration counts, and the anchor objective is kept so a branch that cut nothing off would show.
- It skips 2 instances of the standard set and 5 of Kennington including all four `ken-*` — models whose optimal values land on integers, so there is no fractional column to branch on. "A skip is not evidence about warm starting in either direction, and the count is printed rather than averaged over."

## Topic: the comparison harness's repeatability and its two failures
- source: bench/compare/README.md
- Measured repeatability is 1.4%, from the best control available: JAOS is byte-identical at every rung, so its own cross-rung ratio is a direct reading of the machine — 1.007x, 1.014x and 1.012x with iterations exactly 1.000x. Any claim below that is a claim about the machine.
- Read a rung difference against the competitor itself, not through JAOS: a ratio of two JAOS-versus-them ratios says the same thing with two extra sources of noise in it.
- Equal settings were checked to mean equal answers, because they need not. Against Koch's reference with full-precision output: `25fv47` JAOS 15.0 correct digits against HiGHS 14.9, `bandm` 14.8 against 14.8, `capri` 15.8 against ~16. The first attempt at that check said JAOS delivered 3.47 digits more and was measuring `printf` — HiGHS prints eleven significant digits and SoPlex nine.
- Why each competitor: HiGHS because its dual simplex is Huangfu and Hall's, reference [10] in JAOS's own bibliography; SoPlex because it is what SCIP runs underneath; Clp as "the veteran. A third reading, and the one that shows whether the other two agree by coincidence." SCIP is pinned from the start for M3/M4 and not run until then. GLPK is deliberately absent — "it would be a floor rather than a target".
- Running another solver as a benchmark competitor is not what D12 forbids: that rule is about writing JAOS's code from someone else's. No competitor's source is read.

## Topic: what the tolerance freeze is worth
- source: docs/tolerances.md
- Eight instances were refused at some point and every one closed as a defect with a mechanism, not by moving a number: `pilot-ja` (D21), `finnis` (D23), `nesm` (D25), `grow15` (D26), `etamacro` (D27), `greenbea` (D28), `pilot` (D29), `pilot87` (D30).
- "A tolerance that survived eight opportunities to be blamed and was never the culprit is a number with evidence behind it."
- `etamacro` is the sharpest case because it genuinely was a question about a tolerance's *space* — 4.89e-8 scaled and 1.56e-6 published. The answer was not to change the tolerance or to pick a space but to test a quantity that has neither.
- `pilot` was the one case where a tolerance was genuinely the question, and it was the primal one; the row was real and it was the answer that was wrong. D29's refinement took the residue from 1.73e-6 to 6.73e-13 on a solve that came out cheaper.

## Topic: the two repairs the checker's dual objective must not use
- source: docs/tolerances.md
- Contributing `w · v` makes the term cancel, so on a model whose multipliers all fall under `tol` the gap is identically zero for every feasible point and the checker certifies the whole polytope.
- Choosing the bound nearest `v`, which is what HiGHS does for its own diagnostic, produces negative terms that offset real residuals elsewhere in the model, and computes `(−inf + inf) / 2` on a free variable.
- Both look reasonable and both are recorded so they are not proposed again.

## Topic: what is outside the work budget
- source: docs/work-units.md
- Model loading is not charged, deliberately: the budget is a solve budget.
- Scaling is not charged either, "which is a smaller and less deliberate statement" — the Curtis-Reid conjugate-gradient pass is real work that no unit currently counts. "It is stated here because it is true, not because it was decided."
- Neither pricing form bills its own sweep over the variables, and that predates D40.
- Pricing does bill its walk over the pricing row: the row-wise pass charges `touched + nrow`, and on the Kennington set that single charge is 27% of everything billed.
- Ending a solve is the largest single charge most solves make outside the iterations themselves — a three-row test model went from 4411 units to 8517 when D20's verification refresh was introduced.

## Topic: the work-unit attribution table, and that it is stale by construction
- source: docs/work-units.md
- Shares as of D32: pricing row and ratio test 53.09%, dual update and steepest-edge weights 27.52%, the two FTRANs 6.80%, the row scan 5.62%, refactorization and refreshes 5.07%, the basis update 1.79%, everything outside the solve loop 0.11%.
- The document states these predate D40 and D41: "The ranking has certainly moved; the figures are left as measured rather than rescaled by arithmetic, and the next attribution run replaces them."
- The basis update is 1.8% of an iteration, and the non-update work runs from 4.4x the update's cost on the smallest model to 1450x on the largest.

## Topic: why the scaling factors are powers of two
- source: docs/scaling.md
- Multiplying a double by a power of two changes only its exponent field, leaving the mantissa bit-for-bit intact, so scaling cannot introduce rounding error of its own. "A factor of `1/3.0` would improve the spread on paper and corrupt the data slightly in practice."
- The Curtis-Reid normal equations are singular — adding `k` to every `r_i` while subtracting it from every `c_j` changes nothing — and consistent, so CG from a zero start behaves. Tests therefore assert scaled magnitudes, never individual factors.
- Empty rows and columns carry no information; their factor stays 1.

## Topic: an open item that lives only in a DOC
- source: docs/scaling.md
- "## What is not settled yet — Which mode is the better default across Netlib is a question for the campaign, decided by measurement rather than by preference. Both modes exist so the comparison can be run."
- Curtis-Reid is the current default. No entry in `DECISIONS.md` closes the question of which mode is the better default, and `PLAN.md` does not carry it among its open questions. Recorded here so it is not lost; see INFO in `INGEST-CONFLICTS.md`.

## Topic: the objective constant, and why the reader is not the place to fix it
- source: docs/format-support.md
- An `RHS` entry on the objective row sets the objective constant to the negated value, per classic MPS convention and matching CPLEX's documented behaviour.
- The published Netlib reference optima — both the netlib readme's MINOS values and Koch's exact ones — report the objective without it, so on `e226` (constant `7.113`) a correct JAOS answer differs from both published values by exactly that amount. `grow7`, `grow15` and `grow22` carry the same kind of entry with a value of zero.
- "It is deliberately not handled by making the reader drop the constant: that would break every model whose author meant it, to agree with two reference sets that predate the convention."

## Topic: the public framing of the project
- source: README.md
- Version 0.1.0-dev. "It reads an LP from disk, solves it with a revised dual simplex, and proves the answer right. It does that correctly on all 139 Netlib reference instances."
- Against the field with presolve off and the dual forced on both sides: 3.8x slower than HiGHS and 1.4x slower than SoPlex, on 0.70x SoPlex's iteration count. (Rounded from the D83 figures carried in `SPECS.md`.)
- What it does not do: no way to write a file, or to call it from anything but C; no presolve, no primal simplex, no barrier, no MILP. "Forty-one public functions, and the seven that configure anything set the contract ... never the method."
- Build: `make` (release static library), `make test`, `make sanitize`, `make netlib`, `make pgo`, plus `make netlib-kennington` and `make netlib-infeas`. GCC 14 minimum, Linux only, WSL on Windows.

## Topic: build flags as the README states them to a consumer
- source: README.md
- `make` builds `-O3 -flto -g -DNDEBUG`. `-O3` over `-O2` 1.0055x (noise), `-flto` 1.0330x (the only flag that does anything), `-march=native` 1.0072x (noise, and not portable), PGO 1.1122x (D62).
- `make pgo` is worth three times every flag put together and is not what `make` does, because it takes minutes and cannot run before the instances are fetched — "a library that will not build without fetching 139 models from netlib is a library nobody can package". `make pgo PGO_LOAD="..."` uses a subset.
- `NATIVE=1` exists "and you probably do not want it": the binary dies with an illegal instruction on any CPU older than the one that built it, which makes `libjaos.a` undistributable.
- `LTO=0` drops `-flto` for a toolchain with no linker plugin. The archive is built with `gcc-ar`, not `ar`.
- `-g` stays in the shipping build: "it costs nothing at run time, it is what a profiler reads, and profiling the build that ships is how four of this milestone's entries were found."
- `EXTRA_CFLAGS` exists for one job — sweeping a method constant over a range without editing the source between runs. **"Note that `make` cannot see a flag change, so a sweep must `make clean` between settings or it measures one binary several times."**

## Topic: the document map the project maintains
- source: README.md
- `SPECS.md` what JAOS is built to be; `PLAN.md` what is open; `CHANGELOG.md` what landed and what it cost; `DECISIONS.md` why, with the measurement that closed each; `bench/README.md` the acceptance gate; `bench/compare/README.md` the comparison method.
- "The design is written down. Do not reconstruct it from the code."
- `CHANGELOG.md` is deliberately not in the ingest set: the manifest records that it is history rather than intent, and that this project's own rule is that the changelog is not a decision record.

## Topic: methods PLAN.md marks as worth keeping
- source: PLAN.md
- Attribute by source line, not by named phase — one edit to one inline function routes every work unit through its `__FILE__` and `__LINE__`.
- Make the attribution validate itself: per-line sums must reconstruct each solve's total and the totals must reconstruct the committed baseline, both checked before the numbers are read.
- Re-attribute after every entry that lands. "A ranking three changes stale describes a solver that no longer exists."
- Sweep the trajectory, not just the instances. "It costs minutes with the parallel runner and it has found three defects that 139 instances at one setting did not."
- Report a geometric mean of per-instance ratios, not a sum over the set.
- A green result is not a proof. When changing a checker or a predicate, build the case it must reject and confirm that it does.
- Measure before repairing. "Every failure in this project that looked like a tolerance turned out to be something else."
- Recorded here rather than in `requirements.md` because they are working rules, not deliverables.

## Topic: constants PLAN.md marks settled — do not re-derive
- source: PLAN.md
- `REFACTOR_EVERY` = 64 (D39); `DSE_DRIFT` = 10 (D63); `PIVOT_SEARCH_LIMIT` = 4 (D46); `SPARSE_ALPHA_DEN` = 4 and `SPARSE_RHO_DEN` = 4 (D40, D41, D43, D61); `SPARSE_COL_DEN` = 8 (D45).
- Refused and recorded so they are not re-proposed: restarting the weights to the exact one rather than 1.0 (D63); a crash basis; filtering basic columns out of the pricing sweep (D35); the quadratic slot detachment in a basis update; caching `col_max_abs` (D46); a scatter-form BTRAN (D36); dropping the loan the re-entry's clean-up takes (D74); a certified suboptimality from moving one column alone, as a *verdict* (D73); `restrict` on the LU kernel pointers (D76); partial pricing on the leaving-row sweep (D82); multiple pricing (D84); judging the dual breach in the published space *instead of* the scaled one (D92); forcing the two dense sweeps' helpers inline (D61).
- Taken and recorded as repairing nothing: `settled_dual_violation` reading the published space — 94 of 94 bit-identical, a coherence fix (D92).
