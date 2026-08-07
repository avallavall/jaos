# Tolerances

Every number a solve compares against, where it acts, and what it decides.
All of them are drafts until the Netlib gate closes (PLAN.md 2.6); after
that a change to any of them is a changelog entry.

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
w > 0                 requires v <= lo + tol          (at its lower bound)
w < 0                 requires v >= hi - tol          (at its upper bound)
```

which is dual feasibility and complementary slackness in one test: a
multiplier that is not negligible must point at a bound its value is
actually resting on. A multiplier pointing at an infinite bound is itself a
violation, of exactly its own magnitude. Row multipliers are the duals as
given; column multipliers are the reduced costs `d_j = c_j − A_j · y`,
recomputed here from the original matrix.

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
`sum_v w_v (v_v − bound_v)` with every term non-negative — each one the
complementary-slackness residue of a single variable. That is what gives an
accepted solution a guarantee rather than a reassurance: `P − P* <= gap`,
by weak duality, and the gap is reported.

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
whole standard set (`bench/results/netlib.txt`), and 86 of 94 instances come
back with the checker green at the tolerance above. The eight that do not
split into two groups, and only one of the groups is about a tolerance:

| Instance | worst dual violation | gap | |
|---|---|---|---|
| `finnis` | 28 | 8e-11 | far too large to be a tolerance |
| `greenbea` | 2.66 | 7e-17 | likewise |
| `pilot` | 0.019 | 1.7e-05 | likewise, and misses the objective too |
| `pilot87` | 0.0096 | 6e-05 | likewise |
| `nesm` | 8.0e-06 | 5e-11 | within one order of the tolerance |
| `etamacro` | 1.6e-06 | 4e-09 | just past it |
| `pilot-ja` | 0 | 1.9e-06 | fails on the gap alone, just past it |

A dual violation of 28 is not a number that moves by widening 1e-6, and
neither is one of 2.66. Those are defects to find. The last three are the
only candidates for the tolerance itself being mis-set, and even there the
question is which of the two spaces is wrong rather than what the digit
should be — so nothing here is a case for loosening a number, which would
convert four defects into eight passes and prove nothing (D17).

`grow15` is the eighth failure and is not about tolerances at all: it does
not terminate.

The values also stay drafts in the original sense — nothing is frozen until
the gate closes, and every one of them moves only with a measurement on both
sides.
