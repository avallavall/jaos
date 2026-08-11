# Testing Patterns

**Analysis Date:** 2026-08-12

## Test Framework

**Runner:** Unity v2.7.0, vendored at `tests/vendor/unity/` — the project's
only test-time dependency exception (D15) alongside netlib's `emps`
converter. Provenance is recorded and must be kept current when updated
(`tests/vendor/unity/PROVENANCE.md`):

```
Version: v2.7.0
Source: https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.7.0.tar.gz
Tarball sha256: e84eb301ca7967831e68b1728f911e87fa2d345d8ddb64f897bc2f2ee24a321c
Licence: MIT (verified on import)
Files taken: src/unity.c, src/unity.h, src/unity_internals.h, LICENSE.txt
```

Update it by replacing all four files and that record together — never a
partial update.

**Config:** `-DUNITY_INCLUDE_DOUBLE` is set project-wide (`Makefile:112`) to
enable `TEST_ASSERT_DOUBLE_WITHIN` and friends, since this is a numerical
solver's test suite. Compiled with warnings on but **`-Werror` deliberately
off** for `unity.c` — it's vendored code, not held to the project's own bar
(`Makefile:107-109`, D15).

**No separate assertion library** — Unity's `TEST_ASSERT_*` macros are the
whole of it.

**Run commands** (WSL/Linux/GCC only — see project root `CLAUDE.md`, do not
attempt these on Windows directly):
```sh
make test                    # build and run every test_*.c under dev flags
make sanitize                # same, under ASan+UBSan
./build/dev/test_simplex     # run one binary directly for a subset/-v
```
There is no watch mode, no coverage target, and no `--filter`-by-name flag
at the Makefile level — Unity binaries run every `RUN_TEST` in the file
unconditionally; narrow a run by invoking one test binary directly.

## Test File Organization

**Location:** flat `tests/` directory, one file per source module under
test, co-located by name rather than by mirroring `src/`'s structure
one-for-one (there's a close but not exact correspondence):

| Test file | Tests | Covers |
|---|---|---|
| `tests/test_version.c` | 4 | `src/version.c`, `src/status.c` — version string, status-to-string mappings, default enum values are zero |
| `tests/test_lu.c` | 18 | `src/lu.c` — sparse LU factorization, Forrest-Tomlin updates, singularity handling, determinism |
| `tests/test_scale.c` | 12 | `src/scale.c` — Curtis-Reid / geometric scaling, power-of-two exactness |
| `tests/test_model.c` | 25 | `src/model.c` — model lifecycle, CSC/CSR mirror, add/delete rows and cols, config survival across reload |
| `tests/test_mps.c` | 6 | `src/mps.c` — MPS reader, golden instances + one rejection per failure class |
| `tests/test_lp.c` | 6 | `src/lpfmt.c` — LP-format reader, same golden + rejection pattern |
| `tests/test_check.c` | 18 | `src/check.c` — the independent solution checker |
| `tests/test_simplex.c` | 73 | `src/simplex.c` — the dual simplex method; by far the largest suite (2540 lines) |
| `tests/test_fuzz.c` | 5 | `src/mps.c` + `src/lpfmt.c` reader robustness under corrupted/truncated input |

`tests/data/` holds one hand-crafted malformed file per rejection class for
the readers (18 files: `e_*.mps` for MPS failures, `el_*.lp` for LP-format
failures, plus a few golden `.mps`/`.lp` fixtures like `t1.mps`,
`solve1.mps`, `g1.lp`). `tests/test_fuzz.c` reuses this same corpus as its
seed material rather than a separate fixture set.

**Naming:** `test_<subject>.c`, one Makefile-discovered binary per file
(`$(wildcard tests/test_*.c)`, `Makefile:120,130-131`). Every individual
test function is `static void test_<full_sentence_description>(void)` — the
name states the scenario and expected outcome as a sentence, not just the
function under test:

```c
static void test_singular_matrices_are_reported_not_hidden(void)
static void test_a_waived_sign_condition_is_still_caught_by_the_gap(void)
static void test_reader_rejects_out_of_range_row_index(void)
```

This matters for this project specifically: a failing test's name alone
should tell a reader what broke without opening the file.

**Structure:** every file defines empty `void setUp(void) {}` /
`void tearDown(void) {}` (Unity requires both to exist; JAOS never needs
per-test fixtures — models are built explicitly inside each test) and a
`main()` that lists every test via `RUN_TEST(test_name);` and returns
`UNITY_END()`:

```c
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_identity_factors_and_solves_exactly);
    RUN_TEST(test_permutation_matrix);
    ...
    RUN_TEST(test_updates_are_bit_identical_across_runs);
    return UNITY_END();
}
```

## Compile-Time White-Box Access

Tests are compiled with `-Isrc` and `#include "jaos_internal.h"`
(`Makefile:115-117`) so they can assert on internal struct fields — e.g.
`m->a_start`, `jm_scaled_abs`, `jm_lu` fields — directly, rather than only
through the public API. Files that do this mark it explicitly in the
include line:
```c
#include "jaos_internal.h" /* white-box: the assembled model is inspected */
```
Use white-box access when the property under test is about an internal
invariant (CSC sort order, scaling exactness); stick to the public API
(`jaos_solve`, `jaos_check_solution`) when the property is about solver
behaviour, so the test also exercises what a real caller sees.

## Test Structure and Patterns

**Golden-instance pattern** (readers): build one or two small models by
hand, verify every field of the loaded result field-by-field, then verify
one deliberately malformed file per rejection class returns the right
`jaos_status` (`tests/test_mps.c`, `tests/test_lp.c`).

**Hand-solved-LP pattern** (checker, simplex): construct a tiny LP whose
optimum was worked out on paper, load it, solve or check it, then assert
against the known-by-hand numbers:

```c
/* T1 (minimize):  min x0 + x1  s.t.  x0 + x1 >= 1,  0 <= x <= 10.
 * Any point on the facet x0+x1 = 1 is optimal with objective 1;
 * the optimal row dual is y = 1, giving reduced costs d = (0, 0). */
static jaos_model *make_t1(void) { ... }

static void test_t1_accepts_the_true_optimum(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    ...
}
```

**"Build the case it must reject" pattern** — the project's explicit
testing philosophy (see project root `CLAUDE.md`: "when changing a checker
or a predicate, build the case it must reject and confirm it does"). Test
names like `test_a_waived_sign_condition_is_still_caught_by_the_gap` and
`test_the_gap_can_be_two_large_halves_cancelling`
(`tests/test_check.c:212,571`) are exactly this — cases constructed to be
wrong in a specific way that a naive predicate would miss, not just cases
that happen to pass.

**Residual-based verification** (LU factorization): rather than compare
against a hand-solved answer, solve with the factorization and multiply the
result back through a dense reference matrix, then check the residual is
tiny — so "a bug in the sparse machinery cannot hide behind itself"
(`tests/test_lu.c:1-6`):

```c
static void mat_mul(const mat *m, const double *x, double *out) { ... }
/* Solve with the factorization, multiply back, report the worst residual
 * of both directions. */
static double solve_residual(const mat *m, jm_lu *lu, jm_work *w) { ... }
```

**Reused setup extracted to a named helper**, with a comment explaining why
it's worth naming even for a "trivial" block:
```c
/* Initialise and factor, asserting success. This block was verbatim at
 * fifteen call sites; naming it also names what the assertion is for. */
static void must_factor(const mat *m, jm_lu *lu, jm_work *w) { ... }
```

**Determinism assertions inside unit tests, not just in bench**: multiple
runs of the same randomised case are compared bit-for-bit via
`TEST_ASSERT_EQUAL_MEMORY(&first[i], &x[i], sizeof(double))`
(`tests/test_lu.c`, end of file), because D8 is checked at every level, not
only at the acceptance-gate level.

## Randomised / Property-Style Testing

No dedicated property-testing framework — determinism (D8) rules out
anything backed by libc `rand()`. Instead, each file that needs randomised
cases defines its **own small, explicitly-seeded PRNG**:

- `tests/test_lu.c:28-48` — xorshift64*, with a `rng_val()` wrapper that
  keeps values away from zero ("never tiny enough to be dropped as noise").
- `tests/test_fuzz.c:61-78` — splitmix64, plus `rng_below(n)` for a biased
  (deliberately — reproducibility over uniformity) bounded draw.

**Never use `rand()`, `srand()`, or anything time-seeded in a JAOS test.**
The project's own stated reason: libc's generator isn't specified to
produce the same sequence across platforms, so "a test that cannot be
replayed is no use when it fails" (`tests/test_lu.c:8-10`).

## Fuzzing — `tests/test_fuzz.c`

Targets reader robustness under **damaged** (not just malformed-but-valid)
MPS and LP input — the acceptance gate's condition 4 (`PLAN.md §2.9`). It
exists because `tests/data/`'s hand-crafted rejection files are all
well-formed enough to reach the specific check that rejects them; none is
truncated mid-record, bit-flipped, empty, or random — "the gap is
fuzz-shaped" (`tests/test_fuzz.c:1-9`).

**What it asserts** (deliberately narrow — it says nothing about whether a
mutated file is read *correctly*, since a damaged file has no correct
reading to compare against):
- the reader returns one of the statuses the API declares;
- the error message is never NULL, and empty exactly when the read succeeded;
- a failed read leaves the model's previous problem untouched (the contract
  stated in `jaos.h`);
- a successful read leaves a model with consistent dimensions;
- reading the same bytes twice gives the same answer (D8 reaches parsing —
  this is why `src/mps.c` sets locale explicitly);
- under `make sanitize`, nothing was read or written out of bounds — "that
  last one carries most of the weight" (`tests/test_fuzz.c:23-27`).

**Corpus and mutation strategy:** corpus is the sorted contents of
`tests/data/`; mutations come from the file's own seeded splitmix64, never
`rand()`. Every seeded corrupted file is tried against **both** readers
(MPS and LP), since a corrupted MPS file is corrupted input to the LP
reader too and costs nothing to also try. Case counts are fixed
`constexpr` values, not derived at runtime:

```c
constexpr int EDITS_PER_FILE = 200;  /* per-file targeted single-byte edits */
constexpr int CHAOS_CASES    = 1500;
constexpr int SALAD_CASES    = 3000;
constexpr int64_t REPEAT_STRIDE = 7;  /* every Nth case read twice, compared */
```
`JAOS_FUZZ_SCALE` (environment/build variable) multiplies the seeded case
counts for a longer campaign without changing which cases run first —
determinism of the *sequence* is preserved even when the *count* changes.

Sized deliberately to run in a few seconds under ASan, "because a fuzzer
nobody waits for is a fuzzer nobody runs" (`tests/test_fuzz.c:84-87`) —
truncation is the one exhaustive class, since the whole corpus is ~3 KB.

## Sanitizers — `make sanitize`

Builds every `test_*.c` a second time under `-fsanitize=address,undefined
-fno-omit-frame-pointer` on top of the dev flags (`Makefile:105,175-176`)
and runs the full suite again. This is where `test_fuzz.c`'s
out-of-bounds guarantee is actually enforced — under plain `make test` a
fuzz case only proves the reader didn't crash, which the project calls "the
weakest [property] and the one a fuzzer is least needed for"
(`tests/test_fuzz.c:25-27`). Run both `make test` and `make sanitize` before
considering a change to a reader or to `src/lu.c`/`src/simplex.c` verified.

## The Netlib Acceptance Gate — `bench/`

This is a separate tier from the unit suite: not run by `make test`, built
on demand, and judges the solver end-to-end against externally-sourced
references rather than hand-derived expectations. Full detail lives in
`bench/README.md`; summarized here because it's how correctness is
actually established at the solver level.

**Three instance sets, all gates** (`Makefile` targets, `bench/README.md`):

| Set | Instances | Asks | Target |
|---|---|---|---|
| standard | 94 | solved to a verified optimum | `make netlib` (~85s at `J=12`) |
| Kennington | 16 | same, correctness only (large models) | `make netlib-kennington` (~8min) |
| infeasible | 29 | classified `INFEASIBLE`, no false optima | `make netlib-infeas` (~10s) |

`make warm` / `make warm-kennington` are **not gates** — they measure what
warm re-solve buys over a cold solve as a ratio, and explicitly report a
ratio rather than a pass/fail verdict (project root `CLAUDE.md`,
`bench/README.md` "make warm" section).

**`-jN` matters for time, not for correctness**: the gate is safe to
parallelize because everything it records is an integer the solver
computed (work units, iterations, digests, verdicts) and instances are
independent — the record is provably byte-identical to a sequential run
(checked by diffing, D57). **`-j` does invalidate the printed seconds**
though, since concurrent solves compete for cache/memory bandwidth; a time
ratio requires `J=1`.

**Four things each instance is judged on** (`bench/README.md` "What each
instance is judged on"):
1. **Shape** — row/column counts must match the manifest.
2. **Objective** — within `1e-6 · max(1, |reference|)` of Koch's reference
   optimum.
3. **The independent checker** (see below).
4. **Determinism** — solved twice, with the basis explicitly cleared
   between runs (`jaos_clear_basis`) so the comparison is a statement about
   the solver's cold behaviour and not a trivially-agreeing warm re-solve;
   status, iterations, work units, and every published bit must match.

**A baseline, diffed per-instance, never rewritten as a side effect.**
`bench/netlib.baseline` (and the Kennington/infeasible equivalents) records
what every instance did last time; `make netlib` diffs against it, `make
netlib-baseline` is the *only* command that rewrites it. This split exists
because the gate's own verdict is all-or-nothing and reads `NOT MET`/`PASS`
identically whether a change fixed one instance and broke two, or changed
nothing — the per-instance baseline diff is what actually catches a
regression once every set passes (`bench/README.md` "The baseline, and the
question the gate cannot answer").

## Solution Digests — how bit-identical is actually checked

`bench/run.c` computes an FNV-1a hash over the **raw bytes** of the
solution vectors (not over anything rounded), then requires two independent
solves of the same instance to hash identically:

```c
/* FNV-1a over the raw bytes of the answer. Two solves of one model must
 * produce identical bits, so the digest is taken of the bytes and not of
 * anything rounded on the way. */
static uint64_t digest(const double *v, int64_t n, uint64_t h)
{
    const unsigned char *p = (const unsigned char *)v;
    for (int64_t i = 0; i < n * (int64_t)sizeof(double); i++)
        h = (h ^ p[i]) * 0x100000001b3u;
    return h;
}
```
Called as `digest(x, nc, FNV_OFFSET); digest(y, nr, that)` for the column
values and row duals of each of the two solves; the two 64-bit results must
match exactly. This is what makes "bit-identical" checkable rather than
asserted — the digest, not a diff of floating output, is the record.

## Independent Solution Checker — `src/check.c` / `jaos_check_solution`

The correctness backstop, and structurally independent from the solver on
purpose (D18): it recomputes row activities from the model's own stored
matrix and never consults the simplex's internal bookkeeping — "a simplex
bug cannot sign its own approval" (`tests/test_simplex.c:1-8`). Every
solver test in `test_simplex.c` is judged twice: against a hand-worked
optimum, and by this checker.

**Checks (`jaos_check_report`, `include/jaos.h:571-702`):**
- `primal_feasible` — bound and row-activity violations within tolerance.
- `dual_feasible`, `checked_duals` — sign conditions on reduced costs and
  row duals, complementary slackness; `checked_duals` is false only when
  `row_dual` was NULL.
- `objective_gap`, `gap_positive`/`gap_negative` — the primal-dual gap, kept
  as two non-negative halves rather than one signed number specifically so
  a small gap from real optimality can be told apart from two large terms
  cancelling (`test_check.c:571`, `test_the_gap_can_be_two_large_halves_cancelling`).
- `certified_suboptimality`, `gap_certified`, `dropped_terms`,
  `max_dropped_multiplier`, `unquantified_rays` — a certified *lower bound*
  on how far a point is from optimal, and whether the certificate is
  complete. Added specifically because sign conditions alone can hold on a
  point that's still far from optimal (D47) — see `include/jaos.h:596-673`
  for the full reasoning, which is unusually extensive even for this file's
  comment density and worth reading before touching this struct.

**Verdicts are booleans; magnitudes are raw.** The report always carries
the *raw* violation magnitudes (`max_col_violation`, etc.) alongside the
booleans that compare them to `tol` — so a test (or a caller) can inspect
*how* something failed, not just that it did.

**Tolerance scaling** (`include/jaos.h:715-724`, `docs/tolerances.md`): `tol`
is absolute where it measures a residual directly, but scaled by the sum of
magnitudes of a row's terms where it decides whether a value rests on a
bound — because "how precisely a sum can be placed is set by the terms that
went into it, not by the total they came to." Objective gap is compared
relatively. Don't add a new bound-proximity test against a bare `tol`
without reading `docs/tolerances.md` first (D23 explains why the two kinds
of test are not interchangeable).

## What a Change Is Actually Judged On (D45)

Per project root `CLAUDE.md` and reinforced throughout `bench/`, a change
to JAOS is judged on three things, never fewer: **solution digests** for
correctness (above), **work units** for determinism/regression-detection
across machines, and a **same-instance wall-clock time ratio** (`J=1` only,
never entering a baseline file) to catch what the other two structurally
cannot see. `bench/results/*.txt` and `bench/*.baseline` never contain a
wall-clock number for this reason — only `bench/compare/results/` does,
because a competitive timing comparison is the one context where seconds
are the actual question being asked (`bench/compare/README.md`).

---

*Testing analysis: 2026-08-12*
