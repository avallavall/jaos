# Tolerances

Every number a solve compares against, where it acts, and what it decides.
They were drafts until the Netlib gate closed (PLAN.md 2.6). It has, so they
are frozen at the values below and a change to any of them is now a
changelog entry.

Two spaces are involved and confusing them is the way to misread every
figure below. The solver runs on a **scaled copy** of the model, so its
tolerances are magnitudes in scaled space (see `docs/scaling.md`). The
independent checker runs on the **model as loaded**, so its tolerance is a
magnitude in the units the caller wrote. Neither is converted into the
other; they are separate judgements, which is the point of having both.

## The solver's tolerances

Defined in `src/simplex.c` and `src/lu.c`.

| Name | Value | What it decides |
|---|---|---|
| `PRIMAL_TOL` | 1e-7 | How far a basic variable may sit outside its bound before it counts as violated — so it decides which rows the dual simplex tries to repair, and when there are none left |
| `DUAL_TOL` | 1e-7 | The width of the Harris window: how far a reduced cost may be pushed past feasible in exchange for a larger pivot |
| `PIVOT_MIN` | 1e-9 | Smallest \|alpha\| the ratio test will accept as a pivot at all. Below this a candidate is not eligible, whatever its ratio |
| `LU_PIVOT_TOL` | 0.1 | Markowitz threshold: a pivot must be at least this fraction of the largest magnitude in its column. Sparsity is traded for stability here and nowhere else |
| `LU_UPDATE_TOL` | 1e-9 | Floor on the new diagonal in a Forrest-Tomlin update, relative to the spike's largest magnitude. Deliberately far looser than the Markowitz threshold: after elimination a legitimate pivot can be orders of magnitude below the spike |
| `LU_AGREE_TOL` | 1e-5 | How far the two computations of the pivot element may disagree before the factorization they came through is rebuilt instead of pivoted on. `alpha_q` arrives by BTRAN with the pricing row, `col[r]` by FTRAN for the basis update; they are one number in exact arithmetic, so this is the factorization contradicting itself and not a guess about conditioning. Over all 139 gate instances no pivot reaches 1e-7 and the worst is 7.83e-08; on `pilot87` at a refactorization interval of 128, where the solve grinds 1.38M iterations, it reaches 1.99. The first pivot to cross 1e-7, 1e-6, 1e-5, 1e-4 and 1e-3 is the same one, so this sits in the middle of a four-decade plateau (D86) |
| `IMPLIED_ROUNDS` | 64 | Cap on the checker's bound-propagation rounds — a safety stop and not a quality knob, since the loop exits as soon as a round bounds nothing new. Set where the propagation reaches its fixed point: swept over the standard set, certified answers go 17, 23, 32, 38, 46, 47, 48, 48 at 1, 2, 4, 8, 16, 32, 64, 128 rounds. The cost is flat across the whole sweep — 119 s to 128 s against a gate of about 120 s — so there is nothing to trade against (D91) |
| `DROP_REL` | 1e-14 | A value below this fraction of the basis matrix's largest magnitude is structurally absent. Relative, because an absolute floor would call a uniformly small basis singular |
| `TINY` | 1e-300 | The same floor where no scale is available to compare against |
| `DSE_MIN` | 1e-12 | Floor on a steepest-edge weight. Every weight is a squared norm and so positive by construction; the recurrence subtracts, and subtraction can cancel a small true value to zero. A guard against dividing by zero, not a tuning knob |
| `DSE_DRIFT` | 10.0 | How far a carried weight may sit from the exact one before the whole set is discarded and restarted. Well outside what rounding produces, well inside what one badly conditioned pivot can |
| `ARTIFICIAL_BOUND` | 1e10 | The bound dual phase 1 lends a column whose cost points at a bound it does not have. No verdict depends on it (D19): unboundedness is proven against a ray, and a model this bound cuts off is refused rather than answered. So it decides how often the method has to give up, not whether an answer is true |

Two more numbers in `src/simplex.c` are not tolerances but sit beside them:

| Name | Value | What it decides |
|---|---|---|
| `REFACTOR_EVERY` | 64 | Basis updates before a refactorization. Alongside it: the reactive fallback on a failed update, and one more refactorization at the end of every solve, because optimality is not accepted on carried values (D20). The trigger PLAN 2.5.5 also calls for — watching an FTRAN/BTRAN residual *during* the solve — still does not exist |
| `ITER_SANITY_FACTOR` | 200 | Times `rows + columns + 1`, an iteration ceiling that is not a limit but a guard against a non-terminating loop. Hitting it is a defect in JAOS and is reported as a library error, never as a solve outcome |

## The checker's tolerance

`jaos_check_solution` takes one tolerance from the caller and applies it in
original space. There is no default: a checker that chose its own would be
grading on a curve it set.

Given a claimed `x` and row duals `y`, with activities `a_i = A_i · x`
accumulated in `long double`, and everything canonicalised to minimisation
(for a maximisation model the costs and duals are negated internally):

**Primal.** For each column and each row, the violation of `v ∈ [lo, hi]`
is `max(lo − v, v − hi, 0)`, counting only bounds that are finite. The
report carries the largest column violation and the largest row violation
separately, and `primal_feasible` is both being within tolerance.

**Dual.** For a multiplier `w` attached to a value `v` with bounds
`[lo, hi]`, in minimize-canonical form:

```
|w| <= tol            no condition                    (negligible multiplier)
w > 0                 requires v <= lo + tol · s      (at its lower bound)
w < 0                 requires v >= hi - tol · s      (at its upper bound)
```

which is dual feasibility and complementary slackness in one test: a
multiplier that is not negligible must point at a bound its value is
actually resting on. A multiplier pointing at an infinite bound is itself a
violation, of exactly its own magnitude. Row multipliers are the duals as
given; column multipliers are the reduced costs `d_j = c_j − A_j · y`,
recomputed here from the original matrix.

`s` is the scale of the value being tested, and it differs by kind:

```
row i      s = max(1, sum over j of |A_ij · x_j|)     the row's own traffic
column j   s = max(1, |x_j|)
```

A row activity is a sum, and a sum whose terms cancel cannot be pinned to an
absolute tolerance. Row 3 of Netlib's `finnis` adds terms totalling 4.0e10 in
magnitude and comes to rest 1.5e-6 from its bound, where **one ulp at 4.0e10
is 7.6e-6** — the residue is a fifth of a single rounding step at the scale
the row works at. Judged absolutely at 1e-6, that row is "not at its bound"
and its multiplier of 28 is reported as a violation of 28, on a solution
whose duality gap is 3.96e-11. No double-precision answer can pass that test
and no amount of solver work can produce one; the demand is for seventeen
correct decimal digits of a sum that cancels ten orders of magnitude.

A column value is one published number rather than a sum of cancelling
terms, so it takes the ordinary mixed absolute/relative form and nothing
more. The row case is the one that needed the argument.

**Why the scale cannot excuse a wrong answer.** This test is a diagnostic;
the gap below is the proof, and the two are tied together exactly. Since
`P − D` is the sum of `w_v · (v − bound_v)`, a row waived here at distance
`d` with multiplier `w` still contributes exactly `w · d` to the gap, at full
size and with no cancellation available to it — every term of that sum is
non-negative on a primal-feasible point. So the waiver can decline to report
a discrepancy twice; it cannot hide one. `tests/test_check.c` builds the case
where the sign condition is waived and the answer is refused anyway, with
`0 − (−500)` checked against `1000 × 0.5`, and the case where a row genuinely
off its bound is still reported at the full magnitude of its multiplier.

The exemption is for the condition and for nothing else. **Every
multiplier contributes to the dual objective below, including the ones
held to no condition** — the only thing a negligible multiplier is spared
is being required to rest on a bound.

That distinction is not a detail. `D(y)` is defined as the sum over
variables of the least `w · t` attainable in `[lo, hi]`, which makes it a
function of `y` alone; dropping terms from it by their magnitude is not
part of that definition. What gets dropped is `w · bound`, and that is
small only if the bound is: a multiplier of `1e-7` on a variable resting on
a bound of `1e6` carries `0.1` of dual objective. Discarding it while the
primal still counts `c_j v_j` invents a gap proportional to the tolerance —
which is what used to reject `pilot-ja`, whose duals are exactly correct.

Two other rules close that case and are both wrong, recorded here because
each looks reasonable. Contributing `w · v` makes the term cancel, so on a
model whose multipliers all fall under `tol` the gap is identically zero
for every feasible point and the checker certifies the whole polytope.
Choosing the bound nearest `v`, which is what HiGHS does for its own
diagnostic, produces negative terms that offset real residuals elsewhere in
the model, and computes `(−inf + inf) / 2` on a free variable.

**Gap.** As each multiplier is checked it contributes `w · bound` to the
dual objective, where `bound` is the one its sign points at. The gap is
then relative:

```
gap = |primal_objective − dual_objective| / (1 + |primal_objective| + |dual_objective|)
```

Both objectives appear in the scale, not just the primal. A relative measure
that normalises by one side alone reports a larger error the further the two
are apart, which is backwards — the scale should say how big the numbers being
compared are, not how badly they disagree. This is the form PDLP uses, which
HiGHS adopted for its own gap, and the same shape as the DIMACS error measures
used to validate benchmark results. Changing to it moved no verdict on the
Netlib set: 0 regressed, 0 improved, measured against the recorded baseline.

`dual_feasible` is the largest dual violation and the gap both being
within tolerance.

Because every multiplier contributes, `P − D` is exactly
`sum_v w_v (v_v − bound_v)` — each term the complementary-slackness residue
of a single variable. On a point that is *exactly* primal feasible every one
of them is non-negative, and that is what gives an accepted solution a
guarantee rather than a reassurance: `P − P* <= gap`, by weak duality.

**The halves, and why the gap alone does not carry that guarantee.**
Non-negativity is a property of feasibility, and the checker accepts points
that are feasible only within `tol`. An entity sitting `d` outside its bound
turns its own term negative, so the sum is a difference of two quantities and
not an accumulation of one:

```
Q = sum of the terms that are >= 0        N = sum of |the terms that are < 0|
P − D = Q − N,   gap = |Q − N| / (1 + |P| + |D|)
```

Both are reported, in the objective's own units, as `gap_positive` and
`gap_negative`. Neither decides anything.

They are there because `Q` and `N` cancel, and a gap has no way to say
whether it is small because both halves are small or because two large ones
met. The bound that survives the distinction is `P − P* <= Q`: it is the
positive half alone, so a negative half cannot buy it down. `tests/test_check.c`
builds the case where the gap reads zero on a point carrying 900 of each, and
`finnis` shows the same shape at the size a real instance produces — a gap of
`3.96e-11` over halves of `4.25e-5` and `2.89e-5`, which is to say the gap
understates its own bound threefold.

What this is *not* is a false acceptance, and D24 says so in the same breath
as raising it: hiding a negative half costs an equal positive one, and the
positive half is exactly what bounds the suboptimality. The two halves are an
instrument for a question the gap could not be asked, not a repair to a hole
in it.

**The hole is somewhere else, and it is in the identity rather than in the
halves.** `P − D = sum_v w_v (v − bound_v)` needs every term, and a multiplier
whose sign points at an *infinite* bound has none to give: the term is minus
infinity, because the dual objective of a variable free in the improving
direction is unbounded below. Dropping it leaves a sum belonging to a
different problem — one where that variable had a finite bound — so `Q` stops
bounding anything. Two variables and one constraint build a point that is
arbitrarily suboptimal and on which `Q`, the gap and every violation all read
zero (D47).

`gap_certified` says whether the sum was complete, and `max_dropped_multiplier`
how big the largest missing term's multiplier was. **Neither decides
anything**, and that is not caution: D47 measured the obvious threshold —
judging the multiplier against the traffic of the dot product that formed it,
the same move D23 made for rows — and it separates nothing, because what makes
a dropped term cost anything is the distance the variable would travel, which
is a property of the polytope and not of the column.

**So the distance is computed instead of thresholded.** `certified_suboptimality`
is `|w|` times how far that column can move on its own, with every other
variable pinned where it is. Nothing else moving means no other bound can be
broken, so the direction is feasible for its whole length and the number is a
lower bound on `P − P*` rather than an estimate — arrived at with no basis, no
factorization and no reference value (D73). Where that distance is finite the
product is self-limiting, which is why this needs no threshold: a multiplier
that is really roundoff certifies a roundoff-sized suboptimality. Where it is
infinite the product is infinite for any nonzero multiplier at all, so those
are counted in `unquantified_rays` instead of being reported as a certificate
— split on this checker's own `|w| <= tol`, the definition of nonzero it
already uses everywhere else.

**The primal residue, relative.** `max_row_violation_relative` reports the
worst row residue as a fraction of what that row carries — `sum_j |a_ij x_j|`,
the same quantity the bound-proximity window is built from. It decides
nothing, and D24 is the argument for why it is not allowed to: primal
feasibility is the hypothesis the identity above stands on, so relaxing it
would remove D23's licence rather than extend it. The measurement is kept
because it is real — `finnis` clears the absolute 1e-6 bar with 16% of the
margin to spare while its residue is a tenth of one ulp of the row it sits
in, and the absolute number cannot tell that from a row carrying 0.7.

The four tests are deliberately not independent. Activities come from a
scatter over the matrix while the dual objective accumulates from bounds,
so a corrupted dot product shows up as a nonzero gap even when it also
corrupts the reduced costs it would have to fool. The system is
overdetermined; one broken kernel cannot satisfy all of it.

## Acceptance, for the Netlib gate

Separate from all of the above, and not a solver tolerance: an instance is
accepted when `|obj − ref| <= 1e-6 · max(1, |ref|)` against Koch's
reference values [22], with the checker green in original space. That
criterion is relative, and a test that pins a large objective absolutely is
stricter than the project's own gate — which makes it a test about
floating-point luck rather than about the solver.

## What is not settled

Instances have now argued with them. The Netlib gate has been run over the
whole standard set (`bench/results/netlib.txt`), and **all 94 instances come
back with the checker green** at the tolerance above. All eight of the
original failures closed, and **not one of them closed by moving a number**:

| instance | what it actually was | |
|---|---|---|
| `pilot-ja` | a contribution the checker was dropping | D21 |
| `finnis` | a bound-proximity test judged absolutely on a row that cancels ten orders of magnitude | D23 |
| `nesm` | a settled basis the dual simplex had never been handed back | D25 |
| `grow15` | a cycle that had been read as a stall | D26 |
| `etamacro` | a repair test reading the wrong quantity, in the wrong space | D27 |
| `greenbea` | a column with nowhere to rest, needing a basis change rather than a move | D28 |
| `pilot` | an answer read off an inaccurate solve of a fresh factorization | D29 |
| `pilot87` | a clean-up loop dispatching one column of twelve | D30 |

That is the case for leaving these numbers where they are, made by instances
rather than by argument: every failure anyone was tempted to blame on a
tolerance turned out to be something else. `etamacro` is the sharpest,
because it genuinely was a question about a tolerance's *space* — its breach
is `4.89e-8` scaled and `1.56e-6` published. The answer was not to change
the tolerance or to pick a space, but to test a quantity that has neither:
the term the breach contributes to the duality gap, which comes out the same
number either way.

**Nothing is left of that list**, and the last two are worth a paragraph each
because both were, at the time, the strongest case anyone had for moving a
number.

**`pilot` was the one case where a tolerance was genuinely the question, and
it was the primal one.** Everything else about that answer was right: the
objective inside `2.3e-5` of Koch against a bar of `5.6e-4`, the dual
violation exactly zero, the gap `6.6e-14`. What refused it was
`interval_violation`, an absolute test, on a row `1.73e-6` — 1.73 tolerances
— outside its bound. D24 refused to make that test relative for four reasons;
D28 recorded that one of them had expired, and what replaced it was stronger:
the relative figure said `pilot`'s row was `6.93e-9` of what the row carries
against `8.21e-17` for `finnis`, so a relative window would have waved through
a violation that was real. **The row was real, and it was the answer that was
wrong.** D29 refined both solves of the refresh that verifies an optimum, and
the residue went from `1.73e-6` to `6.73e-13` — four hundred thousand times
narrower than the window anyone was proposing to widen, on a solve that came
out *cheaper*. It reads `9.09e-13` today.

That is the whole argument of this section arriving at its own last case:
the instance that most looked like a tolerance was not one either.

`pilot87` was not a tolerance question at all, and it turned out not to be a
precision one either: an objective 7.6x outside the bar was a wrong answer,
and loosening a number to admit it would have converted a defect into a pass
and proved nothing (D17). The defect was a clean-up loop dispatching one
column of twelve (D30), and the objective now lands 1.33e-7 relative from
Koch's exact value.

That is the argument for the freeze rather than a footnote to it. Eight
instances were refused across the campaign, every one was tempting to blame
on a number, and every one was something else. A tolerance that survived
eight opportunities to be the culprit and never was is a number with evidence
behind it — which is what these now are (D31), and why moving one from here
on takes a measurement on both sides and a changelog entry.
