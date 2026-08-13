# Phase 2: Presolve and postsolve - Pattern Map

**Mapped:** 2026-08-12
**Files analyzed:** 6
**Analogs found:** 5 / 6 (postsolve arena is genuinely novel — see "No Analog Found")

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/presolve.c` (reduced-model builder + 8 reductions) | transform / service | transform (CRUD-like: build-then-discard a working copy) | `src/scale.c` (self-contained array transform, similar size) | role-match |
| `src/presolve.c` (postsolve LIFO arena + workspace lifecycle) | service (owns a workspace) | event-driven (append-then-replay-strictly-LIFO) | `src/lu.c` (`jm_lu_init`/`jm_lu_free` bracketing, and its Forrest-Tomlin eta list as an append-only record) | role-match, no exact append/replay precedent |
| `src/jaos_internal.h` (prototypes, counter struct, arena type) | config/header | — | `jaos_internal.h`'s existing `jm_work`/`JM_WORK_*` block (lines 402-421) and `jm_lu_init`/`jm_lu_free` declarations (508-509) | exact (style precedent) |
| `src/simplex.c` (`jm_dual_simplex` presolve call site, `publish` postsolve replay) | controller (orchestration entry point) | request-response | `jm_dual_simplex` itself (3762-3800) and `sx_init` (556-...), `publish` (3641-3760) — modifying in place, no external analog needed | exact (self-modification) |
| `Makefile` (`JAOS_NO_PRESOLVE`-style build guard) | config | — | `Makefile:94-101`'s existing `EXTRA_CFLAGS` / `PRICE_PARTITIONS_VALUE` dev-switch precedent | exact |
| `bench/run.c` (print per-family counters into record) | utility (reporting) | batch | Existing `run_one`/`run_one_infeasible` double-solve + record printing (lines ~380-580, not re-read this session beyond what CONTEXT/RESEARCH already quoted) | role-match |
| `tests/test_presolve.c` (or extension) | test | request-response (build model, solve, assert) | `tests/test_check.c` (`test_t1_flags_wrong_dual_sign` etc., lines 1-117) | exact |

## Pattern Assignments

### `src/presolve.c` — reduced-model construction

**Analog:** `src/simplex.c`'s `sx_init` (lines 556-616+)

This is the reference implementation of D-06's "build a working copy without mutating the model" — presolve repeats the same move one layer up, before scaling rather than after.

**How `sx_init` builds its own copy without mutating `m`** (`src/simplex.c:556-616`):
```c
static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_lu_init(&s->lu);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    /* Resolved once, here, so that a solve runs on one pair of tolerances
     * from beginning to end. Zero on the model means the caller never set
     * one, which is what makes an untouched model behave exactly as it did
     * before these existed. */
    s->primal_tol = m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol : PRIMAL_TOL;
    s->dual_tol   = m->cfg.dual_tol   > 0.0 ? m->cfg.dual_tol   : DUAL_TOL;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    s->av     = jm_alloc_array(m->num_nz, sizeof(double));   /* OWN copy */
    s->arv    = jm_alloc_array(m->num_nz, sizeof(double));
    s->lo     = jm_alloc_array(s->nvar, sizeof(double));
    s->up     = jm_alloc_array(s->nvar, sizeof(double));
    s->cost   = jm_calloc_array(s->nvar, sizeof(double));
    ...
    s->status = jm_alloc_array(s->nvar, sizeof(jm_var_status));
    ...
```
Then it transforms `m`'s CSC arrays into the copy (scaled here, unscaled for presolve):
```c
    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];
    /* m->a_value is read, never written */
```

`src/presolve.c`'s reduced-model builder should follow this exact shape: `memset` a zeroed struct, size arrays by the *reduced* dimensions (not `m`'s), allocate every array through `jm_alloc_array`/`jm_calloc_array`, then populate by reading `m->a_start`/`a_index`/`a_value`/`col_lower`/`col_upper`/`row_lower`/`row_upper` and never writing back into `m` until postsolve's final write in `publish`.

---

### `src/jaos_internal.h` / `src/alloc.c` / `src/util.c` — the allocation idiom

**Analog:** `src/alloc.c` (whole file, 34 lines) and `src/util.c:14-31`

Every array allocation in the tree goes through these two functions plus the grow macro. The postsolve arena must use the same idiom, not a bespoke realloc wrapper.

**`jm_alloc_array` / `jm_calloc_array`** (`src/alloc.c:1-34`, full file):
```c
/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos_internal.h"

#include <stdckdint.h>
#include <stdlib.h>

/* All array allocations in JAOS go through these two, so an index-arithmetic
 * overflow can never silently turn into a short allocation. This is the C23
 * <stdckdint.h> payoff (DECISIONS.md, D1). */

void *jm_alloc_array(int64_t n, size_t elsize)
{
    if (n < 0)
        return nullptr;
    size_t total;
    if (ckd_mul(&total, (size_t)n, elsize))
        return nullptr;
    if (total == 0)
        total = 1; /* uniform rule: success is always non-NULL */
    return malloc(total);
}

void *jm_calloc_array(int64_t n, size_t elsize)
{
    if (n < 0)
        return nullptr;
    size_t total;
    if (ckd_mul(&total, (size_t)n, elsize))
        return nullptr;
    if (n == 0 || elsize == 0)
        return calloc(1, 1);
    return calloc((size_t)n, elsize);
}
```

**`jm_grow` / `JM_GROW`** — the growable-array mechanism the postsolve arena should use to append records (`src/util.c:12-31`):
```c
/* Grows *arr (elements of elsize) to hold at least need elements. On
 * failure *arr is left untouched, so cleanup still frees the old block. */
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize)
{
    if (need <= *cap)
        return true;
    int64_t ncap = *cap < 16 ? 16 : *cap;
    while (ncap < need)
        if (ckd_mul(&ncap, ncap, (int64_t)2))
            return false;
    size_t bytes;
    if (ckd_mul(&bytes, (size_t)ncap, elsize))
        return false;
    void *p = realloc(*arr, bytes);
    if (p == nullptr)
        return false;
    *arr = p;
    *cap = ncap;
    return true;
}
```
And the macro (`src/jaos_internal.h:343-345`):
```c
bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize);
#define JM_GROW(a, cap, need) \
    ((need) <= (cap) ? true : jm_grow((void **)&(a), &(cap), (need), sizeof *(a)))
```
Use site precedent, `src/util.c:94`: `if (!JM_GROW(m->pool, m->pool_cap, m->pool_len + len))`. The postsolve arena's append operation should look exactly like this: `if (!JM_GROW(pre->arena, pre->arena_cap, pre->arena_len + 1)) return JAOS_ERR_OUT_OF_MEMORY;` then write the new tagged record at `arena_len` and increment.

---

### `src/presolve.c` — workspace lifecycle (init/free bracketing)

**Analog:** `src/lu.c:348-377`, `jm_lu_init`/`jm_lu_free`

**Full lifecycle pair**:
```c
void jm_lu_init(jm_lu *lu)
{
    memset(lu, 0, sizeof *lu);
}

void jm_lu_free(jm_lu *lu)
{
    if (lu->urow)
        for (int64_t s = 0; s < lu->dim; s++)
            jm_svec_free(&lu->urow[s]);
    if (lu->ucol)
        for (int64_t s = 0; s < lu->dim; s++)
            jm_svec_free(&lu->ucol[s]);
    free(lu->urow);
    free(lu->ucol);
    free(lu->l_start); free(lu->l_index); free(lu->l_value);
    free(lu->u_diag);
    jm_svec_free(&lu->ft);
    free(lu->ft_source);
    free(lu->slot_at); free(lu->pos_of);
    free(lu->perm_row); free(lu->perm_col);
    free(lu->inv_col);
    free(lu->tmp);
    free(lu->spike);
    free(lu->mark);
    free(lu->dfs_node);
    free(lu->dfs_next);
    free(lu->pattern);
    memset(lu, 0, sizeof *lu);
}
```
Note `jm_lu_free` zeroes the struct at the end — safe to call twice, and leaves the struct in the same state `jm_lu_init` would. The presolve struct (reduced model + arena) should have the identical pair, `jm_presolve_init`/`jm_presolve_free`, called at the top and bottom of `jm_dual_simplex`, matching how `sx_init` calls `jm_lu_init(&s->lu)` at line 559 and `jm_lu_free(lu)` appears at simplex.c:422,751 on cleanup paths.

D-08 (solve-local, freed with `sx`) means the presolve struct is most naturally a member of `sx` itself (parallel to `s->lu`), built in `sx_init` or immediately before it, freed wherever `s->lu` is freed today — grep those two call sites (`src/simplex.c:422,751` are `jm_lu_free` calls inside `sx`'s own error/cleanup paths) before wiring the new free-call site rather than trusting this line number, since `ARCHITECTURE.md` is stale by its own admission.

---

### Work-unit billing (D-14)

**Analog:** any of the `jm_work_add` call sites in `src/simplex.c` (14 sites found, e.g. lines 744, 995, 1043, 1384, 1639)

**The constants at their declaration** (`src/jaos_internal.h:412-421`):
```c
constexpr int64_t JM_WORK_NONZERO    = 1;     /* nonzero touched in a solve */
constexpr int64_t JM_WORK_ELIMINATED = 2;     /* nonzero eliminated, factor */
constexpr int64_t JM_WORK_FACTOR     = 4096;  /* fixed cost, refactorization */
constexpr int64_t JM_WORK_UPDATE     = 64;    /* fixed cost, basis update    */

static inline void jm_work_add(jm_work *w, int64_t n)
{
    if (w != nullptr)
        w->units += n;
}
```

**Representative call sites** (`src/simplex.c:744, 751-752, 1043`):
```c
jm_work_add(&s->work, JM_WORK_NONZERO);
...
jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                      JM_WORK_NONZERO);
...
jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
```
Every call multiplies a count of touched nonzeros/rows by the relevant `JM_WORK_*` constant and adds it to the same `jm_work` accumulator (`&s->work` here — presolve's is a locally-owned `jm_work` field or the same `s->work` if presolve runs inside `sx`). D-14 says presolve reuses `JM_WORK_NONZERO` (and possibly `JM_WORK_ELIMINATED` for an eliminated row/column) at each reduction's touch points — no new constant should be invented without the same "measured on both sides" discipline the rest of the constants in this block already carry (note their one-line comments name what each charges for, not a number pulled from nowhere).

**A cautionary comment worth reading before billing** (`src/simplex.c:1701-1703`, referenced in the grep output — a deliberate *non*-billing site): the file already has at least one place where `jm_work_add` is *not* called and the comment explains why, worth locating before assuming every touch must bill.

---

### `Makefile` — the build-time switch (D-03)

**Analog:** `Makefile:94-105`, the `EXTRA_CFLAGS` / `PRICE_PARTITIONS_VALUE` precedent

```make
# source between runs — `make EXTRA_CFLAGS=-DPRICE_PARTITIONS_VALUE=4`. Empty
# in every shipping build, and it is a development switch rather than an
# option: which pricing rule runs is the method, and the method is not the
# caller's to choose (D64). Sweeping a constant that must not change a verdict
# is also how three defects were found that 139 instances at one setting did
# not (D39, D47, D72).
EXTRA_CFLAGS ?=

RELEASE_CFLAGS := $(STD) $(WARN) $(FP) -Werror $(SHIP) -g -DNDEBUG $(PGO_CFLAGS) $(EXTRA_CFLAGS)
DEV_CFLAGS     := $(STD) $(WARN) $(FP) -Werror -g -Og $(EXTRA_CFLAGS)
```
The `JAOS_NO_PRESOLVE` guard (name at Claude's discretion, D-03) is wired the same way: `#if !defined(JAOS_NO_PRESOLVE)` around the presolve call site in `jm_dual_simplex`, switched by `make EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`. No new Makefile target or variable is needed — `EXTRA_CFLAGS` is already the hook. Copy the comment style: state *why* it's a dev switch and not an option, citing D64, the same way the existing comment cites it.

---

### `src/simplex.c` — orchestration call sites

**Analog:** `jm_dual_simplex` itself (`src/simplex.c:3762-3800`)

```c
jaos_status jm_dual_simplex(jaos_model *m)
{
    sx s;
    jaos_status st = sx_init(&s, m);
    if (st != JAOS_OK)
        return st;
    clock_gettime(CLOCK_MONOTONIC, &s.started);

    jm_log(m, JAOS_LOG_SUMMARY,
           "dual simplex: %lld rows, %lld columns, %lld nonzeros, "
           "primal tol %.3g, dual tol %.3g",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, s.primal_tol, s.dual_tol);

    jaos_solve_status outcome;
    bool warm = build_warm_basis(&s);
    if (!warm)
        build_initial_basis(&s);
    jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
           warm ? "the basis on the model" : "the slack basis");
    st = run(&s, &outcome);
    if (st == JAOS_OK) {
        if (outcome == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(&s);
            st = reenter_after_settling(&s);
            if (st == JAOS_OK)
                outcome = classify_optimum(&s);
        }
        if (st == JAOS_OK)
            st = publish(&s, outcome);
    }
    ...
```
D-04/D-06's placement: presolve must run **before** `sx_init(&s, m)` (which does the scaling), on `m` as loaded, producing a reduced model that `sx_init` is then handed instead of `m` directly (or `sx_init` gains a second argument/view). The plan's grep instruction is correct — this exact block will shift once presolve's call is inserted, so locate `jm_dual_simplex` by symbol at implementation time, not by these line numbers.

**`publish`'s scaled→original boundary** — the pattern postsolve's reduced→original boundary must sit beside (`src/simplex.c:3641, 3711-3730`):
```c
static jaos_status publish(sx *s, jaos_solve_status status)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    ...
    /* Out of the scaled copy and back into the model's own units. A column
     * carries its factor, a row activity divides its own out; the duals go
     * the other way, because a dual is a rate per unit of the thing it
     * prices. Every entry is written, so no pre-zeroing is needed. */
    const double *rho = m->row_scale, *gamma = m->col_scale;

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = published(gamma[j] * var_value(s, j));
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = published(var_value(s, m->num_col + i) / rho[i]);

    double *y = s->y;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_dual[i] = published(sigma * y[i] * rho[i]);
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_redcost[j] = published(sigma * s->d[j] / gamma[j]);

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = published_status(s->status[j]);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row_status[i] = published_status(s->status[m->num_col + i]);
    ...
```
This loop currently writes `m->sol_col[j]` for every `j` in `[0, m->num_col)` directly indexed by the *scaled copy's* column `j`, because today `s`'s columns and `m`'s columns are the same set. Once presolve reduces the problem, `s`'s columns are the *reduced* set and these loops must instead iterate the reduced index space and scatter into `m->sol_col[orig_col[j]]` via postsolve's index map — with every row/column presolve removed filled in by the LIFO replay **before** or **interleaved with** this loop, so that every entry of `m->sol_col`/`sol_row`/`sol_dual`/`sol_redcost` ends up written (the comment "every entry is written, so no pre-zeroing is needed" is the invariant postsolve must preserve for the *original* index space, not just the reduced one). This is the exact boundary D-11 makes postsolve responsible for.

`published()` (line 3621) and `published_status()` (line 3630) are the small helpers that normalize -0.0 and map `jm_var_status`→`jaos_basis_status`; postsolve-recovered values should be passed through the same two helpers before being written, for the same D21 byte-identity reason.

---

### `tests/test_presolve.c` — the negative-test pattern (D-10)

**Analog:** `tests/test_check.c:69-84`, `test_t1_flags_wrong_dual_sign`, quoted in full as the template for "build the case it must reject":

```c
static void test_t1_flags_wrong_dual_sign(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {-1.0}; /* >= row at its lower bound demands y >= 0 */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    /* Two breaches: the row dual points at a bound that is not active
     * (magnitude 1), and with y = -1 the reduced costs become 2 on
     * interior columns (magnitude 2). The worst one wins. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_dual_violation);
    jaos_model_free(m);
}
```
The shape to imitate: build a hand-solvable model (`make_t1`-style helper, small enough to verify by hand — see `tests/test_check.c:16-32` for the docstring convention naming the optimum and dual by hand before the code), construct a deliberately wrong answer with a comment explaining *why* it's wrong in one sentence, assert the checker (or here, postsolve+checker) rejects it with the specific violation magnitude asserted, not just a boolean. D-10 requires each round-trip test to have this negative sibling: hand postsolve a broken index map, confirm `jaos_check_solution` rejects the result — same `TEST_ASSERT_FALSE(r.dual_feasible)` / `TEST_ASSERT_DOUBLE_WITHIN(..., r.max_dual_violation)` idiom, driven through a full presolve-on solve rather than a hand-supplied `x`/`y`.

---

## Shared Patterns

### The comment voice (quote two, for register)

Every non-trivial line in this tree is annotated with *why*, frequently citing a decision ID. Two representative examples, both already quoted above in context but repeated here as the register to write in:

1. From `sx_init` (`src/simplex.c:565-568`):
```c
    /* Resolved once, here, so that a solve runs on one pair of tolerances
     * from beginning to end. Zero on the model means the caller never set
     * one, which is what makes an untouched model behave exactly as it did
     * before these existed. */
```
2. From `IMPLIED_ROUNDS`'s declaration (`src/check.c:246-264`, quoted in RESEARCH.md — the precedent D-02 explicitly follows):
```c
/* The cap on propagation rounds. Not a quality knob: the loop exits as soon
 * as a round bounds nothing new, so this is the safety stop, and it is set
 * where the propagation reaches its fixed point rather than anywhere useful
 * work is still being cut off.
 *
 * Swept over the standard set, counting certified answers — the canary that
 * had to move, and did:
 *
 *   rounds    1    2    4    8   16   32   64  128
 *   certified 17   23   32   38   46   47   48   48
 *
 * 64 is where it stops changing. ...
 *
 * 8 was here first, chosen by nothing, and it left ten answers uncertified
 * (D91). */
constexpr int64_t IMPLIED_ROUNDS = 64;
```
Neither comment describes *what* the code does mechanically (that's visible from the code itself) — both explain a decision: why the value lives where it does, why it is what it is, and what would break if changed. New code in `src/presolve.c` should hold every constant (round cap, tightening epsilon, duplicate-detection tolerance) to the same standard: a comment naming the decision it encodes and, once swept, the sweep table that set it — not a bare `#define` with no rationale.

### Allocation idiom
**Source:** `src/alloc.c` (whole file), `src/util.c:14-31`, `src/jaos_internal.h:328-329,343-345`
**Apply to:** reduced-model arrays, postsolve arena, per-family counter struct's any array fields.
See full excerpts under "The allocation idiom" above.

### Workspace init/free bracketing
**Source:** `src/lu.c:348-377`
**Apply to:** the presolve struct as a whole (reduced model + arena), if it is scoped as its own type rather than folded directly into `sx`.

### Work-unit billing
**Source:** `src/jaos_internal.h:412-421`, call sites throughout `src/simplex.c`
**Apply to:** every reduction's per-row/per-column touch, and the fixed-point loop's per-round overhead.

### Method-constant + build-guard declaration
**Source:** `src/check.c:246-264` (`IMPLIED_ROUNDS` — round-cap precedent), `Makefile:94-105` (`EXTRA_CFLAGS` — switch precedent)
**Apply to:** the presolve round cap (D-02) and the `JAOS_NO_PRESOLVE` guard (D-03).

## No Analog Found

| File/Concept | Role | Data Flow | Reason |
|---|---|---|---|
| Postsolve LIFO arena (append-only tagged records, replayed strictly LIFO) | service (event log) | event-driven / pub-sub-like (write-then-replay) | No exact precedent in this tree. The closest partial analog is the LU factorization's Forrest-Tomlin eta list (`jm_lu.ft`/`ft_source` in `src/lu.c`, an append-only sparse-vector accumulation consumed during BTRAN/FTRAN) — worth reading for the "append cheaply, walk once" *shape*, but it is not tagged-record/LIFO-replay and does not restore removed structure. The postsolve arena's tagged-record-per-reduction-family, replay-to-restore-original-indices design has no sibling in `src/`; RESEARCH.md's own "Don't Hand-Roll" table reaches the same conclusion and names `jm_alloc_array`/`JM_GROW` as the only reusable *piece*, not the whole mechanism. Design this from the eight reduction families' bookkeeping needs (each tag carries enough to invert its own removal) rather than by copying an existing type. |
| Dual/reduced-cost recovery formula per reduction family | — | — | Not code at all — RESEARCH.md flags this explicitly as `[ASSUMED]`, conceptual-only, not re-derived against `check.c`'s sign convention this session. No in-tree function computes this today because postsolve has never existed. The `sign_condition` quoted in RESEARCH.md (`jaos_internal.h`'s status enum + `check.c:422-481`) is the *target* the formula must satisfy, not a pattern to copy from. |

## Metadata

**Analog search scope:** `src/simplex.c`, `src/lu.c`, `src/scale.c`, `src/alloc.c`, `src/util.c`, `src/check.c`, `src/jaos_internal.h`, `Makefile`, `tests/test_check.c` — all read or grepped this session.
**Files scanned:** 9
**Pattern extraction date:** 2026-08-12
