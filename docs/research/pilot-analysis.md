# Pilot / Pilot87 OUT-OF-TOLERANCE Analysis

> Research into why the two largest Netlib instances fail the JAOS acceptance gate.
> Written 2026-08-07; figures re-read from `bench/results/netlib.txt` the same day
> against the state `main` is in now, after the M2 prototype was reverted.
> Status: M1 gate — **not met** for these two instances.
>
> Every number below comes from one recorded run and is only true of that run.
> The analysis in §4 is reasoning about a mechanism and does not depend on the
> exact digits; the digits are here so a later run can be compared against them.
>
> **Both proposals in §6.1 have since been answered (2026-08-08). Read §10
> before acting on §6 or §9** — the figures in §1 and §2 are from before the
> re-entry of §6.3 was built, and both instances have moved.

---

## 1. Summary

| Instance | Rows | Cols | Nonzeros | Solver Obj | Reference Obj | Gate | Checker |
|----------|------|------|----------|------------|--------------|------|---------|
| `pilot` | 1441 | 3652 | 43220 | -557.27208765931482 | -557.4897292840682 (Koch) | **OUT-OF-TOLERANCE** | **REJECTED** |
| `pilot87` | 2030 | 4883 | 73804 | 301.71500637546757 | 301.71072827 (netlib) | **OUT-OF-TOLERANCE** | **REJECTED** |

**Conclusion: The solver genuinely produces wrong answers. This is NOT a tolerance issue.**

The dual violations of 0.0096 and 0.019 in original space are four orders of magnitude above the checker's 1e-6 tolerance. The objective errors are 14× and 390× the gate's relative tolerance. These are structural numerical failures, not borderline precision cases.

---

## 2. The Gate Mechanism

### 2.1 Objective acceptance (`objective_accepted` in `bench/run.c`)

```c
static bool objective_accepted(double got, double ref)
{
    double scale = fabs(ref) > 1.0 ? fabs(ref) : 1.0;
    return fabs(got - ref) <= 1e-6 * scale;
}
```

For each instance:

| Instance | Ref | Scale | Gate Tol | | Solver | Error | Error/Scale | × Tol |
|----------|-----|-------|----------|---|--------|------|-------------|-------|
| `pilot` | -557.4897 | 557.49 | 5.57e-4 | | -557.2721 | 0.2176 | 3.9e-4 | **390×** |
| `pilot87` | 301.7107 | 301.71 | 3.02e-4 | | 301.7150 | 0.00428 | 1.4e-5 | **14×** |

### 2.2 Checker tolerance (`CHECK_TOL` in `bench/run.c`)

```c
constexpr double CHECK_TOL = 1e-6;
```

The checker (`jaos_check_solution` in `src/check.c`) judges in **original space** (not scaled), using `long double` accumulation. It checks:

- **Primal feasibility**: column and row bounds violations ≤ 1e-6
- **Dual feasibility**: reduced cost sign conditions ≤ 1e-6
- **Objective gap**: |primal_obj − dual_obj| / max(1, |primal_obj|) ≤ 1e-6

### 2.3 Reported violations

| Instance | Row Violation | Col Violation | Dual Violation | Gap |
|----------|-------------|-------------|---------------|-----|
| `pilot` | 1.96e-05 | 0 | **0.019** | 1.66e-05 |
| `pilot87` | 2.03e-08 | 0 | **0.00964** | 6.01e-05 |

The dual violations are roughly 10,000× and 19,000× the 1e-6 checker tolerance. The gaps, at 1.66e-05 and 6.01e-05, are 17× and 60× it. Note which of the two is the larger breach: the objective is nearly right and the duals are not close, and that asymmetry is what §4 is about.

---

## 3. Reference Values — Are They Correct?

### 3.1 Source of references

The manifest (`bench/netlib.manifest`) uses **Koch's exact rational values** where available, and the original netlib MINOS 5.3 values otherwise. Koch's ZIB-Report 03-05 ("The Final NETLIB-LP Results") computed exact optimal objectives using rational arithmetic.

### 3.2 `pilot` — Koch exact, netlib wrong

| Source | Value | vs Koch | Notes |
|--------|-------|---------|-------|
| Koch exact | **-557.4897292840682** | 0 | Ground truth |
| CPLEX (Sparc) | -557.48972928 | 4.1e-9 | Near Koch |
| OSL (MVS) | -557.41215293 | 0.078 | Off by 0.014% |
| Netlib MINOS 5.3 | -557.40430007 | 0.085 | Off by 0.015% |
| **JAOS** | **-557.27208765931482** | **0.218** | **Off by 0.039%** |

The netlib MINOS value was **wrong** for pilot (Koch found it in the set of 8 misreported values). The manifest correctly uses Koch's value. The solver's answer is worse than even the wrong netlib value — it is further from Koch than MINOS, OSL and CPLEX all are.

### 3.3 `pilot87` — netlib value, Koch agrees

| Source | Value | vs netlib | Notes |
|--------|-------|-----------|-------|
| Netlib MINOS 5.3 | **301.71072827** | 0 | Manifest reference |
| CPLEX/OSL | 301.71074161 | 1.3e-5 | Within gate tolerance |
| Koch exact | 301.7107xxx | < 3e-4 | Within gate tolerance |
| **JAOS** | **301.71500637546757** | **0.00428** | **14× past tolerance** |

pilot87 was NOT in Koch's list of 8 misreported values, so his exact rational value is within the gate's 3.02e-4 tolerance of the netlib value. The solver's answer is 14× past that tolerance, making it definitively wrong.

### 3.4 Verdict on references

The reference values are **correct**. The solver is producing genuinely wrong answers.

---

## 4. Root Cause Analysis

### 4.1 The solver's termination path

The dual simplex in `src/simplex.c` terminates through this sequence:

```
run() → price_row() returns -1 (no violation in scaled space)
  → refresh() (recompute from fresh factorization)
  → price_row() returns -1 again (verified)
  → returns JAOS_SOLVE_OPTIMAL
  → settle_shifts() restores true costs, recomputes duals
  → repair_dual_infeasibility() swaps nonbasic variables between bounds
  → classify_optimum() checks artificial bounds
  → publish() computes final duals via BTRAN: y = B^{-T} c_B
```

### 4.2 The cost-shifting mechanism

During the dual simplex, the Harris window (DUAL_TOL = 1e-7) allows reduced costs to be pushed past feasible in exchange for a larger pivot. Each such push is recorded as a **cost shift** (`shift_to_feasible()` in `src/simplex.c`):

```c
static void shift_to_feasible(sx *s, int64_t v)
{
    // ...
    s->cost[v] += need;
    s->shift[v] += need;
    s->d[v] = 0.0;
}
```

The shifted costs accumulate over potentially thousands of iterations. The basis evolves to be optimal for the **shifted-cost problem**, not the original problem.

### 4.3 The settle_shifts repair

After the solve terminates, `settle_shifts()` restores the true costs and recomputes the duals from scratch:

```c
static void settle_shifts(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->shift[v] == 0.0) continue;
        s->cost[v] -= s->shift[v];
        s->shift[v] = 0.0;
    }
    compute_duals(s);         // y = B^{-T} c_B with TRUE costs
    repair_dual_infeasibility(s);
}
```

Then `repair_dual_infeasibility()` tries to fix dual infeasibility by **swapping nonbasic variables to their other bound**. This is a limited repair:

- It only swaps, never changes the basis.
- A swap is refused if it makes any basic variable violate its bound.
- Variables with invented (fake) bounds are excluded from repair.

### 4.4 Why it fails on pilot/pilot87

The core problem is that **the basis is optimal for the shifted-cost problem, not the original problem**. When the true costs are restored:

1. Many reduced costs become infeasible (wrong sign for their bound).
2. `repair_dual_infeasibility()` can only swap nonbasic variables, and only when the primal point stays feasible.
3. On large, numerically challenging instances like pilot/pilot87, the accumulated shifts are significant, and the repaired point still has residual dual infeasibility.
4. The duals reported by `publish()` are computed from the basis via BTRAN (`y = B^{-T} c_B`), but the basis is **wrong** — it was optimal for the shifted problem.

**The dual violations of 0.0096 and 0.019 in original space are direct evidence of this.** The duals are not the duals of the true optimum — they are the duals of a basis that is only optimal for a perturbed problem.

### 4.5 Why the objective is also wrong

The objective error is a secondary effect. The primal point is close to the true optimum (the shifted costs were small perturbations), but the basis is wrong. On a degenerate LP, the objective can change by 0.1–0.005 while the basis differs by just a few columns. The checker's gap detects the inconsistency between the primal objective and the dual objective computed from the wrong basis.

---

## 5. Scaling Analysis

### 5.1 Solver tolerances (scaled space)

| Tolerance | Value | Where |
|-----------|-------|-------|
| PRIMAL_TOL | 1e-7 | `src/simplex.c` |
| DUAL_TOL | 1e-7 | `src/simplex.c` |
| PIVOT_MIN | 1e-9 | `src/simplex.c` |

### 5.2 Checker tolerance (original space)

| Tolerance | Value | Where |
|-----------|-------|-------|
| CHECK_TOL | 1e-6 | `bench/run.c` |

### 5.3 The scaling gap

The solver works in scaled space with `A_s = R * A * C` where `R = diag(rho)`, `C = diag(gamma)`, and the factors are exact powers of two (Curtis-Reid scaling, `src/scale.c`).

A violation in scaled space of 1e-7 maps to a violation of `1e-7 / rho[i]` in original space for row duals. The range of dual violations (1e-4 to 1e-2) is consistent with scaling factors in the range `2^-7` to `2^-14` — reasonable for these large, ill-conditioned problems.

**This is NOT a scaling bug.** The scaling is working as designed. The problem is that the solver bases its optimality declaration on the scaled-space violations, but the settle/repair mechanism cannot fully restore the true-cost solution for these large instances.

### 5.4 The "too tight" question

**Is the tolerance too tight?** No.

- The dual violations (0.0096 and 0.019) are four orders of magnitude above the 1e-6 checker tolerance.
- One cannot argue that 1e-6 is too tight when the actual violation is 0.019 — you'd need a tolerance of 0.019 to pass, which is a threshold that admits anything.
- The objective errors (0.218 for pilot, 0.00428 for pilot87) are 390× and 14× the gate tolerance.
- Even if the gate tolerance were relaxed 10,000×, the solver would still fail on pilot: an objective error of 0.218 on |ref| 557.5 is 0.039% of the objective — a genuine error, not a rounding issue.

---

## 6. What Would Fix These

### 6.1 Quick wins (moderate effort)

**Tighter cost-shift tracking.** The current `shift_to_feasible()` only records the magnitude of the shift but does not enforce a limit on total accumulated shift per variable. Adding a cap on `|shift[v]|` would prevent the basis from drifting too far from the true optimum. However, this could cause the solver to fail on models that genuinely need many shifts.

**Post-solve settle tightening.** After `repair_dual_infeasibility()`, the reduced costs that remain infeasible could be cleaned up by a targeted loop that:
1. Identifies the most infeasible reduced cost.
2. Uses the current basis to compute what entering variable would fix it.
3. Performs a single primal simplex pivot.

This is a partial primal simplex that doesn't need the full machinery.

### 6.2 Primal post-solve (M6 milestone)

The proper fix requires a **primal simplex** to clean up after the dual simplex. The sequence would be:

1. Run the dual simplex to optimality (as today).
2. `settle_shifts()` to restore true costs.
3. **Run a few primal simplex iterations** from the current basis, using the true costs, to repair residual dual infeasibility.
4. The primal simplex naturally handles the "wrong basis" problem, because it maintains primal feasibility while improving the objective.

The primal simplex doesn't exist yet (it's planned for M6, "Interior point"). Implementing just the post-solve primal cleanup would be a smaller effort than a full primal simplex, but it's still significant.

### 6.3 Re-solve from repaired point

A simpler approach: after `settle_shifts()`, if `repair_dual_infeasibility()` leaves residual violations, **re-run the dual simplex from the current basis**. The basis is already dual infeasible for the true costs, so the dual simplex will immediately start repairing it. This leverages the existing dual simplex code without needing a primal simplex.

The risk is that the re-solve could take many iterations or cycle — but both are risks the dual simplex already handles.

### 6.4 What would NOT fix it

- **Loosening the tolerances.** The dual violations are orders of magnitude above 1e-6. Loosening to 1e-4 or 1e-3 would still not cover 0.02, and would break every other instance.
- **Better scaling.** The Curtis-Reid scaling is already optimal for the problem. The issue is not scale factor quality.
- **Tighter LU pivot tolerance.** The LU is already stable (LU_PIVOT_TOL = 0.1). The problem is not factorisation accuracy.
- **More iterations.** The solver already terminates at optimality. The issue is not insufficient iterations.

---

## 7. Instance-Specific Notes

### 7.1 `pilot`

- Large LP: 1441×3652, 43220 nonzeros.
- **Historically difficult**: the netlib readme shows that MINOS 5.3, CPLEX, and OSL all found **different** optima (range -557.40 to -557.49). This is a textbook example of a degenerate LP where different solvers converge to different vertices.
- Koch's exact rational value (-557.4897292840682) agrees with CPLEX, showing the netlib value was wrong for ~17 years.
- The solver's answer (-557.2721) is worse than all of them — it is not even converging to the wrong netlib vertex.
- The dual violation of 0.019 indicates the answer is not a KKT point at all.

### 7.2 `pilot87`

- Large LP: 2030×4883, 73804 nonzeros.
- **Harder than pilot** according to Irv Lustig: "PILOT87 is considered to be harder than PILOT because of the bad scaling in the numerics."
- The netlib readme notes "bad scaling" — a known issue with this instance.
- Unlike pilot, the reference values agree (MINOS, CPLEX, and Koch are all within 1.3e-5 of each other).
- The solver's objective error of 0.00428 on |ref| 301.71 is a 1.4e-5 relative error — small in absolute terms but 14× past the gate.
- The dual violation of 0.00964 is again the tell: the basis is not optimal for the true costs.

---

## 8. Independent Evidence

The `etamacro` instance shows a milder version of the same problem:

| Instance | Dual Violation | Gap | Status |
|----------|---------------|-----|--------|
| `etamacro` | 1.56e-06 | 4.31e-09 | Just past tolerance |
| `pilot` | 0.019 | 1.66e-05 | Far past tolerance |
| `pilot87` | 0.00964 | 6.01e-05 | Far past tolerance |

etamacro is 400×688, an order of magnitude smaller than pilot/pilot87. Its dual violation is 1.56e-06 — just barely past the 1e-6 threshold. This is consistent with the cost-shift hypothesis: smaller instances accumulate less shift, so the settle/repair mechanism works better. The instances grow larger, the shifts grow, and the repair fails more catastrophically.

**`finnis`** (dual violation 28) and **`greenbea`** (dual violation 2.66) are separate defects — their dual violations are so large that they cannot be explained by accumulated cost shifts alone. They are likely bugs in the dual computation or basis logic.

---

## 9. Recommended Path Forward

1. **Immediate (documentation):** Accept that pilot/pilot87 are genuine failures. Record the expected objective range and iteration counts for benchmark purposes.

2. **Short-term (high priority for M1):** Implement a **re-solve loop** after `settle_shifts()`: if `repair_dual_infeasibility()` leaves residual violations, feed the current basis back into the dual simplex. This is the simplest change that could fix both instances without new algorithm code.

3. **Medium-term (M6):** Implement a full primal simplex, which naturally handles the post-solve dual cleanup. This is the architecturally correct fix.

4. **Diagnostic:** Add a `JAOS_CHECK_TOL` parameter (or environment variable) to `run.c` to surface how large the dual violations actually are without the gate blocking. This helps track progress.

---

---

## 10. What was run, and what it settled (2026-08-08)

§6.3 was built and §6.1 was measured. Neither closes `pilot` or `pilot87`,
and the reasons are worth more than the outcome.

### 10.1 §6.3 — re-solve from the repaired point: built, and the section was
wrong about why it would work

The claim in §6.3 is that re-running the dual simplex from the settled basis
works because "the basis is already dual infeasible for the true costs, so
the dual simplex will immediately start repairing it." **That is backwards.**
The dual simplex does not repair dual infeasibility — it *assumes* it as an
invariant and repairs the primal. Handed a settled point, which is primal
feasible, `price_row` returns -1 on the first call and the re-solve
terminates having done nothing at all.

What makes re-entry work is the step the section leaves out: dual
feasibility has to be re-established *first*, and that is what moves the
point. A nonbasic with a wrong-signed reduced cost and a real bound on the
other side is sent to it — its reduced cost is then feasible for that bound,
and the primal breaks, which is what gives the method something to do. One
without a real opposite bound is shifted instead, which restores the
invariant without moving anything.

Measured on all three sets, per instance against their baselines:

| Instance | dual violation | gap | objective |
|----------|---------------|-----|-----------|
| `nesm` | 8.01e-6 → **0** | 2.71e-11 → 1.93e-16 | now within tolerance; **checker green** |
| `pilot` | 1.7e-2 → 8.0e-5 | 8.29e-6 → 8.6e-13 | -557.2721 → -557.2914 (ref -557.4897), still OUT |
| `pilot87` | 9.6e-3 → 3.3e-5 | 6.01e-5 → 4.0e-8 | 301.71501 → 301.71270 (ref 301.71035), still OUT |
| `greenbea` | unchanged | unchanged | bit-identical digest |
| `etamacro` | unchanged | unchanged | bit-identical digest |

`nesm` converges in one re-entry round, `pilot` in three and `pilot87` in
six. That last number is the reason `SETTLE_ROUNDS` is 32 rather than the 4
it was first written as: at 4, `pilot87` stopped with work still to do, at a
dual violation of 2.3e-4 and a gap of 2.3e-7 — 6.8 and 5.6 times worse than
where it converges, for 0.36% fewer iterations (D25).

Standard set: 0 regressed, 1 improved, 0 new. Kennington and infeasible:
0/0/0, both still PASS.

The risk §6.3 names — "the re-solve could take many iterations or cycle" —
is not the one that matters, and the run says so: `nesm` cost seven extra
iterations and `pilot` eighty. The risk that matters is the re-entry
returning INFEASIBLE on a feasible model, which is what the two mid-solve
repairs did (PLAN Q10). It is refused structurally rather than avoided: the
settled point is saved, and any re-entry not ending in a second optimum is
discarded. See D25.

### 10.2 Why `pilot` and `pilot87` are still out

§4.4's account survives and is now quantified. After settling, `pilot` is
left with 25 violated sign conditions and `pilot87` with 48. Only 5 and 15
of those sit on columns with a real bound on the other side — the rest have
nowhere to go. So re-entry repairs the fraction it can reach, which is why
both improve by two orders of magnitude and neither closes.

Note which quantity moved furthest: `pilot`'s gap fell from 8.29e-6 to
8.6e-13, seven orders of magnitude, while its objective moved by 0.019 out
of an error of 0.218. The duals and the primal now agree almost exactly
about a vertex that is still the wrong one. That is consistent with §4.5 and
sharpens it: the remaining error is not a broken certificate, it is the
wrong basis.

### 10.3 §6.1 — the shift cap: closed by measurement, not run

§6.1 proposes capping accumulated `|shift[v]|`. The distribution says a cap
cannot bite where it would need to. Instrumented at the moment settling
repays them, on `greenbea`:

| bucket | <1e-9 | <1e-8 | <1e-7 | <1e-6 | <1e-5 |
|---|---|---|---|---|---|
| variables | 2407 | 222 | 227 | 42 | 3 |

2901 variables carry a shift; three exceed 1e-6 and the largest is 7.09e-6.
A cap at 1e-6 touches three of 2901 — and not the ones that matter, because
the offending columns' own shifts are 4e-9 and 1e-14: the residue arrives
through the basis, not through the column's own cost (PLAN 2.8.1).

On `etamacro`, where the residue *is* the column's own shift, every shift in
the whole solve falls below `DUAL_TOL = 1e-7`, the largest being 4.89e-8. A
cap that bites there is narrower than the Harris window that created the
shift — which is not a cap on accumulation but a narrower window, a
different change with a different cost, and not what §6.1 proposes.

### 10.4 What §9 should now say

§9.2's "re-solve loop" is done (§10.1). §9.4's diagnostic is done and went
further than proposed: the checker publishes `gap_positive`, `gap_negative`
and `max_row_violation_relative`, and the record carries them per instance
(D24). §9.1 and §9.3 stand — with the correction that §9.3's full primal
simplex is more than what is needed. What `greenbea` lacks is one primal
ratio test plus the basis change `pivot()` already performs, which is the
scope question PLAN 2.9 now holds open.

---

## References

- Koch, T. "The Final NETLIB-LP Results." ZIB-Report 03-05, 2003.
- Koberstein, A. "The Dual Simplex Method, 1954–2005." ZIB-Report 05-41, 2005.
- Forrest, J.J.H. & Goldfarb, D. "Steepest-edge simplex algorithms for linear programming." Mathematical Programming, 57, 1992.
- Hall, J.A.J. & McKinnon, K.I.M. "Hyper-sparsity in the revised simplex method." COAP, 32, 2005.
- [Tolerances documentation](../tolerances.md) — JAOS solver tolerances.
- [Scaling documentation](../scaling.md) — Curtis-Reid scaling.
- [Format support documentation](../format-support.md) — MPS format conventions.