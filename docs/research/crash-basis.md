# Crash Basis Techniques for Dual Simplex (JAOS M2)

> Research summary for implementing a crash initial basis in JAOS C23 dual simplex.
> Current state: slack basis (all logical variables basic). Target: reduce iteration count via better B.

---

## Overview

A **crash basis** replaces the trivial slack basis (B = I) with a basis containing structural (decision) variables, giving a better initial vertex and fewer iterations. For dual simplex specifically, the starting basis must be **dual feasible** — a constraint that disqualifies several primal-oriented crash methods without modification.

| Method | Type | Dual-Suitable? | Effort | Iter. Reduction | DSE Compat? |
|--------|------|:---:|:------:|:---------------:|:-----------:|
| Slack Basis | Baseline | ✅ Trivial | None | 0% (baseline) | ✅ Perfect |
| Maros Crash (Triangular) | Greedy pivot-in | ⚠️ Needs mod | Medium | 30–50% primal | ⚠️ Costly init |
| CPLEX/Bixby Crash | Preference sets | ⚠️ Needs mod | Medium | 25–45% primal | ⚠️ Costly init |
| Koberstein Dual Crash | Weighted triangular | ✅ Built for dual | Medium+ | 15–30% | ⚠️ Needs exact init |
| IDIOT (Quadratic Penalty) | Approx solver | ❌ Primal-only | High | Up to 10× (primal) | ❌ N/A |
| Cosine Criterion | Angle-based | ✅ Yes | Low | 20–40% | ✅ Compatible |
| SoPlex STARTER_WEIGHT | Dual-weighted greedy | ✅ Yes | Medium | 20–35% | ⚠️ Needs init |

---

## 1. Slack Basis (Current JAOS Baseline)

**Reference:** Chvátal (1983), Bertsimas & Tsitsiklis (1997), Maros (2003) — *Logical Basis*

All slack (logical) variables are basic. The basis matrix is the identity I.

**Algorithm:** Trivial — set B = {n+1, ..., n+m}.

**Advantages:**
- **No construction cost** — zero time to build
- **B = I** → LU factorization is free (no fill-in)
- **Dual steepest-edge weights γⱼ = 1.0 exactly** — the identity basis has all columns as unit vectors, so the DSE weight is trivially correct
- Works with any pricing rule, any ratio test
- **Dual feasible** if RHS and bounds permit

**Disadvantages:**
- Zero structural variables in the basis → the initial vertex is typically far from optimal
- Phase I (dual) must pivot out all infeasibilities from scratch

**Iteration count:** Baseline (0% reduction).

**Verdict:** Safe default. All crash methods are compared against this.

---

## 2. Maros Crash Basis (Triangular Crash)

**References:**
- Maros & Mitra (1998). *Strategies for Creating Advanced Bases for Large-Scale Linear Programming Problems*. INFORMS J. Computing, 10(2), 248–260.
- Maros (2003). *Computational Techniques of the Simplex Method*, Ch. 9.8.2. Springer.

### Algorithm

A greedy triangularization procedure:

1. Partition A = [Â, I] where Â = structural columns, I = logical columns.
2. Define row counts Rᵢ = nnz in row i of Â, column counts Cⱼ = nnz in column j of Â.
3. Select pivot row i = argmin Rₖ.
   - If Rᵢ = 1 → pivot column is uniquely determined.
   - If Rᵢ > 1 → pick column j with smallest Cⱼ (minimizes fill-in).
4. Pivot column j into the basis at position i.
5. Remove row i and column j from consideration; update R and C counts.
6. Repeat until m columns selected or no candidates remain.
7. Fill remaining basis slots with logical (slack) columns.

**Variants (Maros & Mitra 1998):**
- **CRASH(LTSF):** Adds feasibility scoring — prefers columns whose variable has a wide feasible range.
- **CRASH(ADG):** Adds degeneracy avoidance — penalizes columns likely to cause degenerate pivots.

### Implementation Complexity

- **Effort:** ~300–500 lines of C23
- **Needs:** Row/column count arrays (Rᵢ, Cⱼ), pivot selection loop, triangularity check
- **Dual feasibility:** Not guaranteed — the resulting basis may be primal-oriented. A dual Phase I is still needed.

### Dual Simplex Caveat (Koberstein 2005)

Koberstein's PhD thesis (§7.1) tested crash bases in MOPS (dual simplex with DSE) and found:
> *"Our default is not to use a crash basis for the dual simplex in MOPS."*

**Reasons:**
- Crash basis is denser → slower FTRAN/BTRAN per iteration
- DSE weights are **all 1.0** for slack basis, but must be **exactly computed** for crash basis (up to m BTRAN solves) — expensive
- Even with exact DSE initialization, the per-iteration time increase typically **outweighs the iteration reduction**

**Expected iteration reduction:** 30–50% for primal simplex, but only 10–20% net benefit for dual simplex after accounting for overhead.

---

## 3. CPLEX / Bixby Crash Basis

**Reference:** Bixby (1992). *Implementing the Simplex Method: The Initial Basis*. INFORMS J. Computing, 4(3), 267–284.

### Algorithm

Constructs a preference ordering of variables, then builds a triangular basis:

1. Partition variables into preference sets:
   - **C₁:** Slack variables (lowest priority)
   - **C₂:** Free variables (highest priority — unbounded both sides)
   - **C₃:** Variables with exactly one finite bound
   - **C₄:** Boxed variables (both finite bounds)
2. Within C₂, C₃, C₄: sort by quality metric qⱼ = |cⱼ| × (uⱼ − lⱼ) / ‖A_{.,j}‖₁ (smaller = better)
3. Concatenate sets: C = C₂ | C₃ | C₄ | C₁ (freest variables first).
4. Walk C in order, pivoting each column into the basis if it maintains triangularity.
5. Fill remaining slots with logicals.

**Implementation Complexity:** ~200–400 lines. Simpler than Maros crash.

### Notes for Dual Simplex

- Same DSE weight issue as Maros crash
- Same density problem
- Bixby crash is the **default in CPLEX** for primal simplex but CPLEX uses specialized handling for dual

**Expected iteration reduction:** 25–45% (primal). Net benefit for dual is marginal unless DSE weights are efficiently handled.

---

## 4. Koberstein Dual Crash (Weighted Triangular with DSE Awareness)

**Reference:** Koberstein (2005). *The Dual Simplex Method, Techniques for a Fast and Stable Implementation*. PhD thesis, University of Paderborn, Fakultät für Wirtschaftswissenschaften. §7.1.

### Algorithm

A dual-aware crash procedure that extends the Maros triangular framework:

1. **Build near-triangular basis** identical to Maros triangular crash.
2. **Weight function** for tie-breaking:
   - Prefer variables with **wide primal feasibility range** (high uⱼ − lⱼ)
   - Prefer columns with **low nonzero count** (minimize density)
   - Prefer **free variables** into basis, **fixed variables** out
   - Estimate optimal-basis probability via |cⱼ| and geometric angle arguments
3. **Threshold pivoting** during construction (borrowed from LU factorization) to ensure numerical stability.

### DSE Weight Handling

Koberstein's critical contribution is documenting the DSE weight problem:

- **Slack basis:** γⱼ = 1.0 for all j → correct, trivial
- **Crash basis:** Must compute γⱼ = ‖eⱼᵀB⁻¹‖² for each basic column j
  - Exact: m BTRAN solves (expensive — O(m × nnz(L+U)))
  - Approximate: set γⱼ = 1.0 heuristically → disastrous results (more iterations, not fewer)
  - **Best compromise:** compute exact weights once, then update with DSE update formula during iterations

Koberstein found that even with exact initialization, the time saved by fewer iterations **did not compensate** for the extra per-iteration density and initialization cost in most test models.

**Expected iteration reduction:** 15–30% (dual simplex). Net runtime improvement: **0–5%** in most cases.

---

## 5. IDIOT Crash Algorithm (Quadratic Penalty / Augmented Lagrangian)

**References:**
- Galabova & Hall (2020). *The "Idiot" crash quadratic penalty algorithm for linear programming and its application to linearizations of quadratic assignment problems*. Optimization Methods and Software 35(3), 488–501. DOI 10.1080/10556788.2019.1604702.

### Algorithm

The IDIOT crash is NOT a basis-building heuristic — it is an **approximate LP solver** used as a warm-start:

1. Minimize the **IDIOT function**:
   \[
   h(x) = c^T x + \lambda^T(Ax - b) + \frac{1}{2\mu}(Ax - b)^T(Ax - b)
   \]
   - Early iterations: behaves like **augmented Lagrangian** (λ active)
   - Later iterations: transitions to **pure quadratic penalty** (λ term becomes negligible)
2. Perform component-wise approximate minimization (Gauss-Seidel style).
3. After convergence, run a **crossover** to extract a basic feasible solution.
4. Feed the resulting basis to primal simplex.

### Key Findings

- **Geometric mean speed-up: 1.9×** for Clp primal simplex over 30 test problems
- IDIOT time is only **~6.2% of total** on average
- Huge speed-ups on QAP linearizations (nug15, qap12, qap15): up to **10×**
- Can also find **near-optimal solutions** fast (approximate solve mode)
- **But:** Designed for **primal simplex**. The crossover step produces a primal feasible basis, not a dual feasible one.
- **Not suitable** for dual simplex without significant re-engineering

### Why Not For JAOS Dual Simplex

| Issue | Explanation |
|-------|-------------|
| Output is primal feasible | Dual simplex needs dual feasible basis |
| Crossover is expensive | Adds overhead comparable to Phase I |
| Tuned for Clp primal | Heuristics assume primal pricing rules |
| Parameter sensitivity | Per-problem tuning required (Clp has auto-detection) |

**Expected iteration reduction:** N/A for dual simplex. Would need a dual analogue (not described in literature).

---

## 6. Cosine Criterion (Angle-Based Crash)

**References:**
- Junior & Lins (2005). *A Cosine Criterion to Select a Starting Basis for the Simplex Method*.
- Hu (2007). *A New Starting Basis Method for the Simplex Method*.

### Algorithm

Selects structural columns that form small angles with the objective gradient:

1. Compute the **cosine** of the angle between each structural column A_{.,j} and the objective vector c:
   \[
   \cos\theta_j = \frac{|c^T A_{.,j}|}{\|c\|_2 \cdot \|A_{.,j}\|_2}
   \]
2. Sort columns by descending |cos θⱼ| (smaller angle = more aligned with objective).
3. Build basis by greedily selecting from the sorted list, maintaining nonsingularity via a triangular check.
4. Fill remaining slots with logicals.

### Dual Simplex Viability

- The cosine criterion is **direction-agnostic** so it can be applied to the dual objective
- For dual simplex: use **dual objective gradient** (the RHS b) instead of c
- No DSE weight complication beyond initial computation

**Implementation Complexity:** ~150–300 lines. Lightweight.

**Expected iteration reduction:** 20–40% (both primal and dual). Low overhead makes this more attractive for dual simplex than triangular crash.

---

## 7. Analysis: Why Crash Is Harder for Dual Simplex

The literature consistently shows crash bases are **less beneficial for dual simplex** than primal simplex. The root causes:

### 7.1 DSE Weight Initialization

| Basis | DSE Weight γⱼ | Cost |
|-------|---------------|------|
| Slack (I) | γⱼ = 1.0 (exact) | Zero |
| Any crash | γⱼ = ‖eⱼᵀB⁻¹‖² | m × BTRAN (O(m × nnz)) |

The slack basis gives exact DSE weights for free. Any crash basis requires computing them — and Koberstein showed that **heuristic initialization (γⱼ = 1.0) is catastrophically bad**, causing more iterations than slack basis.

### 7.2 Basis Density

- Slack basis: B = I → density = m/m² = **1/m** (extremely sparse)
- Crash basis: B ≈ Â[:, selected cols] → density ≈ **average column density of Â**
- Denser B → slower FTRAN/BTRAN in every iteration

### 7.3 Dual Feasibility May Be Lost

Primal crash methods produce a basis but don't guarantee dual feasibility (required for dual simplex). A dual Phase I is still needed, consuming many of the saved iterations.

---

## 8. Recommendation for JAOS

### Primary Recommendation: Koberstein Dual Crash (lightweight version)

For a solver already using dual steepest-edge pricing:

1. **Build a near-triangular basis** using Maros-style greedy selection (prefer free variables, wide bounds, sparse columns).
2. **Initialize DSE weights exactly** (one-time cost of m BTRAN solves — acceptable if basis is built once).
3. **Use DSE update formula** for subsequent iterations (existing mechanism).
4. **Accept 15–30% iteration reduction** but only ~5% net runtime gain.

### Alternative: Cosine Criterion (lower risk, simpler)

If implementation time is the constraint:

1. Compute cos θⱼ = |bᵀA_{.,ⱼ}| / (‖b‖₂ · ‖A_{.,ⱼ}‖₂) — dual version.
2. Build triangular basis from sorted list.
3. Same DSE handling as above.

**Lower potential gain (~20%) but simpler code (~150 lines).**

### What NOT to do

- ❌ **IDIOT crash** — primal-oriented, expensive, no dual variant in literature
- ❌ **Heuristic DSE weights (= 1.0)** — Koberstein showed this is worse than no crash
- ❌ **Skip crash entirely** — acceptable for M1 correctness, but M2 should at least gate on whether crash pays off

### Expected Impact

| Metric | Slack Basis | Light Crash | Optimistic |
|--------|:-----------:|:-----------:|:----------:|
| Iterations | 100% | 70–85% | 60% |
| Per-iter time | 1.0× | 1.1–1.3× | 1.05× |
| Total runtime | baseline | 0.85–1.05× | 0.70× |
| Code added | — | ~300 lines | ~500 lines |

The 50% iteration reduction target is achievable for **some problems** but not all. A pragmatic approach: implement the crash, but provide a runtime toggle so it can be disabled when it hurts.

---

## References

1. Bixby, R. E. (1992). Implementing the Simplex Method: The Initial Basis. *INFORMS Journal on Computing*, 4(3), 267–284.
2. Maros, I. & Mitra, G. (1998). Strategies for Creating Advanced Bases for Large-Scale Linear Programming Problems. *INFORMS Journal on Computing*, 10(2), 248–260.
3. Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Ch. 9.
4. Koberstein, A. (2005). *The Dual Simplex Method, Techniques for a Fast and Stable Implementation*. PhD Thesis, University of Paderborn, Fakultät für Wirtschaftswissenschaften. §7.1.
5. Galabova, I. L. & Hall, J. A. J. (2020). The "Idiot" crash quadratic penalty algorithm for linear programming and its application to linearizations of quadratic assignment problems. *Optimization Methods and Software*, 35(3), 488–501. DOI 10.1080/10556788.2019.1604702.
6. Huang, M. et al. (2021). Simplex Initialization: A Survey of Techniques and Trends. arXiv:2111.03376.
7. Junior, H. V. & Lins, M. P. E. (2005). A Cosine Criterion to Select a Starting Basis for the Simplex Method.
8. Forrest, J. J. H. & Goldfarb, D. (1992). Steepest-Edge Simplex Algorithms for Linear Programming. *Mathematical Programming*, 57(1), 341–374.