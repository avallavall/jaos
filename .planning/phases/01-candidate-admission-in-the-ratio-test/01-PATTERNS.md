# Phase 1: Candidate admission in the ratio test - Pattern Map

**Mapped:** 2026-08-12
**Files analyzed:** 5 (src/simplex.c across four distinct changes, src/jaos_internal.h, tests/test_simplex.c, docs/work-units.md, plus DECISIONS.md/bench/results as non-code artifacts)
**Analogs found:** 4 / 5 code changes have a direct in-tree analog; 1 (the debug-assertion idiom) has none — first use in this codebase.

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|---|---|---|---|---|
| `src/simplex.c` — new `jm_`-prefixed list/bitmap maintenance primitives | utility | transform (in-place array/bitmap mutation) | `jm_pattern_order` (`src/simplex.c:1637-1673`) | exact — same problem class ("sparse ascending subset of `[0, nvar)`"), same file, same non-`static` plain-array shape |
| `src/simplex.c` — `dual_ratio_test`'s dense branch, replaced | service (solver-core) | transform (candidate-set build) | the branch itself, unchanged shape (`src/simplex.c:1573-1577`), and `s->anpat >= 0` sibling branch (`1569-1572`) it must match array-position-for-array-position | exact — the pattern branch is the working precedent for "walk a sparse ascending subset and call `admit_candidate`" |
| `src/simplex.c` — the six membership-changing `status[v]` sites | service (solver-core) | event-driven (each site is a distinct state transition) | see exhaustive site table below; `pivot()`'s pair (`2061`, `2064`) is closest to what must run every iteration | exact — all six already exist; this phase adds a hook beside each, not new call sites |
| `src/simplex.c` — D-08 runtime debug-build cross-check | utility (invariant check) | event-driven (once per iteration) | **none** — no `assert(`, no `NDEBUG`, no `#ifdef.*DEBUG` anywhere in `src/` or `include/` today | no analog — first use of this idiom in the codebase; see "No Analog Found" |
| `src/jaos_internal.h` — declarations for the new primitives | config (header/interface) | N/A | `jm_bland_pick`/`jm_harris_pick`/`jm_pattern_order` declarations (`jaos_internal.h:234-235,253-254,284-285`) | exact |
| `tests/test_simplex.c` — new unit tests (D-07's plain-array layer) | test | CRUD (setup/assert, no solver state) | `test_pattern_order_*` (`tests/test_simplex.c:1296-1369`) | exact — same "no `sx`, no model, plain arrays" shape |
| `docs/work-units.md` — dense-branch charge description | config (doc) | N/A | the existing paragraph it must replace (`docs/work-units.md:62-64`) | exact — same file, same section |

## Pattern Assignments

### 1. The maintenance primitives — analog is the `jm_bland_pick` / `jm_pattern_order` / `jm_harris_pick` trio

All three live in `src/simplex.c`, are **non-`static`**, **`jm_`-prefixed**, and take **plain arrays** rather than `sx *` — this is precisely why `tests/test_simplex.c` can call them directly, since `sx` is a `typedef struct { ... } sx;` defined inside `src/simplex.c` (line 283) and never declared in `src/jaos_internal.h`, and both `admit_candidate` (`1527`) and `dual_ratio_test` (`1557`) are `static`.

**Declaration site** (`src/jaos_internal.h:234-235,253-254,284-285`):
```c
int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol);
...
int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den);
...
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words);
```
Each is preceded in the header by a multi-paragraph doc comment stating: what the arguments mean, what invariant the caller must hold on entry/exit, and **why it is reachable from outside `simplex.c` at all** — `jm_pattern_order`'s closing sentence is the template to imitate: *"Reachable from outside the simplex because a defect here is invisible from the solve: a dropped position leaves a variable out of a ratio test that would have been correct without it, and the answer is merely different rather than wrong."* (`jaos_internal.h:280-283`). The new primitives' equivalent argument is Pitfall 1 from RESEARCH.md: a desynchronised list is also invisible from the solve except through D-05's digests.

**Definition site — `jm_pattern_order` in full, the closest analog of all** (`src/simplex.c:1634-1673`):
```c
/* A scatter's record of where it wrote, made ascending and distinct.
 * Documented in the header, beside the two picks, and reachable for the
 * same reason they are. */
int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words)
{
    *words = 0;
    if (n <= 0 || limit <= 0)
        return 0;

    /* The touched range, so that a pattern living in one corner of a large
     * model does not pay for the whole bitmap on the way back out. */
    int64_t lo = (limit + 63) / 64, hi = -1;
    for (int64_t t = 0; t < n; t++) {
        int64_t p = pos[t];
        if (p < 0 || p >= limit)
            continue;
        int64_t w = p >> 6;
        mark[w] |= UINT64_C(1) << (p & 63);
        if (w < lo) lo = w;
        if (w > hi) hi = w;
    }

    /* Reading back over the input is safe: every position is in the bitmap
     * by now, and the distinct count can only be smaller than what went in,
     * so the write index never overtakes anything still needed. */
    int64_t k = 0;
    for (int64_t w = lo; w <= hi; w++) {
        uint64_t bits = mark[w];
        if (bits == 0)
            continue;
        mark[w] = 0;
        while (bits != 0) {
            pos[k++] = (w << 6) + __builtin_ctzll(bits);
            bits &= bits - 1;
        }
    }
    *words = hi >= lo ? hi - lo + 1 : 0;
    return k;
}
```
Note the bitmap-clears-itself convention: `mark[w] = 0` is cleared inside the same loop that reads it, so the caller allocates once and the invariant "`mark` is all zero on entry" holds again on return without a separate clearing pass — the header comment states this explicitly (`jaos_internal.h:268-270`: *"It must be all zero on entry and is all zero again on return — the routine clears exactly the words it set, so the caller allocates it once and never has to."*). Any new bitmap-backed maintenance structure should keep the same "clears itself" property if it reuses `amark`-style storage, or state explicitly why it does not.

`jm_bland_pick` (`src/simplex.c:1612-1632`) and `jm_harris_pick` (`src/simplex.c:1675-1700`) are shorter, purely arithmetic examples of the same shape — two flat loops over plain arrays, no allocation, no `sx`.

**How the test file calls these three directly** (`tests/test_simplex.c:1174-1369`) — no `fresh()`, no `jaos_model`, no `sx`:
```c
constexpr double HARRIS_TOL = 1e-7;

static void test_harris_ignores_a_big_pivot_outside_the_window(void)
{
    const double num[] = {1.0, 500.0};
    const double den[] = {1.0, 100.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, HARRIS_TOL));
}
```
```c
static void test_pattern_order_sorts_and_dedups(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    /* Out of order, one position three times, across four words. */
    int64_t pos[] = {200, 5, 63, 5, 64, 130, 5, 0};
    int64_t k = jm_pattern_order(8, pos, mark, PAT_LIMIT, &words);

    TEST_ASSERT_EQUAL_INT64(6, k);
    const int64_t want[] = {0, 5, 63, 64, 130, 200};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], pos[i]);
    TEST_ASSERT_EQUAL_INT64(4, words);   /* words 0..3 inclusive */
    assert_mark_clean(mark);
}
```
`assert_mark_clean` (`tests/test_simplex.c:1290-1294`) is a small local helper checking the bitmap is all-zero again — the equivalent "structure came back to its resting invariant" check the new primitives' tests should have if they carry a bitmap too.

**Contrast — what these three are NOT (`admit_candidate`, `sx`-taking, `static`)** (`src/simplex.c:1523-1555`):
```c
/* One variable's eligibility, and its place in the candidate arrays if it
 * has one. Split out because the scan around it comes in two forms — over
 * the pricing row's pattern where there is one, over every variable where
 * there is not — and a rule this delicate must not be written twice. */
static void admit_candidate(sx *s, int64_t v, bool below, int64_t *n)
{
    if (s->status[v] == JM_BASIC)
        return;
    ...
}
```
`admit_candidate` and `dual_ratio_test` (`static`, `1557`) are the pattern to avoid for the new maintenance primitives — they are file-private and take the private `sx *`, which is exactly why they cannot be reached from `tests/test_simplex.c` (confirmed: `Grep` for `struct sx`/`typedef struct.*sx` over `src/jaos_internal.h` returns nothing).

### 2. The call sites where the list must be maintained

Twenty `s->status[v] = ...` assignment sites total in `src/simplex.c`; six change basis *membership* (transition into or out of `JM_BASIC`) and are where the new list/bitmap needs a hook. The other fourteen (grouped into three "no" rows below) only toggle between the two bound-status values and touch neither `JM_BASIC` nor `s->basis`/`s->where`.

| Line(s) | Function | Transition | Membership? | Frequency |
|---|---|---|---|---|
| 720 | `build_initial_basis` | `-> JM_BASIC` | Yes — build from scratch | Once per cold start |
| 740,746,748,750,752 | `build_initial_basis` | `-> JM_AT_LOWER`/`JM_AT_UPPER`/`JM_FREE` | Yes — same build | Once per cold start |
| 840 | `build_warm_basis` | `-> JM_BASIC` | Yes — build from scratch | Once per warm start |
| 851,853,855,857 | `build_warm_basis` | `-> JM_AT_UPPER`/`JM_AT_LOWER`/`JM_AT_UPPER`/`JM_FREE` | Yes — same build | Once per warm start |
| 1155,1157,1159 | `repair_singular_basis` | evicted `-> JM_AT_LOWER`/`JM_AT_UPPER`/`JM_FREE` | Yes — insert | Rare (rank < dim) |
| 1163 | `repair_singular_basis` | logical `-> JM_BASIC` | Yes — remove | Same rare path |
| 1470 | `apply_flips` | `JM_AT_LOWER <-> JM_AT_UPPER` | **No** | Every iteration with retired candidates |
| **2061** | **`pivot`** | **`leaving -> JM_AT_LOWER`/`JM_AT_UPPER`** | **Yes — insert** | **Every successful pivot** |
| **2064** | **`pivot`** | **`q -> JM_BASIC`** | **Yes — remove** | **Every successful pivot** |
| 2164 | `repair_dual_infeasibility` | `JM_AT_LOWER <-> JM_AT_UPPER` | **No** | Settle-up pass only |
| 2586 | `arm_reentry` | `JM_AT_LOWER <-> JM_AT_UPPER` | **No** | Re-entry round start only |

**`pivot()` — the one site that runs every iteration** (`src/simplex.c:2058-2066`):
```c
    /* Position r now holds the entering variable, at its new value. */
    s->xb[r] = q_value + theta_primal;

    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    s->where[leaving] = -1;
    s->basis[r] = q;
    s->status[q] = JM_BASIC;
    s->where[q] = r;
```
`leaving` is the variable that must be **inserted** into the nonbasic list/bitmap; `q` is the variable that must be **removed** from it. `pivot`'s own signature is `static jaos_status pivot(sx *s, int64_t r, int64_t q, bool below, ...)` (`src/simplex.c:1923`).

**`build_initial_basis` — the cold-start build** (`src/simplex.c:715-754`):
```c
static void build_initial_basis(sx *s)
{
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->ncol + i;
        s->basis[i] = v;
        s->status[v] = JM_BASIC;
        s->where[v] = i;
        s->dse[i] = 1.0;
    }
    for (int64_t j = 0; j < s->ncol; j++) {
        s->where[j] = -1;
        bool has_lo = isfinite(s->lo[j]);
        bool has_up = isfinite(s->up[j]);

        if (s->cost[j] > 0.0) {
            if (!has_lo) { s->lo[j] = -ARTIFICIAL_BOUND; s->fake[j] = FAKE_LO; }
            s->status[j] = JM_AT_LOWER;
        } else if (s->cost[j] < 0.0) {
            if (!has_up) { s->up[j] = ARTIFICIAL_BOUND; s->fake[j] = FAKE_UP; }
            s->status[j] = JM_AT_UPPER;
        } else if (has_lo) {
            s->status[j] = JM_AT_LOWER;   /* zero cost: either bound is fine */
        } else if (has_up) {
            s->status[j] = JM_AT_UPPER;
        } else {
            s->status[j] = JM_FREE;       /* zero cost, no bounds: d = 0 */
        }
    }
}
```
Every structural (`j < s->ncol`) ends up nonbasic here — this is the natural place to *build* the list from scratch (all `j` in `[0, s->ncol)` are nonbasic after this loop, logicals are all basic), rather than patch it incrementally.

**`build_warm_basis` — the warm-start build, same shape** (`src/simplex.c:833-858`):
```c
    int64_t p = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        jaos_basis_status want = ...;
        if (want == JAOS_BASIS_BASIC) {
            s->basis[p] = v;
            s->status[v] = JM_BASIC;
            s->where[v] = p;
            p++;
            continue;
        }

        s->where[v] = -1;
        if (want == JAOS_BASIS_AT_UPPER && isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else if (isfinite(s->lo[v]))
            s->status[v] = JM_AT_LOWER;
        else if (isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else
            s->status[v] = JM_FREE;
    }
```

**`repair_singular_basis` — evict/insert pair, in a loop** (`src/simplex.c:1141-1165`):
```c
        int64_t leaving  = s->basis[p];
        int64_t entering = s->ncol + i;
        if (s->status[entering] == JM_BASIC) { done = false; break; }

        if (isfinite(s->lo[leaving]))
            s->status[leaving] = JM_AT_LOWER;
        else if (isfinite(s->up[leaving]))
            s->status[leaving] = JM_AT_UPPER;
        else
            s->status[leaving] = JM_FREE;
        s->where[leaving] = -1;

        s->basis[p] = entering;
        s->status[entering] = JM_BASIC;
        s->where[entering] = p;
        i++;
```

**The three "No" sites — bound flip only, membership untouched.** Same one-line idiom at all three, `s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER : JM_AT_LOWER;`:
- `apply_flips` (`src/simplex.c:1470-1471`)
- `repair_dual_infeasibility` (`src/simplex.c:2164-2165`)
- `arm_reentry` (`src/simplex.c:2586-2587`)

None of these three needs a hook — RESEARCH.md's invariant is **"contains exactly `{v : status[v] != JM_BASIC}`"**, checked by transition (did this write leave `JM_BASIC`, enter it, or neither), and all three never touch `JM_BASIC` or `s->basis`/`s->where`.

### 3. The debug-assertion idiom — no analog exists

Confirmed this session: `Grep` for `assert\(|NDEBUG` returns **no matches** in `src/` and **no matches** in `include/`. This phase would introduce the first use of the standard C `#ifndef NDEBUG` idiom anywhere in `src/`.

**Makefile's dev/release split** (`Makefile:103-105`):
```makefile
RELEASE_CFLAGS := $(STD) $(WARN) $(FP) -Werror $(SHIP) -g -DNDEBUG $(PGO_CFLAGS) $(EXTRA_CFLAGS)
DEV_CFLAGS     := $(STD) $(WARN) $(FP) -Werror -g -Og $(EXTRA_CFLAGS)
ASAN_CFLAGS    := $(DEV_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer
```
`-DNDEBUG` is defined **only** in `RELEASE_CFLAGS`. `DEV_CFLAGS` (used by `make test`) and `ASAN_CFLAGS` (used by `make sanitize`, itself `DEV_CFLAGS` plus sanitizer flags) do **not** define it — so a `#ifndef NDEBUG ... #endif` block compiles **in** for `make test` and `make sanitize`, and compiles **out** for every `RELEASE_CFLAGS` build: `make all` (the shipped library, `Makefile:143-146`, built from `$(REL_OBJ)`), `$(B)/bench/run` and `$(B)/bench/warm` (both built with `$(RELEASE_CFLAGS)`, `Makefile:182-183,191-192`), and therefore every `make netlib*`/`make warm*` campaign. This is exactly the "costs nothing in release" shape D-08 asks for, and it is already wired correctly at the build-flag level — nothing in the Makefile needs to change for this phase.

One thing to watch, since this is the first use: `-Werror` is set in both `DEV_CFLAGS` and `RELEASE_CFLAGS`, so anything computed only for the debug-build comparison (a second scan result, a temporary array) must not exist — or must be marked `[[maybe_unused]]` — in a release build where `NDEBUG` compiles the comparison away, or `-Wunused` under `-Werror` will fail the release build.

### 4. The test file's shape — `tests/test_simplex.c`

**`RUN_TEST` registration, in `main`** (`tests/test_simplex.c:2463-2539`) — one `RUN_TEST(name)` line per test, in the order the tests appear in the file, grouped by a `/* ---- Section title ---- */` comment banner above each cluster (e.g. `/* ---- Harris' ratio test ---------------------------------------------- */` at `1168`, `/* ---- Bland's rule, for when Harris' has cycled ------------------------- */` at `1222`). A new cluster for the list-maintenance primitives would follow the same banner-then-`RUN_TEST`-block pattern, most naturally inserted after the `test_pattern_order_*` cluster (`RUN_TEST(test_pattern_order_edge_counts)` at line `2507`) since it is the closest sibling in subject matter, then registered in `main` in the same relative position.

**Test naming**: full descriptive sentences in `snake_case`, e.g. `test_pattern_order_scans_only_the_touched_range`, `test_bland_does_not_let_the_index_beat_the_quotient` — the name states the property being checked, not the function under test plus a number.

**Test body shape for the plain-array helpers** — no `fresh()`/`jaos_model`/`solve_and_verify`, just local arrays, a call, and `TEST_ASSERT_*`:
```c
static void test_pattern_order_drops_what_it_cannot_hold(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {PAT_LIMIT, -1, 7, PAT_LIMIT + 1000};
    TEST_ASSERT_EQUAL_INT64(1, jm_pattern_order(4, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(7, pos[0]);
    assert_mark_clean(mark);
}
```
This is the direct template for D-07's plain-array unit-test layer, including the "shown to fail on a deliberately broken input" requirement CONTEXT.md's `<specifics>` demands — `test_pattern_order_drops_what_it_cannot_hold` is itself a small example of asserting a specific rejection/edge behavior rather than only a happy path. The interleaved-eviction negative case from RESEARCH.md's Pitfall 1 (evict A, insert something into A's old gap, re-evict-and-reinsert A) needs its own test in this same style — build the sequence of insert/remove calls by hand, assert the resulting order matches `{v : status[v] != JM_BASIC}` exactly, including that the "something" inserted into the gap was not silently dropped.

Local helper functions for shared assertions live above the tests that use them, `static`, e.g. `assert_mark_clean` (`tests/test_simplex.c:1290-1294`) — a repeatable-invariant check factored out once and called from every test that needs it, not re-inlined per test.

### 5. The work-unit charge site

**Both `jm_work_add` calls in `dual_ratio_test`** (`src/simplex.c:1569-1577`):
```c
    if (s->anpat >= 0) {
        for (int64_t t = 0; t < s->anpat; t++)
            admit_candidate(s, s->apat[t], below, &n);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    } else {
        for (int64_t v = 0; v < s->nvar; v++)
            admit_candidate(s, v, below, &n);
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    }
```
Line `1572` (pattern branch, out of scope, unchanged) charges `s->anpat * JM_WORK_NONZERO` — one per pattern entry actually visited. Line `1576` (dense branch, **the D-09 target**) currently charges `s->nvar * JM_WORK_NONZERO` — every variable, whether visited productively or not. Once the dense branch walks the new list/bitmap instead of `[0, s->nvar)`, this charge must become "one per entry the new structure actually visited" to match the pattern branch's own convention — the two branches should end up charging the *same way* (one per admitted-or-rejected candidate actually looked at), which is also what keeps `docs/work-units.md`'s existing single paragraph true of both branches at once rather than needing two different paragraphs.

**`docs/work-units.md`'s existing statement of the rule** (`docs/work-units.md:62-64`):
```
**Ratio test and bookkeeping**: building the candidate set charges one per
variable it looked at — every variable when the pricing row is read densely,
and the size of its pattern when it is not (D40) — and the dual update
```
The phrase *"every variable when the pricing row is read densely"* is exactly what stops being true once the dense branch stops visiting every variable — this sentence is the one that needs to change, in the same doc, alongside the code change (D-09's own text: "the improvement would be invisible in the project's own currency" if the doc and the counter disagree).

## Shared Patterns

### The `jm_`-prefixed plain-array testability pattern
**Source:** `jm_bland_pick`/`jm_harris_pick`/`jm_pattern_order`, declared `src/jaos_internal.h:234-235,253-254,284-285`, defined `src/simplex.c:1612-1700`
**Apply to:** any new list/bitmap maintenance primitive (insert-on-leave, remove-on-enter, and the equivalence-check helper itself), so it is unit-testable exactly like `jm_pattern_order` without needing a real `sx` or a solved LP. This is the mechanism that resolves RESEARCH.md's Pitfall 2 (D-07's home): the maintenance primitives get the plain-array treatment, and the end-to-end dense-scan-vs-list-scan comparison (which genuinely needs `sx`) becomes the D-08 runtime assertion instead, inside `simplex.c` where `sx` is visible.

### The bitmap-clears-itself convention
**Source:** `jm_pattern_order`, `src/simplex.c:1660-1671` and its header contract `jaos_internal.h:268-270`
**Apply to:** any bitmap-backed representation chosen for the new structure — a routine that reads a bitmap should leave it zeroed on the way out, so the caller allocates the bitmap once in `sx_init` and never re-clears it. `assert_mark_clean` (`tests/test_simplex.c:1290-1294`) is the matching test-side check.

### Per-solve allocation in `sx_init`, freed in `sx_free`
**Source:** `src/simplex.c:554-596` (`sx_init`) and `489-507` (`sx_free`)
**Apply to:** wherever the new list/bitmap's storage is added to `sx` (Claude's discretion per CONTEXT.md). `apat`/`amark` is the closest existing analogue for a bitmap:
```c
s->apat   = jm_alloc_array(s->nvar, sizeof(int64_t));
s->amark  = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
```
sized off `s->nvar` exactly, both checked in the single `if (!s->av || ... )` null-check block (`588-596`) and both freed in `sx_free`'s unconditional list (`491-507`). CONTEXT.md's own Integration Points section names this exact risk: a field left out of `sx_free`'s list is "the field-by-field save list class of bug `.planning/codebase/CONVENTIONS.md` warns about elsewhere in this codebase."

### Doc comment convention for a newly-reachable helper
**Source:** the comment blocks immediately above each of `jm_harris_pick`, `jm_bland_pick`, `jm_pattern_order` in `src/jaos_internal.h` (`209-233`, `237-252`, `256-283`)
**Apply to:** any new declaration added to `jaos_internal.h`. Each existing comment covers, in order: what the arguments mean, what the return value means including edge cases (`n <= 0`, empty pattern), and a closing sentence stating **why this is reachable from outside `simplex.c` at all** — the new primitives' header comments should follow the same three-part shape, closing with the Pitfall-1-style argument for why the maintenance logic is delicate enough to need its own testable unit.

## No Analog Found

| File | Role | Data Flow | Reason |
|---|---|---|---|
| `src/simplex.c` — D-08 `#ifndef NDEBUG` runtime cross-check block | utility (invariant check) | event-driven, once per iteration | No `assert(`, `NDEBUG`, or `#ifdef.*DEBUG` idiom exists anywhere in `src/` or `include/` today (confirmed by `Grep` this session). The Makefile's `-DNDEBUG` placement (`Makefile:103-105`) already supports the idiom correctly; only the *use* of it in `src/` is new. Planner should treat this as new-idiom-in-the-codebase risk rather than a refactor of an existing pattern, and pay particular attention to `-Wunused`/`-Werror` on both `DEV_CFLAGS` (idiom compiles in) and `RELEASE_CFLAGS` (idiom compiles out) builds. |

## Metadata

**Analog search scope:** `src/simplex.c` (full file structure read via targeted ranges: 283-373 struct definition, 480-600 alloc/free, 700-870 basis builders, 1103-1182 singular repair, 1440-1610 ratio-test/flips region, 1612-1700 the three `jm_`-prefixed helpers, 1923-2085 `pivot`, 2115-2167 `repair_dual_infeasibility`, 2560-2605 `arm_reentry`), `src/jaos_internal.h` (full file, 556 lines), `tests/test_simplex.c` (lines 1-40 header/framework, 1160-1370 the three helpers' tests, 2455-2540 `main`/`RUN_TEST` list), `Makefile` (lines 80-209, flags and build rules), `docs/work-units.md` (lines 40-84, charge-site documentation).
**Files scanned:** 6 (all listed above), plus `Grep` sweeps of `src/` and `include/` for `assert(`/`NDEBUG` and of `src/simplex.c` for the `pivot`/`repair_dual_infeasibility`/`repair_singular_basis` function signatures.
**Pattern extraction date:** 2026-08-12
