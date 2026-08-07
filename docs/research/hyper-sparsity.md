# Hyper-Sparsity for Dual Simplex (M2)

> Research summary for JAOS — exploiting sparse results of FTRAN/BTRAN in dual
> revised simplex with Forrest-Tomlin LU updates.

**Key references:**

- Hall & McKinnon (2005) — *Hyper-sparsity in the revised simplex method and how
  to exploit it*. Computational Optimization and Applications, 32, 259–283.
- Huangfu & Hall (2018) — *Parallelizing the dual revised simplex method*.
  Mathematical Programming Computation, 10, 119–142.
- Gilbert & Peierls (1988) — *Sparse partial pivoting in time proportional to
  arithmetic operations*. SIAM J. Sci. Stat. Comput., 9(5), 862–874.

---

## 1. What Hyper-Sparsity Is

Hyper-sparsity is a property of the **output** of linear-system solves in the
revised simplex method, not of the coefficient matrix itself.

An LP problem exhibits hyper-sparsity when, for a clear majority (≥60%) of
iterations, the **result** of at least one of the three core operations is sparse
(≤10% density):

| Operation | What it computes | Typical result |
|-----------|-----------------|----------------|
| **FTRAN** | `â_q = B⁻¹ a_q` (pivotal column) | sparse in hyper-sparse problems |
| **BTRAN** | `τᵀ = e_pᵀ B⁻¹` (unit vector / DSE weight row) | **most common** hyper-sparse operation |
| **PRICE** | `â_pᵀ = τᵀ N` (pivotal row) | sparse when τ is sparse |

In conventional dense LP solving, even though the RHS (`a_q` or `e_p`) is
sparse, the solution vector is assumed to fill in densely because B⁻¹ is
structurally dense.  Hyper-sparsity exploits the observation that for many
real-world LP problems this **does not happen** — the solution stays sparse.

### Prevalence

In the Hall & McKinnon test set of 54 standard Netlib/Kennington problems:

- **26 problems** exhibited hyper-sparsity in at least one operation (set H).
- **28 problems** did not (set H̄).
- All problems that exhibited hyper-sparsity did so for **BTRAN**; most did so
  for all three operations.
- Network-structured problems (NETGEN, PDS, KEN) are **extremely** hyper-sparse
  (often 90–100% of FTRAN/BTRAN/PRICE results are sparse).

---

## 2. Why It Matters: CPU Profile Without Hyper-Sparsity

For hyper-sparse problems (set H), the % of total solution time spent in each
component **without** hyper-sparsity exploitation is:

| Component | % of total time |
|-----------|:---------------:|
| CHUZC     |      9.3 % |
| I-FTRAN   |      6.0 % |
| U-FTRAN   |      1.5 % |
| CHUZR     |      2.8 % |
| I-BTRAN   |     12.1 % |
| U-BTRAN   |      2.5 % |
| **PRICE** | **49.9 %** |
| INVERT    |      2.1 % |

PRICE dominates (~50 %) because the dense τ vector makes every column of N
participate.  When τ is sparse, the profile shifts dramatically (see §5).

---

## 3. How Hyper-Sparsity Is Implemented

### 3.1 Core Principle: Symbolic Reach Before Numeric Solve

The key idea — formalised by Gilbert & Peierls (1988) and adapted by Hall &
McKinnon — is to determine **which L/U etas must be applied** *before* doing
the floating-point arithmetic, using a **dependency-graph reach** (DFS over the
triangular factor's adjacency).

For a triangular solve `L x = b` where b is sparse:

1. **Symbolic phase:** Starting from the nonzeros in `b`, traverse the
   elimination graph of L to find the *reachable set* — the set of columns whose
   solution values *may* become nonzero. This costs O(|reachable set|).
2. **Numeric phase:** Solve only for the reachable positions in topological
   order.  All other positions are structurally zero and skipped entirely.

This makes the cost `O(|result| + flops)` instead of `O(m + nnz(L))`.

### 3.2 Hyper-Sparse FTRAN (I-FTRAN: solves with L / LU factors)

The standard FTRAN algorithm (Figure 2a in Hall & McKinnon) scans every eta k
from 1…r, checks `b[p_k] == 0`, and skips if so — but the *test* for zero is
the dominant cost when only a handful of etas actually modify the RHS.

**Hyper-sparse FTRAN algorithm** (Figure 3, Hall & McKinnon):

```
1. R = {i : b[i] ≠ 0}         // indices of nonzeros in RHS
2. K = {k : p_k ∈ R}          // etas whose pivot row has a nonzero
3. repeat:
4.     k₀ = min(K)            // earliest eta that may act
5.     b[p_{k₀}] /= π_{k₀}    // apply pivot
6.     for i ∈ nonzero-positions(η_{k₀}):  // fill-in from this eta
7.         if b[i] ≠ 0:  b[i] += b[p_{k₀}] * η_{k₀}[i]
8.         else:         b[i]  = b[p_{k₀}] * η_{k₀}[i]
9.                        add K entries from new nonzeros
10. until K = ∅
```

**Data structures needed:**

- **`P₁[i], P₂[i]`** — for each row i, the index of the first and second eta
  whose pivot is in that row (0 = none).  Since each row is pivoted at most once
  per L factor rank and at most once per U factor rank, this gives ≤ 2 etas per
  row.
- **`K`** — an unordered list (or heap/bucket set) of candidate eta indices,
  dynamically maintained.
- **`R`** — optional list of nonzero positions in RHS (reused by CHUZR).

**Fallback heuristic:** When `|K|` grows large, the cost of searching K exceeds
the cost of the standard scan.  EMSOL compares the average skip through the eta
file against a multiple of `|K|` and reverts to the standard algorithm.

### 3.3 Hyper-Sparse BTRAN (I-BTRAN: solves with Uᵀ / Lᵀ)

BTRAN is harder because the inner product `bᵀ η_k` must be evaluated (no simple
zero-skip test).  Two techniques are used:

**Technique 1 — Row-wise INVERT eta file.**  After INVERT, build an equivalent
row-oriented representation of the eta file.  Then I-BTRAN can use the **same**
forward-scatter algorithm as hyper-sparse FTRAN (above), processing the row-wise
etas in reverse order.  The overhead of building the row-wise representation is
far outweighed by the savings for hyper-sparse problems.

**Technique 2 — Skip-ahead via last-occurrence lists.**  Maintain `Q₁[i]` /
`Q₂[i]` — the index of the last/penultimate INVERT eta with a nonzero in row i.
Given the sparse `τ̃` (from U-BTRAN), find the maximum `Q₁` over rows with
nonzeros — that's the first eta that must be applied.  If all `Q₁` entries are
zero, `τ = τ̃` immediately.

### 3.4 Hyper-Sparse U-BTRAN (Update Eta BTRAN)

When the product-form update is used, the UPDATE etas are applied as:

- Let `P` = set of rows that have been pivotal since INVERT.  The BTRAN result
  `τ̃` is structurally **restricted to P** — fill-in during BTRAN can only occur
  in the pivot row of each eta applied.
- Maintain a dense-but-small rectangular array `Ê` of dimension `|P| × K`
  holding the submatrix of UPDATE etas restricted to rows in P.  Then U-BTRAN
  becomes `K` short dense inner products of length `|P|`.
- If the update etas themselves are sparse, use a search technique: for each
  nonzero in the current RHS, find the *earliest* (from end of file) eta with a
  nonzero in that row.

### 3.5 Hyper-Sparse PRICE

When `τ` is sparse, instead of forming `τᵀ N` as inner products between τ and
every column of N:

- Represent N **row-wise** (in addition to the normal column-wise CSC).
- Form `â_pᵀ = τᵀ N` as a **linear combination of rows of N** weighted by
  nonzero entries in τ.  Only rows of N corresponding to nonzeros in τ are
  touched — all trivial operations are eliminated.
- Maintain a list of nonzero positions in τ (cheaply recorded during BTRAN).
  When τ is hyper-sparse, this list eliminates the need to search for nonzeros,
  and the workspace vector can remain zeroed without explicit re-zeroing.

### 3.6 Hyper-Sparse CHUZC

Maintain a short list `C₀` of the best `s` candidate entering variables (by
weighted reduced cost).  After each iteration, only reduced costs corresponding
to nonzeros in the sparse pivotal row have changed — form `D₀` from those and
merge with `C₀` to get `C₁`.  Periodically reset with a full scan to prevent
sub-optimal candidates from being permanently missed.  A lower-bound mechanism
triggers reset when it detects a possibly better candidate outside the list.

### 3.7 Data Structure Summary

| Structure | Purpose | Dimension |
|-----------|---------|-----------|
| `P₁[i], P₂[i]` | First/second eta with pivot in row i | m × 2 ints |
| `K` (list/heap) | Candidate eta indices for hyper-sparse FTRAN | dynamic |
| `R` (list) | Nonzero positions in RHS (reused by CHUZR) | dynamic |
| Row-wise etas | Row-oriented copy of INVERT eta file for I-BTRAN | same as column-wise |
| `Q₁[i], Q₂[i]` | Last/penultimate INVERT eta with nonzero in row i | m × 2 ints |
| `Ê` (rectangular) | UPDATE eta submatrix for rows in P | `|P| × K` |
| Row-wise N | Row-wise representation of constraint matrix | `nnz(A)` + row offsets |
| `C₀, C₁, D₀` | Candidate lists for hyper-sparse CHUZC | `s` entries each |

---

## 4. Expected Speedup

### 4.1 For hyper-sparse problems (set H)

| Component | Geometric mean speedup |
|-----------|:---------------------:|
| I-FTRAN   | 14.9 × |
| I-BTRAN   | 17.5 × |
| U-BTRAN   |  4.6 × |
| PRICE     | 19.5 × |
| CHUZC     | 12.2 × |
| CHUZR     |  4.6 × |
| INVERT    |  1.8 × |
| **Total** | **5.6 ×** |

Individual problems show much larger gains: DETEQ27 achieves **59×** on PRICE,
KEN-11 achieves **64×** on U-BTRAN, KEN-07 achieves **28×** on U-BTRAN.

### 4.2 For non-hyper-sparse problems (set H̄)

Even here, average total speedup is **1.34×**, mostly from U-BTRAN (20.8×
average), INVERT (1.4×), and PRICE (1.5×) gains.

### 4.3 Overhead

The data structures to support hyper-sparsity add about **8.8%** overhead to
total solution time for hyper-sparse problems (ranging from 0.5% to 18.8%).

---

## 5. Applicability to Forrest-Tomlin Updates

This is the most nuanced question and directly relevant to JAOS (which uses
sparse LU with FT updates).

### 5.1 The Hall & McKinnon (2005) Position

Hall & McKinnon state **explicitly** that the Forrest-Tomlin (and Bartels-Golub)
updates make hyper-sparsity harder to exploit:

> *"If such a procedure were used, the data structures which enable
> hyper-sparsity to be exploited during BTRAN and FTRAN would have to be
> modified after each UPDATE to correspond to the changes in the representation
> of B₀⁻¹. The overhead of doing this may severely limit the value of exploiting
> hyper-sparsity."* (§4.9)

Their EMSOL solver used the **product form (PF) update** because:
- PF leaves the INVERT representation (LU factors) **static** between
  refactorizations — the data structures (`P₁`, `P₂`, `Q₁`, `Q₂`, row-wise etas)
  remain valid after a single INVERT and do not need per-iteration updates.
- PF UPDATE etas are structurally trivial (single column appended per iteration).
- For hyper-sparse problems, the PF update's fill-in is actually milder than
  dense, so its sparsity disadvantage vs FT disappears for these problems.

### 5.2 The Huangfu & Hall (2018) Position — Practical Resolution

Huangfu & Hall's `hsol` solver (basis of HiGHS) **does** combine FT updates
with hyper-sparse FTRAN/BTRAN.  The resolution is:

1. **Hyper-sparsity is exploited primarily in the INVERT (LU) solves** —
   I-FTRAN and I-BTRAN.  The LU factor data structures (L and U in compressed
   column / compressed row form) are **static** between refactorizations,
   exactly as with PF.  The Gilbert-Peierls reach algorithm applies cleanly to
   both L and U triangular factors.

2. **The update part** (U-FTRAN / U-BTRAN) with FT is cheaper per iteration
   than with PF (FT updates modify U in-place rather than appending etas), so
   hyper-sparsity in the update path is less critical.  When hyper-sparsity is
   present, the update path is short anyway.

3. **HiGHS HFactor** uses exactly this approach:
   - LU refactorization with Gilbert-Peierls reach-based hyper-sparse solves.
   - Collective FT updates between refactorizations.
   - Density monitoring: when the reach set exceeds ~m/4 (empirical threshold),
     fall back to standard dense sweep — keeps dense-RHS performance unchanged.

### 5.3 Summary for JAOS

| Aspect | Applies to FT? | Notes |
|--------|:--------------:|-------|
| **I-FTRAN (L solve)** | ✅ Yes | Gilbert-Peierls reach on L; LU factors are static |
| **I-BTRAN (Uᵀ solve)** | ✅ Yes | Row-wise U or transpose reach; factors static |
| **U-FTRAN (FT update)** | ⚠️ Partial | FT modifies U in-place; shorter than PF update path, so less need |
| **U-BTRAN (FT update)** | ⚠️ Partial | Same — FT updates are collective and cheaper than PF |
| **PRICE hyper-sparse** | ✅ Yes | Independent of update method; row-wise N always works |
| **CHUZC hyper-sparse** | ✅ Yes | Independent of update method |
| **Row-wise eta file** | ❌ Not needed | With LU factors, use reach-based triangular solves instead |
| **`P₁/P₂` / `Q₁/Q₂` lists** | ✅ Yes | Applicable to LU factor column counts (which etas affect which rows) |

**Recommendation for JAOS M2:** Implement hyper-sparse FTRAN/BTRAN using the
Gilbert-Peierls reach (DFS over L/U dependency graphs), modelled after the
HiGHS HFactor approach.  This works directly with LU factorization and FT
updates because the LU factors themselves are static between refactorizations.
The PRICE operation should use row-wise N and the sparse τ vector.  Add density
monitoring to fall back to dense solves when the solution fills in.

---

## 6. Gilbert-Peierls Reach Algorithm (For LU Factors)

The Gilbert-Peierls algorithm provides the **symbolic reach** that underpins
modern hyper-sparse triangular solves:

```pseudo
function triangular_reach(L, b):
    // L is unit lower triangular, b is sparse RHS
    // Returns reachable set S — structural nonzeros of solution x

    visited = boolean array(m, false)
    S = empty stack/queue
    
    function dfs(j):
        if visited[j]: return
        visited[j] = true
        for i > j where L[i][j] ≠ 0:    // nonzeros in column j of L
            if not visited[i]:
                dfs(i)
        push j onto S
    
    for j in nonzeros(b):
        dfs(j)
    
    return S  // in topological order (DFS postorder → reverse)
```

After the reach is computed, the numeric solve processes only the positions in
`S` in topological order (`for j in S: x[j] = b[j]; then for i in
off-diagonals of column j of L: b[i] -= L[i][j] * x[j]`).  The cost is
`O(|S| + flops-in-S)`, which for hyper-sparse problems is dramatically less than
`O(m + nnz(L))`.

For the U factor (upper triangular, column-oriented), the same approach works:
the dependency direction is reversed (columns depend on rows below), and the
DFS starts from nonzeros in the RHS and follows U's column adjacency downward.

---

## 7. Implementation Plan for JAOS

### Phase 1: Reach-Based Triangular Solves (I-FTRAN / I-BTRAN)

1. Add a `reach` function that, given a sparse RHS and a triangular factor (L or
   U), computes the reachable set using DFS.
2. Add a `solve_from_reach` function that performs the numeric solve only on the
   reachable positions.
3. Gate both behind an `is_hyper_sparse` flag computed from a running density
   average.

### Phase 2: Hyper-Sparse PRICE

1. Build/maintain a **row-wise** representation of the constraint matrix A
   (alongside the existing CSC).
2. In the PRICE operation, iterate over nonzeros in τ (from BTRAN) and
   accumulate rows of the row-wise representation.
3. Density-switch back to standard column-inner-product PRICE when τ density
   exceeds threshold.

### Phase 3: Hyper-Sparse CHUZC

1. Maintain a priority queue or bounded list of the best `s` entering candidates.
2. After each iteration, update reduced costs only for columns that have nonzeros
   in the sparse pivotal row.
3. Periodically (based on lower-bound check) refresh with full scan.

### Phase 4: Integration with FT Updates

1. The reach data structures are **built during INVERT** and remain valid until
   the next refactorization.
2. FT updates modify U in-place — the reach structures for U need updating
   (column adjacency changes).  This can be handled incrementally when a spike
   column is merged into U, but the simplest approach for M2 is to only use
   hyper-sparse solves on the LU factors directly (the INVERT solves) and use
   standard techniques for the U-FTRAN/U-BTRAN through FT updates (which are
   already cheaper than PF update path).

### Threshold Tuning

- Trigger hyper-sparse path when running density < 10% (Hall & McKinnon
  criterion).
- Fall back to dense path when reach set exceeds 25% of m (HiGHS heuristic).
- Re-evaluate density every 100 iterations, or after each INVERT.

---

## 8. References

1. **Hall, J.A.J. & McKinnon, K.I.M.** (2005). Hyper-sparsity in the revised
   simplex method and how to exploit it. *Computational Optimization and
   Applications*, 32, 259–283.
   [PDF](https://webhomes.maths.ed.ac.uk/hall/MS-00/MS0015.pdf)

2. **Huangfu, Q. & Hall, J.A.J.** (2018). Parallelizing the dual revised simplex
   method. *Mathematical Programming Computation*, 10, 119–142.
   [arXiv:1503.01889](https://arxiv.org/pdf/1503.01889)

3. **Gilbert, J.R. & Peierls, T.** (1988). Sparse partial pivoting in time
   proportional to arithmetic operations. *SIAM J. Sci. Stat. Comput.*, 9(5),
   862–874.

4. **Forrest, J.J.H. & Tomlin, J.A.** (1972). Updated triangular factors of the
   basis to maintain sparsity in the product form simplex method. *Mathematical
   Programming*, 2, 263–278.