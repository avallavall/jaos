# M2 — LP Fast: Implementation Plan

> Based on research at `docs/research/hyper-sparsity.md` and `docs/research/crash-basis.md`.
> Target: 5.6× geometric-mean speedup on hyper-sparse problems via Gilbert-Peierls
> reach-based triangular solves, hyper-sparse PRICE/CHUZC, and a Koberstein dual crash basis.
> M2 also addresses the deferred data-structure items from PLAN.md §2.11.

---

## 1. Implementation Order

The components are ordered so that each one can be independently tested and benchmarked
against the current M1 baseline. Later components depend on the density-monitoring
infrastructure built for earlier ones.

| Order | Component | Why this order |
|:-----:|-----------|----------------|
| 1 | **Density monitoring & hyper-sparse gate** | Shared infrastructure for all components; no changes to solves yet |
| 2 | **Reach-based I-FTRAN (L solve)** | Biggest single win (14.9×); independent of other changes |
| 3 | **Reach-based I-BTRAN (Uᵀ solve)** | Similar pattern to I-FTRAN; 17.5× potential |
| 4 | **Hyper-sparse PRICE** | Requires sparse τ from BTRAN; 19.5× on hyper-sparse problems |
| 5 | **Hyper-sparse CHUZC** | Leverages sparse pivotal row from PRICE; 12.2× |
| 6 | **Position map for `jm_svec_erase`** | Defers to after solves are benchmarked to measure impact (D17) |
| 7 | **Koberstein dual crash basis** | Independent of hyper-sparsity; 15–30% iteration reduction |
| 8 | **Integration & density threshold tuning** | Cross-component tuning; benchmark suite |

---

## 2. Component Details

### 2.1 Density Monitoring & Hyper-Sparsity Gate

**Estimated LOC:** ~80

**Files to modify:**
- `src/jaos_internal.h` — add fields to `jm_lu` and declare `jm_lu_density_reached`
- `src/lu.c` — add running density tracking in `jm_lu_ftran` / `jm_lu_btran`

**Key code additions:**

```c
// In jm_lu struct (jaos_internal.h):
struct {
    int64_t  calls;           // total FTRAN/BTRAN calls since last INVERT
    int64_t  dense_calls;     // calls where result density exceeded threshold
    double   running_density;  // exponential moving average
    bool     hyper_sparse;    // currently using hyper-sparse path
    double   density_threshold; // default 0.10 (10%)
} ftran_stats, btran_stats;
```

**Functions:**
- `jm_lu_update_density(jm_lu *lu, bool is_ftran, int64_t nnz_result)` — update EMA
- `jm_lu_should_use_hyper(jm_lu *lu, bool is_ftran)` — returns true when running density < threshold
- Called at the end of each `jm_lu_ftran` / `jm_lu_btran` after computing result density

**Reset:** Reset stats when `jm_lu_factor` runs (after each refactorization).

**Verification:** Check that `density_threshold` controls the gate; verify with a known
dense-RHS problem that the dense path is always taken.

---

### 2.2 Reach-Based I-FTRAN (L Solve)

**Estimated LOC:** ~180

**Files to modify:**
- `src/lu.c` — new functions
- `src/jaos_internal.h` — new function declarations

**Key code locations:**

**New functions in `lu.c`:**

```c
// Gilbert-Peierls DFS to compute reachable set of L
// L is unit lower triangular, stored by columns.
// b_sparse is an array of (index, value) pairs of nonzero RHS entries.
// Returns the reachable set S in topological (reverse postorder) order.
static int64_t reach_l(
    const jm_lu *lu,
    const int64_t *b_idx, int64_t b_n,
    int64_t *S /* out: reachable slots */,
    bool *visited /* scratch, [dim] */
);

// Numeric solve for reachable positions only.
// S[0..nS-1] is the reachable set in topological order from reach_l.
static void solve_from_reach_l(
    const jm_lu *lu,
    const int64_t *S, int64_t nS,
    double *y /* in/out: dense working vector */
);
```

**`reach_l` algorithm** (from hyper-sparsity.md §6):
1. Initialize `visited[:] = false`, `S` empty.
2. For each nonzero index `j` in the permuted RHS (`b`):
   - If not visited, call `dfs(j)`.
3. `dfs(j)`:
   - Mark `visited[j] = true`.
   - For each `i` in L's column `j` (off-diagonals), where `i > j`:
     - If not visited, `dfs(i)`.
   - Push `j` onto `S` (postorder → topological order when reversed).
4. Return `S` (natural order is reverse topological).

**`solve_from_reach_l` algorithm**:
1. For `k = 0..nS-1` (forward through S, which is reverse topological):
   - `j = S[k]` (slot index in L order).
   - `ys = y[j]`.
   - If `ys == 0.0`, continue.
   - For each `p` in `L[:, j]` off-diagonals: `y[L_index[p]] -= L_value[p] * ys`.
2. (FT etas are applied after L in the existing `ftran_prefix` — they remain unchanged.)

**Modify `jm_lu_ftran` (lu.c:681):**
- After permuting RHS into slot order, check `lu->ftran_stats.hyper_sparse`.
- If hyper-sparse:
  - Build sparse index list of nonzeros in the permuted RHS.
  - Call `reach_l` to get reachable set.
  - Call `solve_from_reach_l` for L solve.
  - Apply FT etas (unchanged).
  - For U solve: use reach-based U solve (see §2.3).
- If dense: use existing path (unchanged).
- After solve, call `jm_lu_update_density` with result density.

**Data structures needed:**
- `bool *visited` — scratch array, reuse `lu->tmp` or add a dedicated boolean array.
- `int64_t *S` — reachable set stack, size `dim`. Add to `jm_lu` scratch or use a local
  allocation (growable).

**Fallback:** When `|S| > dim / 4` (HiGHS heuristic), skip the sparse path entirely.

---

### 2.3 Reach-Based I-BTRAN (Uᵀ Solve)

**Estimated LOC:** ~160

**Files to modify:**
- `src/lu.c` — new functions
- `src/jaos_internal.h` — declarations

**Key code locations:**

**New functions in `lu.c`:**

```c
// Gilbert-Peierls DFS for U^T (upper triangular, column-oriented).
// U^T solve is `x = U^{-T} b`, which is forward substitution through U by columns.
// The dependency graph follows U's column adjacency: a column j depends on columns
// i > j that have nonzeros in row j of U.
static int64_t reach_ut(
    const jm_lu *lu,
    const int64_t *b_idx, int64_t b_n,
    int64_t *S /* out */,
    bool *visited /* scratch */
);

// Numeric solve for reachable positions in U^T.
static void solve_from_reach_ut(
    const jm_lu *lu,
    const int64_t *S, int64_t nS,
    double *y /* in/out */
);
```

**`reach_ut` algorithm:**
U^T is lower triangular. The U matrix is stored by column in `lu->ucol`. For U^T solve,
we process column by column in forward slot order. The dependency graph:
- Column `j` (slot `s`) has nonzeros in rows `> s` (stored in `lu->ucol[s]`).
- These nonzeros mean that unknowns at those positions depend on the unknown at `s`.
- So the DFS starts from nonzeros in the RHS and follows each column's adjacency
  upward (to higher-indexed slots).

**Modify `jm_lu_btran` (lu.c:709):**
The BTRAN code currently does:
1. Permute: `y[s] = x[perm_col[s]]` (dense).
2. U^T solve: forward slot order, dense dot product.
3. FT etas: reverse order.
4. L^T solve: backward slot order, dense dot product.
5. Permute back: `x[perm_row[s]] = y[s]`.

With hyper-sparsity:
- After step 1, build sparse index list of nonzeros in `y`.
- Call `reach_ut` to get reachable set for U^T.
- Call `solve_from_reach_ut` for U^T solve.
- For L^T solve (step 4), use analogous reach on L^T.
- FT etas (step 3) are already sparse-friendly (just iterate over `lu->ft.n`).

**L^T reach:** L^T is upper triangular. The reach works backward: start from nonzeros
in the RHS and follow L's row adjacency (equivalent to L^T's column adjacency)
downward.

---

### 2.4 Hyper-Sparse PRICE

**Estimated LOC:** ~120

**Files to modify:**
- `src/simplex.c` — `price_entry`, `price_and_select`, `sx` struct
- `src/jaos_internal.h` — optional new fields or declarations

**Key code locations:**

**Modify `price_and_select` (simplex.c:818):**

Currently, after BTRAN fills `s->rho` (the pricing row), the PRICE loop iterates over
every nonbasic variable and calls `price_entry` — which for each structural column
scans all its nonzeros to compute `rho' * A[:,j]`.

With hyper-sparsity:
- After BTRAN, `s->rho` is sparse (most entries are zero).
- Build a list `nonzeros_in_rho` — indices where `|rho[i]| > 0`.
- Compute `alpha[v] = rho' * A[:,v]` as a **linear combination of rows of N**:
  ```c
  // For each nonzero in rho, scatter its row of A into alpha
  memset(alpha, 0, nvar * sizeof(double));
  for (int64_t k = 0; k < nnz_rho; k++) {
      int64_t i = rho_nonzero_idx[k];
      double ri = rho[i];
      // Row i of A is in CSR format (ar_start, ar_index, ar_value)
      for (int64_t p = m->ar_start[i]; p < m->ar_start[i+1]; p++)
          alpha[m->ar_index[p]] += ri * m->ar_value[p];
  }
  // Logical variables: alpha[ncol + i] = -rho[i] (already handled)
  ```

**Precondition:** `jm_model_ensure_rowwise(m)` must be called before this can run.
The CSR mirror already exists in `jaos_model` (`ar_start`, `ar_index`, `ar_value`).

**Density switch:** Compute `nnz_rho` after BTRAN. If `nnz_rho > nrow * 0.25` (or
some threshold), fall back to the standard column-by-column inner product loop.

**Work unit accounting:** The row-wise PRICE charges `nnz_rho * avg_row_nnz` work units
instead of the current `nvar * avg_col_nnz`. The work counter needs updating.

**Modify `sx` struct (simplex.c:115):**
- Add `int64_t *rho_nonzero_idx;` — scratch array for nonzero positions in rho.
- Add `int64_t rho_nonzero_cap;` — capacity.
- Add `double *alpha;` — already exists.

---

### 2.5 Hyper-Sparse CHUZC

**Estimated LOC:** ~100

**Files to modify:**
- `src/simplex.c` — `dual_ratio_test`, `run`, `sx` struct

**Key code locations:**

**New data in `sx` struct:**
```c
// Hyper-sparse CHUZC candidate list
int64_t *cand_list;    // [s] best entering candidate indices
double  *cand_weight;  // [s] weighted reduced costs
int64_t  cand_s;       // number of candidates tracked (s ~ 50–100)
int64_t  cand_iters;   // iterations since last full scan
int64_t  cand_reset_interval; // reset every N iterations (e.g., 500)
```

**Algorithm:**
1. After each iteration, the pivotal row `alpha` is sparse (only a few nonzeros in
   nonbasic columns). Only reduced costs corresponding to those columns changed.
2. Form `D₀` = the set of nonbasic columns with nonzeros in `alpha` whose reduced cost
   changed this iteration.
3. Merge `D₀` with the existing `C₀` (best `s` candidates) to produce `C₁`.
4. The CHUZR selection (which row leaves) comes from `price_row` (unchanged).
5. The ratio test (`dual_ratio_test`) iterates over the full nonbasic set — this is
   the simplest approach for M2. For a more aggressive optimization, the ratio test
   could also be restricted to the candidate list, but that risks missing the true
   blocking variable. Keep the full scan for correctness.

**Actually, the simpler and more impactful approach for M2:**
Since `dual_ratio_test` iterates over all `nvar` variables to build the candidate set,
and the main goal is to speed up the full iteration, the hyper-sparse CHUZC optimization
should focus on **reducing the cost of the `alpha` pricing loop** (already handled in
§2.4) and **speeding up the reduced-cost update** in `pivot()`.

**In `pivot()` (simplex.c:886):**
The loop `for (int64_t v = 0; v < s->nvar; v++)` updates every nonbasic reduced cost.
With hyper-sparse `alpha`:
- `alpha[v]` is zero for most `v`.
- The update `d[v] -= theta_dual * alpha[v]` is a no-op for those `v`.
- Use the sparse index list of nonzeros in `alpha` (reuse from PRICE) to only touch
  the affected variables.

**Net impact:** The `d[v]` update loop is O(nvar) but does only arithmetic (no memory
traffic), so this is a smaller gain than the other components. The research doc reports
12.2× speedup for CHUZC, but much of that comes from the candidate-list approach in
EMSOL's product-form context. For JAOS with FT updates, the biggest CHUZC-related gain
is from the sparse alpha in the reduced-cost update.

---

### 2.6 Position Map for `jm_svec_erase`

**Estimated LOC:** ~60

**Files to modify:**
- `src/lu.c` — `jm_svec_erase`, `jm_lu_update`
- `src/jaos_internal.h` — `jm_svec` struct

**Current problem (PLAN.md §2.11):**
`jm_svec_erase` does a linear scan through the vector to find index `i`, then swaps
it with the last entry. During a basis update, each slot detachment calls erase once
per entry of the outgoing slot's row and column — O(f²) where `f` grows with each
update.

**Fix:**
Add a position map to `jm_svec`:

```c
typedef struct {
    int64_t *idx;
    double  *val;
    int64_t  n, cap;
    int64_t *pos;   // [??] position of each index in the vector, or -1
    // Only allocated for ucol and urow, which are the hot paths in updates.
    // For small vectors (FT etas, etc.), pos stays NULL and erasing falls back
    // to the linear scan.
} jm_svec;
```

**Trade-off:** The position map adds memory (one `int64_t` per possible index, i.e.,
`dim` per `jm_svec`). For `urow` and `ucol` (each `dim` vectors), this is
`2 × dim × dim × sizeof(int64_t)` = 2 × dim² × 8 bytes. For `dim = 10000`, that's
1.6 GB — unacceptable.

**Alternative approach — only for `ucol` and `urow` of the *active* slot:**
The update only needs fast erase for the outgoing slot's column (in `ucol`) and row
(in `urow`). These are specific vectors. Instead of a full position map, use a
**stamp-based dedup** approach:
- Add a `stamp` array of size `dim` to `jm_lu` (reuse `lu->tmp` as a boolean array,
  or add a dedicated `int64_t *erase_mark`).
- In `jm_lu_update`, before erasing, stamp the indices of the outgoing row/column.
- `jm_svec_erase` becomes O(1) by checking the stamp first.

**Simpler fix for M2:** Given the complexity, delay this to a follow-up unless
benchmarking shows it's a bottleneck. The PLAN.md says "deferred by measurement",
so measure first.

---

### 2.7 Koberstein Dual Crash Basis

**Estimated LOC:** ~350

**Files to modify:**
- `src/simplex.c` — new `build_crash_basis`, modify `jm_dual_simplex`
- `src/jaos_internal.h` — optional new enum

**New function:**

```c
// Builds a near-triangular crash basis using Koberstein's dual-aware variant.
// Replaces the slack basis (B = I) with a basis containing structural variables.
// Returns JAOS_OK even if fewer than m structural columns were selected
// (remaining slots are filled with logicals).
static jaos_status build_crash_basis(sx *s);
```

**Algorithm** (from crash-basis.md §4, Koberstein Dual Crash):
1. **Build row/column counts** from the constraint matrix A (structural columns only):
   - `R[i]` = nnz in row i of A
   - `C[j]` = nnz in column j of A
2. **Greedy triangular selection** (Maros-style):
   - Loop until m columns selected or no candidates:
     - Find row `i` with smallest `R[i]` among remaining rows.
     - If `R[i] == 1`: column is uniquely determined.
     - If `R[i] > 1`: pick column `j` with smallest `C[j]` (min fill-in).
     - Tie-breaking: prefer free variables (wide bounds), low column density,
       large |cⱼ| × (uⱼ − lⱼ) / ‖A[:,j]‖₁.
     - Pivot column `j` into basis at position `i`.
     - Remove row `i` and column `j`; update `R` and `C`.
3. **Fill remaining slots** with logical (slack) columns.
4. **Initialize DSE weights exactly:**
   - Compute `γⱼ = ‖eⱼᵀB⁻¹‖²` for each basic column `j`:
     - For each basis position `i`, set `e[i] = 1.0`, BTRAN, compute squared norm.
     - This is `m` BTRAN solves — expensive but one-time.
   - Store in `s->dse[i]`.
5. **Compute initial primal and dual** via `refresh` (already exists).

**Modify `jm_dual_simplex` (simplex.c:1389):**
- After `sx_init`, if crash basis is enabled (toggle), call `build_crash_basis`
  instead of `build_initial_basis`.
- If crash basis fails or is disabled, fall back to `build_initial_basis`.

**Toggle:** Add a `bool use_crash` flag to `sx` struct (or a simple config mechanism).

**DSE weight initialization** (critical per Koberstein):
```c
// Exact DSE weight initialization: m BTRAN solves
for (int64_t i = 0; i < s->nrow; i++) {
    // Set e_i
    memset(s->rho, 0, nrow * sizeof(double));
    s->rho[i] = 1.0;
    jm_lu_btran(&s->lu, s->rho, &s->work);
    // Compute ||e_i^T B^-1||^2
    double norm2 = 0.0;
    for (int64_t k = 0; k < nrow; k++)
        norm2 += s->rho[k] * s->rho[k];
    s->dse[i] = norm2 > DSE_MIN ? norm2 : DSE_MIN;
}
```

**Verification:** Compare iteration count and total runtime with/without crash on
Netlib problems. Expect 15–30% iteration reduction but only ~5% net runtime gain.

---

## 3. Verification Strategy

### 3.1 Per-Component Unit Tests

| Component | Test | Location |
|-----------|------|----------|
| Density gate | Verify switch triggers at 10% threshold | `tests/test_lu.c` |
| Reach I-FTRAN | Solve known sparse system, compare result with dense FTRAN | `tests/test_lu.c` |
| Reach I-BTRAN | Same for BTRAN | `tests/test_lu.c` |
| Hyper-sparse PRICE | Compare row-wise vs column-wise pricing on same data | `tests/test_simplex.c` |
| Crash basis | Verify basis is nonsingular, DSE weights are positive | `tests/test_simplex.c` |

### 3.2 Correctness Verification

Every test must pass the **determinism harness** (D8): solve twice in-process, check
bit-identical results. Hyper-sparsity changes the work counter, so work units will
differ from the M1 baseline — that is expected and correct.

1. **`make test`** — all existing unit tests must pass (no regressions).
2. **`make sanitize`** — ASan/UBSan on all tests.
3. **Netlib gate** — all 94 instances must solve to optimal, checker green.
4. **Determinism** — each instance solved twice, results identical.

### 3.3 Benchmarking: Measuring Speedup

**Measurement host** — set up per Q4 (PLAN.md §3). The measurement must be on a
dedicated machine with no background load, running release builds.

**Benchmark harness** (`bench/`):
- Extend the existing Netlib runner to report wall-clock time per instance.
- Two modes per component:
  - **Baseline:** M1 code (no hyper-sparsity, no crash).
  - **With optimization:** component enabled.
- For each instance, record:
  - Iterations, work units, wall-clock time, FTRAN/BTRAN calls, FTRAN/BTRAN
    result densities.

**Key metrics:**

| Metric | What it measures | Target |
|--------|-----------------|--------|
| Iterations | Solve quality | Same or fewer (crash: 15–30% fewer) |
| Work units | Deterministic cost | Lower (5.6× geometric mean) |
| Wall time | Real speedup | Lower (matching work-unit reduction) |
| Hyper-sparse fraction | % of FTRAN/BTRAN calls with sparse result | ≥60% on hyper-sparse problems |
| Density distribution | Histogram of result densities | Bimodal: mostly <10% or >90% |

**Protocol:**
1. Run the full Netlib set with `-O2 -DNDEBUG` (release build), 5 repetitions per
   instance, report median time.
2. Group instances into hyper-sparse (set H) and non-hyper-sparse (set H̄) based on
   observed density stats.
3. Report geometric mean speedup within each group.
4. For the crash basis, additionally report iteration counts with/without crash.

### 3.4 Density Threshold Tuning

After the initial implementation, run a parameter sweep on the hyper-sparse Netlib set:

| Parameter | Range | Default |
|-----------|-------|---------|
| Hyper-sparse trigger density | 5%, 10%, 15%, 20% | 10% |
| Dense fallback reach-fraction | 15%, 25%, 33% | 25% |
| Crash basis toggle | on/off | gated by problem size |

Pick the combination that maximizes geometric mean speedup across set H while
minimizing regression on set H̄.

---

## 4. Summary

| Component | Order | LOC | Files | Key Functions |
|:----------|:-----:|:---:|:------|:--------------|
| Density monitoring | 1 | 80 | `lu.c`, `jaos_internal.h` | `jm_lu_update_density`, `jm_lu_should_use_hyper` |
| Reach I-FTRAN | 2 | 180 | `lu.c` | `reach_l`, `solve_from_reach_l`, modify `jm_lu_ftran` |
| Reach I-BTRAN | 3 | 160 | `lu.c` | `reach_ut`, `solve_from_reach_ut`, modify `jm_lu_btran` |
| Hyper-sparse PRICE | 4 | 120 | `simplex.c` | modify `price_and_select`, add row-wise scatter |
| Hyper-sparse CHUZC | 5 | 100 | `simplex.c` | sparse alpha in `pivot()`, `dual_ratio_test` |
| Position map (erase) | 6 | 60 | `lu.c` | modify `jm_svec_erase`, `jm_lu_update` |
| Crash basis | 7 | 350 | `simplex.c` | `build_crash_basis`, modify `jm_dual_simplex` |
| **Total** | | **~1050** | | |

**Key code locations summary:**

| File | Existing function | Change |
|:-----|:-----------------|:-------|
| `src/lu.c:681` | `jm_lu_ftran` | Gate on density; call reach-based L + U solves |
| `src/lu.c:709` | `jm_lu_btran` | Gate on density; call reach-based U^T + L^T solves |
| `src/lu.c:93` | `jm_svec_erase` | Add position-map fast path (or stamp-based) |
| `src/simplex.c:353` | `price_entry` | May be replaced by row-wise PRICE |
| `src/simplex.c:818` | `price_and_select` | BTRAN + sparse τ → row-wise alpha accumulation |
| `src/simplex.c:886` | `pivot` (d update loop) | Use sparse alpha index list |
| `src/simplex.c:375` | `build_initial_basis` | New sibling `build_crash_basis` |
| `src/simplex.c:1389` | `jm_dual_simplex` | Add crash basis call before `run()` |
| `src/jaos_internal.h:263` | `jm_lu` struct | Add density stats fields |
| `src/jaos_internal.h:115` | `sx` struct | Add crash basis fields, sparse-index scratch |

**Verification checkpoints:**
1. After each component: `make test` + `make sanitize` pass.
2. After components 2–3: Netlib gate passes (same iterations, fewer work units).
3. After component 4: Work units decrease further; iterations unchanged.
4. After component 7: Iterations decrease 15–30% on crash-amenable problems.
5. Final: `bench/` results archive with wall-clock measurements for M1 vs M2.