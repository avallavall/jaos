# Tolerances

Every number a solve compares against, where it acts, and what it decides.
They were drafts until the Netlib gate closed (PLAN.md 2.6). It has, so they
are frozen at the values below and a change to any of them is now a
changelog entry.

Three spaces are involved and confusing them is the way to misread every
figure below. The solver runs on a **scaled copy** of the model, so its
tolerances are magnitudes in scaled space (see `docs/scaling.md`). The
independent checker runs on the **model as loaded**, so its tolerance is a
magnitude in the units the caller wrote. Presolve also runs on the model as
loaded, and runs there *before the scaling exists* — so its constants are
magnitudes in the caller's own units too, but they are not the checker's
either: the checker's is a caller's diagnostic choice for judging a finished
answer, and presolve's decide what the solver is handed in the first place.
None of the three is converted into another; they are separate judgements,
which is the point of having them apart.

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

Three more numbers in `src/simplex.c` are not tolerances but sit beside them:

| Name | Value | What it decides |
|---|---|---|
| `REFACTOR_EVERY` | 64 | Basis updates before a refactorization. Alongside it: the reactive fallback on a failed update, and one more refactorization at the end of every solve, because optimality is not accepted on carried values (D20). The trigger PLAN 2.5.5 also calls for — watching an FTRAN/BTRAN residual *during* the solve — still does not exist |
| `ITER_SANITY_FACTOR` | 200 | Times `rows + columns + 1`, an iteration ceiling that is not a limit but a guard against a non-terminating loop. Hitting it is a defect in JAOS and is reported as a library error, never as a solve outcome |
| `WARM_REPAIR_MAX_SHORT` | 4 | How many basic members a mapped starting basis may be missing and still be repaired by promoting logicals rather than refused. It decides cost and never an answer: past the cap `build_warm_basis` returns false and the solve starts cold, which is always correct. Swept on both sides over every distinct shortfall in the set, from "never repair" to "always" — netlib work geometric mean 0.2553, 0.2089, 0.2047, **0.1916**, 0.1886, 0.1895, 0.1874, 0.1938 … 0.2605 at caps 0, 1, 2, **4**, 5, 6, 7, 8 … 596, with the worst per-instance ratio holding at 4.65 through cap 4 and then stepping to 15.48 at 7 (`greenbea`) and 172.03 at 345 (`dfl001`). The mean is flat across 1..7 and the worst case is not, so the value sits at the end of a plateau rather than at the minimum: 7 is 2.2% better on the mean for a worst case 3.3x larger. Kennington does not vote — all five of its short solves are short by exactly 1, so every cap at or above 1 gives it the whole gain, 0.0572 → 0.0070. **The relative shape was swept too and is worse**: capping `S/nrow` reaches only 0.2081 and meets the 15.48 cliff with 8 instances admitted, where the absolute cap admits 31 before reaching it, because `greenbea` is 7 short of 1954 rows — the smallest relative shortfall in the set and one of the two worst outcomes (D151) |

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

## Presolve's tolerances

Defined in `src/presolve.c`. Presolve runs on the model as loaded, before
`sx_init` computes any scaling (D-04), so nothing here is comparable with the
solver's table above — those are magnitudes in scaled space and the scaling
depends on a matrix presolve has just changed. Nor is the checker's `tol`
usable here: it is a number the caller supplies to judge a finished answer,
and it was never measured for deciding whether to fold a bound. Every
constant below is new and each arrives with its own sweep.

| Name | Value | What it decides |
|---|---|---|
| `PRESOLVE_ROUND_ULPS` | 8 | Whether a residue left by a running difference is a number. **Four sites ask it**, and the list has been wrong in this table before, so it is spelled out: the singleton row's fold, asking whether a column's interval has genuinely emptied; the emptied row, asking whether its bounds still admit zero after every column removed from it shifted them; the frozen row, asking whether its activity range has left its own bounds; and clause 1 of the activity pass, asking whether a live row can be satisfied at all. The window is this many `DBL_EPSILON` times the scale that produced the residue, never this value alone (D23's argument, in presolve's own space), **plus the count of terms that running difference accumulated** — see "the scale, and the term count" below. The scale is the row's traffic at three of the four; the fold divides it by its own coefficient, because it judges `cur_rl[i] / a` and the error came down with it. Set from a measurement of the residues themselves rather than from a sweep: instrumenting all three sites over all three sets emits 32240 probe lines, of which 12 carry a residue above zero (the fold site prints only on a collapse, so its share of that total is collapses rather than reaches), **all twelve on `netlib-infeas`** and none below 3.69e8 ulps. So no feasible model on the 139 puts a residue anywhere in (0, 3.69e8 ulps], the constant may be set anywhere in that interval, and 8 is taken because it is where `ps_row_tol` already is. Swept 1, 2, 4, 8, 16, 64, 256 with `make clean` between settings: solved 94, objective ok 94, checker ok 94 and 7598 rows / 24695 columns removed at every one. The canary flips four times inside the grid and the seven binaries have seven distinct md5s — the second check is the one that matters, because the canary's four conflicts do not separate 2 from 4 or 8 from 16 (D103) |
| `PRESOLVE_IMPLIED_FREE_ULPS` | 8 | Whether the box a row implies on a singleton column lies inside the column's OWN box, so the column's bounds can never bind and it can be substituted out. Subtracted from the column's bounds rather than added to the implied ones, so the family declines at exact equality: being wrong here is silent, and drops a bound that was real (D106). The window is this many `DBL_EPSILON` times `max(1, |b|, traffic) / |a_ij|`, which is the row sum's residue carried through the division that produces the implied end. **It is a switch, not a dial.** Swept 0, 1, 8, 64, 4096 with `make clean` between settings: rows removed set-wide read 9992, 8639, 8639, 8639, 8639, `maros-r7` reads 984, 980, 980, 980, 980, and solved / objective ok / checker ok are 94 at every one. One step, between 0 and 1, and four decades of nothing above it — the family's firing is bimodal, because an implied box is either comfortably inside the column's box or exactly at its bound and almost nothing lands in a 1e-12 relative band. The canary is in the instance rather than in a model built for it: 4 of `maros-r7`'s 984 candidate rows sit at exact equality, so 0 must read 984 and anything above it 980, and it does. That canary separates 0 from the rest and nothing else, so the check that carries the plateau is the second one — five settings, five distinct md5s of `presolve.o`. **Zero is not obviously wrong and is not free**: it removes 1353 more rows and reads a geometric mean of 0.9627x against 8, but `d2q06c` costs 2.2163x there, which crosses `bench/run.c`'s own 2.0x work bar. So 8 ships and the question of whether the window's absolute floor should exist at all is open in `TODO.md` |
| `JM_PRESOLVE_ROUNDS` | 16 | Cap on presolve's fixed-point rounds (D-02) — a safety stop and not a quality knob, since the loop exits as soon as a round changes nothing, and it lands on top of the structural backstop `num_row + num_col + 1` rather than above it. Set where the propagation reaches its fixed point: swept over the standard set, rows removed go 6060, 7178, 7549, 7596, 7598, 7598, 7598, 7598 at 1, 2, 4, 8, 16, 32, 64, 128 rounds, and columns removed 22671, 24300, 24629, 24693, 24695, 24695, 24695, 24695. The canary is a chain of 200 singleton rows built to resolve one link per round, and it reads 1, 2, 4, 8, 16, 32, 64, 128. The cost is flat across the whole sweep — 97.2 s to 103.6 s at `J=12` against a set that takes about 99 s — so there is nothing to trade against |

## The scale, and the term count

**All three sites scale by the row's traffic, and that took three decisions to
arrive at.** The pass that tests a frozen row for feasibility after the round
loop used to scale by `ps_bound_scale(cur_rl[i], cur_ru[i])`, and the paragraph
here used to call that forced. It was not:

- `row_traffic[i]` no longer saturates to `+inf` when a column with a
  half-infinite box is relaxed out of a row. It accumulates only what a finite
  end absorbed (**D155**), so the reason the bound scale stood in expired.
- The traffic is what the window has to cover, and the bound's magnitude is
  not (**D159**).
- The bound's magnitude is worse than merely irrelevant. It is the magnitude
  of ONE END, and the test compares a computed activity against the OTHER
  (**D161**): `-1e12 <= x0 + x1 <= 0` with both columns cost-0 in `[1e-4, 1]`
  is infeasible by 2e-4, and the lower bound alone bought a window of 1.78e-3
  on the upper side. It published `optimal`. The same model with
  `rl = -INFINITY` was refused correctly all along, which is the control.

`ps_row_tol` still cannot be used at that site: it scales by the LIVE traffic,
and a frozen row's live traffic is routinely zero, which collapses the window
to 1.776e-15 absolute whatever the row's scale.

**And the constant alone is not the whole window, because `cur_rl`/`cur_ru` are
a running difference (D162, D163).** Every removed column subtracts its own
`a * v` from both, with no compensation, so each subtraction rounds by up to
half an ulp of the partial it produces and the error after k of them goes with
k. Eight ulps covers a k of about three; **the largest k on the three sets is
325**, at the frozen-row test on Kennington, and this table owns that number —
the source comments point here rather than repeating it. So every window that
judges one of those numbers carries a second term:

> `k * DBL_EPSILON * (|the end being tested| + row_traffic[i])`

Three things about it, each of which was got wrong first and measured or tested
into shape (`bench/measurements/02-72/`):

- **The scale is not the traffic alone.** A partial is bounded by
  `|row_lower[i]| + traffic`, and near the firing boundary `cur_rl[i]` is near
  the ACTIVITY rather than near zero, so the traffic does not dominate. A row
  of activity 1e9 with 300 removals totalling 0.9 of traffic carries about
  1.8e-5 of error against a traffic-only window of 6.8e-14.
- **It is ONE end and not `ps_bound_scale`.** Scaling by the larger of the two
  brings D161's defect back through the count — two cost-0 singleton
  relaxations are two shifts, and `2 * eps * 1e12` is 4.4e-4 against an
  infeasibility of 2e-4. D161's own test caught it.
- **It is zero at k = 0**, which is what keeps D161 for a row nothing was ever
  removed from.

It is ADDED to the eight ulps, never substituted for them, so no window can
come out narrower than it was — narrowing one of these is what produces a false
INFEASIBLE. Measured over the three sets before landing: **0 verdicts flip** on
any of the 139, the twelve genuine infeasibility firings in `netlib-infeas`
survive it, and **the widest ABSOLUTE window is 6.587e-08**, at Kennington's
frozen-row test, against 6.494e-08 without the count. This table owns that
figure too.

**The base moved as well as the k-term (D162), and the entry did not say so at
first.** The two halves are added where the wider scale used to be taken, so a
row with both traffics large pays eight ulps twice instead of once, whatever k
is. At k = 0 that is 8 ulps of 1, which is 1.78e-15, because a traffic above
zero implies a shift count above zero at every producer.

**The fourth site landed one entry later (D163)**, and the three that came
first were described here as "every site that judges a running difference"
while the fold did not carry the count. That sentence was false for one commit.
The fold now takes it, scaled by `row_traffic[i] / |a|` and by the end
`tightens_lo`/`tightens_hi` says the running difference supplied.

**The reopen condition is the absolute window and not a ratio.** A set carrying
a larger `rg.traffic` than Kennington's 3.66e7, or a row with a shift count far
above 325, is where these windows stop being comfortably under `PRIMAL_TOL`.

**And the count does not cover an error that arrives inside a VALUE.** When the
fold fixes a column at `cur_rl[i] / a`, that number carries row i's accumulated
error, and the row receiving the fixed column is charged one shift at its own
traffic. `bench/measurements/02-73/` has the model where that publishes
INFEASIBLE on a feasible model. **Carrying the error into the window was built
and refused** (D164): it stops the refusal and then publishes a point violating
two rows by 7.5 times `CHECK_TOL`, because a window decides whether to refuse
and cannot correct a value that is already wrong.

**The error itself was removed instead (D165), and this whole section now
describes a window covering something that is no longer there.**
`cur_rl`/`cur_ru` keep their residue through a Neumaier accumulator, so the
running difference no longer drifts and the fold fixes columns at the value the
model actually has. The counts still ship: taking them out NARROWS four
windows, which is the direction that refuses feasible models, so it is its own
change with its own measurement and `TODO.md` carries it. Until then two
mechanisms cover the same error: `ps_bound_shift` stops it happening, and the
counts described here would still cover it if it did.

## The proxy the constant used to rest on

Kept because it is what "measured from one side only" looks like, and because
the number it produced is still true.

> accumulated shift / `ps_bound_scale(cur_rl, cur_ru)` ≤
> `PRESOLVE_ROUND_ULPS` = 8

Measured over the 8293 frozen rows the standard set produces: worst ratio
**2.718e-4** (`finnis` row 99), or 4.834e-4 counting one rounding per stored
entry rather than the eight the proxy assumes.

The scale mostly recovers itself, which is why the margin is so wide: in an
emptied frozen row one bound falls to zero and the other retains the magnitude
that was subtracted, so `ps_bound_scale` returns the shift. `greenbea` row 57
carries 660 of shift and reads `cur_rl = -660, cur_ru = 0`. The 60 rows of
8293 where that does not happen are the ones with an infinite bound, where the
scale falls to its floor of 1; their largest shift is 153.

**It bounds the window from one side only.** It says the window is not too
tight. Nothing in it says the window is not too wide, and at 1e-9 it was: on a
row of magnitude 1e9 the window is 1.0, and

```
min x0  s.t.  x0 + x1 == 1e9 + 1,  x0 in [0.5, 0.5],  x1 in [0, 1e9]
```

is infeasible by 0.5 and walked straight through it, reaching
`jm_postsolve_solved` and publishing OPTIMAL with `x1` half a unit above its
own declared upper bound. The model's ratio is about 1.0, far inside the
5.6295e5 the proxy allowed, so no amount of tightening the proxy would have
found it (D103).

The residue measurement replaced the proxy on both sides. Of the 19082 frozen
rows across all three sets, exactly 4 carry a residue above zero and all four
are on `netlib-infeas` at 1.5e15 ulps or more, so a window of 8 refuses
everything that should be refused and nothing else. `galenet` and `pilot4i`
are still caught (D102), now with margins larger than D102 recorded rather
than smaller.

**One number in `src/presolve.c` is deliberately not in this table.** The
three readings of a row's activity range — the model is infeasible, the row
is forced to an extreme, the row can never bind — each ask whether a computed
sum equals a bound. That is not a judgement and has nothing to tune: the only
thing that can separate two numbers that should be equal is the rounding in
the sum, so the window is a small multiple of `DBL_EPSILON` times the traffic
through it. Making it a tunable instead cost 02-04 a campaign, and the raw
readings are in `bench/measurements/02-04/`.

`ps_row_tol` therefore keeps its own literal 8 and does NOT read
`PRESOLVE_ROUND_ULPS`, though the two agree today. 02-09 routed it through the
shared function for a few hours and put those three readings on the
`EXTRA_CFLAGS` hook, which review caught: `make netlib
EXTRA_CFLAGS=-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=64` reproduced 02-04's failure,
`pilot` INFEASIBLE with column 3554 pinned. Two constants that happen to be
equal are not one constant. If they ever have to move together, that is a
decision with a measurement behind it and not a shared symbol.

**Bound tightening is not here because it does not ship.** 02-04 built the
family, measured six variants of it against the standard set and refused all
six: every one returned INFEASIBLE on models that have an optimum. The
evidence is in the same directory and the reasoning is in `src/presolve.c`
beside the reading that would have been the fourth.

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
