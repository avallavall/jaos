# The Netlib campaign — the measurement record

The M1 acceptance gate on the standard Netlib set, from the first end-to-end
run to `gate: PASS`. This is the measurement record, not the plan and not the
decisions: `DECISIONS.md` carries what each measurement decided (D19–D30) and
`TODO.md` carries what is still open. It lives here because several of the
measurements below exist nowhere else, and because the sequence itself turned
out to be the most useful thing the campaign produced — every failure that was
tempting to blame on a tolerance was something else, and each wrong reading is
what pointed at the next.

Written when the gate closed. Nothing here is current state; read the record
under `bench/results/` for that.

---

`bench/` exists and the gate has been run end to end on all 94 instances of
the standard set. It is met. The record is `bench/results/netlib.txt`; what
it says, and what each line of it is asking for, is below.

| | |
|---|---|
| shape correct | **94 / 94** |
| solved to optimal | **94 / 94** |
| objective within tolerance | **94 / 94** |
| independent checker green | **94 / 94** |
| deterministic across two solves | **94 / 94** |

The readers are the part that came out clean: every instance in the set
loads with exactly the row and column counts two independent canonical
sources agree on. Determinism holds everywhere a solve finished.

The remaining failures were three different problems, and they did not
share a fix. All but one are closed:

- **`e226` — closed, and not where it looked.** The reader was right: the
  objective constant follows the documented convention and always did.
  What differs is the reference — neither published Netlib set includes
  the constant, so a correct answer misses both by exactly it. The gate
  records the constant per instance and compares against reference plus
  constant; the reader was left alone on purpose, and `tests/test_mps.c`
  now says why. Details in `docs/format-support.md`.
- **`grow15` — closed, and it was a cycle rather than a stall.** The
  internal iteration guard tripped at 189201 iterations, correctly
  reported as a JAOS defect rather than a hard model. Q10 read it as the
  stall it had been waiting for; instrumented, it is a **cycle of period
  four** over two rows and four variables, repeating bit for bit from
  iteration ~3000. Half its iterations take a real dual step of ~1.7e-6
  and the four cancel exactly, so "no iteration makes progress" was not
  what was happening.

  Cycles have a cure stalls do not. Bland's rule — the real one: exact
  minimum quotient, no Harris window, smallest index on *both* choices —
  solves it. It cannot be the default, because it costs 25x on `25fv47`
  and takes `grow22` from 2179 iterations to no answer at all, so it is a
  fallback that a detected cycle switches on and progress switches off.
  `grow15` solves in 21653 iterations at sixteen digits of Koch's value,
  and every instance that does not cycle is bit-identical. D26 carries
  the mechanism and the two-sided measurement that sets its one constant.
- **Dual conditions the checker rejected on seven instances** —
  `etamacro`, `finnis`, `greenbea`, `nesm`, `pilot`, `pilot87` and
  `pilot-ja`. Three are closed (`pilot-ja` by D21, `finnis` by D23,
  `nesm` by D25) and four remain; each closure is recorded below with
  what it turned out to be.

  They were read as two groups by magnitude, then as three. Measured one
  at a time they are **four distinct defects**, and grouping them by the
  size of the reported number was the mistake: what the checker reports
  is `|w|`, the magnitude of the offending multiplier, and a multiplier's
  magnitude says nothing about how far anything is from where it should
  be. `finnis` was reported at 28 and was the most accurate of the seven.

  **`pilot-ja` — closed, and it was the checker.** Its dual violation
  was exactly zero; it was rejected on the gap alone, at 1.87e-6.
  Judging the *same* solution at smaller tolerances gave 1.87e-6 at
  1e-6, 1.99e-7 at 1e-7 and 9.36e-16 at 1e-8: a gap falling in
  proportion to the threshold it is measured against, which is not
  something a wrong answer does. The cause was one line in
  `src/check.c` dropping a multiplier with `|w| <= tol` from the sign
  conditions *and* from the dual objective, as though those were the
  same claim. A multiplier too small to impose a sign condition still
  carries `w · bound`, and that product grows with the bound. Every
  multiplier now contributes; the exemption covers the condition only.
  D21 records why, and `docs/tolerances.md` carries the rule.

  Two repairs that also close this case were tried and are wrong, kept
  here because both look reasonable and one of them passed the whole
  gate. Contributing `w · v` cancels the term — and on a model whose
  multipliers all sit under `tol` the gap is then identically zero for
  any feasible point, so the checker certifies the entire polytope. It
  passed 98 unit tests and all 94 instances with a regression-free diff
  before that was found. Choosing the bound nearest `v`, which is what
  HiGHS does for its own diagnostic, manufactures negative terms that
  offset real residues elsewhere, and evaluates `(-inf + inf) / 2` on a
  free variable.

  What the fix does not change: `etamacro`, `nesm`, `finnis`,
  `greenbea`, `pilot` and `pilot87` are unaffected, and the gate stays
  at NOT MET. Checker-green goes from 86 to 87.

  **`etamacro` at 1.56e-6 and `nesm` at 8.01e-6** are the opposite case:
  their dual violations do not move at all as the checker's tolerance
  drops, so those are real breaches, small but genuine.

  They are also, and this was not known when the group was written, the
  instances the reference work itself singles out. Koch [22] reports
  that "the current development version of SoPlex using 10^-6 as
  tolerance finds true optimal bases to all instances besides `d2q05c`,
  `etamacro`, `nesm`, `dfl001`, and `pilot4`", settled only by moving
  from 64-bit to 128-bit arithmetic; and that CPLEX 8.0 at default
  settings misses `etamacro`, `d2q06c` and `scsd6`, needing tolerances
  at 10^-9, aggressive scaling and preprocessing off.

  Checked against the current run, which is the useful part:

  | instance | SoPlex 1e-6 | CPLEX default | JAOS |
  |---|---|---|---|
  | `dfl001` | misses | — | **checker ok** |
  | `pilot4` | misses | — | **checker ok** |
  | `d2q06c` | — | misses | **checker ok** |
  | `scsd6` | — | misses | **checker ok** |
  | `nesm` | misses | — | REJECTED, dual 8.01e-6 |
  | `etamacro` | misses | misses | REJECTED, dual 1.56e-6 |

  (`d2q05c` is not in the standard 94.) So four of the six that the
  reference solvers needed special settings for come out clean here,
  and the two that do not are `nesm` and `etamacro` — with `etamacro`
  the one instance that defeats CPLEX at defaults, SoPlex at 1e-6, and
  JAOS alike. Failing there in double at a 1e-6 tolerance is documented
  behaviour of the field rather than a JAOS peculiarity. That does not
  make it acceptable; it says what closing it is likely to cost, and
  that the answer is probably precision rather than a bug.

  The hypothesis
  for them, worth writing down before it is lost, is that the checker
  accumulates in `long double` where the solver works in `double`, so a
  residual the solver reads as just inside `CHECK_TOL` comes back from
  the checker just outside it. The fix would be for the solver to settle
  against a tightened bound — half of `CHECK_TOL`, say — and leave the
  rest as margin. This was prototyped on an unmerged branch and is not
  merged: that version carried debug instrumentation, duplicated the
  tolerance into a third file, and assigned `DSE_MIN` — the floor on a
  steepest-edge weight, which is a squared norm — to a reduced cost. The
  idea is worth taking; that implementation is not. Whatever replaces it
  is judged per instance against `bench/netlib.baseline`, since a
  tolerance change touches every instance at once and `etamacro` alone
  cannot say what it cost.

  **The six, measured.** For each one, the entity the checker rejects,
  the multiplier on it, its distance from the bound that multiplier
  points at, and the traffic through it — for a row, the sum of
  `|a_ij x_j|` over the row; for a column, of `|c_j|` and the `|a_kj y_k|`.
  The last column is the one that separates them:

  | instance | offender | \|w\| | distance | traffic | dist/traffic | gap |
  |---|---|---|---|---|---|---|
  | `finnis` | row 3 | 28 | 1.52e-6 | 4.0e10 | **3.8e-17** | 3.96e-11 |
  | `greenbea` | col 4669 | 2.66 | **infinite** | 2.66 | — | 3.57e-17 |
  | `etamacro` | col 63 | 1.56e-6 | 1.29 | 0.566 | 2.27 | 1.86e-9 |
  | `nesm` | col 2667 | 8.01e-6 | 95.1 | 1.13e-3 | 8.4e4 | 2.71e-11 |
  | `pilot` | row 603 | 1.9e-2 | 7.01e-6 | 558 | 1.26e-8 | 8.29e-6 |
  | `pilot87` | col 4554 | 9.6e-3 | 0.197 | 0.242 | 0.814 | 1.12e-5 |

  **`finnis` — closed, and it was never a solver defect.** Row 3 is a
  `>= 0` row. Its activity comes out at 1.52e-6 from terms whose
  magnitudes sum to 4.0e10, and one ulp at 4.0e10 is 7.6e-6 — the residue
  is a fifth of a single rounding step at the scale the row works at. The
  checker's "is it at its bound" test was `v <= lo + tol` with `tol`
  absolute, so on this row it demanded seventeen correct decimal digits of
  a sum that cancels ten orders of magnitude. No double-precision answer
  could pass it, the duality gap said the answer was right at 3.96e-11,
  and the solve published no violated sign condition in scaled space at
  all.

  The window is now `tol · s`, with `s` the sum of the magnitudes of a
  row's terms and `max(1, |x_j|)` for a column — D23, formulas in
  `docs/tolerances.md`. What keeps it from being the gate made easier is
  an identity rather than a convention: `P − D = Σ w_v (v − bound_v)` with
  every term non-negative on a primal-feasible point, so a row waived at
  distance `d` with multiplier `w` still contributes exactly `w · d` to
  the gap. The waiver can decline to report a discrepancy twice; it cannot
  hide one. `tests/test_check.c` carries the case where the sign condition
  *is* waived and the answer is refused anyway on the gap, checked as
  `0 − (−500)` against `1000 × 0.5`.

  Measured on all three sets: `finnis` goes from REJECTED to checker ok
  with its dual violation at exactly 0 rather than merely smaller, and
  **nothing else moves at all** — 0 regressed, 1 improved, 0 new on the
  standard 94; 0/0/0 on the other two. `pilot`'s row 603 also clears,
  without changing its verdict: it fails on the gap and on the objective,
  which none of this touches.

  **Where each residue comes from, measured rather than reasoned — and
  the first measurement asked the wrong question.** The obvious
  explanation for a published reduced cost with the wrong sign is Q10's:
  the dual simplex shifts costs to hold dual feasibility, and what the
  shifts were hiding reappears when they are called in. Recording
  `shift[v]` before `settle_shifts` zeroes it, beside the residue it was
  supposed to explain, said the shifts accounted for one case of three.

  That reading was wrong, and it was wrong because a reduced cost does
  not only depend on its own column's cost. `d_j = c_j − y' M_j` and
  `y = B^-T c_B`, so a shift resting on a *basic* variable moves every
  nonbasic reduced cost at once, and the violating column need carry no
  shift of its own at all. Measuring `d` on both sides of the settlement
  rather than the shift on one column says so plainly:

  | instance | d before settling | d after | own shift | shifts on the basis |
  |---|---|---|---|---|
  | `greenbea` col 4669 | **+5.67** | −1.33 | 4e-9 | 907 basics, max 7.09e-6 |
  | `greenbea` col 4770 | **+15.0** | −5.28 | 1e-14 | " |
  | `nesm` col 2667 | **+5.12e-5** | −2.00e-6 | 3e-19 | 187 basics, max 1.11e-6 |
  | `etamacro` col 63 | 0 | +4.89e-8 | −4.89e-8 | 20 basics, max 4.35e-8 |
  | `finnis` | *no residue at all* | — | — | — |

  Every one of them is dual feasible before the shifts come off. So all
  three are Q10's residue after all, by two routes: directly, through the
  column's own shift, which is `etamacro`; and through the basis, which is
  `greenbea` and `nesm` and is the one nobody had looked for.

  **What that route costs is the finding.** On `greenbea`, removing
  shifts of at most 7.09e-6 from 907 basic variables moves one reduced
  cost from +5.67 to −1.33 — a swing of 7.0 out of a perturbation of
  7e-6, an amplification of a millionfold. That is `B^-1` on a basis this
  badly conditioned, and it says the size of the residue is not evidence
  about the size of the cause. Q10 said the residue would survive to the
  published reduced costs and that the free repair could not always reach
  it. Both hold. What it did not anticipate is that a perturbation far
  below every tolerance in §2.6 can arrive as a violation of five.

  **`finnis` produces no violated sign condition in scaled space.** Not a
  small one — none. Its rejection exists only in the checker's
  original-space view, which is the cleanest possible confirmation of the
  paragraph above: the solve is dual feasible on its own terms and the
  test it fails is one no double can pass.

  **`etamacro` is the shift residue, and it is only rejected because of
  scaling.** Column 63 rests at its upper bound of 1.2853 with a scaled
  reduced cost of +4.89e-8 where the sign condition wants it non-positive
  — a breach less than half of `DUAL_TOL`, which is to say inside what
  this solver calls zero. Its column scale is 1/32, and `publish` divides
  by it, so 4.89e-8 leaves as 1.56e-6 and lands just past the checker's
  1e-6. The residue is real, the shift explains it, and the reason it is
  visible at all is that a tolerance held in scaled space is being read
  against one applied in the original.

  **Reading the repair threshold in the original space closes it, and
  costs `pilot87` entirely. Measured 2026-08-08, not merged.** The
  re-entry (D25) decides whether a breach is worth repairing by comparing
  `d[v]` against `DUAL_TOL` — a number in the space the solver works in,
  not the one the answer is published in. Judging it where it will be
  read instead (`d[j]/gamma_j` for a column, `d[ncol+i]·rho_i` for a row
  multiplier, which is exactly what `publish` emits) is the only change,
  and it is the tightened-settling idea this section already called worth
  taking:

  | | result |
  |---|---|
  | `etamacro` | REJECTED → **checker ok**, dual 1.56e-6 → 0, 9 extra iterations |
  | `nesm` | REJECTED → checker ok (it already was, by the flip) |
  | 51 instances to `pilot` | nothing else moves, total work −0.0% |
  | **`pilot87`** | **stops solving** — the iteration guard trips at 1382801, against 50616 |

  Two improved, two regressed, and the two regressions are the same
  instance losing its answer altogether. That is the shape both earlier
  repairs of this residue took (Q10) and it is worse than the defect: a
  model with a perfectly good optimum comes back as a JAOS defect. The
  mechanism is not mysterious — a threshold read after dividing by a
  small scale factor admits far more columns as movable, so rounds keep
  finding work and 32 of them are 27 times the iterations the instance
  needs.

  What this settles is that the choice of space is load-bearing rather
  than presentational, and that "settle against a tightened bound" is not
  free: it buys `etamacro` at a price nobody had priced.

  **A second attempt on a quantity that has no space. Measured
  2026-08-08, also reverted, and it got much further.** The point the
  first attempt makes is that *any* rule reading the breach must pick a
  space. There is one that need not: the term the breach contributes to
  `P − D`, which for a nonbasic on a bound with a wrong-signed reduced
  cost is `|d|` times the width of its box. `publish` divides `d` by the
  same `gamma` it multiplies the value by, so the product is identical in
  both spaces — and it is exactly what the checker now reports as `Q`.
  Verified rather than argued: `etamacro`'s three movable breaches
  contribute 2.011e-6, 6.26e-7 and 1.676e-7 in scaled space, summing to
  the 2.805e-6 the checker publishes in the original.

  Judged on the contribution instead of the breach:

  | | |
  |---|---|
  | `etamacro` | REJECTED → **checker ok**, 9 extra iterations |
  | the other 93 of the standard set | **bit-identical**, `pilot87` included |
  | total work over the 94 | +0.0% |
  | infeasible set | PASS, 0 regressed |
  | **`pds-20`** (Kennington) | **work 3.2x, iterations 47785 → 136750** |

  It also fixed a hand-built three-column test whose optimum is readable
  by eye — the solver had been stopping 5e-8 above it with a certificate
  that did not carry, and PLAN 2.8 recorded that as a defect. So the rule
  is not merely tuning: it corrects a wrong answer on a model where no
  reference value is involved.

  **And on its own it is wrong, because of what it does to `pds-20`:
  work 3.2x, 47785 iterations becoming 136750.** Instrumented there, the
  re-entry runs all 32 rounds without converging, and **every column it
  flips has a reduced cost below `DUAL_TOL`** — the smallest run from
  2.2e-11 to 1.7e-10, three to four orders below what this solver calls
  zero. Their contributions clear the threshold only because the boxes
  are 900 to 4955 wide.

  So the contribution answers *is this worth moving* and answers it well;
  it does not answer *is there anything here at all*, and on a wide box it
  multiplies rounding noise up past any threshold. `etamacro` hid that
  because its own reduced costs (3.06e-8, 4.89e-8) sit within half an
  order of `DUAL_TOL` rather than four below it.

  **The third attempt is the merged one, and it is D27.** What was
  missing is a test for "this reduced cost is a number rather than
  rounding" that does not simply exclude `etamacro` — `DUAL_TOL` on the
  breach does exclude it. D23's own shape supplies it: `d_j = c_j −
  y' M_j` is a sum, and a sum is known no more finely than the terms that
  went into it, so a reduced cost means something where it stands above
  `eps` times the traffic through its column. That is a computed
  quantity, not a constant fitted to an instance, and it is not a second
  test bolted on — a product is only as good as its factors, and on a
  wide box the first factor was rounding.

  Measured over both feasible sets, on every column the re-entry would
  consider. Five instances of the 110 have any:

  | instance | columns | `|d| / (eps · traffic)`, smallest |
  |---|---|---|
  | `etamacro` | 3 | **5.055e8** |
  | `pilot87` | 15 | 6.985e10 |
  | `pilot` | 5 | 6.339e11 |
  | `nesm` | 1 | 3.199e13 |
  | **`pds-20`** | 14 | **2.133** |

  Seven orders of daylight, over 110 instances rather than the two the
  previous paragraph was arguing from. `pds-20` keeps exactly one column
  — the one whose traffic *equals* its `|d|`, a single term with nothing
  to cancel, so its reduced cost is exact however small — and it flips
  once and converges: 47786 iterations against a baseline of 47785.

  **`greenbea` was the same residue arriving through the basis, and was
  the largest open item of the set. Closed by D28.** Ten columns rested at
  their lower bounds with scaled reduced costs from −0.019 to −5.28, and
  every one of them was dual feasible until `settle_shifts` ran. Column
  4669 was the worst the checker saw: lower bound 0, no upper bound, zero
  cost, reduced cost −2.665 unscaled, so its multiplier pointed at a bound
  the model never declared. The objective was nonetheless right to 2e-7
  relative and the gap 3.57e-17, because the checker adds no dual term for
  a multiplier aimed at an infinity.

  So the solve ended on a basis that was dual infeasible by a wide margin,
  at a vertex that was primal optimal, and declared OPTIMAL. `nesm` was the
  same thing two orders of magnitude smaller, and D25 closed that one by
  moving its column to the other bound it had.

  `greenbea`'s ten had no other bound, which is what made them the hard
  case and what made every threshold blind to them: the term a repair test
  could weigh is `w · bound`, and there is none for an infinity. What they
  needed was to travel until something stopped them — a primal ratio test,
  which the scope question admitted (D28). Eight pivots. The objective goes
  from −72555233.859378919 to **−72555248.129846007** against Koch's exact
  −72555248.129845992, fifteen significant digits, and the dual violation
  from 2.66 to **0**.

  Where the amplification comes from is not mysterious, but it is not the
  shifts' size either: a shift is repaid at the very end, and a variable
  that was shifted while nonbasic keeps the perturbed cost when it enters
  the basis, so `c_B` carries it and `y` is perturbed for the rest of the
  solve. 907 of greenbea's basics are in that state at the finish. Both
  candidate repairs that moved the repayment *earlier* were measured and
  reverted (Q10): on a basis this ill-conditioned they turn a small final
  violation into a false infeasibility.

  **What worked instead was to move it later, and the set splits on one
  property (D25).** After settling, put the nonbasic set back on the
  feasible side of its sign conditions and run the dual simplex again from
  there — sending a column to its other *real* bound rather than moving a
  cost. Flipping breaks the primal, and primal infeasibility is what the
  method exists to remove. Measured, after settling, over the residual
  sign conditions each instance is left with:

  | instance | residual | with a real opposite bound | outcome |
  |---|---|---|---|
  | `etamacro` | **0** | — | untouched, bit-identical |
  | `greenbea` | 10 | **0** | untouched, bit-identical |
  | `nesm` | 1 | 1 | **closed**: dual 8.01e-6 → 0 |
  | `pilot` | 25 | 5 | dual 1.7e-2 → 8.0e-5, gap 8.3e-6 → 8.6e-13 |
  | `pilot87` | 48 | 15 | dual 9.6e-3 → 3.3e-5, gap 6.0e-5 → 4.0e-8 |

  The two that do not move are outside the mechanism by construction
  rather than by bad luck, and for different reasons. `etamacro` has
  nothing to repair — its breach is inside `DUAL_TOL` in scaled space and
  is a scaling artefact, as recorded above. `greenbea`'s ten all rest at a
  lower bound of 0 with no upper bound at all, so there is nowhere to send
  them; that is the travelling nonbasic, and it is what §2.1 puts outside
  M1.

  **What the rounds do, measured, because two things about them were
  assumed.** Exactly three instances of the 94 re-enter at all, and they
  converge in one round (`nesm`), three (`pilot`) and six (`pilot87`).
  Neither assumption held:

  - *The round cap was deciding an answer.* It was first written as 4,
    which is precisely where `pilot87` still had work to do. Running to
    convergence instead takes its dual violation from 2.28e-4 to 3.33e-5,
    its gap from 2.27e-7 to 4.03e-8 and its objective error from 3.21e-3
    to 2.35e-3, for 182 extra iterations out of 50434 — better on every
    measure for a third of one percent of the work. `SETTLE_ROUNDS` is
    now 32 and is a backstop rather than a limit meant to bind (D25).
  - *The residue does not fall monotonically*, so the loop must not be
    allowed to judge its own progress. On `pilot` the worst breach
    standing at the top of each round runs 4.65e-3, 4.79e-3, **7.85e-2**,
    2.87e-6: round 2 begins seventeen times worse than the solve ended,
    and it is the round that produces the final drop of three orders of
    magnitude. A rule that kept the better of two consecutive points —
    which is the obvious safety measure to reach for — would have stopped
    after round 0 and thrown that away.

  **The other candidate repair, `pilot-analysis.md` §6.1, is closed by
  measurement rather than run.** It proposes capping accumulated
  `|shift[v]|`. Instrumenting the distribution at the moment settling
  repays it, on `greenbea`: 2901 variables carry a nonzero shift, of which
  2407 are below 1e-9, 227 fall in `[1e-8, 1e-7)`, 42 in `[1e-7, 1e-6)`
  and **three** exceed 1e-6, the largest being 7.09e-6. A cap at 1e-6
  therefore touches three variables of 2901 — and not the ones that
  matter: the offending columns' own shifts are 4e-9 and 1e-14, because
  the residue arrives through the basis. On `etamacro`, where the residue
  *is* the column's own shift, every shift in the solve falls below
  `DUAL_TOL`; the cap that would bite there is narrower than the Harris
  window that created it, which is not a cap on shifts but a narrower
  window, a different change with a different cost and not what §6.1
  proposes.

  **`pilot` and `pilot87` are simply less accurate**, and their gaps say
  so on their own — 8.3e-6 and 1.1e-5 against a 1e-6 tolerance, where
  every other instance in the table is at 1e-9 or below. They are the
  worst-conditioned models in the set and they miss the objective as well,
  which the next item takes separately. Whatever closes `finnis` will not
  close these.

  **The primal test has the same shape of problem, and D23 deliberately
  did not touch it.** `interval_violation` is still absolute. Measuring
  the worst row violation against the traffic through that row, as D23
  does for the sign condition:

  | instance | row violation | traffic | relative | ulp(traffic) |
  |---|---|---|---|---|
  | `finnis` | 8.44e-7 | 4.0e10 | **2.1e-17** | 7.6e-6 |
  | `greenbea` | 4.31e-9 | 6.6e8 | 6.6e-18 | 1.2e-7 |
  | `adlittle` | 4.55e-13 | 2589 | 1.8e-16 | — |
  | `25fv47` | 1.30e-12 | 1031 | 1.3e-15 | — |
  | `nesm` | 1.00e-8 | 0.70 | **1.4e-8** | 1.1e-16 |
  | `pilot` | 1.96e-5 | 1129 | **1.7e-8** | 2.3e-13 |

  The absolute rule is **both too strict and too lax**, and which one it is
  depends on nothing but the row's scale. `finnis` passes it by 16% of the
  margin while its residue is a tenth of one ulp of the row — one more
  rounding step in the wrong direction and a correct answer would be
  refused on the primal too, as it already was on the dual. `nesm` passes
  it comfortably at 1e-8 absolute while being a hundred million ulps out
  relative to a row carrying 0.7.

  Between the instances that are clearly fine (1e-15 to 1e-17 relative)
  and the two that are clearly not (1.4e-8, 1.7e-8) there are seven orders
  of magnitude of daylight, which is the measurement §2.6 had been waiting
  for.

  **Closed by D24: the primal test stays absolute.** Not because the
  measurement is wrong — it stands — but because primal feasibility is
  the *hypothesis* of the identity D23 rests on. `P − D = Σ w_v (v −
  bound_v)` has non-negative terms only where `v` is inside its bounds, so
  relaxing that does not extend D23, it removes what D23 stands on; and an
  infeasible entity's term turns negative, offsetting real residues
  elsewhere — the fungibility defect D22 already refused in writing. It
  also buys nothing: exactly one instance of the 94 exceeds 1e-6 on a row
  (`pilot`, already rejected twice over). D24 carries the rest, including
  the one form that would be safe if this is ever revisited — `min(tol,
  tol·s)`, narrowing rather than widening.

  **What the argument turned up, now built.** The gap is `|Q − N|`, with
  `Q` the positive terms and `N` what a within-tolerance primal violation
  contributes negatively; the two cancel and the checker cannot tell a
  small gap from two large halves. Both halves are now accumulated apart
  and published — `gap_positive` and `gap_negative` in
  `jaos_check_report`, alongside `max_row_violation_relative`, which is
  the relative primal residue D24 said it would keep in the report and out
  of the predicate. Three public fields, two `long double` adds, no
  verdict moved, and the record carries all of it per instance.

  Putting it on the 94 turned the constructed case into a measured one,
  and into a common case rather than a curiosity. On **35 of the 93
  instances that reach an optimum, `Q` exceeds `|Q − N|` by more than a
  factor of two** — `pilotnov` by 157, `greenbeb` by 34, `finnis` by 3.
  The gap those instances report is not the bound they are entitled to.

  The size matters before the ratio does: every one of those `Q` values is
  tiny in absolute terms — 7.85e-10 on `pilotnov` against an objective of
  order 4.5e3 — so the certificates were sound throughout and no verdict
  was ever wrong. What changed is that soundness is now something the
  record shows instead of something the identity was assumed to deliver.
  `P − P* ≤ Q` is a bound a reader can check, on an instance that
  *passes*, which is the only kind where this was ever going to be
  visible.
- **`pilot` and `pilot87` miss the objective as well**, by 2e-4 and 6e-5
  relative. These are the worst-conditioned instances in the set and are
  expected to be last; they are listed apart because an objective error
  is a different claim from a dual residue, even on the same instance.

None of this is a tolerance to be widened. §2.6 stays where it is until
there is a measurement on both sides of each number (D17).

---

## How the last two were read before they were closed

**Steps 1, 2 and 3 are done.** The two in-scope repairs were run, the checker
was instrumented, and the scope question that waited on them has been decided.
The order mattered: without the measurements, step 3 could only have been
argued.

- *§6.3, re-entry from the settled basis* — built, D25. Closed `nesm`. The
  failure both earlier attempts produced, a feasible model returned
  INFEASIBLE, is structurally refused: the settled point is saved and a
  re-entry that ends in anything but a second optimum is discarded.
- *§6.1, a cap on accumulated `|shift[v]|`* — closed by measurement rather
  than by a run. §2.8.1 carries the distribution: on `greenbea` three
  variables of 2901 exceed 1e-6 and the offending columns' own shifts are
  4e-9 and 1e-14, so a cap cannot reach them.
- *`Q` and `N`*, plus the relative primal residue — in `jaos_check_report`
  and in the record (D24). No verdict moved, and they turned out to be what
  D27 needed: the quantity that decides a repair is the one the checker
  publishes as `Q`.
- *`grow15`* — not part of the plan and the largest single change. It was a
  cycle of period four, not the stall Q10 diagnosed, and Bland's rule as a
  detected-cycle fallback closes it (D26).
- *`etamacro`* — closed by D27 after two attempts that were measured and
  reverted, both recorded in §2.8.1 because each is what pointed at the next.
- **The scope question — decided.** A primal ratio test enters M1; the primal
  simplex stays out, and §2.1 now says in writing where that line runs. D28
  carries it. `greenbea` closed for eight pivots, at fifteen significant
  digits of Koch's exact value, and `pilot`'s objective came inside tolerance
  from 390x outside it.

**What is left of 1a is one instance, and it is not the kind of failure the
gate started with.**

1. **`pilot` — closed by D29, and it was a residual of the basis solve.** What
   follows is the record of how it was read before that, because both wrong
   readings were reasonable and the second is the one §2.5.5 had written down.

   **`pilot`, on one row at `1.73e-6`.** Its objective is right, its dual
   violation is exactly zero, its gap is `6.6e-14`. The only thing refusing
   it is `interval_violation`, an absolute test on a row 1.73 times the
   tolerance out. This is D24's question and D28 records that one of D24's
   four arguments — that a relative rule "buys no verdict" — is now false.
   The other three stand, and the first is still sufficient on its own:
   primal feasibility is the hypothesis D23's identity rests on, not a test
   beside it. If it is revisited, the only safe form is the one D24 already
   names: `min(tol, tol·s)`, which narrows and can only turn acceptances
   into rejections.

   **Measured three times. It is neither a tolerance nor a violated bound,
   and it is not the trigger §2.5.5 named either.**

   First, the relative figure D24 put in the report, which needed no new run:
   `pilot`'s row residue is `6.93e-9` of what the row carries, against
   `8.21e-17` for `finnis`, `1.76e-16` for `adlittle` and `6.08e-14` for
   `25fv47` — seven to nine orders above the band a healthy row sits in, about
   3e7 ulps of a row carrying 250. So a relative window of `tol · s` would be
   `2.5e-4` wide there and would wave a real discrepancy through. D24 is
   supported by a measurement now rather than by the absence of one.

   Then the solver's own view of the same point, and this is the finding:
   **no basic variable is outside its bound at all.** The worst violation in
   scaled space is exactly `0`. So the `1.73e-6` is not a bound test failing
   anywhere — it is the difference between two computations of one quantity.
   The solver carries each row's activity in a logical variable obtained from
   `x_B = -B^-1 (N x_N)` on the scaled copy; the checker recomputes
   `sum_j a_ij x_j` from the matrix as loaded and the published `x`. They
   disagree by `1.73e-6`, which is a **residual of the basis solve**, not a
   primal infeasibility.

   That relocates the defect. It is not `interval_violation`, not `PRIMAL_TOL`
   and not a space mismatch: it is how accurately `B^-1` is applied on
   `pilot`'s basis.

   It is also the one place D18's argument for an independent checker pays
   off in the direction nobody was watching: checker and solver agree about
   the model, and disagree about the arithmetic.

   **And the cure §2.5.5 named is not the cure, which is the third
   measurement.** This paragraph used to end by calling for a stability
   trigger watching an FTRAN/BTRAN residual during the solve. But the
   residual is measured against a factorization D20 has just rebuilt, so
   refactorizing earlier cannot reach it — the error is the backward error of
   the triangular solves on this basis, not drift in a patched LU. What
   reaches it is **one step of iterative refinement** on that solve:
   `7.06e-6` becomes `9.09e-13`, and the rejected row `1.73e-6` becomes
   `6.73e-13`.

   Where to apply it had a price on it. Refining every solve was measured and
   is the sixth instance of the failure this milestone keeps producing:
   `pilot-ja`, a model with a known finite optimum, comes back **INFEASIBLE**,
   and `pilot87` pays **4.5x** the work. Refining only the primal is no better
   in kind — it takes `pilot`'s dual violation from `0` to `0.0688`. What
   holds is refining both solves, at the one refresh that verifies an optimum:
   mid-solve the two vectors choose a pivot and a trajectory is not more
   correct for better numbers, while at the end they *are* the answer. **93 of
   the 94 instances take exactly the iteration count they took before**, and
   total work over the set falls 0.029%; Kennington and the infeasible set are
   0/0/0 and the infeasible record is byte for byte identical. D29 carries it.

2. **`pilot87`, on its objective by 7.6x** — `2.28e-3` of error against
   `3.02e-4`. Its dual violation is `1.87e-5` and its gap `2.75e-8`, both
   improved by an order of magnitude, and it is the worst-conditioned model
   in the set. Nothing built so far moves it and no mechanism now in hand
   points at it. It is the whole of what stands between the standard set and
   condition 1a, and it may end up as an exception with a measured mechanism
   and a frozen bound — which was option 2 of the scope question, and it is
   available for one instance without being available for the gate.

One correction that removes an argument from that option, and it has now
half-expired: on `pilot`, JAOS *was* further from Koch than MINOS 5.3, OSL and
CPLEX all are, and all three ran in double
(`docs/research/pilot-analysis.md` §3.2). It no longer is — D28 brought it
within `2.3e-5`. So "the limit of double precision" was never available for
`pilot` and is now visibly not, which is worth remembering when the same
argument is offered for `pilot87`: it needs to be made about `pilot87` and
measured there, not inherited.
