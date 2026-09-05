# Changelog

All notable changes to JAOS. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Entries say what changed and what it cost. The reasoning lives where it
belongs: `DECISIONS.md` for closed decisions, `TODO.md` for what is still
open, `bench/README.md` for the gate, and the commit each entry came from.

## [Unreleased]

### Added

- **Rows, columns and the objective have names.** Both readers keep the
  file's, the setters take one, and a row nobody named is called by its
  position, `R<i+1>`, `C<j+1>`, `COST`. Both writers print them, the
  solution file carries and checks them, the CLI prints `cost X1` instead
  of `cost 0`, and `jaos_col_index` resolves one. The LP scanner takes the
  CPLEX symbol set in a name now, and the LP writer refuses a name it
  cannot read back, pointing at MPS (D284, `bench/measurements/02-188/`).

- **The solution file carries a certificate.** An infeasible or unbounded
  solve writes its Farkas multipliers or its ray as `ray` records under
  `status infeasible` or `status unbounded`; `jaos_read_certificate` reads
  them back, `jaos_solution_file_status` says which kind a file holds, and
  `jaos check` prints the kind first and judges a certificate from the
  model alone, exit 0 when certified (D285).

- **A command-line tool.** `make cli` builds `build/cli/jaos` from
  `cli/jaos.c`, over the public header only. `jaos solve FILE` prints one
  fact per line and every line but `time` is byte-identical between runs;
  the exit code is the verdict, 0 optimal to 5 usage. `jaos convert IN OUT`
  moves between MPS and LP; `check`, `iis`, `verify` and `ranging` expose
  the checker, the IIS, the exact proof and the ranging tables, one field
  per line. `tests/cli.sh` runs under `make test`, 123 checks
  (`docs/cli.md`).

- **The objective's sense and constant read back and change.**
  `jaos_objective_sense`, `jaos_objective_offset` and their two setters. A
  model read from a file could not say whether it was a maximum, and only
  `jaos_load_lp` could set either, so the Python `Problem` rebuilt the model
  to flip the sense and solved cold; it goes through the setters and
  re-solves warm now (D283).

- **The matrix reads back.** `jaos_col_entries`, `jaos_row_entries` and
  `jaos_coefficient`: one column, one row or one entry, sorted, no explicit
  zeros. A row comes off the row-wise mirror the solve keeps, so the read
  derives nothing new. On `Model` in Python as `col_entries`, `row_entries`
  and `coefficient` (D283).

- **`jaos_read_solution` reads back the solution file JAOS writes.** The
  format had a writer and no reader -- `docs/format-support.md` said so in
  as many words -- so the one file format this library invented was the one
  it could not read. The model decides the shape: the counts must match and
  every record's name must be the one this library generates for that index,
  so a file from another model is refused rather than read into the wrong
  arrays. Only `status optimal` and only finite numbers, because only those
  are ever written. Every output is optional, and it installs nothing: a
  warm start from a file is a read and then a `jaos_set_basis`. Reached from
  Python on `Model` and `Problem`. Fourteen refusal classes are pinned, each
  with its own message (D282, `bench/measurements/02-187/`).

- **The MPS reader takes an `OBJNAME` section.** It says which free row is
  the objective in a file that has more than one, and the reader refused the
  whole file for it before. The same two spellings `OBJSENSE` has; every
  other `N` row stays an ordinary free row, including the ones before the
  named one; without the section the first `N` row is the objective, as
  before. Four refusals by name, and the missing-name one is reported at
  `COLUMNS`, the first line at which every row is known. No gate instance
  uses the section, so all three sets are byte-identical and the evidence is
  the validation: the tests are watched failing both against a reader that
  refuses the section and against one that parses it and ignores it (D280,
  `bench/measurements/02-185/`).

- **Exact evaluation of a claimed point**, `jm_exact_evaluate`: the
  objective and the worst row and column violation with no rounding in the
  walk and one at the end. It runs on a dyadic `m * 2^e` rather than a
  general rational, so a term is a shift and an addition instead of a gcd
  and two divisions, and `ken-13` costs 0.02 s. Measured against the
  checker over the gate: every objective agrees to the ulp, 75 of 110
  worst-row violations differ by 1e-11 or less, and no verdict moves, so
  nothing is exposed and the condition that would change that is in
  `bench/refusals.txt` (D267, `bench/measurements/02-173/`).

- **Exact integer and rational arithmetic**, in `src/exact.c`, for verifying
  an answer rather than computing one. Big integers on 32-bit limbs with no
  allocation and no external library, and rationals kept in lowest terms.
  Every finite double converts exactly, so a model's data carries with no
  rounding; an operation that runs out of limbs says so instead of wrapping.
  Internal for now: the verifier that will read it is still missing, and
  `SPECS.md` section 5 moves from missing to partial (D266).

- **An irreducible infeasible subsystem.** `jaos_iis` names, for an
  INFEASIBLE answer, the bound sides that are infeasible on their own and
  all needed: Chinneck and Dravnieks's sensitivity filter over the
  published certificate, then their deletion filter, one warm re-solve per
  candidate on a private zero-cost copy, reached from Python on `Model`
  and `Problem`. The solver is the oracle on the 29 reference infeasibles:
  28 pass, and `cplex2`, infeasible by less than the tolerance, keeps three
  of its 232 members that a cold re-solve does not need (D264,
  `bench/measurements/02-171/`).

- **Sensitivity and ranging.** `jaos_cost_ranging`, `jaos_rhs_ranging`
  and `jaos_bound_ranging` report, for every cost and every row and column
  bound, the interval it may take with the basis behind the last optimum
  staying optimal: the textbook ratio tests on that basis refactored over
  the model as loaded, reached from Python on `Model` and `Problem`. The
  solver is the oracle in the tests (D258). With it, a column the caller
  fixed is published at the bound its reduced cost points into, in the
  simplex and at postsolve, so a warm re-solve that opens the other side
  costs nothing. The basis is factored with the solve's own power-of-two
  scaling, which is what the pilot family's conditioning needed (D260,
  `bench/measurements/02-167/`).

- **An exact verifier's budget, answered before the verifier exists.**
  Bareiss's entries are minors of the basis, so the Hadamard bound on
  `log2 |det B|` is one pass over the basis columns in floating point, before
  a limb is allocated, and it is the refusal test and the capacity test in
  one. **97 of 110 gate bases fit in the 4096 bits `src/exact.c` already
  has**, so a verifier with no block-structure work is buildable on 88% of
  the gate. The thirteen that do not fit are a count problem rather than a
  magnitude one — every `ken` and `pds` among them has a largest column of
  0.79 bits and `ken-18` has 88775 basic structurals — which is what block
  triangular form addresses and what raising the limb count would only buy
  at a quadratic cost (D271, `bench/measurements/02-176/`).

- **Block triangular form of the published basis**, a maximum transversal
  (Duff 1981) then the strongly connected components (Pothen and Fan 1990),
  both deterministic. It takes D271's 97 of 110 to **110 of 110**, and
  `ken-11` and `ken-13` come out fully triangular, every block a single row.
  The block work is free: `ken-13` is 59.57 s of solve and 0.00 s from the
  read through the basis build (D272, `bench/measurements/02-177/`).

- **An exact verifier for a published basis**, `jaos_verify`. It proves, with
  no tolerance anywhere, that the basis the last solve published certifies its
  answer — or names the row or column that breaks it and by how much, or
  refuses with the bits it would have needed. The basis is rebuilt over the
  integers, split into blocks by a maximum transversal and the strongly
  connected components, and each block eliminated by Bareiss's fraction-free
  method. **30 of the 110 gate bases prove, 74 are refused a priori, 6 are
  disproved** (D274, `bench/measurements/02-179/`). The six are the result and
  not a defect: five have one nonbasic reduced cost of exactly the wrong sign
  and `sierra` one basic value exactly outside its bound, all five orders or
  more below the solver's own tolerances, so the answers are right and the
  bases are optimal only to a tolerance. Cost follows the largest block:
  `pds-02` is 2953 rows at 0.02 s, `sc205` is 205 rows with a block of 184 at
  7.34 s. The review of the diff found eleven defects, two of them wrong
  answers.

- **`verify()` on the Python layer**, on both `Model` and `Problem`, with
  `Proof`, `ProofStage` and `VerifyReport`. The first of its four tests pins
  the ctypes layout against the C struct: a field out of place reads its
  neighbour and every assertion after it is about the wrong bytes.

### Changed

- **The LP reader takes a bound written value-first either way round**:
  `10 >= x` and `8 >= y >= 2`, alongside the `l <= x <= u` forms it already
  took. The first operator says which side the leading value is and the
  second must point the same way, so `3 <= w >= 8` is refused at the first
  operator's line, in the same words a ranged constraint gets. The writer
  emits only `l <= x <= u`, so no round trip takes the new path and all
  three gate sets are byte-identical (D281, `bench/measurements/02-186/`).

- **`make record-check` refuses a measurement README whose heading names
  another directory.** A measurement directory is one decision's evidence,
  and nothing said so: on 2026-09-05 a session took an id that was already
  used, wrote a second decision's readings into it and overwrote the first
  README, with no error anywhere. Judged on the 123 of 179 directories whose
  heading already names an id; it fires 0 times today, so the canary beside
  it is what shows it works (D279, `bench/measurements/02-184/`). The rule
  D269 actually asked for — a reading committed after the README that cites
  it — is **refused** on its own firing population: 3 of 227 pairs survive
  every filter and none of the three is a defect (`bench/refusals.txt`).

- **The LP reader folds a constant inside a constraint** into the
  right-hand side with its sign flipped, so `3x + 5 <= 10` is the row
  `3x <= 5`; on a two-sided row both ends shift by it. It used to refuse
  one and tell the caller to do the subtraction. A signed number at the
  head of a constraint is still a left-hand bound where a relation follows
  it, so `3 x + y >= 2` keeps its `3` as a coefficient. LP round-trip
  coverage over the 139 gate instances is unchanged at 138 / 1 refused /
  0 differ, with the same single free row as its cause, and all three gate
  sets are byte-identical (D278, `bench/measurements/02-183/`).

- **The checker computes every figure in `double` now.** D270 converted its
  primal walk; this finishes the file -- the dual walk's objective and both
  gap halves, the reduced cost, `certified_step`, `implied_bounds`'s two
  row-range sums, and both certificate checkers, each a Neumaier sum over
  Dekker's exact products. `long double` is 64 mantissa bits on x86-64 and
  113 on aarch64 and these figures are printed into `bench/results/`, so the
  checker was breaking the cross-machine claim the solver refuses that type
  to keep. **No verdict moves on any of the 139 gate instances** and every
  solution digest is byte-identical; twelve netlib lines move a checker
  figure in its third significant digit (D277,
  `bench/measurements/02-182/`).

- **The LP writer takes a row with no coefficients**, as a zero term against
  column 0, which the reader drops on the way back in. **138 of the 139 gate
  instances round-trip now, 1 is refused, 0 differ**, against 104 and 35
  before. The refusal it replaces was right that LP has no form for an empty
  constraint body and wrong that this made the row unwritable: a term whose
  coefficient is zero is an ordinary term, and the writer already emitted
  those in the objective. The one still refused is a free row, which LP
  genuinely cannot say (D276, `bench/measurements/02-181/`).

- **`JM_EXACT_LIMBS` is swept, and the verifier's refusal is the constant
  rather than the mathematics.** Over the standard 94, doubling it four times
  takes the proved count from 28 to 57 and the refused count from 60 to 24,
  so 70 of 94 get a verdict at 32768 bits. **The value stays at 128 for the
  seconds**: the last doubling costs 2500 s against 142, a multiply being
  quadratic in limbs. The sweep also showed the two capacity constants
  pulling against each other — a wider limb count shrinks the block
  `VERIFY_BLOCK_BYTES` admits, and `degen3` goes from BROKEN to refused
  between 128 and 256 for that reason alone — and that **thirteen of the 94
  netlib bases are exactly infeasible**, by 1.7e-18 to 1.1e-13, which is four
  orders below the solver's own tolerances and leaves every published answer
  right (D275, `bench/measurements/02-180/`).

- **The verifier's budget was measured on a matrix it cannot hold, twice.**
  D271 and D272 both bounded `log2 |det B|` by the basis entries as read;
  Bareiss needs those entries to be integers, and scaling a row to make one
  costs 53 to 72 bits. Measured over the same 110 bases: **86 fit in 4096
  bits, not 97 and not 110**, and 85 also fit a dense elimination's memory
  and time. `pilot87` is out by a factor of 47 and is not reachable by
  widening the capacity constant — 2556 limbs and a 21 GiB block. The 19
  instances whose entries are already integers, every `ken` and every `pds`
  among them, keep the earlier figures unchanged (D273,
  `bench/measurements/02-178/`).

### Fixed

- **A gap term that overflows no longer poisons the half it lands in.**
  Where `w * (v - bound)` overflows, the correction for what the subtraction
  lost overflowed the other way, so `+inf` and `-inf` met in one accumulator
  and made a NaN: an abort with asserts on, and a NaN in `gap_positive` and
  `relative_suboptimality` without them. Found by review rather than by the
  139-instance run, which is nowhere near 1e300. Two smaller ones with it:
  `jaos_check_ray`'s row movement was an uncompensated sum that decides
  `certified`, and `implied_bounds` could write an infinity as a column
  bound, leaving a box no point is inside (D277).

- **The checker broke the cross-machine claim the solver refuses `long double`
  to keep.** `long double` is 64 mantissa bits on x86-64 and 113 on aarch64,
  and `src/check.c`'s figures are printed into `bench/results/`, one of them
  into the baselines. D162 and D168 both refuse the type in the solver for
  exactly that reason and the checker was never revisited. Its **primal**
  walk is a Neumaier sum over Dekker's exact products in `double` now, which
  is portable and holds more: against exact arithmetic the worst-row figures
  that disagree go from 75 of 110 to 37, with none newly disagreeing. The
  instances with the largest disagreements do not move, because the figure is
  a subtraction that cancels and no accumulator reaches it. **The dual walk
  is still `long double`** and is named as open (D270,
  `bench/measurements/02-175/`).

- **Six of D264's numbers were from a run its own directory superseded eleven
  minutes later**, and one of them had been copied into six other files.
  `cplex2` keeps three of its **232** members a cold re-solve does not need,
  not "three of 338 sides", and they are column sides rather than row sides.
  The argument D264 makes reproduces at HEAD; the evidence file did not
  contain it, because the arm that answers OPTIMAL was added to the replay
  after its output was last written. `SPECS.md` also read 77/11/6 for the
  primal's reach where `bench/results/primal.txt` says 73/16/5, and listed a
  closed presolve item as open (D269, `bench/measurements/02-171/`).

- **`docs/feature-matrix.md` claimed the IIS was Gurobi's alone among the open
  solvers.** HiGHS ships `Highs::getIis` and SCIP 10.0 an IIS Finder, both
  before that page was written. Six rival cells corrected, eight prose
  statements repaired, and the version line moved from 0.1.1 to 0.2.0.

- **`docs/claims.txt`'s exact-arithmetic guard was blind to what landed.** Its
  pattern named `mpq_t`, `jaos_exact` and `jaos_verify`; the code that shipped
  is `jm_exact_evaluate` and `jm_dyadic_*`, so `make record-check` stayed green
  through D266 and D267. It guards the verifier now, and nine candidate names
  for one are shown to turn the check red
  (`bench/measurements/02-174/canary-claims.txt`).

- **Nine defects in the exact arithmetic, found by review and not by its own
  139-instance run.** Four could give a wrong answer, four were tests and
  comments claiming more than they checked, and one was the record.
  A refused walk published the objective and left
  `row_violation` at zero and `row_at` at -1, which reads as a clean point;
  it is built in a local and poisoned with NaN on failure now. Both
  conversions back to a double rounded twice for a subnormal result, wrong
  on 1.0% of them; they round at the 2^-1074 grid. `jm_dyadic_add`'s limb
  comment reasoned about a pair of doubles where the evaluator adds
  products, which span twice as many bits and can genuinely refuse. And
  `jm_dyadic_mul` added exponents unchecked, which is signed overflow after
  53 squarings. Five tests, each validated against the tree with its own
  repair removed. On the test side: a skip branch that never ran, a `-0.0`
  converted away before it was tested, a comparison that would answer
  without comparing at a swept-down `JM_EXACT_LIMBS`, and four measured
  numbers restated where D262 and the bench records own them
  (D268, `bench/measurements/02-174/`).

- **The record said `src/check.c` uses a compensated sum and it does not.**
  `act[i] += term` and `primal_obj += c_j x_j` are plain `long double`
  running sums; `split_term` splits the dual gap for D219 and the
  compensated accumulators D168 and D169 measured are in `src/simplex.c`.
  D267's verdict does not move, but the false sentence hid a middle option
  between the checker and exact arithmetic, which is open work now (D268).

- **The LP writer's coverage number had no owner.** `SPECS.md` cited a re-run
  that was never committed; `TODO.md` and `docs/format-support.md` still read
  102 of 139 round-tripping with two ranged refusals, and still said
  `jaos_read_lp` rejects a ranged constraint, which D239 changed. Measured at
  HEAD: 104 round-trip, 35 refused, 34 for an empty row and 1 free. No code
  changed (D265, `bench/measurements/02-172/`).

- **A cost-0 singleton column open below published minus infinity.** The
  presolve replay took the column's lower bound when the row asked nothing
  from below, and that bound can be -inf: an answer that said OPTIMAL with
  a NaN row activity. The branch now mirrors the finite upper end. Found by
  the IIS filter's zero-cost copies; no gate instance reaches it, every
  digest byte-identical (D264).

- **The independent checker's objective is exact on 110 of 110 too.** It
  was 109: `long double` cannot hold a binary64 product's 106 bits, and
  `finnis` carried 3.2e12 of `sum |c_j x_j|` in columns published on lent
  bounds, which put the checker 790 ulps out. With those retired the worst
  disagreement on either side is 0.493 ulp (`ship08l`). D173's refusal of
  `finnis` expired with its premise, the fourth time in this repository
  (D262, `bench/measurements/02-169/`).

- **A bound the solve lent itself is retired before the answer goes out.**
  A column left resting on one was published nonbasic on a bound the model
  has not got, at 1e10, and `c'x` then cancelled 1e10-magnitude terms down
  to the objective: `finnis` was wrong in its eighth significant figure and
  now lands on the netlib reference to 3e-8, with its checker row violation
  down from 8.4e-7 to 1.6e-13. Over 200000 small models the published
  answers carrying such a status go from 1717 to 0, and from 1701 to 0 with
  presolve compiled out. 138 of the 139 gate instances are byte-identical;
  `finnis` costs 3.96% more work (D261, `bench/measurements/02-168/`).

- **An inverted box is answered INFEASIBLE.** A lower bound above its
  upper by more than presolve's rounding window is refused at the solve's
  entry in every build; both builds had answered OPTIMAL on one, cold and
  warm, and the ranging oracle found it (D259).

- **The published basis has the promised count on every gate solve.**
  Every postsolve status is decided from the reduction's structure, never
  from an equality test on a value the replay rounded, and a value the
  structure puts on a bound is published as that bound. 46 wrong netlib
  solves become 0, D170's five reduced-cost sign breaches become 0, and
  the gate reading is in D257 (`bench/measurements/02-166/`).

### Added

- **The Python binding reaches the certificates.** `Model.certificate`,
  `unbounded_ray`, `check_certificate` and `check_ray`, with the two
  report tuples, and `certificate` / `unbounded_ray` on `Problem`, one
  entry per constraint or variable in the order they were added. The
  binding covers every call in `jaos.h` again; 68 tests.

- **A certificate crosses back through presolve.** A ray the simplex
  proved on a reduced model is lifted into the caller's rows or columns,
  and each presolve proof site seeds its own from the bound it refused;
  the one refusal left is an inverted box. Under the default build 28 of
  the 29 reference infeasibles certify at 1e-7 and `gran` at 1e-9, with
  every gate digest byte-identical (D256, `bench/measurements/02-165/`).

- **An unbounded answer can prove itself too.** `jaos_unbounded_ray`
  publishes the direction from the simplex's three ray-proof sites — the
  single column off its loan, D247's combined direction, the primal's
  unblocked column — and `jaos_check_ray` judges it from the model alone.
  Presolve-proved unboundedness publishes none, the same named gap as the
  Farkas side (D255).

- **An infeasible answer can prove itself.** `jaos_certificate` publishes
  the Farkas ray behind a simplex-proved INFEASIBLE and
  `jaos_check_certificate` judges it from the model alone, both halves of
  the gap published. All 29 reference infeasibles certify under the
  reference build; presolve-proved infeasibility publishes no ray yet
  (D254, `bench/measurements/02-164/`).

### Changed

- **BTRAN's L' pass walks its pattern, not every slot.** The reachability
  DFS the U' pass has had since D36, over a row structure of L built once
  per factorization. Every digest and iteration count on all 139 instances
  is byte-identical; Kennington reads 0.9862x in work and the standard set
  pays 1.0088x for the edge billing (D253, `bench/measurements/02-163/`).

- **A fixed column never joins the dual ratio test.** `admit_candidate`
  refuses `lo == up`, the rule the primal pricing already applied: it never
  limits the step, and its width-zero flip only toggled its published
  label. Netlib work geomean 0.9449x, Kennington 1.0050x; grow22 pays
  2.09x and stair's bound reads 2.12e-09, both named and accepted
  (D252, `bench/measurements/02-162/`).

### Fixed

- **`record_diff.py` watched a floor the runner dropped a week earlier.**
  Its RSUB mirror still said 1e-9 where `bench/run.c` moved to 1e-16 on
  2026-08-24, so it reported 2 of the 6 flags the runner printed on the
  same records. Both constants read 1e-16 now, validated by the two
  instruments printing the same six lines on D252's diff.

### Added

- **A ray that needs several columns at once is proved now.** When every
  single held column is blocked, the lent-bound verdict moves them all off
  their loans together at unit rate, one FTRAN, and declares UNBOUNDED on
  the same no-real-bound line as before; D241's named example answers from
  both methods where it refused. A subset or unequal-rate ray still reaches
  the refusal, which says both directions were tried. All three sets and the
  forced-primal record byte-identical (D247).

### Changed

- **`SETTLE_ROUNDS_PRIMAL` is 256, and 512 is refused on a byte-identical
  record.** The trace behind it: three of the seven round-exhausted
  re-entries are stuck flat — no budget reaches a frozen trajectory — and
  of the four still descending, 256 converts `woodw` and moves `stocfor3`
  to an honest overrun. The forced primal reads 77 ok / 11 overrun / 6
  disagreed; the gate cannot see the constant and the campaign confirms it
  (D251, `bench/measurements/02-161/`).

### Fixed

- **A budget expiring inside the settling re-entry publishes as the stop
  it is, not as a numerical error.** Seven of the forced primal's fourteen
  "not dual feasible" refusals were the harness's 10x work budget dying
  mid-re-entry; they read **overrun** honestly now (76 ok / 10 overrun / 8
  disagreed), and a caller's `jaos_set_work_limit` expiring there gets the
  resumable WORK_LIMIT verdict the header promises. Gate byte-identical on
  all three sets (D250, census in `bench/measurements/02-160/`).

- **A bound-flip walk that exhausts within the feasibility tolerance ends
  in a pivot now, not in a false INFEASIBLE.** Four lines at the walk's
  exit put its last retiree back as the blocker; every walk the branch
  does not end is byte-identical, and the gate confirms it — three sets
  `0 regressed` with `bench/results/` unchanged. `agg`'s forced primal
  goes DISAGREE → ok on the same objective as the dual, 76 of 94 now.
  Three repair shapes were measured; two are refused in the entry (D249).

### Changed

- **agg's re-entry failure is diagnosed: a one-ulp residue read as
  infeasibility.** The bound-flip walk absorbs the armed point's 57911
  violation to within 7.28e-12 and its strict `> 0.0` blocking test
  publishes that as INFEASIBLE at round 0. Deterministic on both verdict
  passes; two loan hypotheses and one census instrument refuted on the way
  (D248, `bench/measurements/02-159/`). The repair is open in `TODO.md`.

- **Dual fixing is measured, and refused.** The family counter's dual-fixing
  arm — its least validated — is calibrated now: a known answer of seven on
  `02-07/validate.c`'s model, and the armed historical defect reads eight.
  The validated shares are 0.67% of netlib's live columns and 1.09% of
  fome's against 02-154's 5% bar, so the seventh presolve family is not
  built (D246; the reopen script is in `bench/refusals.txt`).

## [0.2.0] — 2026-09-02

The reach release. JAOS reads gzip input through an inflate written here
(D240), reads and writes ranged LP rows (D239), and is usable from Python: a
ctypes binding over the whole C API with a modeling layer on top, standard
library only (D243). Underneath, the four answers that stopped measurably
short of the published optimum are gone (D184), the record is machine-checked
— `make test` fails when a document disagrees with the code — and a change is
judged on five metrics instead of three (D206, D225). A primal simplex exists
behind a development switch; a derived pricing rule for it was measured and
refused (D244, D245).

One honest number the other way: the competitive gap is wider than at 0.1.1
on all three solvers, because D184 bought correctness with work and nothing
has bought it back yet. `SPECS.md` section 8 carries the direction.

### Added

- **An LP can be written in Python now, not only loaded.** `Problem` takes
  variables, expressions and ordinary comparisons, builds `jaos_load_lp`'s
  arrays whole at solve time, and sends a later bound or cost change through
  the C setters instead, so the next solve resumes warm. The seven C calls
  the binding did not reach — add and delete of rows and columns,
  `jaos_set_basis`, `jaos_check_solution`, the progress callback — are bound.
  `make python-test` is 61 tests; three armed defect shapes each redden it
  (`bench/measurements/02-158/`).

### Changed

- **The re-entry's round backstop is per method now, and the forced primal
  goes from 61 of 94 agreeing with the dual to 75.** Fourteen of its thirty
  failures were the round budget and not a numerical problem: on `25fv47` all
  32 rounds run with the violation still falling, 784.9 to 10.8, and the
  objective descending throughout.

  **`SETTLE_ROUNDS` stays 32 and `SETTLE_ROUNDS_PRIMAL` is 128**, because the
  dual path does reach 32 — `wood1p` costs 1.49x work at the higher bound for
  a bit-identical answer, same digest and same basis. The gate's `0
  regressed` never said otherwise: its bar is 2.0x. The shipped path is
  byte-identical by construction (D245, `bench/measurements/02-157/`).

- **An approximate steepest-edge rule for the primal was derived here, built,
  swept and refused.** The maintainer chose deriving one over buying the
  paper Devex is in. The derivation stays in
  `docs/research/approximate-edge-pricing.md`; the code does not.

  It loses to Dantzig on the standard 94 at every setting swept. Maintaining
  the weights costs **10.9% work at the control, where no decision changes at
  all**, and only "never reset" ever earns that back. Every setting agrees
  with the dual on fewer instances than Dantzig, 61 down to 53, and
  `bench/primal.c` says agreement is the gate and speed only the report. The
  candidate is kept as measured so a retry starts from it (D244,
  `bench/measurements/02-156/`).

- **D101's reopen condition is executable instead of manual**, and `make
  refusals` runs it. The three deferred presolve families find **zero**
  removable rows and zero removable columns on all 15 plato instances, 1.06M
  live rows and 3.5M live columns. netlib is the control and reproduces
  D101's 151 removable rows exactly.

  Nothing was built. The bar for "non-trivial" is 5% of a set's live rows or
  columns and is argued rather than measured: a first draft used 1%, which
  would have reopened the question on netlib's own 0.913%. `plato-fome`
  reports 3934 dual-fixing candidates, a family nothing in the record has
  costed (D242, `bench/measurements/02-154/`).

### Added

- **JAOS is usable from Python.** `python/jaos.py` wraps `libjaos.so` with
  ctypes and the standard library, so it needs no compiler, no header and no
  package index — the same no-dependency rule the C library holds. `make
  shared` builds the library and `make python-test` runs 27 tests. `make
  test` does not, on purpose: five configurations must not depend on an
  interpreter.

  All 27 passed first time, so three defect shapes were armed and each
  reddens the tests meant to catch it. Two arms were wrong before they were
  right, and one of them found that the suite checked array lengths and never
  values (D243, `bench/measurements/02-155/`).

- **The primal simplex decides an unbounded ray it meets in phase 2**,
  instead of refusing there. The condition added is that no borrowed cost is
  outstanding; the rest of D19's proof was already standing at that line, and
  a second opinion written for it turned out to restate the first and was
  deleted.

  **0 firings in 102 phase-2 ratio tests on the standard set**, because the
  dual re-entry takes the model after about one phase-2 iteration. Two
  constructed models reach it and both go red without the verdict. A separate
  ray that moves two columns at once is still refused, and the refusal no
  longer claims the optimum is finite: capping that model gives exactly -2x
  the cap over nine orders of magnitude (D241, `bench/measurements/02-153/`).

- **Both readers take a gzip-compressed file.** `src/inflate.c` is a gzip and
  DEFLATE decoder written here, because JAOS links nothing but libc and libm.
  The choice is made on the file's first two bytes, so no new call and no new
  argument. Both trailer fields are checked, so a damaged instance is refused
  rather than solved as a different model.

  **369 comparisons against the real `gzip` at three levels, over 123 netlib
  and infeasible instances, all byte-identical**, plus 31 of the large
  Kennington and plato files. 13 unit tests cover the three DEFLATE block
  types, every optional header field, several members in one file and six
  rejections; the fuzzer's corpus now carries the `.gz` fixtures, so the
  decoder is mutated under ASan with everything else.

- **`jaos_read_lp` accepts a ranged constraint, and `jaos_write_lp` writes
  one.** `l <= expr <= u` and its mirror, both operators pointing the same
  way. A leading number is only the left bound when an operator follows it,
  so `3 x + y >= 2` still reads its 3 as a coefficient.

  **104 of the 139 gate instances round-trip through LP now, against 102**,
  and ranged refusals go from 2 to 0. `SPECS.md` said all three refusals
  would close at once with this change and that was wrong: the 1 free row and
  34 empty rows are not a reader problem — LP cannot write a constraint with
  no terms, and the two-sided form takes numbers rather than `inf` (D239).

- **Two singleton-row replay contracts are asserts: the row's coefficient is
  the divisor, and the bound it claims to own is one it induced.** Both hold
  on 94 standard and 29 infeasible instances, both are reached (70 and 61),
  and both are fired by a defect.

  **One contract on the debt list was not taken, because it cannot fail**: it
  proposes asserting the equality that defines the branch it sits inside. A
  proposed assert can be already implemented (D236), false (D235), or
  unfalsifiable; this is the third kind. The interior property also turns out
  to be guarded twice, so breaking one guard fires nothing — the same shape as
  D232's FORCING assert and D233's reenter clears (D238,
  `bench/measurements/02-151/`).

- **Scaling never moves a bound's finiteness, and a loan only ever replaced
  an infinity.** Two asserts at the start of a solve: `sx_init` checks
  `isfinite(s->lo[v])` against the model's for every variable, and
  `build_initial_basis` checks that a faked end holds the artificial value.
  The first is what makes every downstream `isfinite` test mean what it says
  — a scale factor reaching zero would turn a real bound into an absent one
  and the solve would stop enforcing it, with neither the checker nor a
  digest able to report why.

  Both hold on all 139 instances, and the inverted canaries show each is
  reached on 129. The canary is also what caught the arm stopping one step
  too early: the loan assert lives in `build_initial_basis`, which runs after
  `sx_init`, so the first run measured an assert no instance executed (D237,
  `bench/measurements/02-150/`).

- **Two more `presolve.c` contracts are asserts: boxes only narrow, and the
  reduced matrix never carries a row index of -1.** An inverted column box is
  legal input, so the first excludes those columns rather than claiming
  anything about them. Both are reached (inverted, they fire on 131 and 115
  of 139 instances), quiet on all 139, and fired by a realistic one-line
  defect — the three questions an assert has to answer, and the first batch
  here to answer all three (D236, `bench/measurements/02-149/`).

  **Five items on the assert-debt list turned out to be already
  implemented**: the `start_*_status` pair and
  `jm_model_publish_objective`'s precondition at D219, `amark` and the
  reduced model's non-aliasing at D223, and `pilot`'s 2.31e-05 at D184. None
  was a defect in the code; each was a statement that stayed as written while
  the code moved past it.

- **Four `presolve.c` contracts are asserts, and one of them was wrong until
  the gate population said so.** The round count never passes the structural
  backstop; `row_traffic` is a magnitude; an already-infinite row end is not
  subtracted from; and the empty row's bound-scale fallback is unreachable.

  The third was written as an equality first — a finite row end stays finite
  through the singleton-column shift — and that is false: `!free_col` means
  at least one column bound is finite, not both, so a column boxed at
  `[0, +inf)` turns a finite end infinite. It fired 58 times before anything
  was committed. The fourth needed the assert inverted rather than a defect,
  because an assert that is never evaluated is never violated; inverted it
  fires on 5 of 94, so the branch is reached and the comment D155 left behind
  is measured now (D235, `bench/measurements/02-148/`).

- **Four scratch contracts in `simplex.c` are asserts, and one of them
  covers a path nothing checked.** `apat` names every slot where `alpha` can
  be nonzero and `rpat` every slot where `rho` is — both as a count, because
  `jm_pattern_order` already guarantees the pattern is distinct and that is
  what lets a count decide membership without a marker array the function
  does not own. `c1` is all zero after its incremental clear.

  The fourth is `nbmark` matching `status`, asserted at `refresh`'s
  successful exit. D223 put that walk in `dual_ratio_test`, which is the dual
  path; `run_primal`, `run_primal_phase1` and `primal_cleanup` all reach
  `pivot()` without it, so the primal had been maintaining the bitmap with
  nothing checking it. Nothing was wrong — 139 instances on the dual and 33
  in both methods, zero assertion lines. Each assert is proven by a realistic
  one-line defect, and the two that print the same expression are told apart
  by the function name (D234, `bench/measurements/02-147/`).

- **`s->verified` has one writer and one reader, and the reader asserts what
  is true of it.** `set_verified` writes the flag and zeroes a debug-only
  pivot counter beside it; `verified_fresh` reads it and asserts that no
  pivot has happened since it was set. Fifteen writes and seven reads go
  through the two, so the counter cannot drift from the flag.

  The assert the comment purge asked for was `assert(!s->verified)` at
  `pivot()`, and it is false: 6 pivots out of 1033526 over the 139 gate
  instances enter with the flag still set, on `etamacro`, `wood1p` and
  `pilot87`, so adding it would have aborted three instances in every debug
  build. What is true is that no reader ever sees a verification a pivot has
  spent — 0 times in that same census. Only a canary fires the new assert;
  three of the four clears in the source turn out to be dominated, and the
  record says so rather than claiming a defect proved it (D233,
  `bench/measurements/02-146/`).

- **Ten of `simplex.c`'s and `presolve.c`'s prose contracts are checks.**
  Eleven asserts, because the forcing row's replay needed two: the cumulative
  iteration cap's floor as a `static_assert`, a retired bound-flip
  candidate's finite box, the phase-1 append count, the lent objective
  restored on entry, and `jm_pattern_order`'s output no longer than its
  input; in presolve, the compaction's two passes agreeing, a FORCING row
  pinning only at the caller's own bounds, a free column really free at
  replay, the arena walk's lower bound, a non-zero divisor, and a singleton
  column's exactly-zero cost. Nine fire on their own arm; the build refuses
  the tenth (D232, `bench/measurements/02-145/`).

  An arm passes only on the exact expression glibc prints, which is stricter
  than D227 to D230 and is what caught a breaker whose suite died on a double
  free — exit 134, no assert reached, and the old rule would have credited
  the assert with that catch. One assert cannot be fired at all, and a census
  says why rather than leaving a gap: no FORCING row over 139 instances pins
  a column at a bound the caller's model did not carry, and the
  `col_pending_dual` test ahead of it takes every rejection.
  `assert(!m->scale_valid)` was proposed and refused, because it fires on the
  existing suite: the flag says the scale factors exist, not that the stored
  matrix was touched. All eleven hold on all 139 instances under `-UNDEBUG`
  (D152's rule): 139 records, zero assertion lines. Free on the gate, because
  asserts compile out under `-DNDEBUG` — 110 solution digests and 29
  infeasibility verdicts unmoved, `bench/results/` byte-identical.

- **Three writers: `jaos_write_mps`, `jaos_write_lp`,
  `jaos_write_solution`.** MPS expresses everything a model can hold; the LP
  dialect is narrower, so a ranged row, a free row and a row with no
  coefficients are refused by name. What JAOS writes it reads back as the
  same model, and that is checked rather than assumed: 139 of 139 gate
  instances round-trip field for field, LP takes 102 of them and refuses 37,
  and 5,110,541 values pass through the number writer without one reading
  back different. Cutting the writer to six digits turns 47 of 94 instances
  and three tests red; the digits are not a style question.

  Counting what LP could not express is what found the writer's one real
  defect: the objective listed only the costed columns, and because LP format
  numbers a column where its name first appears, 83 of the 139 wrote a valid
  file that read back as a different model with no error anywhere. Every LP
  test had given all its columns a cost. Free on the gate — every digest and
  work figure byte-identical, because nothing here is on a solve path (D226,
  `bench/measurements/02-138/`).

  `numerics-reviewer` found the one the campaign could not: nothing on the
  gate solves to a non-finite answer, and `jaos_write_solution` handed one
  straight to a printer that asserts its argument reads back. Two columns
  fixed at 1e300 with opposite costs reach an optimum whose objective is a
  NaN; that aborted on every build with asserts and wrote `objective -nan` on
  the ones without, in whatever spelling the host libc uses. It is refused by
  name now, and the control arm for it is why every arm reads the test
  binary's exit code rather than the failure count — an abort prints no count
  at all (D226).

- **Software prefetching is refused, and the arbiter meant to judge it turns
  out to be blind.** The candidate was built at the paper's own schedule and
  is bit-identical on all three sets. `tools/icount.sh -m` read 0.99996,
  which is close enough to exactly 1 to be a broken instrument rather than a
  change that does nothing — and a canary with eight scattered prefetches per
  iteration, enough to thrash any real cache, moved it 0.061%. Valgrind does
  not simulate prefetch instructions. D225 named prefetching first among the
  cases the miss count covers and that part of it is wrong; `CLAUDE.md` now
  says so.

  With no readable metric left, the time ratio was 1.0268x on the five
  heaviest instances — inside this host's 6.27% floor, so INCONCLUSIVE, with
  four of five slower and the one reading outside the floor a slowdown.
  Refused. The census that preceded it is kept and stands on its own: 54.1%
  of inner-loop iterations happen in loops longer than the 64-iteration
  look-ahead, so the technique applies and it is the instrument that is
  missing (D231, `bench/measurements/02-143/` and `02-144/`).

- **The assert debt's test half is closed.** Thirteen tests across
  `check.c`, `lu.c`, `model.c` and `jaos_internal.h`, each proven to go red
  when the sentence it states breaks. Four of the listed items turned out not
  to be tests at all: an injected allocator under LSan, a census of 23 million
  pivots, a three-build record comparison, and a measurement of which build
  carries which guard.

  The lesson is about controls. Four arms across the four campaigns came back
  green or aborting where a red test was expected, and every one was a defect
  in the instrument: a test that could not see its own break, a break placed
  where the code overwrote it, a counter that never fired because the worker
  `_exit`s, and an assert that fires before any test can report. A campaign
  whose arms are all expected to pass would have shown none of them.

  One finding is a real hazard rather than a curiosity: letting
  `jm_pattern_order` keep a position equal to `limit` is a segfault in a
  release build, because `limit` sizes the bitmap (D229, D230,
  `bench/measurements/02-141/` and `02-142/`).

- **The LU half of the debt, and three of its four sentences could never
  have been tests.** A failed update's silence is one, and it is a test.
  `grow_pair`'s second-array failure needed an injected allocator, because
  both arrays hold eight-byte elements and no input makes only the second
  grow fail. The `piv_n == 0` shortcut needed three builds, because one
  binary runs one path; removing it leaves the record byte-identical and
  breaking it does not, which is what makes the first half mean anything.

  And `find_pivot`'s zero-count bucket turns out to be dead: 0 of 23,103,784
  accepted pivots on the standard set came from it, skipping it entirely
  leaves every record byte-identical, and the unit suite is green with the
  loop starting at one. The bound stays and the measurement now sits beside
  it, because nothing else guards it. The census caught its own first
  version too — `bench/run`'s workers `_exit`, so an `atexit` report never
  fired, and the probe's own "the counter was never live" check is what said
  so instead of a plausible zero (D228, `bench/measurements/02-140/`).

- **The checker's five contracts have tests, and one guard turns out to
  live only in a debug build.** Every multiplier contributes `w * bound` to
  the dual objective including the exempt ones; `note_dropped` counts a
  1e-15 multiplier with no magnitude exemption; the implied box is exactly
  what the row implies and its infinite terms are counted rather than
  summed. Each proven to go red when its sentence breaks, and the exemption
  turns out to be load-bearing for three tests that already existed.

  The sixth became the finding. Removing `certified_step`'s clamp aborts the
  suite on D219's assert — and under `-DNDEBUG`, which is what ships, the
  same break leaves everything green. Three guards sit on that property and
  a release build keeps one. Running the same breaker twice is what measured
  it, and no earlier control campaign here did that (D227,
  `bench/measurements/02-139/`).

- **The first four tests of the assert debt.** `jm_bland_pick` compares its
  minimum exactly, pinned at one ulp; a non-positive `nvar` leaves the
  nonbasic bitmap alone; a zero-length allocation is not a failure; and
  `jm_two_product_residue` returns 0.0 past 2^996, where the product is still
  finite but the Dekker split overflows. Each proven to go red when its
  contract breaks -- and two of them turn out to be load-bearing beyond their
  own test, `jm_alloc_array(0)` failing four tests when broken. The residue
  test exposed that the function guards that case TWICE, and that either half
  suffices: the top `2^996` test is a fast path, the closing `isfinite` is
  what makes it correct (D224, `bench/measurements/02-137/`).

- **`jaos_internal.h`'s contracts are asserts in the files that implement
  them.** `jm_harris_pick`'s non-negative numerators, positive denominators
  and valid return; `jm_pattern_order`'s scratch bitmap clean on entry and
  left clean, output ascending; `nbmark` equal to the non-basic set, rebuilt
  and compared at its one reader; a presolve record's index original at every
  push; `jm_postsolve_expand` entered only when presolve reduced, with the
  reduced model aliasing nothing of the caller's; every scale factor an exact
  power of two. All six hold on all 139 instances, and each was proven against
  the defect it exists for (D223, `bench/measurements/02-136/`). Asserts
  compile out under `-DNDEBUG`, so all three gate sets are byte-identical.

- **Four of `check.c`'s contracts are asserts.** The dual walk's two sums and
  their model-only pair are magnitudes before the gap is formed;
  `certified_step` is only reached where the column's opposite bound is
  infinite and never returns a negative distance; `implied_bounds` only ever
  tightens. Each proven against the defect it exists for — and the arm that
  first fired nothing was measuring the models, not the assert: it could only
  bite on a column shape none of the 94 has (D221,
  `bench/measurements/02-135/`). Asserts compile out under `-DNDEBUG`, so all
  three gate sets are byte-identical.

- **Four of `model.c`'s contracts are asserts and a fifth is a build error.**
  The two start-basis arrays are a pair, checked at the three functions that
  read either as a claim about the other; `jm_model_publish_objective`
  checked one of the seven things its comment promised and checks all seven
  now. `-ffast-math` and `-Ofast` are refused by `#error`, because they
  enable the associative maths that deletes the compensated sum's residue.
  Each assert was proven against the defect it exists for, and the control
  needed three attempts: two versions reported 0 firings on arms that were
  dead, and the third found that **the gate cannot reach the pair assert at
  all** — it never changes a model's dimensions after solving one — so that
  arm runs the unit suite (D219, `bench/measurements/02-134/`). Asserts
  compile out under `-DNDEBUG`, so the three gate sets are byte-identical.

- **The primal phase 1 stops when its own total infeasibility doubles.** That
  quantity is a sum of bound violations and cannot rise under an exact pivot;
  it rises when the basis has gone near singular and `refresh` recomputes `xb`
  from it. `PHASE1_RISE_MAX = 1.0` sits in the middle of a plateau eight
  decades wide, censused over all 110 forced-primal solves: every threshold
  from 1e-5 to 1e+2 stops `pilot87` at the same iteration and nothing else at
  all, with `woodw` 2.3e+7 times below it. It refuses only on a point it has
  recomputed first, the D20 shape the neighbouring exits use. `pilot87` ends
  with a published refusal that says why, at 19532 iterations and 4234464248
  work units against 381886 and 179611375417 — **2.36% of the work**, and its
  running minimum had last improved 362354 iterations before the rule fires.
  The campaign goes to 61 ok / 30 disagree / 3 overrun from 61 / 29 / 4, and
  the low arm of the sweep is a real loss: at 1e-12 the rule costs `pilot`
  its answer (D218, `bench/measurements/02-133/`). Behind `cfg.force_primal`,
  so no shipped solve reaches it; all three gate sets byte-identical.

### Changed

- **A change is judged on five things, not four.** `tools/icount.sh -m`
  reports L1 data read misses from callgrind's simulated cache instead of
  instructions, deterministic to the miss and with the hardware prefetcher
  modelled. Instructions judge a change that does LESS work; misses judge one
  whose mechanism is memory-level, because every prefetch is a retired
  instruction and the instruction count reports such a change as worse while
  it is faster. Found when a literature pass turned up the first candidate of
  that kind, which the existing arbiter would have rejected (D225).

### Fixed

- **A cited measurement directory has to be in the repository.**
  `record-check` tested `os.path.isdir`, which is this disk and not repository
  membership, so `make test` passed here and failed in every clone: two lines
  of `TODO.md` cited a directory `7ac820f` untracked on purpose. It reads the
  tracked set now. Two refinements the fix forced —
  `bench/measurements/README.md` is scanned at last, although it is the index
  of those directories, and raw `.txt` readings are exempt, because one
  captures a `git status` that legitimately names the untracked directory and
  rewording a reading to satisfy a check would falsify it. Proven in a
  checkout carrying only tracked content: PASS there, and still FAIL on a
  citation of a directory in no tree (D222).

- **`record-check` reads the measurement records too.** A directory's
  `README.md` is prose that cites decisions and its `.txt` is the reading a
  decision was closed on, and neither was ever scanned — so
  `02-134/assert-control.txt` could claim a decision number that does not
  exist while the script that wrote it cited a different one, which means the
  committed record was not the output of the committed script. Scanning them
  raises the citation count from 3491 to 4460 and finds exactly that one bad
  citation in the whole measurement history.

- **`make refusals` says which committed records it replaced.** Every re-test
  `tee`s into its own measurement directory, so the target overwrites the
  reading its refusal was decided on; `bench/refusals.txt` warned about it in
  prose and left the rest to memory. It now names the dirtied files and says
  to keep them or restore them. Not undone automatically: a re-test at a newer
  tree is often the reading worth keeping, and which it is this time is not a
  script's decision.

- **`jaos_set_coefficient` invalidates the derived copies through
  `model_matrix_is_stale`** instead of setting the three flags inline. The
  effect was the same, but the two lists could drift, and the comment saying
  all five matrix modifications go through one place was already false when
  the comment purge found it and deleted it. The claim is true now and the
  sentence is back (D219).

- **D76's re-test proves its patch applied, and the record says so.** It
  reported "the refusal holds" while `tools/icount.sh` printed its D82 canary
  and exited 2, and the evidence file carried nothing separating "`restrict`
  changes no instruction" from "the patch never applied" — identical counts
  are what both look like. The record now counts the qualifier in both trees
  before reading any instruction count, and refuses when the measured tree
  does not actually differ: 0 occurrences in the reference against 7 in the
  patched tree. The refusal holds, and for the first time the reading says
  why rather than only what.

- **`geomean.py` can read `bench/results/primal.txt`.** That record carries two
  solves a line, as `dual=iters/work primal=iters/work`, and has no `work=`
  field at all, so `--metric work` matched nothing and the script exited 2
  saying "no instance appears in both files" — which named neither the cause
  nor the fix, and sent every primal work figure through `--pairs` by hand.
  A new `--side dual|primal` reads the wanted half of the pair, and the error
  now says how many instances came out of each file and which flag to pass.
  Validated on the case it must reject: `--side` against a gate record, which
  has no such pair, still exits 2 rather than averaging nothing.

- **Every measurement script derives the repository root instead of writing it
  down.** The literal made a script measure the main tree whatever worktree it
  ran in, which is how a three-ref attribution returned one number three times
  (D215). **It was 48 scripts, not the 28 D215 counted**: the literal is
  written three ways and that count came from one grep. Checked by
  `bench/measurements/02-132/run-root-check.sh` — no literal left, all 48
  parse, none of the replacements landed inside a single-quoted heredoc where
  it would not expand, and the same script reads the main tree from the main
  tree and a worktree from a worktree at three different working directories.
  `make refusals` exits 0 and runs one of the 48 (D217).

### Added

- **Eight of `lu.c`'s contracts are asserts instead of sentences.**
  `grow_pair`'s capacity, `mult_set` clean at each pivot step, `keep <= k` in
  the one-walk column update, both renumber maps total, `btran_u_pattern`'s
  stamp and return, and the spike above the diagonal after `jm_lu_update`'s
  permutation. Proven on 94 real factorizations with
  `make netlib EXTRA_CFLAGS=-UNDEBUG`: **0 fired live, 85 fired** with the
  `mult_set` clear loop deleted, all of them the step-top assert. `-DNDEBUG`
  removes them, so the three gate sets are byte-identical and all five
  configurations build and pass (D216, `bench/measurements/02-131/`).

### Fixed

- **`make refusals` found D211's refusal expired, and the script that
  re-tests it could not have said where.** D212's two-pass ratio test removed
  the rise that made a phase-1 stop rule impossible: `pilot-ja` rose 25.0449
  above its running minimum and still finished `ok` when D211 was written, and
  rises 3.3348e-12 now. The attribution needed three fixes of its own --
  `bench/measurements/02-126/relrise.sh` hardcoded the repository root, so run
  from a worktree it measured the main tree and returned one number at three
  different refs. It derives `root` from its own location now; **28 other
  scripts under `bench/measurements/` still do not**, one of them a refusal
  re-test (D215, `bench/measurements/02-130/`).

- **`can_move` compares a rate against a rate, and in both spaces.** Its last
  line multiplied the reduced cost by the distance to the other bound and
  tested that product against `DUAL_TOL`, which bounds a reduced cost at every
  other site that reads it. It is `breached(s, v)` now, the filter
  `wants_a_pivot` already applies to the complementary case. `netlib` comes
  back 94 of 94 bit-identical and `netlib-infeas` 29 of 29; on Kennington two
  instances move and both get cheaper, `pds-20` publishing the same objective
  from a different vertex for **a fifth of the work** (90938 iterations to
  44790) and `pds-06` for 0.8297x, a work geometric mean of **0.8930x** over
  the 16. D27 chose the product to avoid choosing a space, and the instance it
  chose it for is the one that pays for it (D214,
  `bench/measurements/02-129/`).

- **The pricing row's pivot element is judged against its own terms too, and
  `PIVOT_MIN` is documented as what it actually is.** The three sites that
  test `alpha[q]` were absolute; they now apply `PIVOT_MARGIN` against
  `sum_i |rho_i * a_iq|` as well. The measurement behind it is the surprise:
  every one of the thirteen calls the absolute test rejects over the standard
  94 has `alpha[q]` equal to its own traffic **to seventeen digits** — a
  one-term dot product, the best determined a number gets. `PIVOT_MIN` there
  is a **stability** floor, not a noise floor, and `docs/tolerances.md` said
  otherwise. The noise floor was simply absent, and `scsd1` was pivoting on a
  value standing at a third of one ulp of its own terms. Costs a work
  geometric mean of **1.000001x on the dual solve** and 1.000496x on the
  forced primal; every digest, iteration count and objective on all 139 gate
  instances is unchanged (D209, `bench/measurements/02-124/`).

- **The two primal ratio tests judge a pivot against the column's own scale,
  not against an absolute 1e-9.** `PIVOT_MARGIN = 1.0`: an entry of
  `B^-1 M_q` must stand above one ulp of that column's largest entry to be
  pivoted on. It decides what may be pivoted on, never what blocks. The
  forced-primal campaign goes to **56 of 94 agreeing** from 55, `pilot4`
  DISAGREE → ok, and the one instance that refused calling itself defective
  — `pilot87`, on an FTRAN residue of 1.59e-07 where the true entry is
  exactly zero — is gone. **All three gate sets are byte-identical**, records
  and work units both: the floor is not reached from the dual path (D207,
  `bench/measurements/02-122/`).

### Added

- **The record is checked with the code.** `make test` runs `make record-check`
  (`tools/record-check.py`): every cited decision exists, every constant in
  `docs/tolerances.md` matches the source, every `SPECS.md` label is present
  tense and every `partial` row says what is missing, every evidence script's
  anchor still matches or is pinned to its commit, and `docs/claims.txt` lists
  what the record says does not exist — the line that fails when one of them
  lands. First run: **147 failures**, all fixed or pinned (D206).

- **`tools/icount.sh`, a deterministic instruction count inside the solver.**
  Callgrind over `jm_dual_simplex*`; identical run to run. The third metric
  beside digests and work units, for what seconds cannot resolve on a host that
  repeats to 6.27%. D199's clear reads **1.00017**; D76's `restrict` retires
  **identical** instructions on `maros-r7` (D206, `02-119/`).

- **`bench/refusals.txt` and `make refusals`.** Every refusal with what would
  reopen it; the ones with a script are re-run. The first run found D184
  expired: `can_move`'s units are **live** on the primal campaign, `scsd1` and
  `scsd6` move (D206, `02-118/`).

- **A planning-era appendix in `DECISIONS.md`.** 85 `D-NN` citations named
  decisions deleted with `.planning/` (D98), and the same number named two
  decisions in two phases. Both lists, one line each, so a citation resolves.

### Changed

- **The two primal ratio tests are Harris's two-pass form.** Pass one widens
  every basic bound outward by `primal_tol` and takes the smallest quotient;
  pass two returns the **largest pivot** whose exact quotient still fits, both
  through `jm_harris_pick`, the generic routine the dual side has used since
  D26. Bland's rule keeps the exact minimum, because its finiteness argument
  needs a fixed rule. D207's relative floor now compacts the candidate list, so
  a floored row neither pivots nor blocks. The forced-primal campaign goes to
  **60 of 94 agreeing** from 56, overruns 8 → 4, work geomean 3.9470 → 3.8224;
  `wood1p` publishes a different vertex of the same optimal face for **22% less
  work**, and `pilot87` still overruns (D212, `bench/measurements/02-127/`).

- **The Harris width in those two tests is half `primal_tol`, not all of it.**
  `PRIMAL_HARRIS_DELTA = 0.5`, multiplied onto the per-model field so a
  tolerance override keeps the phase-1 bound. Seven widths swept: the
  forced-primal campaign is flat at **61 of 94** from 0.01 to 0.5, the same 61
  instances name for name, and all three gate sets stay byte-identical anywhere
  from 0 to 10. No reading chooses inside that plateau, so the value is argued
  rather than fitted: a factor of two under the bound
  `docs/research/harris-primal.md` sets, the ratio MINOS and SNOPT start EXPAND
  at, and a power of two so the product is exact. `pilot87`'s divergence does
  not follow the width (D213, `bench/measurements/02-127/`).

- **`SPECS.md` is present tense only.** Rows say what exists, what is partial
  and what is missing, with the decision beside each. History left the rows;
  one row that said four instances were off the optimum had been closed by
  D184 and did not know it.

- **Seven present-tense claims that expired when the primal landed** are
  gone: two "no primal pivot before M6" comments, "`force_primal` has no
  reader", "`certified_suboptimality` decides nothing", and three more (347c4fb).

- **Four source files keep their contracts and lose their essays.** `lu.c`,
  `model.c`, `check.c`, `jaos_internal.h`: 1450 comment lines to 690, every
  code token unchanged by construction, every digest unchanged by the gate.
  Three false sentences went with them. What survived as prose and deserves an
  assert is one debt list in `TODO.md`.

- **`CLAUDE.md` has three process tiers**, so a bench tool costs the gate once
  and not the solver's full loop.

### Added

- **A watcher can stop the primal phase 1.** Phase 2 and the dual both offered
  `progress_cb`; phase 1 did not, so a caller could not see or stop the part of
  a forced-primal solve that spends **39.5%** of its iterations. `infeas_best`
  carries phase 1's own total instead of the 0.0 that claimed the point was
  already feasible (D200).

- **`make primal` says which method did the work.** Every record line carries
  `split=p1:N/p2:N/dual:N`, the overrun branch included, and the summary leads
  with the campaign's totals: **phase 1 39.5%, phase 2 0.0% (97 iterations),
  dual re-entry 60.5%**. The solver gains `n_phase1_iters`, assigned after the
  phase-1 call so it is written on **every** exit — reading it from a
  success-only log line is what made D194 wrong about eight instances. Matches
  02-108's independent probe exactly, on the totals and per instance. Cost:
  all three gate sets byte-identical (D197, `bench/measurements/02-110/`).

- **A primal phase 1, and the reach goes from 0 of 94 to 64 of 94.** Composite,
  short-step, no artificial variables: it works on whatever basis it is given,
  which is what crossover needs. **54 instances now agree with the dual**,
  objectives within tolerance and both answers through the independent checker,
  at a work geometric mean of **3.8332x** — what Dantzig pricing costs. 8
  overrun a 10x budget, and **32 still fail where the dual succeeds**. The
  first published figures were 64 and 3.2352x; D191 corrects them. The defect it shipped with was
  found by a loan of exactly 1.0: phase 1's own reduced costs are of magnitude
  one, and `update_dual` was lending the **model's** costs against them (D190,
  `bench/measurements/02-103/`).

- **A phase-2 primal simplex**, priced by Dantzig's rule and sharing `pivot()`
  and `build_pricing_row` with the dual. Behind `cfg.force_primal`, which is a
  development switch and not an option (D64). **It refuses to start from a
  primal infeasible point rather than reporting `INFEASIBLE` or running
  anyway**, because there is no primal phase 1 yet, and a cold basis is dual
  feasible rather than primal feasible: its reach on the standard set is
  **zero of 94**, and `make primal` reports that as `unreached` rather than as
  an error. Devex is deferred — its weight recurrence is in no readable source
  (D188, `bench/measurements/02-101/`).

### Fixed

- **An abandoned solve published the previous solve's iteration total.**
  `solve_iters` is written inside `publish()`, and `publish()` runs only on
  `JAOS_OK`, so a refused solve kept whatever the model was solved with last
  time. `bench/primal` solves with the dual first and subtracts, so `pilot87`
  reported `dual:20835` for a re-entry that never ran — 38000 minus 17165,
  two different solves. `jm_dual_simplex` zeroes all three counts on entry and
  writes the total on the abandoned branch. The campaign headline moves from
  536270 to 515435 (D202, `bench/measurements/02-115/`).

- **The most common primal failure had no sentence.** 31 of 94 forced-primal
  solves end at `NUMERICAL_ERROR` because the settled point is not dual
  feasible, and that site wrote no message, so `jaos_model_error` was empty on
  exactly the instances anyone would open it for. **0 of 31 DISAGREE lines
  carried a message; 31 of 31 do now.** The verdict names the breach and the
  start it came from (D205).

- **`make primal`'s record can be re-derived from itself.** Eight defects, all
  about the record rather than the solver: the split column was dropped for
  `ERROR` and `skipped` while the headline still counted them (`pilot87` alone
  hid **5.1%** of the phase-1 total), the `no split` sentinel was written too
  late for six early failures and every dead worker, `fail()` overwrote the
  solver message one branch after recovering it, and `note` was 64 bytes
  against messages of about 250. A designed refusal is classified by its
  phase's own sentence now, and one that cites D19 and matches neither phase is
  an error rather than being refiled in silence — the third wrong match in
  this classifier in two milestones (D202, D204, D205).

- **The campaign's iterations-by-method line names its carriers.** It is a sum
  over the set, and `d2q06c` and `dfl001` are **42.1%** of it, so "phase 1
  39.5%" was largely a statement about two instances. The two largest carriers
  and the **median per-instance phase-1 share, 57.3%**, are printed beside it
  (D204).

- **Six defects in the primal phase 1 a review found.** `infeas_best` stayed at
  `HUGE_VAL` for a solve that skipped phase 1, so a warm or crossover basis
  reported `inf` to every progress callback. Two `!s->verified` gates returned
  a library error where the four they mirror return a verdict, and a fifth in
  phase 1's `needs_refactor` block was missed with them. The `s->col` contract
  check compared with `==` under a comment claiming bit for bit, which fires on
  a NaN both sides computed and is blind to the `+0.0` over `-0.0` it exists to
  catch; it is a `memcmp` into a buffer allocated once rather than per flip.
  The lazy allocation guarded one pointer while allocating two, and phase 2's
  refusal still said JAOS has no phase 1 (D202).

- **Softening those gates stranded the refusal message.** `jm_dual_simplex`
  copies the solve's message off the reduced model only when
  `st != JAOS_OK`, which a soft return makes false — so it was lost on the 86
  of 94 instances presolve reduces. The condition reads the outcome now, and
  narrowly: all eleven `jm_set_err` sites pair with a numerical refusal and
  none with INFEASIBLE, UNBOUNDED or a budget, so a wider test would only
  attach a stale sentence to a designed verdict (D205).

### Changed

- **Both parsers of the SUMMARY sentence are gone.** `solve_primal_iters` and
  `solve_phase1_iters` join `solve_iters` on the model, written on every exit.
  `bench/primal.c` and `tests/test_simplex.c` each carried a copy of one
  backwards character walk, and they were not the same parser — the bench
  required a substring the test did not, so editing that sentence could leave
  the suite green while the campaign reported no split on all 94 (D202).

- **D199's clear was accepted on work units alone, and now has its seconds.**
  The wall-clock ratio is **1.0007** over four phase-1-dominated instances
  against **0.9853** over two the change cannot reach, both far inside this
  host's 6.27% repeatability. No effect in either direction, and no density
  fallback is warranted (D203, `bench/measurements/02-116/`).

- **`docs/work-units.md` documents the primal phase 1's charges**, which had
  no entry while the first of them changed twice (D198, D199).

- **`bench/measurements/02-101/`'s negative control runs again.** It anchored
  on a 30-line copy of `run_primal`'s body including a message a later commit
  rewrote, so it exited 1 on ANCHOR NOT FOUND and its evidence stopped being
  re-derivable. It anchors on the function signature now, asserted unique. All
  42 measurement scripts from 02-9x on were checked; no other anchor has
  drifted.

Cost of the whole set: **110 solution digests and 29 infeasibility verdicts
unmoved, over 139 instances** — all three gate sets byte-identical to the tree
before it. `make configs` 5/5.

### Fixed

- **`s->col`'s contract is an assert, not a comment.** `primal_bound_flip` reads
  `B^-1 M_q` from a buffer with **five other writers**, two of which alias it as
  `rhs`. The check recomputes the column into its own buffer and compares bit
  for bit. Compiled out by `-DNDEBUG`, so the gate never sees it; validated by
  perturbing `s->col` and watching the suite abort, which also proved the suite
  reaches the flip at all (D201, `bench/measurements/02-114/`).

- **Phase 1 refreshes before it refuses.** Two of its four refusals — "nothing
  improves" and "no declared bound stops this column" — are verdicts, and both
  read numbers `pivot()` had been stepping in place through a patched
  factorization. D20 refuses that and phase 2 has gated on it since it landed.
  Cost: nothing measurable, and the branch is **reached 0 times on all 94**,
  with the probe's positive control firing in the same table (D200,
  `bench/measurements/02-113/`).

- **The phase-1 clear stops sweeping every variable to undo the basis.**
  `primal_phase1_costs` cleared `nvar` doubles to zero at most `nrow` entries;
  it now clears exactly what the previous call set. Cost: **0.9452** over the
  53 shared instances, cheapest `standata` at 0.8511, and **0 iteration counts
  and 0 objectives moved** — checked, not asserted. Four campaign verdicts
  recover and the totals return to 55 measured and 7 overrun. Against the
  under-billed tree that D198 replaced it is 1.0042: the clear is now charged
  for the positions it visits (D199, `bench/measurements/02-112/`).

- **Phase 1 bills the sweep it does, not just the scan.** `primal_phase1_costs`
  cleared `nvar` doubles and charged `nrow`, so phase 1 was under-reported by
  `nvar` on every one of 336660 iterations. Cost: **every instance moved and
  none was identical**, work geometric mean **1.0625**, worst `standata`
  1.1759. Two campaign verdicts move with it — `pilot-ja` and `standmps` no
  longer fit the 10x budget — which is the budget seeing the work, not the
  solver doing more. All three gate sets byte-identical (D198,
  `bench/measurements/02-111/`).

- **Four records that described a solver that no longer exists.**
  `run_primal`'s header said "phase 2 only", "there is no primal phase 1 yet"
  and "a cold start never gets here"; D195 measured 0 of 94 skipping phase 1.
  `bench/primal.c` said the same in a comment and in the message it prints
  (D198).

- **The iteration guard names the phase it is reporting.** Phase 1, phase 2 and
  the dual's re-entry share one cap and each tests the cumulative `s->iters`
  against it, so phase 2's message read `after N primal iterations` off a count
  that included phase 1's. Both primal messages now carry the phase's own
  count, the position in the solve, and the cap. **The sharing itself stays**:
  phase 1 spends at most **1.68%** of the cap, on `pilot-ja`, and the reopen
  conditions are written beside it. Cost: all three sets byte-identical
  (D196, `bench/measurements/02-109/`).

- **`refresh` is the third place a cost is lent, and it was the unguarded
  one.** Its repair sweep runs `shift_to_feasible` over every variable after a
  singular-basis repair or a warm start, and unlike the other two sites it read
  no flag. It **fires 30 times inside the primal phase 1 on 11 of the 94
  standard instances**, shifting 3990 costs on `pilot` alone. Guarded now.
  Cost: all three gate sets byte-identical; on the primal campaign 52 of 53
  shared instances bit-identical in work, **54 agreeing becomes 55 and 8
  overrun becomes 7** — `pilot-ja` and `pilotnov` gained, **`pilot4` lost**
  (D193, `bench/measurements/02-105/`).

- **Bland's rule reaches the primal's leaving variable.** Phase 2 had the
  entering half and broke equal ratios on the first row **position** scanned;
  phase 1 had neither half, so neither phase had a finiteness argument.
  `jm_primal_row_wins` is the leaving half, tested against seven cases
  including the direction of the comparison and the refusal to open a window at
  the minimum. `primal_cleanup` passes `false` and is untouched. Cost: nothing
  measurable — all three sets byte-identical, `make primal` reproducing D191's
  counts and 3.8332x exactly, and **0 phase-1 arms in 94 instances**, with the
  probe validated against a forced stall (D192,
  `bench/measurements/02-104/`).

- **`make test` compiles the bench runners.** Nothing in the project loop built
  `bench/run.c`, `bench/warm.c` or `bench/primal.c` — not `test`, not
  `sanitize`, not `configs` — so `-Werror` on 2000 lines of in-tree tooling was
  not a guarantee. A dead string match in `bench/primal.c` survived a full loop
  cycle because of it (D191), and `bench/warm.c`'s `-O2` break had to be found
  by hand (02-99). Compiled, not run: running them needs the network.

- **The primal lent the model's cost vector against phase-1 gradients, and the
  guard that was supposed to stop it was applied at one of the two sites it
  documented.** `share2b` carried 43 loans totalling 2773.0 when the primal
  declared optimality, the worst of them 1026.11 on a variable whose true cost
  is zero, and every one raised in phase 1. So the branch's headline "64 of 94
  agree" was partly an artifact of that perturbation, recovered afterwards by
  the settling and the dual re-entry. The honest figure is **54**. Also in the
  harness: a designed refusal was matched on a string the same branch deleted,
  so `make primal` exited 1 on an expected outcome (D191).

- **The primal published a value outside a declared bound, as `OPTIMAL`.** No
  basic variable can express the entering column's *own* opposite bound, so a
  ratio test built from rows walks past it: on a two-row model it published
  `x = 10` against a declared upper bound of 1, objective -10 against a true
  -3.75, with only the independent checker refusing the point. The repair is a
  bound flip sized from `real_upper`/`real_lower` — never `up`/`lo`, which
  still hold the bounds dual phase 1 invented — and it costs no solve, because
  the ratio test has already left `B^-1 M_q` where it can be read. Unreachable
  before the pricing rule landed, which `TODO.md` §0 had predicted in as many
  words (D189, `bench/measurements/02-102/`).

- **The simplex's error messages never reached the caller on any model presolve
  had reduced.** `jm_set_err` writes to the model the simplex ran on, which is
  the reduced one; the caller holds the original. Exactly **8 of 94** standard
  instances carried a message through, and they are precisely the eight
  presolve does not touch. What was lost includes the iteration guard's "this
  is a JAOS defect" and `classify_optimum`'s refusal to answer past a bound
  phase 1 lent. Older than the primal method, found by it (D188).

### Added

- **`bench/primal.c` and `make primal`**, a third runner beside the gate and
  `warm`: it solves each instance with the dual and then with the primal on the
  same unperturbed model, requires the two answers to agree, and reports a work
  ratio rather than a verdict. It exists because the gate structurally cannot
  see a primal simplex — a cold start is dual feasible and not primal feasible,
  so the solver never chooses that path. `cfg.force_primal` carries the switch
  and **has no reader yet, deliberately**: that is what let the instrument be
  validated before the feature exists. 94 of 94 bit-identical at a ratio of
  exactly 1.0000, and three doctored copies each caught by a different branch
  (`TODO.md` §0 stage 0, `bench/measurements/02-99/`).

### Changed

- **The primal clean-up stops pricing its row the expensive way.** Both methods
  need row `r` of `B^-1` and of `B^-1 M`; `primal_cleanup` repeated the work
  rather than sharing it, and the repeat was never equivalent — a dense BTRAN
  plus a column-by-column `price_entry` pass, which costs the whole matrix
  however sparse `rho` is, where `price_all` skips a row of the matrix per zero
  of `rho` (D35). Both call `build_pricing_row` now. `wood1p` 0.9682x,
  `etamacro` 0.9996x, `pilot87` **1.0000017x the wrong way** because the sparse
  route bills the pattern ordering a dense BTRAN never charged for. 91 of
  netlib's 94 bit-identical, the other two sets bit-identical throughout, and
  **0 digest changes with every iteration count unmoved** (D187,
  `bench/measurements/02-100/`).

### Fixed

- **`bench/warm.c` would not compile at `-O2`.** It read 79 characters into a
  64-byte `note` field; `snprintf` truncated the excess, so no note was ever
  wrong and no measurement was affected, but `-Werror=format-truncation`
  refuses it below the `-O3 -flto` the Makefile uses. Both runners now size the
  buffer to the destination. Found because `bench/primal.c` inherited the same
  lines. Also `.PHONY` was missing `warm`, `warm-kennington`, `primal` and
  `primal-kennington` (`bench/measurements/02-99/`).

### Changed

- **`TODO.md` §0 has a build order, and the Devex blocker turns out to block
  stage 5 alone.** Dantzig pricing is fully specified and needs no paper, so the
  first primal is buildable now; Devex is an optimisation on top. Eight stages,
  each with what blocks it. **Two corrections come with it.** The forcing switch
  must be on `m->cfg` and not a build flag, because the harness has to run both
  algorithms on one instance in one process as `warm.c` does. And Hall &
  McKinnon, read at source, say cycling occurs for the most negative reduced
  cost *and* steepest edge, so the Bland fallback is needed from stage 1 rather
  than when the pricing gets clever. **Stages 1–5 must move zero digests on all
  three gate sets**, because a cold start cannot reach the primal path at all —
  which makes the ordinary campaign a real test of them. Documentation only.

- **The primal simplex literature is read and written down**
  (`docs/research/primal-simplex.md`), which closes §0's first prerequisite.
  Four choices come back settled: Devex over exact primal steepest edge, which
  costs an extra BTRAN *and* an extra full PRICE; Maros's piecewise-linear
  phase 1, which must start from a given basis or it is useless for crossover;
  Harris two-pass in its primal form, which is the original form; and the snap
  the checker forces, because Harris and EXPAND both accept a point outside its
  bounds. **A premise broke**: crossover as published starts from an interior
  point and JAOS has none, so §0's chain assumes a starting point that does not
  exist. **No PDF was reachable**, so the citations are Crossref-checked and
  several weight-update formulas are marked to check at source. Documentation
  only.

- **Three defects repaired in `docs/research/crash-basis.md`.** It listed a
  solver source file, `Clp/src/Idiot.cpp`, as a reference, which D12 forbids
  outright; the line is gone and Galabova & Hall stands alone. Koberstein's
  thesis is the University of Paderborn, not TU Berlin, in both places. Galabova
  & Hall is *Optimization Methods and Software* 35(3), 488–501, not *Mathematical
  Programming Computation*, and both copies of the title were incomplete — the
  DOI was re-checked at Crossref from this session rather than taken on report.
  Documentation only.

- **`TODO.md` §0's reuse question is narrower than it was written**, because a
  primal iteration already runs here: `primal_cleanup` picks a column, calls
  `primal_ratio_test`, and hands both to the same `pivot()` the dual uses. What
  is missing is a pricing rule, a phase 1, and three gaps in the ratio test —
  the worst of them a correctness gap, since the entering column can never
  bound-flip. §0 also gains the harness (`bench/warm.c` is the template), the
  scratch conflict a primal ratio test would create, and two constraints its
  table did not carry: D82/D84 refused pricing schemes on **wrong answers**,
  and D45 says the work counter cannot see the trade. Documentation only.

- **The primal simplex has a third dependent, and neither `SPECS.md` nor
  `TODO.md` §0 named it.** `classify_optimum` refuses out loud when a column is
  held by a bound dual phase 1 lent it and a real constraint stops it short of
  infinity; D19 says reaching that optimum needs a primal pivot and owns the
  population that gets there. It is not a gate item — every solve in
  `bench/results/*.txt` ends `optimal` or `infeasible` — so closing it needs
  D19's generated-LP sweep and not a campaign. Documentation only.

- **The primal simplex is the next work, and `TODO.md` §0 is the item.** Chosen
  by the maintainer on 2026-08-25 after items 1 and 5 closed and everything
  left needed either a decision or a feature. It jumps §6's proposed order,
  which puts cheap breadth first, because it is the only thing unblocking work
  already analysed: primal simplex → crossover → D97. §0 carries what it needs
  before any code, what is already decided about it, and the one dead site it
  wakes up. Documentation only.

- **No mapped basis arrives long in 101 calls**, so `build_warm_basis`'s
  refusal of a long count holds and the cheapest proposed route to §2 is
  closed. A demotion there would have needed no new rank machinery —
  `repair_singular_basis` is already downstream — and there is nothing to
  demote. The census also prices D151's cap per instance: **35 of 90 netlib
  warm starts fall back to a cold solve** because their shortfall is past 4,
  and Kennington loses none. `fit1p` publishes a basis over by 21 and maps
  short by 241, so the two counts are unrelated. No source change (D186,
  `bench/measurements/02-98/`).

- **The gate has an absolute bar on suboptimality now, `RSUB_CEILING = 1e-6`**,
  which closes `TODO.md` item 5. Everything beside it has its zero point in the
  baseline, so an answer that was already bad when the baseline was written
  read as permanently fine. Placed on **123 solves across five sets** — worst
  1.4e-07, so it clears by 7.1x — and it fires on nothing today, with
  `bench/results/*.txt` byte-identical. **Validated against the case it must
  reject**: on the solver as it stood before D184 it fires on `pilot` at
  6.91e-05 and `pilot87` at 2.54e-06, stays quiet on `wood1p`, and the verdict
  goes `gate: NOT MET`. It could not be placed until D184 fixed those two
  (D185, `bench/measurements/02-97/`).

- **`DUAL_TOL` is 1e-9 and the four netlib instances that published a point off
  the optimum no longer do.** `pilot` 2.312e-05 → 5.266e-09, `pilot87` and
  `scsd6` publish Koch's optimum exactly, `etamacro` 1.315e-08 → 1.137e-13. The
  price is a work geometric mean of **1.0339x on netlib** and **1.0976x on
  Kennington**, with `d2q06c` at 5.319x and `pds-20` at 4.815x past the 2.0x
  bar; `gate: PASS` on all three sets and no answer got worse. Kennington's
  half is new — D174's sweep was netlib only. Taken on the maintainer's
  decision (D184, `bench/measurements/02-96/`).

- **`pilot87`'s suboptimality bound moves because its dual solution is not
  unique**, which is the standing debt D92 opened. At `REFACTOR_EVERY` 8 and
  256 it publishes the identical objective and two different digests;
  splitting them shows the priced primal answer does not move — 738 of the 987
  columns that move cost exactly zero and the other 249 move by at most
  4.44e-15 — while 1817 of 2030 duals move, 166 by more than 1e-9 relative and
  none changing sign. `gap_positive` follows the duals, so it moves. The
  mechanism is established; D92's 3900x span is not reproduced and the entry
  says so. No source change (D183, `bench/measurements/02-95/`).

- **`plato-nug` solves one of three, which is a different answer from either of
  the two §4 allowed for.** `nug08-3rd` solves in 34424 iterations and
  294654930775 work units, `checker=ok` and `det=ok`; `nug20` and `nug30` do
  not finish in 3600 s and 1800 s. The family is not ordered by rows — `nug20`
  has 4488 fewer rows than `nug08-3rd` and does not finish in five times the
  time. **Presolve removes nothing at all on the one that solves**, and across
  the committed records it reaches a median of 0 to 3% of rows on the plato
  sets against 9% on netlib. No source change (D182,
  `bench/measurements/02-94/`).

- **The warm campaign is run on a fourth instance set for the first time, and
  §3 is not reopened by it.** `plato-fome`: 4 instances, **0 repairs fired**,
  because the mapped basis arrives short by a constant **5.6% of rows** —
  681, 1357 and 2720 on a family that doubles exactly — against a cap of 4.
  Neither cap shape reaches that: 5.6% is 15x D151's best relative sweep.
  What the set does say is §2's price. The three instances publishing a wrong
  basic count are exactly the three whose warm re-solve does bit-identical
  work to the cold one, and `fome21`, which publishes an exact count, saves
  **47%**. `fome13`'s over-count of 53 beats netlib's worst of 21. No source
  change (D181, `bench/measurements/02-93/`).

- **`REFACTOR_EVERY` is swept and it never had been**, which closes a standing
  debt `TODO.md` credited with three of M1's four defect closures. Six settings,
  each its own tree and its own binary: **no answer changes verdict at any
  interval**, 94 netlib and 29 infeasible instances at six settings. 64 is not
  the work minimum — 32 is 8.6% better on the geometric mean — and it stays for
  the worst case and because at 32 `pilot87` loses three orders of accuracy
  without any verdict reporting it. The sweep also publishes `pilot`'s exact
  optimum at three of the six intervals with no tolerance touched, and reaches
  D174's own 5.266e-09 at a fourth. Comment and doc only, object-identical
  (D180, `bench/measurements/02-92/`).

- **The wider-than-the-row rule the invalid-basis item asks for has a supply on
  19 of 24 instances and none at all on two.** A public-API probe counts the
  basic variables resting exactly on their own bound, which is what such a rule
  would demote. `fit1p` and `share1b` have zero candidates at every tier down
  to 1e-9 relative, so no rule of that family closes them at any tolerance. The
  probe also reaches 24 instances where 02-48 reaches 48 solves, from code the
  two share none of. No source change (D179, `bench/measurements/02-91/`).

- **`scsd1` and `degen2` do not lose the same way**, and `src/simplex.c` said
  they did. Only `degen2` is D148's guard: its settled warm point carries a
  dual violation of 12.91 and the trajectory is thrown away. `scsd1`'s guard
  never fires — it reaches a dual feasible point and genuinely runs 314
  iterations against cold's 89. So `TODO.md` §3 asked for a predictor of
  something that happens once in twenty, and eleven quantities the repair
  knows before the solve were measured against the split with no separation.
  Comment only, object-identical; the two warm records are refreshed because
  they were 24 `src/` commits stale (D178).

- **The gate's suboptimality predicate was watching 4 solves out of 110**, and
  none of Kennington's 16 — that set's worst `rsub` is `4.18e-14`, five decades
  under `RSUB_FLOOR = 1e-9`. The floor drops to `1e-16` and the reach goes to
  84 solves. The floor's stated reason is refuted by D171's own committed
  numbers: it moved 88 of 94 digests and moved `rsub` by at most 1.688x, so
  nothing would have fired at any floor at all. 1e-16 is where the headroom
  under the factor of 2 falls from 1.86x to 1.18x. 110 solution digests and 29
  infeasibility verdicts unmoved, `bench/results/*.txt` byte-identical (D177).

- **D97's §8d refusal is three to four orders wider than the hazard it
  prevents, so the design is rewritten around that.** Counted at `7c7375c`
  where the tightening exists, the refusal declines 50.2% of netlib's imposed
  bounds and 82.3% of Kennington's; the configuration it protects against —
  two columns of one row both resting at bounds that row imposed — occurs
  **12 times in 98146 opportunities**, identical at 1e-7 and 1e-6, on four
  instances in 110. **The better design, postsolve detection, is blocked**:
  the collision leaves the point one constraint short of a vertex, which needs
  a crossover, and `SPECS.md` has crossover and the primal simplex behind it
  both missing. So the first version is the refusal narrowed to equality rows,
  over-paying by three orders because this tree has no crossover. No decision
  and no source change (`bench/measurements/02-87/`, `02-88/`).

- **Presolve's objective offset is refused rather than compensated**, which
  closes at all four sites the class D168 opened. It is measurably dead:
  replacing the reduced model's whole offset with `1e300`, and then with
  `NaN`, leaves all three sets bit-identical to a control that itself
  reproduces the committed records exactly (D176). No source change.

- **`settled_objective` is compensated**, with Dekker's split, because it
  ranks two rounds and `take_best_if_better` publishes the winner — so the sum
  decides which point the caller receives. `obj_add` and `two_product_residue`
  move from `src/model.c` into `src/jaos_internal.h` as `jm_obj_add` and
  `jm_two_product_residue`, so the published objective and this share one copy.
  The naive sum's failure is a tie: a cost of 1e16 met before 256 unit terms
  publishes exactly 0.0 for two points worth 0 and 256. 139 of 139
  bit-identical, `bench/results/*.txt` byte-unchanged, and no solve on the
  three sets reaches the case — 304 settling comparisons, 0 verdict flips
  (D175).

- **`DUAL_TOL` has a sweep now**, and `docs/tolerances.md` says what it
  actually decides: not only the Harris window, but what the solve calls zero
  for a reduced cost, which is when it stops. It is the whole of `pilot`'s
  2.31e-05 — presolve and `PRIMAL_TOL` move nothing — and at 1e-9 all four
  netlib instances that publish a point off the optimum are repaired, three of
  them for less work, `pilot87` and `scsd6` exactly. The value is unchanged:
  the same setting turns the gate red on six instances (D174). No source
  change.

- **`jaos_objective` is the correctly rounded exact objective of the published
  point on 110 of 110**, worst 0.493 ulp, measured against an oracle that
  rounds nowhere. `finnis` is refused: its 7.62e-05 gap to Koch is 0.107 of
  `eps * sum |c_j x_j|`, the floor arithmetic sets for a model carrying
  3.198e+12. What the same reading found instead is `pilot`, 2.31e-05 above
  the optimum at 1.87e+08 of its own floor and the only netlib instance HiGHS,
  SoPlex and Clp all beat (D173). No source change.

- **The published reduced costs contradict the published basis on five netlib
  instances**, worst 15018.5 on `nesm`, and nothing was asking: the checker
  recomputes `d` from `y` and never reads `col_dual`, and `basis=` is a hash
  compared only against a previous hash. It is §2 and not a new defect — every
  firing instance also fails the count promise, 25 of the 27 columns rest
  exactly on their own lower bound, and `price_entry`'s naive dot product is
  refused as the explanation. What it changes is the item's stated cost, which
  was "a lost warm start" (D170).

- **Carrying a fold's error into the receiving row's window is refused**, and
  the entry is kept because the refusal is the useful part: it does stop the
  false INFEASIBLE, and then publishes `optimal` with both rows violated by 7.5
  times `CHECK_TOL`. A window decides whether to refuse; it cannot correct a
  value that is already wrong, so widening one converts a loud failure into a
  silent one (D164). The pinned change-detector it left behind is what fired
  when D165 repaired the error at its source, and it now asserts the answer.

- `TODO.md`'s published-basis figures are re-measured. netlib reads 46 solves
  wrong and 142 exact, not the recorded 48 and 140 — **it had been stale since
  the previous session** and five places cited it. Nothing in the gate could
  have caught that: `basis=` is a hash, so it detects a change and never
  reports a count. D165's compensation moved the worst over-count 23 → 18 and
  the sum +262 → +250 without changing how many solves are wrong, which by this
  item's own stated measure is not progress (D167).

### Removed

> **For whoever writes the release note:** the two `Fixed` entries about
> counting terms (D162, D163) and this `Removed` entry are the same work
> arriving and leaving inside one unreleased version. **The net change a user
> sees is one thing** — presolve's row bounds are compensated, so it no longer
> refuses models it can solve — and the four windows end where they started.
> The intermediate steps are kept here because the tests and models they
> produced still ship; the reasoning is `DECISIONS.md`'s.

- `row_shifts`, `ps_shift_excess` and `ps_end_scale`. D162 and D163 added them
  to widen four windows by the number of removals a row's bounds had absorbed;
  D165 stopped those bounds drifting, so they covered a quantity that is now
  zero. `src/presolve.c` is 196 lines shorter. It NARROWS four windows, which
  at the emptied-row test is the direction worth having — that test is the last
  word and too wide there accepts an infeasible model silently. The evidence is
  that all five tests the counts were built for still pass without them, and
  139 of 139 instances are bit-identical (D166).

### Fixed

- **The two-product could publish `inf`, and this closes the hole it shipped
  with.** Veltkamp's split rounds the high part, so `|ah|` can exceed `|a|` by
  up to 2^-26 and `ah * bh` overflows for a product within about 1.5e-8 of
  `DBL_MAX` — while the product itself is finite and both factors are three
  hundred orders below the guard on them. One column with a cost of
  1.4521649423643159e+281 at 1.237940034936653e+27 published `inf` where the
  naive sum publishes 1.7976931194842639e+308. Testing the residue instead of
  the factors covers every overflow route. `BIG` is written `0x1p996` now,
  because the decimal it carried was one ulp below it, and `FLT_EVAL_METHOD`
  is a `static_assert` — the split is silently wrong at 2, which is x87 (D172,
  found by `numerics-reviewer`).
- **D171 cost two netlib solves their valid published basis**, 46 → 48 with the
  worst over-count +18 → +21, measured at three trees. The gate reported
  `0 regressed` on it, because `basis=` is a hash that detects a change and
  never reports a count. It bought three published column values that sat
  outside their own declared bounds. Both are in the record; neither is traded
  away quietly.

- **The published objective recovers what each product lost**, which a
  compensated sum could not reach: `c_j * x_j` is rounded once before it is
  added, and the bound is `eps` times the sum of the term magnitudes. Dekker's
  split, exact under `-ffp-contract=off`, with a `2^996` overflow guard. The
  minimum model is two columns where every accumulator over the rounded
  products gives 0 and the answer is 1 (D172).
- **109 of 110 published objectives now agree with `jaos_check_solution`
  exactly** — netlib 93 of 94 against 83, Kennington 16 of 16 against 15 — and
  **nothing moves the wrong way on either set**, where D169 had four going
  further at the last bit. The one left is `finnis` at 2.2992e-08, inside the
  `long double` checker's own error of about 3.5e-07 at that cancellation.
  `gate: PASS` on all three, `0 regressed`, **0 digest changes anywhere**, 11
  netlib and 1 Kennington `obj` figures moved and nothing else.

- **The refinement residual is compensated too**, and D168's refusal of it is
  overturned by measurement. `subtract_basis_times` forms `b - B x_B` for the
  iterative-refinement step; `b` had been compensated since D168 and the
  subtraction had not. The refusal argued from the terms — they are products of
  an FTRAN output that already carries the factorization's error — which is
  true and does not follow, because the residual is what the correction is
  computed from (D171).
- Three Kennington instances published a column outside its own declared bound
  and no longer do: `pds-20` 8.81e-13 → 5.05e-28, `pds-10` 2.84e-14 → 2.52e-29,
  `pds-06` 4.26e-14 → 1.58e-30, with `rowrel`'s worst over the set going
  8.81e-13 → 8.88e-17. `gate: PASS` on all three sets, `0 regressed, 0 improved,
  0 new`, work geometric mean 1.0000x on both, 88 netlib digests and 11
  Kennington ones moved, netlib-infeas 0 digest changes. The symmetric change on
  the dual side was built in the same run and **is refused**: it changes not one
  direction count on either set.

- **`jaos_objective` is the objective of the solution the model is holding**,
  which `jaos.h` promised and neither producer kept. The sum was naive, so
  costs of +1e16, then 256 costs of 1, then -1e16 on columns fixed at 1
  published 0 where the answer is 256 — while `jaos_check_solution`, the same
  library in `long double`, read 256. And the two presolve paths reported the
  reduced model's objective rather than a sum over the values the caller reads.
  One function now does both, at all three publication points (D169).
- 81 of netlib's 94 published objectives agree with the checker exactly,
  against 34 before; 57 closer, 4 further, all four at the last bit. `gate:
  PASS` on all three sets with `0 regressed, 0 improved, 0 new` and **0 digest
  changes anywhere** — 62 netlib instances and 13 Kennington ones moved on
  `obj` and on nothing else. What is left is the rounding of each `c_j * x_j`
  to a double, which is 2.65e-05 on `finnis` against 5.1e-05 for the whole
  naive sum, and needs a two-product rather than an accumulator.

- The simplex accumulates its right-hand side with compensation. `-N x_N` is
  built by walking the nonbasic columns in column order, so a row that meets a
  large term before many small ones lost the small ones outright — each below
  half an ulp of the running total. On a model whose feasible point is exactly
  representable the `-DJAOS_NO_PRESOLVE` build read INFEASIBLE at every removal
  count; it reads OPTIMAL now, and the control 1e-2 from feasible is still
  refused everywhere. That build is the oracle every presolve entry here is
  judged against (D168).
- `gate: PASS` on all three sets, `0 regressed, 0 improved, 0 new`, 139 of 139
  `checker=ok`. netlib 69 bit-identical, 25 moved, 23 digest changes; the other
  two sets bit-identical. Work geometric mean 0.9996x, worst `pilot87` 1.0372x.
  The Neumaier step is not billed, so the cost is seconds: four bit-identical
  instances span 0.9501x to 1.0302x over two runs of the `-j 1` protocol, which
  is this host's own repeatability. `bench/results/netlib.txt` is rewritten; the
  baseline is not.

- Presolve's row bounds keep their residue. `cur_rl[i]` and `cur_ru[i]` were
  the only running sums in `src/presolve.c` without compensation; they get a
  Neumaier accumulator now, and still hold `sum + comp` after every update so
  no read site changed. This removes the error D162 and D163 widened four
  windows to cover and D164 could not repair from a window at all: the chained
  model now publishes the oracle's answer bit for bit, with both rows at
  residual zero (D165).
- **The first change in this class that moves the gate.** netlib 79
  bit-identical, 15 moved, 14 digest changes; netlib-infeas and Kennington
  bit-identical. Work geometric mean 1.0000x on every set, iteration counts and
  reduction counts identical on all 15, every one `objective=ok checker=ok
  det=ok`. `bench/results/netlib.txt` is rewritten; the baseline is not, because
  the gate reads `0 regressed, 0 improved, 0 new` against it.

- The singleton row's fold counts its shifts too — the fourth read of the same
  running difference, where D162 repaired three. `x_big + 256 columns fixed at
  2^-25 == 1e9` with `x_big` in `[0, 1e9 - 2^-17]` has an exactly representable
  feasible point and was refused; the reference build solves it and the repaired
  objective matches it to the last bit. The count takes the end
  `tightens_lo`/`tightens_hi` says the running difference supplied. It widens
  what the collapse branch below admits, which leaves `|a|` times the gap on the
  row; 0 collapses in 100018 folds over the three sets, unchanged (D163).
- Two tests D162 needed and did not have: one that fails when `ps_end_scale` is
  neutered to a constant, which D162's own test does not, and a control 2e-4
  from feasible against a window of 5.862e-5 rather than a decade away.
- 94, 29 and 16 instances bit-identical, 0 digest changes, `gate: PASS` on each.

- Presolve's three infeasibility windows count the terms subtracted from the
  row's bounds. `cur_rl[i]` and `cur_ru[i]` are a running difference with no
  compensation, so eight ulps covered about three removals and the largest row
  on the three sets carries 325. A constructed model reads INFEASIBLE at 256
  removals with an exactly representable feasible point, and stops at 128 —
  the pair that separates the two windows. The count multiplies the magnitude
  of the end being tested plus the row's traffic; scaling it by the larger of
  the two ends brings D161's defect back and D161's own test refuses it (D162).
- 94, 29 and 16 instances bit-identical to the committed records, 0 digest
  changes, `gate: PASS` on each, and the twelve genuine infeasibility firings
  in `netlib-infeas` unmoved. Widest absolute window 6.494e-08 → 6.587e-08 on
  Kennington, under `PRIMAL_TOL` 1e-7.


## [0.1.1] — 2026-08-20

Eight correctness items, D154 to D161, of which **four were wrong answers
rather than latent risks**: presolve refused three models
`-DJAOS_NO_PRESOLVE` solves, and a collapsed fold published a value outside a
bound the caller declared.

**The gate is bit-identical on all 139 instances for every one of them.** No
campaign could have found any of them, and each needed a constructed model.
That is what this tag is for: `0.1.0` names a tree that gets three LP shapes
wrong and reports the fourth outside its own promise.

Two things the reviews turned up that are not in any single entry. Eleven
findings across five reviews were all real, four of them defects in the repair
under review — a tolerance argument reads as sound in the source and is only
settled by a model. And the diagnostic probes were counting low, because
`-j N` tears one `fprintf` across children sharing a stderr while leaving the
line count intact; every probe writes one `write(2)` per record now.

### Added

- `make configs` builds all five configurations — plain, `-DJAOS_NO_PRESOLVE`,
  both fault builds and `sanitize` — each after `make clean`, and fails if any
  one does. `make` does not track a change in `EXTRA_CFLAGS`, so a second
  `make test` with a different flag silently re-runs the first binary; the
  `make clean` between runs is the whole target. Five full rebuilds, so it is
  not part of `make test` (D154).
- `.claude/skills/jaos-measure/scripts/comment_only.sh` says whether a `src/`
  edit can reach the release object, which is what decides if a campaign
  carries over. Drops `-g` and `-flto` and matches the source basename, each
  of which otherwise reports a difference that is not one (D154).

- Every debug build now checks that each published row activity matches the
  published columns, for rows whose logical is basic, to `(n-1)·eps` times the
  row's traffic. A row resting on a bound is skipped: its activity is the
  tight value, and the column sum carries the basis solve's primal residual
  instead. Validated against two injected faults — 45 of 94 and 81 of 94, both
  silent with asserts off — and clean on all 139 instances. D152's property
  survives: all 94 standard instances still run under `-UNDEBUG`. Inert in the
  release build, measured: the objects keep the parent's md5. Four earlier
  versions of the predicate each reported a defect that was not there, and
  `bench/measurements/02-62/` carries what each of them falsely said (D153).

### Changed

- The two `row_traffic` fallbacks the source called unreachable are asserted
  rather than described. One predicate, `ps_traffic_usable`, at both reads and
  at the post-loop sweep, because the sweep runs after the round loop and
  cannot speak for a read inside it. With the pre-repair accumulation restored
  netlib aborts on 45 of 94, at the sweep and not at either read — so the
  unreachability holds from the assert side too. 139 instances under
  `-UNDEBUG`, 0 aborts; the release object is unchanged (D157).
- The destroyed row width has a stated bound instead of an overstated one.
  `src/presolve.c` said a row collapsed from `[1, 2]` to a single double makes
  the answer "wrong by up to that width"; the width that dies is always below
  one ulp of the activity it constrains, because a shift small enough to leave
  a small remainder subtracts exactly. 0 of 8942 shift events lose any width
  on the three sets, and seven forced shapes are bit-identical to the
  reference build, including a surviving singleton at `a = 1e-12`. No code
  changed; the claim did (D156).

### Fixed

- The frozen-row test's window drops the row's far bound, which it took its
  number from. `-1e12 <= x0 + x1 <= 0` with both columns cost 0 in [1e-4, 1]
  is infeasible by 2e-4 and read `optimal` with x = {1e-4, 1e-4} on every tree
  since that window was written — the 1e12 lower bound bought a window of
  1.78e-3 for a test on the upper side. The same model with an infinite lower
  bound was refused correctly all along, which is the control. Narrowing a
  window is the direction that refuses good models, so: 139 instances
  bit-identical, five configurations, 227 tests, and every frozen-row test
  D159 added still passes (D161).
- The activity pass's INFEASIBLE clause gets its own window and stops refusing
  a second model the solver can solve. It shared one window with FORCING and
  REDUNDANT, where a wider window fires MORE rather than less, so only this
  clause could take one. 3307656 rows over the three sets, 0 verdicts flipped,
  the widest absolute window unchanged, 139 instances bit-identical. The
  window is `8 * DBL_EPSILON * max(1, rg.traffic, row_traffic)` — the bound
  scale is deliberately NOT in it, because the first version had it and
  published `optimal` on a model infeasible by 1e-3 (D160).
- The frozen-row feasibility test scales its window by what the comparison is
  made of instead of by the magnitude of one operand, and it stops refusing a
  model the solver can solve. `min x1 s.t. 1e9*x0 + x1 + x2 <= 1e9` with a
  frozen row and x1 bounded below by 1e-10 read INFEASIBLE where
  `-DJAOS_NO_PRESOLVE` reads OPTIMAL — an infeasibility a thousandth of one
  ulp of the row's own activity. 0 of 19114 frozen rows flip a verdict on any
  of the three sets, so the campaign cannot see it: 139 bit-identical, `gate:
  PASS` on each. The widest window this produces is 6.5e-8 against a
  `PRIMAL_TOL` of 1e-7 (D159).
- The collapsed fold's midpoint is clamped into the column's own box, which
  closes both halves of the last item with no stated error bound. A
  singleton-row fold can no longer publish a value outside a bound the caller
  declared — the overshoot was capped only by a window carrying
  `row_traffic/|a|` — and the same clamp restores the dual owner a collapse
  used to leave nameless, taking `max_dual_violation` from 1 to 0. It costs a
  doubled row residual, deliberately: the midpoint was splitting the admitted
  gap across two tolerances rather than reducing it. An inverted column box is
  left alone, because there is no box to clamp into and `jaos.h` calls that
  legal input. 0 collapses in 100018 folds over the three sets, 139 instances
  bit-identical, `gate: PASS` on each, 222 tests (D158).
- `row_traffic[i]` accumulates only the magnitude a still-finite row end
  actually absorbed. It used to add `max(|cmax|, |cmin|)` whatever happened, so
  a cost-0 singleton column with one infinite bound saturated it to `+inf` for
  the rest of the run — dead on 9008 of netlib's 16618 frozen rows, where the
  repaired value recovers `greenbea` row 57's 660. No solve moves: 94, 29 and
  16 instances bit-identical, 0 digest changes, `gate: PASS` on each. A debug
  sweep now asserts that a row with a finite end has a finite budget; it
  aborts on 45 of 94 without the repair, and it rests on measured headroom
  rather than on a structural argument that turned out false (D155).
- Three of the five build configurations did not compile: the reference build
  and both fault builds, since D151, on one unguarded test helper. Repaired,
  and two further failures behind it — D153's row-activity check aborted three
  of eight test binaries under a fault build, and `tests/test_simplex.c`
  carried no fault guard at all. All five green now; 8 of 8 off-by-one and 2
  of 2 wrong-dual negative tests still run and still pass. `presolve.o` keeps
  its md5, so the gate campaign carries over (D154).
- The singleton-column replay's comment said nothing revisits a frozen row for
  infeasibility. The frozen-row feasibility test has done exactly that since
  2026-08-14, the same day the comment was written and one commit later.
  Corrected rather than deleted, and it now names the residue that does still
  arrive: a model infeasible by less than 8 ulps of the row's bound scale,
  which is D152's clamp, 93 against 1.3e-15 (D154).
- The singleton-column replay clamps its published value into the column's
  own recorded box. Ten records across `bnl1` and `finnis` published up to
  1.3e-15 outside a bound the caller declared, which `jaos.h` promises does
  not happen. Those two instances move — digest, basis and row-relative
  residual — and the other 137 of the 139 are bit-identical; all three sets
  read `gate: PASS` with `0 regressed, 0 improved, 0 new`.
  **A whole build configuration came back with it**: all 94 standard
  instances now run under `-UNDEBUG`, where eleven aborted before, so every
  assert in the solve is exercised on them for the first time. The emptiness
  assert that blocked them is removed rather than given a tolerance — two
  windows were built and both are refuted in the entry (D152).

## [0.1.0] — 2026-08-19

The first tagged point. Everything below this heading was already written;
what the tag adds is a name for a tree whose state is known, so a later
"it worked before" has something to point at.

What that tree is, measured rather than asserted: the three gate sets read
`gate: PASS` with `0 regressed, 0 improved, 0 new` over 94 + 29 + 16
instances; `make test`, the `-DJAOS_NO_PRESOLVE` reference build and
`make sanitize` all exit 0; 151 decisions are recorded with the measurement
that closed each.

**What it does not claim.** M2, the speed milestone, is open — its criterion
is a time ratio and it has not been met. `SPECS.md` carries the feature matrix
and shows JAOS present in four of nine measured areas. Two defects are known,
written down and unrepaired: eleven of the 94 standard instances publish a
column value up to 1.3e-15 outside its own declared bound, which is why no
assert-enabled build can run them; and 48 netlib solves publish a basis whose
count is wrong. Neither makes an answer wrong today and both are in `TODO.md`
with their measurements.

### Changed

- A mapped starting basis that arrives short of `nrow` basic members is
  repaired rather than refused, by promoting row logicals — uncovered rows
  first, then fixed index order — when the shortfall is at most 4
  (`WARM_REPAIR_MAX_SHORT`, swept on both sides). Past the cap the solve
  still starts cold. Warm re-solve work falls from a geometric mean of
  0.2553 to 0.1916 on netlib and 0.0572 to 0.0070 on Kennington, with the
  worst per-instance ratio at 4.65 where the uncapped repair reached 172.03;
  `disagreed=0, rejected=0` on both sets. The three gate sets are unmoved —
  the gate solves from a fresh load and never reaches this path (D151).

### Fixed

- The solve reads its own settled dual violation before publishing OPTIMAL.
  An uncertified point from a warm start is retried once, cold, from the
  slack basis (work and clock carried); an uncertified cold start reports
  `JAOS_SOLVE_NUMERICAL_ERROR`, with no warm memory offered on either path.
  A hostile `jaos_set_basis` start made `degen2` publish a wrong objective
  as OPTIMAL on 16 of 16 trials; the 80-trial probe now reads 0 wrong, the
  three sets are bit-identical, and `jaos.h`'s promise is enforced rather
  than assumed (D146–D148). Also fixed on the way, from review: the reduced
  path stored warm memory on `NUMERICAL_ERROR`, and the interrupted
  postsolve return leaked one scratch array.

### Added

- Every optimal record line carries `basis=`, an FNV-1a hash of the
  published statuses, beside `digest=`, and the determinism predicate now
  requires both cold solves to publish the same basis bit for bit. Validated
  against the two cases it must reject, then measured: `det=ok` on all 110
  optimal gate solves and no predicate moved; the three committed records
  rewritten deliberately with the field (D150). A change that moves only
  the basis stopped being invisible to the gate.

- `test_a_long_mapped_basis_falls_back_cold`: a caller basis whose count is
  exact in the original space can map LONG onto the reduced model when
  presolve removes a row whose stored logical is nonbasic; the fallback to
  cold is now pinned, premise asserted, so any future count repair or trim
  moves a test deliberately (D145).

### Fixed

- `ps_singleton_col_swap` decides from the recovery itself — the published
  value against the record's own bounds — instead of from `sol_col_status`,
  which `JM_PS_SINGLETON_ROW`'s replay rewrites to `BASIC` on 230 netlib
  records (D140). Behaviour-identical today: all three sets bit-identical
  (110 digests and 29 verdicts unmoved over 139 instances) and the published
  basis counts read 02-48's numbers exactly. Two asserts now enforce that a
  recorded nonbasic status still matches the value.

- A restored cost-0 bounded column singleton that comes back strictly inside
  its own bounds now takes its surviving row's logical out of the basis. It
  must be `BASIC` — a nonbasic variable rests on a bound and this one rests on
  neither — and its row survives, so nothing was paying for the position. The
  partner is forced rather than chosen and the exchange pivot is `a_ij`, which
  presolve already requires non-zero. **Kennington publishes a valid basis on
  every solve now**, and netlib's worst error falls from 596 to 23 with the
  exact-count solves going 56 → 140 (D139). No value moves: `c_j = 0`, so a
  basic column needs a zero dual, which the reduced solve already had.

- A restored singleton row's basis status is decided after the replay, from
  its own dual and its final activity, on both postsolve paths. The replay
  decided it from the row's own term alone, before later records added theirs
  through `ps_row_add`. **Every under-count in the published basis is gone** —
  60 netlib solves and 16 Kennington ones — the exact-count solves go 56 → 88
  and 8 → 24, and Kennington's worst error falls from 12104 to 119 (D138). The
  gate is bit-identical on all three sets, which is the proof no value moved
  and also why it cannot judge this: the digest covers x and y, not the basis.

### Changed

- `bench/results/warm.txt` and `warm-kennington.txt` rewritten again, first
  re-measure since D138/D139. Kennington's work ratio improves 0.0873 →
  0.0572; netlib's regresses 0.0696 → 0.2553, because the now-correct
  published basis maps SHORT onto the reduced model — 35 netlib and 5
  Kennington warm solves fall back cold with an exact orig-space count,
  worst shortfall 596 members (D143). The mapping repair is TODO's item 2.

- `bench/results/warm.txt` and `warm-kennington.txt` rewritten. They were 21
  `src/` commits old and predated presolve. The work ratio reads 0.0696 on
  netlib against the old 0.0164 and 0.0873 on Kennington against 0.0041, and
  **26 netlib instances plus 6 Kennington ones now read exactly 1.0000** — a
  warm re-solve doing bit-identical work to the cold one. That is the
  basis-count defect, measured: 23 of netlib's 92 and 6 of Kennington's 11
  lose the warm start outright (D129). Not a gate regression and not a wrong
  answer; the three gate sets are unaffected.

### Fixed

- `shift_to_feasible` records what the cost actually moved by, not what was
  asked for. The two differ when `need` is below half an ulp of the cost —
  16.7% of netlib's lends — and the old record then claimed a loan that was
  never made, which forced `settle_shifts` to re-price. Two calls out of 290
  across the gate; one instance moves, `fit1d` at 0.9921x, and 110 solution
  digests and 29 infeasibility verdicts are unmoved (D128). `d[v] = 0.0` is
  untouched: removing it was measured and refused (D126).

- Three comments in `src/simplex.c` about the shift record. Two cited 186 for
  the columns whose cost moved while the record read zero; that number is 67,
  and 186 belongs to a different line of the same measurement. The third now
  says what `shift` **is** exact about — it holds every loan lent since the
  last repayment, nothing is lost from it, and it is written at three sites
  and nowhere else, measured at runtime against a dropped loan (D124).
  Comments only: 94, 29 and 16 instances bit-identical to the committed
  record.

### Added

- `publish` asserts that no loan is outstanding before it writes the duals.
  `sol_dual` is a BTRAN of `s->cost` and `sol_redcost` is `s->d`, so a loan
  left in the costs would be published; the objective never could be, because
  it is built from `m->col_cost`. On the OPTIMAL branch only — any other
  outcome never settles, is entitled to carry loans, and publishes zeros. It
  fires on none of the 128 instances an assert-enabled build can run, and a
  negative control confirms it can fire (D123). No behaviour changes: 94, 29
  and 16 instances bit-identical to the committed record.

- `docs/diagrams/` — seven files of mermaid diagrams: repo map, module call
  graph, the solve pipeline, the simplex iteration, presolve/postsolve, the
  outcome paths, and the gate. `docs/architecture.html` compiles the same set
  into one page with a short explanation per diagram. Traced from the source
  at `33bb85d`; they describe structure, own no number, and point at
  `DECISIONS.md` and `docs/tolerances.md` for everything measured. No
  behaviour changes.

- `test_the_basis_count_promise_breaks_on_a_declined_column` pins `TODO.md`'s
  own named minimum case for the basis-count defect — 2 basic against
  `num_row = 1`, and 1 in the reference build. It sits on a column the implied
  free family declines by margin rather than by order, because the file's other
  pin on that defect stops detecting anything the moment that family takes its
  column (D118). No behaviour changes.

- **A fourth instance set.** Fifteen larger models from Mittelmann's mirror,
  in three families: `pds-30` … `pds-100`, which continue the `pds-02` …
  `pds-20` already in Kennington from the same 1990 paper; `fome11` … `fome21`,
  whose first three double exactly in both dimensions; and three `nug` QAP
  relaxations, a shape no model in the tree had. `bench/fetch.sh` gains a
  `bz2-emps` pipeline and six `make plato-*` targets exist. **It is not part of
  the gate.** `pds` 8/8 and `fome` 4/4 solved, checker ok and deterministic;
  `pds` alone costs 23016 s of solve time and `pds-100` 6.425e11 work units,
  twenty times the whole standard set (D115).

- `bench/run.c` gains a third expectation, `-e noref`, for a set with no
  published optimum. Shape, checker verdict, determinism, digest and work units
  are asked exactly as for the other sets; only the comparison against a
  reference is gone, and it prints `objective=none` rather than `ok`. The
  manifest's `source` column and the flag must agree, and **both directions are
  refused before any instance runs** — the silent one is a referenced manifest
  under `-e noref`, which would stop checking Koch's optima and still print
  `gate: PASS` (`bench/measurements/02-22/`).

- **The implied free column singleton.** A column with one matrix entry, in an
  equality row that already confines it strictly inside its own box, is
  substituted out exactly and the row goes with it. Nothing is narrowed and
  nothing is published, so this is not the bound tightening D97 refused.
  Removes 1041 rows, 2040 columns and 47043 nonzeros over 17 of the 94
  standard instances, and **980 rows from `maros-r7`**, which is the figure
  stated before the code existed. Kennington is bit-identical, as its counter
  said it would be. Work over the standard set: geometric mean **0.9527x**,
  `maros-r7` 0.0156x, `greenbeb` 1.5126x (D106). `PRESOLVE_IMPLIED_FREE_ULPS`
  is its margin and is swept in `docs/tolerances.md`.

- `bench/compare` gains the **P0** rung: presolve on both sides, the dual
  forced, no crash basis, one thread. It is T0 with one line changed per
  competitor, and it is the rung the M2 gate is judged on now that JAOS
  presolves. T0 through T3 keep their definitions and their records. JAOS
  reads 4.13x HiGHS, 1.18x SoPlex and 3.50x Clp per solve, on 2.29x, 1.73x and
  2.39x the cost of an iteration, and is faster than SoPlex on 10 of 21 (D104).

### Fixed

- **A repayment restores the cost instead of subtracting the loan.** The dual
  method borrows costs to keep dual feasibility; calling one in used to
  subtract the record back out, which is not bit-exact. On `pilotnov` under a
  refused presolve candidate the loans on one column reached 1.6e+32 against a
  cost of magnitude one, 67 costs ended permanently wrong with the worst off by
  55.11, and the solve published an objective 29% off as `optimal`. A
  write-once `cost0` now holds the model's own scaled cost and both repayment
  sites restore from it; `primal_cleanup` moves the reduced cost by what the
  cost actually moved, not by the record. Gate green on all three sets;
  geometric mean **1.0001x** on netlib and **0.9975x** on Kennington; 29 of 29
  infeasible instances bit-identical; iterations move on one instance of 139;
  34 solution digests move where the round trip was not exact. `jaos-measurer`
  returned ACCEPT from its own binaries (D122).

- **`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` is green for the first time.**
  Two tests in `tests/test_presolve.c` pinned a presolved value with no
  reference-build case, so the project's only oracle for output no predicate of
  the three sets reads was red and nobody ran it. Both now assert the reference
  build's own answer beside the presolved one — 2 basic against 3, INFEASIBLE
  against OPTIMAL — rather than being guarded out, so each pair is the
  executable size of the defect `TODO.md` carries. `make test` and
  `make sanitize` are unaffected; no measured value moves.

- The baseline header named `make netlib-baseline` for every set, which was
  wrong for five of the six baselines and told a reader to run a command that
  would rewrite a different file. It now names the shape, `*-baseline`. A
  `noref` baseline also carries a line saying its `objective` column of zeroes
  means NOT VERIFIED rather than wrong. No measured value moves.

### Changed

- D97's first reopen precondition is met (D114): the over-tightening that
  refused six bound-tightening designs is derived, from an excavation of
  the minimal failing design at `7c7375c`. A forcing window scaled by the
  activity's magnitude (941.58 on a row whose bounds are 0, via a
  tightening-materialized `6.687e10` upper bound) certified 5.86 of real
  slack as binding and pinned the vertex `pilot` cannot have — no epsilon
  reaches it, and the shipped forcing family already carries the correct
  bound-scaled window. Chain, exhibit and retry requirements in
  `bench/measurements/02-21/`. The dual postsolve precondition stays open.

- `stocfor3`'s presolve gap is counted (D113): HiGHS rule ablation at the
  P0 options shows its **Aggregator** alone collapses the 8416-row
  reduction to 2859 and costs 2.31x iterations when off — 14788 against
  JAOS's 18431, near parity — while eleven of twelve other rules change
  nothing. The iteration half of the 30.0x is equality substitution, whose
  bound-transfer machinery sits behind D97; §3 now lists it as the third
  prize there. The `maros-r7` calibration found the instrument's limit:
  HiGHS's base rules cannot be suppressed. `bench/measurements/02-20/`.

- §2 closed: the unbounded-relative-widening refusal for the cost-0
  singleton column is refused on its own counter (D112). 8617 firings over
  60 standard instances, 94% on equality rows, 98.6% past the row's scale —
  the rule would be the family's off switch — and `grow7`/`grow15`/`grow22`
  carry the same maximum widening (5.524e5) with opposite outcomes, so the
  discriminator cannot discriminate. Distribution and instrument in
  `bench/measurements/02-19/`.

- §1c's precondition met: the postsolve recovery error is demonstrated on a
  constructed model — a degree-2001 equality row at traffic 2e8 publishes
  the recovered column 4.1e-6 outside its own bound where the margin
  promises 3.6e-7, an 11.4x breach, predicted bit for bit before the run.
  The case runs through `jm_postsolve_solved`, which no instance reaches.
  The settlement (compensate with `ps_acc`, or degree-scale the margin)
  stays open and is campaign-judged. `bench/measurements/02-18/`.

- §1e closed: `maros-r7`'s 15.7x per-iteration drop is the factor fill
  collapsing (D110). L falls 28.5x, the whole factor 7.9x, the fill ratio
  4.801 to 1.457 on a model that shrank 31%, with the refactorization
  cadence unchanged — measured per refactorization on both sides of D106,
  with the pre side reproducing D46's committed 4.801x as its calibration.
  The same instrument then read the comparison's tail at HEAD: `stocfor3`
  1.036 (no fill; its 30.0x is presolve's), `pilot87` 3.610, `pilot` 3.261.
  Readings in `bench/measurements/02-17/`.

- §1b closed: the implied-free window's `max(1, scale)` floor declines
  nothing — a floor-less build at the shipping 8 ulps reproduces all 94
  standard instance lines bit for bit, digests included, after proving
  itself by reproducing the committed margin-0 `maros-r7` reading (D109).
  The 1353 rows between margins 8 and 0 are declined by any nonzero window,
  not by the floor. `ULPS = 8` ships unchanged. Readings in
  `bench/measurements/02-16/`.

- §1b's blocker cleared: `d2q06c`'s 2.2163x at margin zero is four more rows
  of 2171 buying 1.7525x iterations, the D108 trajectory class, with the
  extra iterations on degraded pricing (`jm_dse_update` 0.45x per iteration
  against `jm_harris_pick` 1.42x — D63's restart mechanism). No relaxation
  defect hides in the number. The floor question in `TODO.md` §1b now stands
  alone. Readings in `bench/measurements/02-15/`.

- §1d closed: `greenbeb` pays D106's overcost in iterations (1.3779x,
  per-iteration flat) and `scfxm3` per iteration in the ratio-test path
  (`update_dual` 1.71x against an LU side at 1.05–1.11x), measured from the
  committed records and a callgrind pair calibrated to reproduce both
  records exactly. Two mechanisms, both downstream of an exact substitution,
  no site-local predictor — a trajectory refuse rule is refused (D108).
  Readings in `bench/measurements/02-14/`.

- The inequality half of the implied free column singleton is refused on its
  count (D107). A sign-classifying counter, calibrated on a hand model and on
  02-10's committed values, reads **341 sign-ok rows of the 3315 as loaded**
  — 10%, not the two thirds on record — 304 of them on the six `ship*`
  instances, none on `stocfor3`, zero on Kennington. The sign condition
  declines nothing a feasible bounded model can carry, and that is derived in
  the entry. Readings in `bench/measurements/02-13/`.

- `bench/compare`'s P0 rung re-taken after D106, at `a88e99b`: **3.15x HiGHS,
  0.95x SoPlex, 2.57x Clp** per solve, on 2.04x / 1.51x / 1.95x the cost of
  an iteration — faster than SoPlex on the geometric mean for the first time,
  11 of 20 per instance. `maros-r7` fell from 72.5x HiGHS to 1.33x; the worst
  instance is now `stocfor3` at 30.0x (`TODO.md` §5). The pre-D106 reading is
  kept at `bench/compare/results/P0-2026-08-14.txt`.

- Presolve carries its own objective, `cur_cost[]`. Every family read
  `m->col_cost` directly, so no reduction could change a cost even in
  principle; the implied free column singleton has to, because eliminating a
  column from a row pushes its cost onto every other column in that row
  (`TODO.md` §1). Nothing writes the array yet — it is a copy of the caller's
  costs for the whole run, and the reduced model, the records and the
  objective offset all read it instead. 110 solution digests and 29
  infeasibility verdicts unmoved, over 139 instances.

- Presolve is complete at five families. The three that were scoped and never
  built — duplicate rows, duplicate columns, dominated columns — are deferred
  with the count that defers them: 471 removable rows of about 297000 and 1526
  removable columns of about 1029000 across the three sets, 0.15% of each, and
  concentrated in `cre-a` and `d6cube`. The reopen condition is a model
  population where the committed counter reports more, so it is executable
  rather than an opinion (D101). Nothing is removed and no behaviour changes.

### Fixed

- The postsolve replay's accumulation of `sol_row` is Neumaier-compensated
  (`ps_row_add`, one carry per row, folded once per walk), and every reader
  reads sum plus carry — so the recovered implied-free column stops
  inheriting the running sum's `n·eps·traffic` error, which 02-18 showed
  breaching a stated bound by 11.4x the margin's promise. Verdicts,
  iterations and work units identical on all 139; 9 netlib digests moved
  where the replay's rounding lived, infeas and Kennington bit-identical;
  `jaos-measurer` ACCEPT; netlib baseline rewritten deliberately and
  confirmed (D111). The 02-18 model is pinned as a test that fails on the
  uncompensated tree.

- A removed column's share of every row it touches. `JM_PS_SINGLETON_COL` and
  `JM_PS_FREE_COL_SINGLETON` wrote their own row and stopped, so every other
  row those columns touch — all of them dead, since both families need a live
  degree of one — was left short by that column's contribution, and
  `jaos_solution` published it. No digest covers a row activity and the
  checker recomputes its own, so nothing saw it until the implied free column
  singleton read one: `greenbeb`, `modszk1` and `tuff` reported row
  violations of 900, 1.67e5 and 27.7 with no column violation beside them
  (D106).

- `make test EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` compiles again.
  `make_frozen_row_infeasible_model` had no fault-build guard and
  `-Werror=unused-function` refused the whole file, so no negative test in
  `tests/test_presolve.c` could be run at all.

- `bench/compare/run-compare.sh` prints a competitor's summary when one of its
  instances reports zero iterations. Clp does, on `d6cube`, `maros-r7` and
  `woodw`, whenever it may choose its own algorithm; dividing by that count is
  a fatal error in awk rather than a NaN, so the whole block aborted before
  its first line and Clp's summary was silently absent from T1, T2 and T3 from
  2026-08-11. The records held the data throughout. Such an instance now stays
  in the time row, leaves the two iteration rows, and the count and names of
  what left are printed. Recomputed, Clp reads 3.79x, 5.88x and 5.92x at those
  three rungs (D104).

- Presolve reads the model's sense. Every cost-direction and dual-sign rule in
  the file was stated for MINIMIZE, which is the canonical form `src/check.c`
  and `src/simplex.c` convert into, and presolve was the stage that did not
  convert. `max x1` with `x1` an empty column of cost 1 in `[0, 5]` published
  objective 0 instead of 5, and in `[-inf, 5]` reported UNBOUNDED on a model
  whose optimum is 5. netlib is entirely MINIMIZE and `tests/test_presolve.c`
  had zero MAXIMIZE cases, so neither the gate nor the suite could see it.
  110 solution digests and 29 infeasibility verdicts unmoved (D103).

- `PRESOLVE_ROUND_ULPS = 8` replaces `PRESOLVE_TIGHTEN_EPS = 1e-9` at the
  three sites that ask whether a residue is rounding, and the old constant is
  deleted rather than left unread. The window is relative, so at model scale
  1e-9 stopped being small: a row missed by 1.5 came back OPTIMAL, a column
  published a fifth of a unit above its own declared upper bound, and D102's
  frozen-row test was walked through by a model infeasible by 0.5. Nothing on
  the three sets moves, because no feasible model among them carries a residue
  at any of the three sites — the twelve that do are all on `netlib-infeas`
  and none is below 3.69e8 ulps against a window of 8 (D103).

- A relaxed row is tested for feasibility once the boxes are final, so an
  infeasible model is no longer published OPTIMAL. Both the row pass and the
  activity pass skip a frozen row, correctly, and between them nothing asked
  whether it could still be satisfied: `min x0 s.t. x0 + x1 = 100, x0 in
  [4,4], x1 in [0,3]` came back OPTIMAL with `x1 = 96` against a box of
  `[0,3]` and a column violation of 93. No digest moved on any of the 110
  optimal instances, which is the whole claim — the change can turn OPTIMAL
  into INFEASIBLE and nothing else. Work rises where frozen rows are walked
  (+111057 netlib, +68894 Kennington, +3732 infeasible), and two infeasible
  instances now leave in presolve rather than the simplex: `pilot4i` from 408
  iterations and 7063304 units to 0 and 13185 (D102).
- `bench/run` prints per-instance seconds to six decimals instead of three.
  At millisecond resolution a solve under 500 us printed `0.000` and carried
  no time ratio at all, and the ones that did print landed on so few distinct
  values that a ratio between two runs read exactly 1.0000x for reasons that
  had nothing to do with the solver: `adlittle` and `stocfor1` both read
  `0.001`, and now read 0.000710 and 0.000708. The clock already supplied
  nanoseconds. Console only, so all three records come back byte-identical.
- A folded singleton row's multiplier now goes to the row whose bound the
  column rests on. Several rows can fold into one column, the replay reaches
  them in LIFO order, and the test that chose between them read the column and
  never the row, so the first record took the whole reduced cost and the row
  that owed it published zero. Standard set 89/94 to 94/94 checker ok, which
  closes the gate; 127 of the 139 records bit-identical and `netlib-infeas`
  entirely so; no work unit, iteration count or presolve dimension moved on any
  set. Seven further records move their digest, a dual moving between two
  dual-feasible assignments, and the published basis moves on three instances
  with `warm` unmoved on all three (D100).
- A cost-0 singleton column is recovered against the row bounds recorded when
  it left, not the row's original pair. The replay is LIFO, so the columns
  removed before it had not yet added their share to the activity it was
  judged against, and the published point missed the row by exactly their sum
  (D99). Standard set 79/94 to 89/94 checker ok, Kennington 12/16 to 16/16 and
  back to no field differing from its pre-defect baseline, the 29 infeasible
  lines unmoved; no work unit, iteration count or presolve dimension moved on
  any instance of any set. Five rejections remained after it, dual-side and
  independent, and D100 closed them.
- `jm_postsolve_solved` initialises the basis statuses it publishes. The
  arrays are allocated without zeroing and no replay writes a surviving frozen
  row's status, so what went out there — and into the next solve's warm start
  — was whatever the heap held. ASan and UBSan do not see an uninitialised
  read; valgrind does, and now reports none. The value is defined and still
  not the right one (`TODO.md`).

### Changed

- The planning layer is retired (D98). The record is `SPECS.md`, `TODO.md`,
  `DECISIONS.md`, this file, `docs/` and `bench/`; the per-change process is
  the loop in `CLAUDE.md`; raw measurement records live in
  `bench/measurements/<id>/`. `.planning/` (194 files) is deleted, its open
  items moved to `TODO.md` and its owed decision entries written (D94–D97).

### Added

- Presolve and postsolve, first five reduction families: empty rows and
  columns, singleton rows, cost-0 singleton columns (D95), fixed columns,
  forcing and redundant rows, behind a cascading round loop with measured
  caps (`JM_PRESOLVE_ROUNDS = 16`, `PRESOLVE_ROUND_ULPS = 8`, each swept with
  a canary). A model presolve solves outright publishes with no simplex run.
  `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` compiles it out and reproduces the
  pre-presolve baselines bit for bit (D96). Bound tightening was built six
  ways, measured and refused (D97). The checker's remaining refusals were
  dual-side and closed by D99 and D100.
  **What it cost, presolve on against presolve off**, geometric mean of
  per-instance ratios: work 0.810x on the standard set, 0.651x on Kennington,
  0.084x on the infeasible set where ten models never reach the simplex.
  Seconds on the six instances it removes the most from, `J=1`, minimum of
  three alternating rounds: **about 0.3x** against a negative control that
  reproduces at 1.0. An independent re-run of the same protocol reads
  0.3066x and 1.0012x, so the centre holds and four significant figures do
  not (D103). Two instances get much worse and are handed to `TODO.md`: `grow22`
  11.16x work and 2179 to 16381 iterations, `grow7` 8.56x and 544 to 4804.

- `jaos_solve_time`: seconds the last solve took. `SPECS.md` had carried this
  as missing since M1 while the premises required it — every run reported wall
  clock and the library did not, so anyone who was not JAOS's own bench runner
  could not get the number the whole competitive comparison is about. It
  covers the same span as the work counter, taken beside it at the end of
  publishing so neither is short by one BTRAN, and it shares one clock reader
  with the time limit so the figure a caller sees and the figure the budget
  was judged against cannot drift. Retired with the rest of the answer when
  the model is modified: seconds for a withdrawn result are a number about
  nothing. **The only non-reproducible number in the API**, and the header
  says so and says not to diff it against a stored one.

- `jaos_set_progress_callback`: a solve can be watched, and stopped. A watcher
  is told the iteration count, the work units and the total primal
  infeasibility — and no objective, because a dual simplex carries a point it
  cannot vouch for until it finishes and this library does not hand back
  numbers it will not stand behind. It may look and it may stop; it may not
  steer, and it may not touch the model. Asked every 64 iterations and never
  on a clock, so *when* it is asked is reproducible and the same answers give
  the same solve bit for bit — a callback that always continues returns the
  same bits as no callback, over all 139. A stop is the new
  `JAOS_SOLVE_INTERRUPTED`, appended to the enum rather than inserted, and it
  keeps the basis it stopped on, so solving again resumes instead of starting
  over (D79).

- `jaos_add_rows`, `jaos_add_cols`, `jaos_delete_rows` and
  `jaos_delete_cols`: the problem itself can grow and shrink, not only its
  numbers. Additions append, so no existing index moves and the prefix of
  every array copies straight over; a new row is a transpose rather than an
  append, and counting per column first makes it one rebuild instead of one
  insertion per entry. Deletion takes a **set** of indices, because deleting
  one at a time leaves the caller tracking a renumbering that shifts under
  them — given the set, JAOS renumbers once and refuses an index named twice.
  The stored basis survives exactly when what is left is still a basis, which
  is the one rule `jaos_set_basis` already enforces: rows arrive basic and
  columns nonbasic, so additions keep it and deletions usually do not. All 139
  digests unmoved — no path the gate walks calls any of this (D77).

- `jaos_check_report` gains `certified_suboptimality` and
  `unquantified_rays`: how much better the objective provably gets, and how
  many directions could not be quantified. The step comes from moving one
  column on its own with every other variable pinned — feasible by
  construction, so it is a guaranteed minimum and `|w| * t` is a certified
  lower bound on `P - P*` needing **no basis, no factorization and no
  reference value**, which is what D47 costed this route at. It recovers D47's
  constructed case exactly, 0.1, on a point where every other number reads
  zero. Largest certificate over all 110 reference answers: 4.98e-16, so it
  does not false-alarm; five instances carry a ray whose rate the checker
  calls zero, and those are counted rather than reported as infinite
  suboptimality. Decides nothing, and on the evidence it is not fit to: on
  `pilot` at the refactorization intervals where this solver is known to
  return an answer 1.04e-3 off with the checker green, it reads the same
  ~1e-25 it reads on the correct answers. A column moving alone is stopped by
  the first tight row and a vertex is what tight rows are, so it cannot fire
  where simplex answers live. Sound, and uninformative (D73).
- `jaos_check_report` gains `gap_certified` and `max_dropped_multiplier`: the
  checker now says when `gap_positive` is not the bound on suboptimality that
  `jaos.h` documents it as being. It stops being a bound whenever a
  multiplier's sign points at an infinite bound, because the term it owes the
  dual objective is minus infinity and gets dropped — two variables and one
  constraint build a point that is arbitrarily suboptimal with every number in
  the report reading zero. Neither field decides anything and no verdict
  moved: three gates PASS, 0 regressed. **98 of the 110 accepted answers carry
  a dropped term**, against the 15 D47 estimated, and not one of them reaches
  1e-6 — the whole exposure sits inside what the checker already calls zero
  (D71).

- **Resumable budgets.** A solve stopped by a work or time limit keeps the
  basis it stopped on, so raising the limit and calling `jaos_solve` again
  continues instead of starting over — until now the run's work was thrown
  away, which made a budget a way to abandon work and nothing else. An
  INFEASIBLE or UNBOUNDED verdict keeps one too, for the branch-and-bound node
  that differs by one bound. A numerical failure keeps none: it cannot corrupt
  an answer, but it is the one state the solver does not vouch for. There is
  still nothing to read in between, and `jaos_basis` still refuses — a
  stopping point is not a solution (D70).
- `make warm` and `bench/warm.c`: what warm re-solve buys, which the gate
  cannot say — the gate solves each instance once from a fresh load, and that
  is the one case warm starting does not touch. One branch-and-bound branching
  step per instance, warm against cold on the same perturbed model:
  **0.0055 of the iterations and 0.0166 of the work** over 92 of the standard
  94, and **0.0006 and 0.0041** over 11 of Kennington's 16 — geometric means,
  no disagreements and nothing the checker refused on either set. `grow15`
  takes 1 iteration against 20305, `cre-b` 1 against 17132, and the work ratio
  improves with model size because a warm solve's floor is two
  refactorizations however big the problem. Three checks before believing it —
  the cold number matches the gate's own iteration counts, the branch moved
  the optimum on 85 of 92 and 7 of 11, and every warm answer was verified
  independently. `make warm-kennington` runs the second set (D69).
- `jaos_col_cost`, `jaos_col_bounds` and `jaos_row_bounds`: the model can be
  read back, not only written. A caller who loaded the model knows what is in
  it; one who read it from a file does not, and being allowed to change a
  bound with no way to see it is not an API. The first program to need them
  was JAOS's own campaign, which cannot take a branching step without knowing
  whether `floor(x_j*)` is still above the column's lower bound (D69).
- **Warm re-solve.** A solve that reaches an optimum leaves its basis on the
  model, and the next solve starts from it: change a bound and solve again,
  and nothing has to be called. The basis is stored apart from the answer for
  exactly this reason — the answer is discarded when the problem moves, the
  basis is not. `jaos_set_basis` hands one in from elsewhere and
  `jaos_clear_basis` forgets it, without which one solve would make every
  later solve a re-solve for good. A basis that is wrong costs iterations and
  never the answer; one that would leave a nonbasic variable with no bounds was
  refused and the solve ran cold, because that pins a row's activity at zero
  and the method could not always price it back off — **that refusal is gone
  as of D90**, once D85 taught the clean-up which way a free column improves.
  All 139 answers unmoved (D68).

- `jaos_set_coefficient`: one matrix entry can be changed, created or
  removed. Three operations under one name, because the stored matrix keeps
  its columns ascending by row index with no duplicates and no explicit
  zeros — so zero deletes the entry and a new index inserts one in sorted
  position. Unlike a bound or a cost it invalidates the row-wise mirror and
  the scaling, both of which are computed from the matrix (D67).
- `jaos_set_col_cost`, `jaos_set_col_bounds` and `jaos_set_row_bounds`: a
  loaded model can be changed instead of rebuilt. Each one discards the
  answer the model was holding — an optimum computed for the problem as it
  stood describes a different problem once a bound moves, and reading it back
  would be a wrong number returned with confidence. Neither touches the
  matrix, so the scaling and the row-wise mirror stay correct and are left
  alone. `lower > upper` is accepted and comes back infeasible, which is the
  rule `jaos_load_lp` already applies (D66).
- `jaos_set_log_callback` and `jaos_set_log_level`: the solver stops being
  silent. Four levels, and no default destination — nothing is written until
  a callback is installed, because a library that writes to stdout cannot be
  embedded. Paced by iteration count and never by a clock, so two runs of one
  model produce the same lines. The closing line reports refactorizations,
  weight restarts and stalls: the three events four diagnoses this milestone
  had to patch counters into the solver to see. Solving at full verbosity
  returns the same bits as solving silently, checked bit for bit and over all
  139 instances (D65).
- `jaos_set_primal_tolerance` and `jaos_set_dual_tolerance`: the first two
  settings a caller has beyond the budgets. Both default to 1e-7 and 0
  restores that; a value that is not finite and non-negative is refused with
  a reason rather than clamped. All 139 answers are unmoved, because a model
  that sets nothing behaves exactly as before. The API configures the
  contract — precision, limits, where output goes — and never the method:
  which pricing rule, when a weight stops being worth carrying, when to
  refactorize are the solver's to decide (D64).
  it, and compiles again from the profile that produced. Worth **1.1122x**
  over the plain shipping build — three times what every optimisation flag in
  it is worth put together — with the gate green and all 94 digests unmoved.
  Kept out of `make` because it takes minutes and needs the instances
  downloaded first (D62).
- `-j N` on the acceptance runner and `J=N` on every netlib make target: the
  instances are solved N at a time, one process each, and the record is
  reassembled in manifest order. All 139 lines come out byte-identical to the
  sequential record. The standard set takes 84 s instead of eight minutes,
  the infeasible set 9 s instead of two and Kennington 8 min 21 s instead of
  thirty. The seconds printed alongside are
  inflated by the contention and the runner says so on every parallel run
  (D57).
- `jaos_basis` reports where every column and row activity rests in the basis
  behind the answer — basic, at a bound, or free. It is the part of a
  solution the values cannot carry: a basic variable sitting exactly on a
  bound reads no differently from a nonbasic one resting there, and only one
  of the two is a constraint the optimum is held by. PLAN 2.4 promised it and
  M1 had not delivered it (D33).
- A primal ratio test, used only to clean up after the dual solve. `greenbea`
  goes from REJECTED to checker ok in eight pivots, at fifteen significant
  digits of Koch's value, and `pilot`'s objective comes inside tolerance from
  390x outside it. Grows M1's declared scope by that ratio test and nothing
  else; the primal simplex stays out (PLAN 2.1). 0 regressed, 2 improved on
  the standard 94; 0/0/0 on the other two sets (D28).
- Bland's rule as a fallback a detected cycle switches on. `grow15` had been
  running to the iteration guard at 189201 iterations and now solves in 21653.
  Costs nothing where nothing cycles: `grow22`, `grow7` and `truss` are
  bit-identical (D26).
- The dual simplex re-enters from the settled point, moving a nonbasic to its
  other bound and running again. `nesm` goes from REJECTED to checker ok for
  seven iterations; `pilot` and `pilot87` improve two orders of magnitude on
  the dual violation. 0 regressed, 1 improved; 0/0/0 elsewhere (D25).
- Three fields in `jaos_check_report`: `gap_positive` and `gap_negative` split
  the gap into the two sums it is the difference of, and
  `max_row_violation_relative` reports the row residue against the row's
  traffic. No verdict reads any of them. The record carries all three per
  instance (D24).
- A fuzzer for both readers, `tests/test_fuzz.c`: truncation at every offset
  of every corpus file, seeded edits, random bytes, keyword salad, and named
  edge cases, each fed to both readers. 11543 cases in the suite, 1.6M under
  `JAOS_FUZZ_SCALE=200`, clean under ASan+UBSan. Closes the M1 gate's
  condition 4, which had been asserted and never tested (PLAN 2.9).
- Baselines for the Kennington and infeasible sets, with
  `make netlib-kennington-baseline` and `make netlib-infeas-baseline`. Both
  gates report PASS, which is the state in which a summary line cannot show a
  change (D21).
- `bench/run.c` takes `-e infeasible`: shape and determinism still hold, the
  reference objective and the checker drop out, and an instance returning an
  optimum fails as a false optimum. The only check in M1 that looks for a
  wrong answer rather than confirming a right one (PLAN 2.9).
- The two remaining M1 instance sets, pinned and running:
  `bench/netlib-kennington.manifest` (16) and `bench/netlib-infeas.manifest`
  (29), expanded with netlib's `emps` — fetched and checksummed by
  `bench/fetch.sh`, never stored here (PLAN Q6, D11). Kennington passes 16 of
  16 on the first run; the infeasible set refused 28 of 29 with no false
  optima. Each set fetches into its own directory, because `greenbea` names
  two different models across sets.
- `bench/netlib.baseline` records what every instance did, and `make netlib`
  diffs against it. The gate's own verdict read `NOT MET` for almost the whole
  of M1 and reads `PASS` now, and at neither end can it answer the question
  every change raises — did this make anything worse? Work may grow 2x before
  it counts as a regression.
  `make netlib-baseline` rewrites it and is never a side effect of running
  the gate (D21).

### Benchmark harness

- **Rungs T1 to T3, and Clp as a third competitor.** The ladder now says what
  the missing features are worth: free algorithm choice **nothing**, on
  iteration counts identical to being forced; presolve **1.42x** against HiGHS
  and **1.14x** against SoPlex; stock defaults nothing further (D81). Clp
  builds through its own pinned CoinUtils and Osi chain, and its reading is
  the point of having a third: the rivals disagree about JAOS's iteration
  count — 1.47x, 0.70x, 1.67x — and agree about the cost of an iteration —
  2.54x, 1.92x, 2.26x (D83).

- **The comparison was timing a warm re-solve.** It solves each model N times
  and keeps the fastest, and once a solve began leaving its basis behind every
  repeat after the first re-solved warm in one iteration and the minimum
  picked it — `25fv47` recorded 0.0015s and 0 iterations against a true 0.49s
  and 9459. The basis is cleared before each timed solve now, and the driver
  checks itself: cold repeats are bit-identical, so a disagreement in
  iterations or work aborts rather than being averaged in. No library code was
  involved and no committed figure was affected (D80).

- `EXTRA_CFLAGS` on the release and dev builds, empty in every shipping build,
  so a method constant can be swept over a range without editing the source
  between runs.

### Changed

- **The dual ratio test's dense scan walks the nonbasic set** rather than every
  variable, and bills what it visited rather than the dimension. The trajectory
  is identical on all 139 instances — 110 digests and 29 infeasibility verdicts
  unmoved — so the work counts fell because the charge was redefined underneath
  it, not because the solver took a different path. It costs 1.60% more
  instructions on `truss` and buys no time this host can resolve (D93).

- **The checker separates the suboptimality bound from the verdict**, which is
  what closes D47. A bound implied by the rows is sound but slack, so its term
  in the duality identity survives at an optimum and measures the bound rather
  than the point; feeding that to `dual_feasible` rejected correct answers, so
  the verdict now reads only terms from bounds the model declared and the
  bound keeps every term. With that the propagation is safe to iterate, and
  certification goes from 12 of 110 accepted answers to **64** — `etamacro`,
  the case D47 named as live at 2.25e-07, is now certified with nothing
  dropped. New `relative_suboptimality` reads 6.9e-05 where `pilot` is right
  and 5.02e-03 where it is wrong; the gate watches it in place of the dropped
  term, which propagation has reduced to noise (D91).

- **A warm start no longer refuses a basis with a free nonbasic.** D68 put
  that refusal in because the method could not price such a column back off
  zero and would publish a suboptimal point as OPTIMAL; D85 repaired that, so
  the refusal lost its premise. `cycle` goes from paying the full cold price —
  1537 iterations, its warm figures identical to its cold ones — to 16
  iterations and a ninth of the work. All 139 answers unmoved, since no cold
  solve reaches this code. The test that pinned the refusal now pins the
  repair, on D68's own example (D90).

- **The re-entry loop publishes its best round rather than its last.** On a
  trajectory that oscillates the two are different, and which one got
  published was decided by where `SETTLE_ROUNDS` happened to fall. "Best" is
  lexicographic — a round whose dual violation is inside tolerance beats one
  that is not, and only then does the lower objective win — because the first
  criterion tried, plain lowest objective, publishes a point the independent
  checker rejects. Both quantities are compared in the model's own space,
  since the scaling factors are per column and differ between the rounds being
  compared. `pilot87` at a refactorization interval of 24 now publishes a
  point whose dual violation the checker reads as zero, for 2.9e-9 relative of
  objective. No answer in the gate moves: at the shipped interval the loop
  converges and ends on its own best round (D89).

- **The acceptance gate watches the checker's dropped term**, which is the one
  correctness quantity no predicate covered. `checker` stays green while the
  guarantee behind it stops holding — that is the whole of D47 — and D82 is
  the receipt: a change that published an answer out of tolerance on `pilot`
  passed this gate. Judging a dropped term needs knowledge the checker does
  not have; noticing it *grow* does not. Baselines carry it, growth past 2x
  above a floor of 1e-9 is a regression, and both constants are measured: a
  solver change that moved no answers moved 0 of 94 dropped terms, while the
  case this must catch is a factor of 40. Calibrated by injecting a fault and
  confirming it is caught. Older nine-field baselines still read (D88).

- **The checker bounds unbounded variables by what the constraints imply**,
  so a dual term that used to be minus infinity — and was dropped, taking
  `gap_positive`'s guarantee with it — is finite wherever a row bounds the
  column with the others inside their boxes. An implied bound is satisfied by
  every feasible point, so adding it changes neither the feasible region nor
  the optimum, and a dual bound for the tightened problem holds for the
  original. Certification roughly doubles, 12 of 110 accepted answers to 27,
  and D47's constructed case goes from every number reading zero on a point
  0.1 from optimal to refusing it with `gap_positive` at exactly 0.1. All 139
  digests unmoved. **It does not close D47:** `pilot` at intervals 24, 32 and
  96 still reads green on an answer 1.04e-3 out of tolerance, because no
  single row bounds the column that costs it (D87).

### Fixed

- **A residue only a primal pivot can remove is no longer hidden by the
  scaling.** `wants_a_pivot` and the primal clean-up's re-check asked for a
  sign breach past `DUAL_TOL` in the solver's scaled space, while the answer is
  judged in the model's own: on `pilot87` a logical with a reduced cost of
  6.53e-09 scaled carries **1.67e-06 published**, because its row's scale
  factor is 256, and the point went out as OPTIMAL with the checker refusing
  it. They ask for a breach in **either** space now — the union and not the
  published reading, because substituting drops twenty-six candidates across
  that instance's three solves to gain two. `pilot87`'s gate answer and its
  warm re-solve are bit-identical and `etamacro`'s certificate tightens 14x;
  92 of 94 standard instances are untouched and all three gates read 0
  regressed (D92).

- **`bench/warm.c` ran the independent checker on the warm answer only**, so
  the perturbed model's cold solve was the only published answer in this
  repository that nothing judged — and it was the one that was wrong. It judges
  both now and names which side was refused. Its parallel path also parsed a
  failure note with `%79s`, so every note containing a space reached the
  summary as its first word (D92).

- **A pivot is refused when the factorization contradicts itself**, which is
  the stability trigger PLAN 2.5.5 asked for and never got. Each iteration
  already computes the pivot element twice — `alpha_q` by BTRAN, `col[r]` by
  FTRAN — so the check is free; the entering column is now transformed before
  anything is mutated, which is what leaves an iteration to abandon. Past
  `LU_AGREE_TOL` the pivot is declined, unbilled, and a rebuild asked for,
  unless the factorization was already fresh — where the basis itself is the
  problem and declining forever would be the only alternative to proceeding.
  `pilot87` at a refactorization interval of 128 went from tripping the
  iteration guard after 1,382,801 iterations to OPTIMAL in 214,631; all 139
  digests unmoved, because no pivot in the gate comes within four decades of
  the threshold (D86).

- **A nonbasic free variable with a negative reduced cost is no longer
  invisible**, which could publish a suboptimal point as OPTIMAL. Zero is the
  only dual feasible reduced cost for a variable with neither bound, so it is
  the one status whose cost can be wrong in either direction; `wants_a_pivot`
  and `primal_ratio_test` both read the status instead, which puts a free
  variable in the upper-bound branch and repairs one sign only. Both now read
  the sign of the reduced cost, which is bit-identical for a bounded status —
  `dual_breach` has already fixed the sign there — and all 139 answers, work
  units and iteration counts are unmoved. On the constructed model the answer
  goes from 0.0 to -6 (D85).

- **Loading a problem no longer discards the logging callback.** A model
  configured before it was loaded lost `jaos_set_log_callback` and
  `jaos_set_log_level` in silence — the natural order to write it in, and
  broken since logging landed. The list of settings that survived a load was
  the defect rather than its contents: the comment beside it had already
  warned that a setting added without being added to the list would be lost,
  and that is exactly what happened to the next setting added. Configuration
  is now one sub-struct that the load saves and restores whole, so there is
  nothing left to forget. All 139 answers unmoved (D78).

- A solve that failed reported none of what it did. The three counts —
  refactorizations, weight restarts, stalls — were logged on the success
  branch only, which is the branch nobody has to investigate. And the
  iteration guard said how many iterations it had taken and nothing else; it
  now also says how long the total infeasibility has stood still and whether
  Bland's rule was on, which is the difference between a cycle the
  anti-cycling rule never caught and one it caught and could not finish.
  Establishing which of those `pilot87` was took two runs and a throwaway
  build (D72).

- `docs/tolerances.md` listed `pilot` and `pilot87` as refused by the checker
  and counted "92 of 94 instances" green. Both closed two decisions ago —
  `pilot` by D29, `pilot87` by D30 — and the document's own next paragraph
  narrated the second repair while the table above it still called it
  outstanding. All 94 are green. In a repository whose first rule is that the
  design is written down and must not be reconstructed from the code, a
  document that contradicts both the code and itself is the expensive kind of
  defect (D71).

- A basis of nothing but structurally empty columns made `jaos_solve` fail
  instead of reporting the model infeasible. `refactorize` asked for capacity
  equal to the basis's nonzero count, and a request of zero leaves the arrays
  unallocated, so the factorization was handed a null index array and refused
  it as bad input. Unreachable from the slack basis, where every logical
  carries an entry; a warm start reaches it by keeping a column basic after
  the last coefficient in it is deleted (D68).

- A valid model could come back as `JAOS_ERR_INVALID_INPUT` — the code the
  library reserves for a caller's mistake — from inside `jaos_solve`.
  `pivot()` reports a failed basis update by asking for a rebuild and
  returning success, and `primal_cleanup` was the one loop that pivoted
  without reading that flag: both triangular solves return without writing
  once the factorization is wrecked, so the ratio test and pricing row that
  follow were computed from whatever the buffers last held, and the update's
  own guard was what stopped it. The loop now leaves and lets the caller
  refresh. `pilot` at a refactorization interval of 48 goes from that error
  to `optimal`; the other eleven cells of the same sweep and all 139
  reference instances are unchanged, digest for digest (D48).

- A model with a finite optimum could be reported `INFEASIBLE`. The verdict
  is reached when no pivot clears `PIVOT_MIN`, and those are exactly the
  magnitudes that drift in a factorization patched by many updates — so
  optimality was being re-checked against a fresh factorization (D20) while
  infeasibility was accepted the first time it was reached. It now gets the
  same second opinion. Costs 0.04% and moves no iteration count; the 29
  genuinely infeasible models are still refused, 29 of 29 (D39).

### Changed

- The shipping build is `-O3 -flto` instead of `-O2`, and there is one set of
  flags rather than the two the plan proposed. Measured over the whole
  standard set with every verdict, iteration count and digest unmoved: `-O3`
  1.0055x, `-flto` 1.0330x, `-march=native` 1.0072x on top of LTO — noise,
  and it makes the archive undistributable, so it stays as `NATIVE=1`.
  Removing the work counter and the clock check was measured too, at 0.987x
  and 1.004x: the public budget API is free (D62).
- The LU's elimination walks each column once, in place, instead of copying
  it into a dense buffer and back out. The multipliers belong to the pivot
  rather than to any one column, so they are scattered once per pivot and
  every column of the pivot row meets them where it stands. On `maros-r7`
  those two copies were 42% of the whole program against 6.5% for the
  subtraction they carried. `maros-r7` 26.965 s -> 22.635 s (1.191x), and
  **1.569x against where M2's speed work started this session**. All 139
  digests, iteration counts and work units unmoved; clean under ASan and
  UBSan (D59).
- The LU's elimination asks for a column's capacity once per column instead
  of once per entry it writes. On `maros-r7` that append ran 1,552,126,296
  times for a quarter of the whole program's instructions, against 2.9% for
  the one line in the same loop that bills a work unit. `maros-r7` 35.512 s
  -> 27.711 s (1.281x) and `pilot87` 32.233 -> 27.562 (1.169x) — the two
  instances that carry 74.1% of the standard set's work — with all 139
  digests, iteration counts and work units unmoved (D58).
- The elimination stops rebuilding a column when there is nothing to
  eliminate. It scattered every column of every pivot row into a dense
  buffer and pushed the survivors back regardless of what the pivot column
  held — and on a basis that is already triangular it holds nothing, so the
  pass was copying each column onto itself. On `fit2p` that was 97% of
  pivots and 344 million appends per eleven factorizations. **`fit2p` goes
  from 8.25 s to 2.46 s**, and with the entry below, from 12.87 s to 2.46 s.
  Surgical by construction — `maros-r7`, the highest-fill instance, moves
  1.02x. All 139 reference instances identical, digest for digest (D56).
- Appending to a sparse vector tests its own capacity instead of calling
  across a translation unit to be told there was room. `jm_svec_push` called
  `jm_grow` twice per element and `jm_grow` lives in `util.c`, so in the
  build JAOS ships — `-O2`, no LTO — that was 63% of every instruction
  executed on `fit2p` and charged no work unit at all. **`fit2p` goes from
  12.87 s to 8.42 s and `maros-r7` from 50.5 s to 36.7 s**; under `-flto` it
  changes nothing, because the compiler was already doing it. All 139
  reference instances identical, digest for digest (D55).
- The FTRAN reports where its answer is nonzero too, and the steepest-edge
  recurrence and both updates of `x_B` walk that instead of every row. This
  one needed no ordering, because all three readers are elementwise — which
  is why its threshold sits at half the other two. Work falls **1.557x on
  Kennington**, 1.010x on the standard set and 1.037x on the infeasible one:
  the largest single entry of M2 so far, with no digest and no iteration
  count moved (D44).
- The BTRAN reports where its answer is nonzero, taken from the pass that
  was already visiting every slot to permute it back, and pricing walks that
  instead of scanning the whole pricing row. On Kennington that scan was 27%
  of everything the solver billed. It needed a second loop of the same
  length to go with it — the reset of basic slots — because one charge had
  been standing for both. Work falls **1.255x on Kennington**, 1.007x on the
  standard set and 1.008x on the infeasible one; **all 139 instances get
  cheaper and none gets dearer**, with no digest and no iteration count
  moved (D43).
- The exact steepest-edge weight is summed over the pattern of the row it is
  the norm of, instead of over the dimension. That pattern costs nothing to
  have: pricing already walks the whole of `rho` in ascending order looking
  for rows to skip. Work falls **1.208x on Kennington**, 1.009x on the
  standard set and 1.025x on the infeasible one — all 139 instances cheaper,
  no digest and no iteration count moved. The other half of the same charge,
  the steepest-edge update itself, needs FTRAN to hand over a pattern and is
  still open (D42).
- The dual step walks the pricing row's pattern as well, which is the other
  and larger half of the same idea. It needed one thing the ratio test did
  not: the loop also repairs reduced costs that have drifted past their
  bound, so skipping a variable is only safe where that repair would have
  done nothing. `duals_dirty` names that condition and the two places that
  break it pay one full sweep to restore it. Work falls **1.451x on
  Kennington**, 1.015x on the standard set and 1.015x on the infeasible one,
  again with **no digest and no iteration count moving**. Kennington is now
  1.895x cheaper than when M2's pricing work began (D41).
- The pricing row is read through its pattern rather than in full wherever
  that pattern is small. `alpha = rho' M` is dense storage holding a sparse
  vector — 0.1% of the variables on `ken-18`, which is two thirds of the
  Kennington set's dense sweeping on its own, against 83.6% on `osa-60` —
  and both the ratio test's scan and the clear that starts the next
  iteration walked every slot to find the few that were there. Work falls
  **1.306x on Kennington**, 1.014x on the standard set and 1.012x on the
  infeasible one; 138 of 139 instances get cheaper, none gets dearer, and
  **no digest and no iteration count moves anywhere** (D40).
- BTRAN solves only the slots that can produce a nonzero, found by searching
  the factor's dependency graph instead of computing zeros to discover they
  are zero. 96.7% of them are. Work falls 1.040x on the standard set, 1.095x
  on the infeasible one and 1.057x on Kennington — `ken-18` included — with
  **no digest and no iteration count moving anywhere**, because the skipped
  slots are exactly zero rather than nearly zero (D38).
- A published zero is now always `+0.0`. `-0.0` is the same number and every
  check agreed it was, but it made two identical answers differ in bytes —
  which is what a solution digest compares. Normalising moves 90 of 94
  digests and not one work unit, and without it D38 could not have been told
  apart from a change that alters answers (D37).
- Pricing walks the matrix by row instead of by column, so a zero in the
  pricing row skips a whole row of the matrix rather than being spread
  across every column that touches it. Total work over the 110 solved
  reference instances falls 1.19x, 96 of them get cheaper, the best is 2.21x
  and the worst 14 pay up to 5% — and **not one solution digest or iteration
  count moves**, on any of the three sets (D35).
- The claim that a different C library cannot change a JAOS answer is
  measured rather than argued. `log2` in the scaling was the only libm result
  IEEE leaves unpinned anywhere in the solver; perturbing it moves no scale
  factor of the 139 instances until the offset is about 4x10^8 ulps, and the
  probe was shown to fire before its silence was believed (D34).
- The work-unit table loses its per-iteration row. The weight is zero and it
  is measured, not assumed: the basis update turns out to be 1.8% of an
  iteration rather than the whole of it, and the other 98% is already charged
  by dimension. No figure anywhere moves — this is an attribution of the
  units, not a reweighting of them (D32).
- The re-entry weighs a column on what its wrong sign costs the objective,
  not on the size of the breach. `etamacro` goes from REJECTED to checker ok
  for 9 extra iterations; every other instance of the standard 94 is
  bit-identical and `pds-20` costs one iteration more. Two earlier forms of
  this were measured and left out, one of them costing `pds-20` 3.2x — both
  in D27.
- The checker's bound-proximity test scales with what the value being tested
  is made of: the window is `tol * s`, with `s = max(1, sum_j |A_ij x_j|)`
  for a row and `max(1, |x_j|)` for a column. Row 3 of Netlib's `finnis`
  carries 4.0e10 of traffic and was being asked to rest within 1e-6 of a
  bound where one ulp is 7.6e-6. `finnis` goes from REJECTED to checker ok
  and nothing else moves: 0 regressed, 1 improved, 0 new on the standard 94,
  0/0/0 on the other two sets. D23 carries the identity that makes the
  loosening safe and the case it must still reject; `docs/tolerances.md`
  carries the formulas.
- The checker's primal feasibility test stays absolute. Extending D23 to it
  was measured and refused: primal feasibility is the hypothesis of the
  identity D23 rests on rather than a test beside it, and exactly one
  instance of the 94 exceeds 1e-6 on a row. D24 has the reasoning, and the
  `Q`/`N` instrumentation the argument turned up as pending work.
- The checker's relative gap is scaled by `1 + |primal| + |dual|` rather than
  `max(1, |primal|)`, which reported a larger error the further the two
  objectives were apart. This is the PDLP form HiGHS adopted. No verdict
  moved on the Netlib set, which is the only reason it is in.
- The checker no longer drops a small multiplier from the dual objective.
  Below tolerance it is still held to no sign condition, but it contributes
  `w * bound` like any other, because what gets dropped is small only if the
  bound is. That is what had been rejecting `pilot-ja`, whose dual violation
  is exactly zero; checker-green went from 86 to 87 with nothing else moving.
  D22 has the reasoning, including two repairs that were measured and
  rejected first — one of which passed the entire gate while being vacuous.
- The solver core is back to where it was on 2026-08-06, reverting ten
  commits that had not been run against the Netlib set. Measured per instance
  rather than by the summary line, they fixed `grow15` and `nesm` and cost
  `pilot-we` (a feasible problem reported infeasible), `pilotnov` and
  `grow22` (2179 iterations to 167865). The summary line was identical before
  and after, because the gains and the losses cancelled.
- A model whose optimum lies beyond the bound dual phase 1 lends is refused
  with a numerical error naming the column, rather than answered. Reaching
  such an optimum needs a phase 1 that lends nothing. No instance of the 139
  ever reached the lent bound, so the loan size is a performance parameter and
  not a correctness risk, and Q9 closed on that (D31).
- `jaos_solution` refuses to report anything for a solve that found no
  optimum, under the same rule as `jaos_objective`: a buffer of zeros cannot
  be told apart from an answer that is genuinely zero.
- Builds pin `-ffp-contract=off`. C23 lets the compiler fuse `a*b+c` where
  the hardware offers it, which would make the same model produce different
  bits on different machines — the opposite of what D8 promises.
- `docs/plan-m2.md` is now `docs/research/plan-m2.md`, alongside the notes it
  is built on. It is a proposal; `PLAN.md` is the plan. Its speedup figures
  come from the literature and none has been measured in JAOS (D17).

### Fixed

- The primal clean-up decides which columns want a pivot before it moves any
  of them, and calls in each candidate's own cost loan before judging it. It
  had been able to take exactly one pivot per call: `wants_a_pivot` reads the
  duals out of `rho`, which the first pivot overwrites with a pricing row, and
  underneath that `pivot()` lends away every other candidate's sign condition.
  `pilot87` goes from an objective `2.28e-3` out to `1.33e-7` relative, its
  dual violation from `1.87e-5` to `0`, and it gets cheaper. **The standard
  Netlib gate now reports PASS** — 94 of 94 on every condition. 0 regressed,
  3 improved; 92 of the 94 keep their exact iteration count (D30).
- The refresh that verifies a declaration of optimality refines its two
  solves — one step of iterative refinement on `x_B` and on `y`. `pilot` was
  rejected on a row `1.73e-6` outside its bound that no basic variable
  violates; it is the residual of the basis solve, and refining takes it to
  `6.73e-13`. Checker goes from 92 to 93 of 94. The solve loop is deliberately
  left unrefined: 93 of the 94 instances take exactly the iteration count they
  took before, and total work falls 0.029% (D29).
- A basis the factorization finds singular is repaired rather than ending the
  solve. `jaos_internal.h` had always said rank deficiency is a fact the
  caller acts on by replacing basis columns, and the caller did not: pairing
  the rows the factorization could not cover with the basis positions it
  could not use gives a completion that is triangular by construction.
  `gran` reaches INFEASIBLE where it used to give up after 1728 iterations,
  taking the infeasible gate to 29 of 29, and no instance of the standard set
  moves. `repair_singular_basis` carries the proof and why no unit test can produce one.
- Every reference optimum in `bench/netlib.manifest` is now Koch's.
  `maros-r7` and `pilot87` had fallen back to the netlib readme, so `pilot87`
  was judged against a value 1.26e-6 from the exact optimum where this gate's
  tolerance is 1e-6. The report's PostScript carries the table as literal
  text where its PDF did not; `bench/koch-refs.py` extracts it and
  `bench/koch-verify.py` reproduces 82 of the pinned references exactly with
  none in disagreement. No verdict moves.
- The acceptance runner reads the whole manifest before solving anything, and
  closes the file first. It used to parse a line, solve that instance, then
  parse the next, holding the file open across the run; a rewrite in that
  window shifted every later byte offset and `sctap2` was judged against a
  reference no line of the file ever carried. Instance paths are now
  bound-checked rather than silently truncated.
- PLAN 2.8 and Q10 recorded a cause for the dual violations that the
  measurement does not support, and both now carry the measurement instead.
  The six open instances had been grouped by the size of the number the
  checker prints — which is a multiplier's magnitude, not a distance —
  and measured one at a time they are four separate things. Two repairs for
  the largest group were implemented, measured and reverted; both reported
  `greenbea`, a feasible model, as INFEASIBLE.
- `jaos_check_solution`'s contract in the public header said "tol is an
  absolute tolerance on violations" after D23 had already scaled the
  bound-proximity test. It now says which half is which.
- A solve no longer stops on values it carried rather than computed. Basic
  values and the factorization both drift as pivots accumulate, so a solve
  stopped exactly when its numbers looked feasible without being it. The
  point is recomputed from a fresh factorization and priced again before the
  answer is accepted — on `afiro`, the difference between finishing 1.8e-5
  from a bound and finishing on it. Costs one refactorization per solve,
  which the work counter bills (D20).
- An unbounded model is identified by a direction along which the objective
  has nothing to stop it, checked against the bounds the model declares. It
  used to be identified by a variable coming to rest on a bound the solver
  had invented, which a model with a large but finite optimum does too. On
  3000 generated LPs, 8 called unbounded now solve to an optimum the checker
  accepts, with no other model changing its answer or its work (D19).
- The Netlib gate accounts for the objective constant an MPS file can declare
  through an `RHS` entry on its objective row. JAOS applies it, following the
  convention CPLEX documents; the published optima do not, so a correct
  answer differed from both reference sets by exactly that constant on the
  one instance where it is visible. The reader is unchanged, deliberately.
- Settling up can no longer park a basic variable on an artificial phase-1
  bound, which would manufacture the evidence of unboundedness after the
  verdict was already read.
- A failed solution-buffer allocation no longer leaves the model believing
  the buffers exist; a later solve would have written through the missing
  ones.
- The published work count includes the final kernel run of publishing
  itself. It used to be taken one BTRAN too early.
- The release build, which `main` could not complete: `build_crash_basis`
  declared `nsel` twice and left a variable unused, and `-Werror` refused the
  object. Nothing depended on either variable.
- A 648-byte leak across 13 allocations that AddressSanitizer reported in the
  LU tests, gone with the revert of the code that introduced it.

### Added — the initial implementation

- `bench/`: the Netlib acceptance gate, runnable. `make netlib` fetches the
  94 instances of the standard set — pinned by sha256 in a committed
  manifest, never stored in the repository — and judges each solve against
  the published optimum, the independent checker, and a second solve that
  must agree bit for bit. Reference values are Thorsten Koch's exact
  rationals where they exist; the netlib readme's differ from them beyond
  the gate's tolerance on eight instances. The last full run is recorded in
  `bench/results/netlib.txt` and the README summarises it. The gate is not
  met yet, and the record is what says so.
- `docs/tolerances.md`: every number a solve compares against, which space it
  acts in, and the formulas the independent checker judges with.
- `docs/work-units.md`: what a work unit is, where each weight is charged, and
  what sits outside the budget — so a work limit can be chosen rather than
  guessed.
- `README.md`: what JAOS is, what it solves today, how to build it, and the
  design commitments that constrain it.

- `DECISIONS.md`: the durable record of closed design decisions — language and
  toolchain, dependency policy, determinism, problem scope, and the rules under
  which correctness and speed may be claimed.
- `PLAN.md`: the staged build order for the whole declared scope, and a fully
  specified first milestone — LP correctness on the Netlib set — with acceptance
  criteria, draft tolerances and work-unit weights, and a verified bibliography.
- Build scaffold: `make all | test | sanitize | clean` on GCC 14 / C23; the
  public header `jaos.h` with status codes and version query; Unity v2.7.0
  vendored under `tests/` as the dev-time test harness.
- Model core: create/load/query/free for LPs in bounded form, with validating
  copy-on-load (sorted CSC, explicit zeros dropped, structural errors rejected)
  and an internal row-wise mirror.
- Independent solution checker: judges a claimed primal (and optionally dual)
  solution against the model in original space — bound and activity violations,
  dual sign conditions including complementary slackness, and the objective gap.
- MPS reader (`jaos_read_mps`): fixed and free layouts, RANGES, all continuous
  BOUNDS types, OBJSENSE, objective constants, Fortran D exponents,
  locale-independent number parsing; every rejection carries a line number via
  `jaos_model_error`. Integer constructs are recognized and rejected until MILP
  lands. Dialect decisions documented in `docs/format-support.md`.
- LP-format reader (`jaos_read_lp`): CPLEX-style core dialect with line-wrapped
  expressions, labels, glued coefficients, repeated-variable summing, all bound
  forms including `free` and infinities. Ranged constraints, constants inside
  constraints and integer sections are recognized and rejected with line
  numbers.
- Matrix scaling: Curtis-Reid by default, geometric-mean equilibration as an
  option. Factors are exact powers of two, so applying them adds no rounding
  error, and the stored matrix is left untouched. See `docs/scaling.md`.
- Sparse LU factorization with Markowitz threshold pivoting, and the forward
  and transposed triangular solves built on it. A singular matrix is reported
  through the factorization's rank rather than treated as a failure.
- Forrest-Tomlin basis updates: replacing one basis column costs work
  proportional to the change instead of a full refactorization. A replacement
  that would leave an untrustworthy pivot is refused rather than accepted
  quietly.
- Deterministic work counter wired into the factorization and solve kernels —
  the currency of the reproducible budget.
- Dual simplex: `jaos_solve` finds optimal solutions for linear programs with
  bounds, ranged rows and equalities, in either objective sense, and reports
  infeasible models as infeasible. Results come out through `jaos_solution`,
  `jaos_objective` and `jaos_status_of`.
- Budgets: `jaos_set_work_limit` (reproducible) and `jaos_set_time_limit`
  (wall-clock), reported back through `jaos_work_units` and
  `jaos_iterations`. Pricing and the ratio test are charged to the work
  counter, so a work limit bounds the whole solve rather than only its
  linear algebra.
- A model this build cannot solve yet reports `JAOS_SOLVE_UNSUPPORTED` with
  an explanation, rather than being rejected as invalid input.
- `jaos_objective` reports through a status and an out-parameter, so a
  genuine objective of zero is distinguishable from having no answer.
- Dual steepest-edge pricing: the solver picks which violated constraint to
  repair by how far the repair actually travels, not by the size of the
  violation as written. A model in mixed units no longer sends it after the
  row with the smallest units first.
- Harris' two-pass ratio test: the pivot is chosen by spending a bounded
  amount of dual feasibility — one tolerance's worth — on a better
  conditioned one. Where many candidates block at the same point, which is
  most of a degenerate solve, this is what decides between them.
- Bound flipping in the ratio test: a variable with two finite bounds no
  longer stops an iteration short. It is swapped to its other bound and the
  step carries on, so a model whose answer fills one bounded column after
  another is solved in one long step instead of one step per column.
- The dual feasibility the ratio test spends on better-conditioned pivots is
  now lent rather than given away: the cost of the affected column is moved
  just enough to keep it feasible, the loan is recorded, and it is called
  back before any answer is reported, so what comes out belongs to the
  problem that was asked about. Where calling it back leaves a column on the
  wrong side, it is swapped to its other bound whenever that keeps the
  solution feasible — which can only improve the objective.
- Steepest-edge pricing weights are checked against an exactly known value
  every iteration, repaired where the exact one is available, and restarted
  when they have drifted past being informative. Pricing that has quietly
  stopped meaning anything costs iterations rather than answers, so nothing
  else would have reported it.
- Solving now runs on a scaled copy of the model, computing a Curtis-Reid
  scaling first if none was chosen, so a model whose coefficients span many
  orders of magnitude is solved on numbers that do not. The model itself is
  untouched and every answer — values, activities, duals, reduced costs —
  comes back in the units it was written in.
