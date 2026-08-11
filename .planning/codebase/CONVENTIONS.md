# Coding Conventions

**Analysis Date:** 2026-08-12

JAOS has no linter, no `.clang-format`, and no style guide document — the
conventions below are read off ~16,000 lines of `src/`, `include/` and
`tests/`. They are unusually load-bearing for this project: several exist to
protect bit-identical results (D8), not just readability, so deviating from
them can be a correctness bug, not a style nit.

## Naming Patterns

**Two prefixes, one line between them:**

- `jaos_` — public API. Every symbol in `include/jaos.h` (functions, types,
  enum values as `JAOS_*`). This is the *only* public header; anything not
  declared here carries no stability promise (`include/jaos.h:3-4`).
- `jm_` — internal implementation. Every symbol in `src/jaos_internal.h` and
  defined in `src/*.c` (functions, types, and `JM_*` macros/constants).
  Example: `jm_dual_simplex`, `jm_lu_factor`, `jm_alloc_array`, `JM_GROW`.

A file never mixes the two namespaces for its own new symbols: `src/lu.c`
declares and defines only `jm_lu_*`; `src/model.c` implements the public
`jaos_*` model-lifecycle calls plus a couple of `jm_model_*` internal
helpers used by other translation units.

**Everything is snake_case** — functions, variables, struct fields, enum
values (`JAOS_ERR_INVALID_INPUT`, `jm_var_status`, `JM_AT_LOWER`). No
camelCase anywhere in `src/` or `include/`.

**Types:** `typedef struct { ... } jm_foo;` — the struct tag is usually
omitted and the typedef name carries the `jm_`/`jaos_` prefix directly, e.g.
`jm_lu`, `jm_svec`, `jm_nmap`, `jm_work`, `jaos_check_report`.

**Local/loop variables** are short and conventional: `i`, `j`, `k` for
indices (row, column, nonzero-slot respectively, kept consistent across the
codebase), `n`/`dim` for a size, `m` for `jaos_model *`, `lu` for `jm_lu *`,
`w` for `jm_work *`. Descriptive names are reserved for anything whose
meaning isn't obvious from the algorithm (`alpha`, `tau`, `rho` keep their
paper names — see `src/jaos_internal.h:170-207`).

**Constants** are `constexpr` (C23), not `#define`, and are named
SCREAMING_SNAKE_CASE at file scope: `PRIMAL_TOL`, `DSE_DRIFT`,
`SPARSE_ALPHA_DEN` (`src/simplex.c:44-120`). Every one carries a comment
explaining not just what it is but where its value came from — a decision
ID, a sweep, or a measurement (see "Decision-ID citations" below). There is
no unexplained magic number in the hot paths.

## Code Style

**Formatting:** no automated formatter; style is applied by hand and is
consistent enough that it reads as one. 4-space indentation, no tabs (except
in `Makefile`, where make requires them). K&R-ish brace placement:

```c
jaos_status jaos_model_new(jaos_model **out)
{
    if (out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    *out = nullptr;
    jaos_model *m = jm_calloc_array(1, sizeof *m);
    if (m == nullptr)
        return JAOS_ERR_OUT_OF_MEMORY;
    *out = m;
    return JAOS_OK;
}
```

Function-opening brace on its own line; every other brace (`if`, `for`,
`while`, `struct`) on the same line as its keyword. A single-statement `if`
body is written without braces, on the next line, indented — never on the
same line as the condition.

**Compiler flags are the linter.** `Makefile` builds with
`-std=c23 -Wall -Wextra -Wpedantic -Werror -ffp-contract=off` for both
release and dev builds (`Makefile:48-58,103-104`); nothing merges with an
open warning. `sanitize` adds `-fsanitize=address,undefined`. There is no
separate style-check target — clean compilation under `-Werror` is the bar.

## Header and Include Discipline

- **One public header only:** `include/jaos.h`, guarded `JAOS_H`, wraps
  everything in `extern "C"` for C++ callers, and is included with
  `#include <stdint.h>` / `#include <stdbool.h>` (the latter skipped under
  `__cplusplus`).
- **One internal header:** `src/jaos_internal.h`, guarded `JAOS_INTERNAL_H`,
  `#include`s `jaos.h` first. Declares `struct jaos_model`, `jm_config`, the
  dual-simplex kernels, the LU-factorization API, the growable-vector and
  name-map helpers, and the deterministic work-counter macros.
- **Tests are white-box on purpose:** `tests/*.c` include both `jaos.h` and
  `jaos_internal.h` (Makefile's `TEST_INC` adds `-Isrc`) — "white-box
  assertions on the data structures are part of their job"
  (`Makefile:115-116`).
- **Include order**, consistent across every `.c` file: the project's own
  header(s) first (own internal/public header, `unity.h` in tests), a blank
  line, then system/libc headers in alphabetical order. Example
  (`src/mps.c:18-27`):

```c
#include "jaos_internal.h"

#include <ctype.h>
#include <inttypes.h>
#include <locale.h>
#include <math.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
```

- Every source and header file opens with a one-line (or short paragraph)
  comment describing the file's job and closes the header block with
  `SPDX-License-Identifier: Apache-2.0` on its own line.
- `#define _POSIX_C_SOURCE 200809L` appears at the top of any `.c` file that
  needs POSIX beyond strict `-std=c23` (e.g. `clock_gettime` in
  `bench/run.c`, `src/simplex.c`) — placed before any `#include`, with a
  one-line comment explaining why strict ISO doesn't have it.

## Error Handling and Status Conventions

**One status enum for the library, another for a solve.** `jaos_status`
(`JAOS_OK`, `JAOS_ERR_INVALID_INPUT`, `JAOS_ERR_OUT_OF_MEMORY`,
`JAOS_ERR_IO`, `JAOS_ERR_NUMERICAL`) is the return value of every fallible
call. `jaos_solve_status` is a separate, larger enum read back via
`jaos_status_of()` — deliberately distinct because hitting a work or time
limit is an honest report of where the solver stopped, not a failure
(`include/jaos.h:48-50`).

- **Data never leaves through a return value.** Every fallible function
  returns `jaos_status`; results come back through output parameters
  (`include/jaos.h:38-39`).
- **`JAOS_NODISCARD`** (a macro that degrades to nothing on compilers
  without `[[nodiscard]]`) marks every public function whose result must be
  checked — which is nearly all of them (`include/jaos.h:23-28`).
- **NULL-model calls are tolerated, not undefined behaviour**, and read as
  the natural empty/failure value: `jaos_num_col(NULL) == 0`,
  `jaos_model_error(NULL) == ""`, a NULL model passed to a setter returns
  `JAOS_ERR_INVALID_INPUT` (`src/model.c:73-88`).
- **Short functions** (most setters/getters) use flat early-return
  validation — no `goto`, no cleanup needed because nothing has been
  allocated yet (`src/model.c:82-96`).
- **Longer functions with intermediate allocations** use a single
  `goto done;` (or `goto found;` for a different kind of early exit)
  cleanup label, setting a local `jaos_status st` and jumping rather than
  duplicating teardown at each failure point (`src/lu.c:489-660`):

```c
if (!jm_svec_push(&e.col[j], i, v) || !pat_push(&e.row[i], j)) {
    st = JAOS_ERR_OUT_OF_MEMORY;
    goto done;
}
```

- **Detail messages** live on the model, not on the return value: a fixed
  `char err[256]` field (`src/jaos_internal.h:140`), written through
  `jm_set_err(m, fmt, ...)` — a `printf`-style internal helper the compiler
  itself checks via `[[gnu::format(printf, 2, 3)]]`
  (`src/jaos_internal.h:334-335`). `jaos_model_error()` returns `""` when
  the last operation succeeded, never NULL.
- **A caveat that isn't a failure** (e.g. `scale_clamped`) is a boolean
  field on the model rather than routed through `err`, because it describes
  a successful call with a footnote (`src/jaos_internal.h:84-89`).
- **Overflow is checked, never trusted to wrap or truncate silently.**
  `<stdckdint.h>`'s `ckd_mul` guards every size computation that multiplies
  a caller-supplied count by an element size (`src/alloc.c:11-33`,
  `src/util.c:14-31`) — this is called out in the Makefile comment as "the
  C23 `<stdckdint.h>` payoff (DECISIONS.md, D1)".

## Memory Allocation Strategy

**All array allocation goes through two functions**, never raw
`malloc`/`calloc` at a call site for a caller-sized array
(`src/alloc.c`, `src/jaos_internal.h:287-291`):

```c
void *jm_alloc_array(int64_t n, size_t elsize);   /* uninitialized */
void *jm_calloc_array(int64_t n, size_t elsize);  /* zeroed */
```

Both reject `n < 0`, both overflow-check the `n * elsize` multiply, and
both guarantee **success is always non-NULL** — even `n == 0` returns a
valid pointer (`malloc(1)`/`calloc(1,1)`), so a NULL return unambiguously
means failure and a caller never has to special-case an empty array.

**Growable buffers** go through `jm_grow` / the `JM_GROW` macro
(`src/util.c:14-31`, `src/jaos_internal.h:293-307`) — capacity doubles
(minimum 16), overflow-checked, and on failure `*arr` is left untouched so
cleanup can still free the old block:

```c
#define JM_GROW(a, cap, need) \
    ((need) <= (cap) ? true : jm_grow((void **)&(a), &(cap), (need), sizeof *(a)))
```

**Plain `free()`** is used directly everywhere for teardown — no wrapper —
relying on `free(NULL)` being a no-op. `model_release_arrays()`
(`src/model.c:31-63`) frees every owned array unconditionally, then
`memset`s the struct to zero rather than freeing field-by-field-with-checks.

**Configuration survives a reload as one sub-struct, not a field list.**
`jm_config cfg` inside `jaos_model` is saved and restored as a whole around
`model_release_arrays`'s `memset`, specifically because a field-by-field
save/restore list has already been the source of two real bugs — the primal
tolerance and later the log callback were each found missing from such a
list (`src/jaos_internal.h:91-106`, `src/model.c:54-62`). **When adding a
new caller-facing setting, add it to `jm_config`, not to a hand-maintained
save list.**

## Determinism Rules (D8) — the rule that overrides normal C practice

This is the single most important convention in the codebase and the reason
several others exist. JAOS must produce **bit-identical results on every
machine and every run**:

- **No clock decides an outcome.** `jaos_solve_time` is the one number on
  `jaos_model` explicitly documented as non-reproducible, and nothing
  inside the solver may read it back (`src/jaos_internal.h:118-121`). Work
  units, not wall-clock time, gate iteration/refactorization decisions.
- **No reassociating floating point.** `-ffp-contract=off` is set in the
  Makefile and described as "load-bearing, not decoration" — without it the
  compiler may fold `a*b+c` into an FMA on a target that has one and not on
  one that doesn't, producing different bits on aarch64 vs x86-64 for the
  same model (`Makefile:52-58`).
- **No iteration order that depends on memory address.** Sparse patterns
  are canonicalized (`jm_pattern_order`) rather than iterated in bitmap/hash
  order, specifically because "every consumer of a pricing row breaks its
  ties by scan position" (`src/jaos_internal.h:256-284`).
- **No unseeded randomness anywhere**, including in tests. Randomised tests
  and the fuzzer use hand-written, explicitly seeded PRNGs —
  `xorshift64*` in `tests/test_lu.c:28-40`, `splitmix64` in
  `tests/test_fuzz.c:61-76` — never `rand()`, because libc's generator is
  not specified to produce the same sequence across platforms
  (`tests/test_lu.c:8-10`).
- **Scaling factors are exact powers of two** so applying one introduces no
  rounding error of its own (`src/jaos_internal.h:74-79`).
- A solver run is verified deterministic by hashing its own output (FNV-1a
  over the raw double bytes — see TESTING.md) and comparing two independent
  solves bit for bit, not just comparing rounded objectives.

## Decision-ID Citations — cite the record, don't re-argue it

Comments throughout `src/`, `include/` and `tests/` cite closed decisions
from the root `DECISIONS.md` in the form `(D47)`, `(D8)`, sometimes two
together (`D45, D46`). There are roughly 200 such citations in code (versus
~113 in the `.md` docs), which makes this the codebase's dominant comment
idiom. Examples:

```c
/* -ffp-contract=off in the Makefile is load-bearing, not decoration. */
```
```c
/* A divisor rather than a fraction so that the crossover can be swept by
 * changing one integer, which is how it was chosen (D40). */
```
```c
/* Two variables and one constraint are enough to build a point that is
 * arbitrarily suboptimal ... (D47). */
```

**What this means for new code:**

- **A constant or tolerance that isn't obviously derivable is expected to
  carry the decision that closed it** — either inline (`(D47)`) or by
  pointing at where the measurement lives (`PLAN.md 2.6`, `docs/tolerances.md`).
  A bare unexplained numeric literal in `src/` is the exception, not the
  norm; `src/simplex.c:40-160` is the canonical example of the expected
  density.
- **The citation is a pointer, not a summary.** Comments say *why this
  number and not another*, and send the reader to `DECISIONS.md` for the
  measurement rather than re-deriving the argument inline. Don't duplicate
  a `DECISIONS.md` entry's reasoning in a source comment — cite it.
- **`PLAN.md §N.N` citations** appear alongside `(D…)` ones for work that is
  still open or was scoped by the plan rather than closed by a decision
  (e.g. `src/simplex.c:40-41`, `"PLAN.md 2.6"`). Use `DECISIONS.md` for
  something settled and measured; `PLAN.md` for something still being
  built toward.
- Per project root `CLAUDE.md`: reasoning about a **closed** decision goes
  in `DECISIONS.md`; what's still **open** goes in `PLAN.md`; a feature's
  existence/absence goes in `SPECS.md`; `CHANGELOG.md` is a changelog, not a
  decision record. Comments in code should link to the right one rather
  than re-explaining it.

## Comment Style and Density

Comments in this codebase are **prose, not decoration** — full sentences,
often several per function, explaining *why* a design is the way it is, not
*what* the next line does. A representative density: `src/simplex.c` is
3616 lines and roughly a third of it is comment blocks; `include/jaos.h`
runs 733 lines for what is fundamentally a ~40-function API surface,
because nearly every function has a multi-paragraph doc comment explaining
the contract, the reasoning, and the edge cases (see
`include/jaos.h:164-186` for jaos_set_col_bounds as a typical example).

**Patterns that recur throughout:**

- **Rhetorical emphasis via bold-in-comment** (`**...**`) for the single
  most important sentence in a block — a habit carried over from the `.md`
  docs into C comments, e.g. `/* **The basis survives too, and re-solving
  starts from it.** ... */` (`include/jaos.h:180`).
  files.
- **A short header comment on every file** stating its one job in a
  sentence or two, before the SPDX line (`src/status.c`, `src/util.c:1-5`).
- **Struct/field comments explain the invariant, not just the type** —
  e.g. every field of `struct jaos_model` in `src/jaos_internal.h:46-141`
  carries what invalidates it, who writes it, and when it's dropped.
- **Section-divider comments** using a fixed-width rule of dashes, used to
  break a long internal header or source file into named blocks:

```c
/* --------------------------------------------------------------------- */
/* Deterministic work counter (D16)                                      */
/* --------------------------------------------------------------------- */
```

- **Measured numbers are stated, not asserted** — a comment claiming a
  behaviour costs or saves something nearly always gives the number and
  which instance/set it came from (e.g. `src/jaos_internal.h:299-302`: "On
  `fit2p` that discovery was 24% of every instruction the solver
  executed... (D55)"). An unmeasured claim reads as a red flag in this
  codebase, not a style choice — see `CLAUDE.md`'s rule that "every number
  needs a measurement on both sides."
- **JSDoc/Doxygen-style tags are not used.** Public API docs are plain
  block comments directly above the declaration in `include/jaos.h`, no
  `@param`/`@return` annotations.

## Function Design

- **Public API functions are thin**: validate arguments, delegate to an
  internal `jm_*` helper or mutate the model directly, return
  `jaos_status`. Most are 5-15 lines (`src/model.c:82-96`).
- **Internal kernels can be long and are not split for line-count's own
  sake** — `jm_dual_simplex` and the LU factorization routines run to
  hundreds of lines because the algorithm's control flow (phase 1/phase 2,
  refactorization triggers, ratio-test fallback to Bland's rule) is kept in
  one place rather than fragmented across many tiny functions that would
  each need the same shared state passed around.
- **`static` functions are file-local helpers** with a short, purpose-named
  identifier (no `jm_`/`jaos_` prefix needed since they're not visible
  outside the translation unit): `model_release_arrays`, `nmap_rehash`,
  `fnv1a` (`src/model.c:31`, `src/util.c:35,70`).
- Parameters follow a fixed order where a pattern recurs across the public
  API: `jaos_model *m` (or `const jaos_model *m`) first, then the entity
  being addressed (`col`/`row` index), then the data.

## Module Design

- **Exports are grouped by file around one responsibility**: `model.c`
  (lifecycle + CSR mirror), `simplex.c` (the dual simplex method only),
  `lu.c` (sparse LU factorization, standalone and reusable outside the
  simplex), `check.c` (the independent checker — deliberately depends on
  nothing from `simplex.c`, per D18), `scale.c`, `mps.c`/`lpfmt.c` (the two
  file-format readers), `alloc.c`/`util.c` (shared low-level helpers),
  `status.c` (enum-to-string only), `version.c`.
- **No barrel files** — there's a single flat `src/` directory; nothing
  re-exports another translation unit's symbols.
- **`jaos_internal.h` is the seam.** Anything two `.c` files need to share
  is declared there; nothing is shared via ad hoc `extern` in a `.c` file.

---

*Convention analysis: 2026-08-12*
