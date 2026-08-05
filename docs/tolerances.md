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
| `ARTIFICIAL_BOUND` | 1e10 | The bound dual phase 1 lends a column whose cost points at a bound it does not have. **Known to be wrong in both directions** — see PLAN.md Q9, which is open |

Two more numbers in `src/simplex.c` are not tolerances but sit beside them:

| Name | Value | What it decides |
|---|---|---|
| `REFACTOR_EVERY` | 64 | Basis updates before a refactorization. The stability trigger PLAN 2.5.5 also calls for does not exist yet; only this interval and the reactive fallback on a failed update do |
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
|w| <= tol            no condition, no contribution   (negligible multiplier)
w > 0                 requires v <= lo + tol          (at its lower bound)
w < 0                 requires v >= hi - tol          (at its upper bound)
```

which is dual feasibility and complementary slackness in one test: a
multiplier that is not negligible must point at a bound its value is
actually resting on. A multiplier pointing at an infinite bound is itself a
violation, of exactly its own magnitude. Row multipliers are the duals as
given; column multipliers are the reduced costs `d_j = c_j − A_j · y`,
recomputed here from the original matrix.

**Gap.** As each multiplier is checked it contributes `w · bound` to the
dual objective, where `bound` is the one its sign points at. The gap is
then relative:

```
gap = |primal_objective − dual_objective| / max(1, |primal_objective|)
```

`dual_feasible` is the largest dual violation and the gap both being
within tolerance.

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

The values above are drafts in the specific sense that no instance has
argued with them yet. The Netlib campaign is where they are either
confirmed or moved with a reason, and `ARTIFICIAL_BOUND` is already known
to need replacing rather than adjusting.
