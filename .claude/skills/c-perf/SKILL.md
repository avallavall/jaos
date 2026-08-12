---
name: c-perf
description: Aggressive C performance work for a numerical library that must produce bit-identical results on every machine and every run. Load before optimising a hot path or proposing a compiler flag. Carries the forbidden list first, because most standard "extreme C optimisation" advice silently destroys reproducibility — then memory layout, killing quadratic scans, branch behaviour, deterministic sorting, allocation strategy and what GCC actually does with C23.
---

# Fast C without losing reproducibility

**Not this skill, if the algorithm is still open.** `sparse-simplex-perf` owns
the factor-of-N question — hyper-sparsity, pricing, the ratio test, presolve.
This one is the percentage question: how the C executes a decision already
made. Reaching for it first is the standard way to spend a week buying 3%.

This library guarantees that the same input produces the same answer and the
same search path, on every machine, every time. That guarantee disqualifies a
large part of the usual performance toolkit. Read the exclusions first — they
are not style preferences, they are correctness.

## Forbidden, and exactly why

| Technique | What it breaks |
|---|---|
| `-ffast-math`, `-Ofast`, `-funsafe-math-optimizations` | Reassociates and contracts floating point, and enables assumptions about NaN/Inf that are false in a solver. Destroys reproducibility and every error bound the tolerances rest on |
| `-ffp-contract=fast` — **which is the C default** | Fuses `a*b+c` into a single FMA wherever the target has one. Numerical kernels are made of exactly that pattern, so the identical source produces different bits on a machine with baseline FMA (aarch64) than on one without (x86-64 without `-mfma`). Compile with `-ffp-contract=off` and treat that flag as load-bearing |
| Hand-written SIMD reductions | A vectorised sum reassociates the additions, so it does not equal the scalar sum. Admissible only if it reproduces the scalar result bit for bit — which means a fixed number of accumulators combined in a fixed order, independent of the array length and the alignment |
| `-march=native` | Not wrong in principle, but it makes the binary's results a function of the build machine. If used, the target must be pinned and recorded, and results compared across architectures |
| Iteration order derived from an address | Hashing on pointers, traversing in allocation order, `qsort` on a pointer key. Two runs get different addresses and therefore different paths |
| Randomised algorithms and random restarts | Same input must take the same path. A PRNG is admissible only with a fixed seed that is part of the recorded state |
| Reading another solver's source to copy a technique | This project is written from published literature only — papers, theses, textbooks. That is stricter than the law requires and deliberately so |
| Adding a dependency to get a fast routine | Nothing outside the C standard library. Implement it here |
| Multithreading a hot loop | Parallelism is a separate, later design with its own determinism argument. Racing to a result is not a performance win here |

Everything below is admissible.

## Where the wins are, in order

For a sparse numerical kernel the ranking is not the usual one:

1. **Doing less work.** Algorithmic: exploiting sparsity so a solve touches
   the nonzeros it must rather than the whole vector. This dwarfs everything
   below, often by a factor, not a percentage.
2. **Removing quadratic behaviour.** An O(f²) scan on a hot path beats any
   constant factor you can win elsewhere.
3. **Memory traffic and layout.** In sparse kernels, cache misses dominate
   arithmetic by an order of magnitude.
4. **Branch behaviour.** Real, but a mispredict costs ~15 cycles against
   ~200 for a trip to DRAM.
5. **Instruction-level micro-optimisation.** Last, and rarely worth the
   readability.

Measure before assuming the ranking holds for your case.

## Killing quadratic scans: the position map

A structure that locates an entry by scanning a row or column pays O(f) per
lookup and O(f²) per pass:

```c
/* scan to find row i inside column j */
for (int64_t p = start[j]; p < start[j+1]; p++)
    if (index[p] == i) { ...; break; }
```

The fix is an auxiliary map from key to position. Two disciplines make it
safe rather than dangerous:

- **A cache that can be wrong is worse than a scan that is slow.** Prefer a
  map rebuilt at one well-defined moment — a refactorization, a compaction —
  over one patched at every write site.
- **Assert the invariant, do not comment it.** A dense `int64_t pos[n]` costs
  8n bytes; an assert in dev builds costs nothing in release. A comment
  saying "this must stay in sync" has a measured failure rate of 100% given
  enough time.

## Memory layout

- **Structure of arrays, not array of structures**, for anything iterated in
  bulk. A loop reading one field of a struct pulls the whole cache line and
  wastes the rest.
- **One arena beats many growable arrays.** Per-object `realloc` churn costs
  allocator time and destroys locality. An arena freed as a unit is both
  faster and simpler to reason about — when the lifetimes really are the same.
- **A cache line is 64 bytes** — eight doubles. A sparse gather that touches
  one nonzero per line is the worst case, and it is exactly what a sparse
  solve does. Which is why reducing the *number* of touches beats making each
  touch cheaper.
- **Do not clear a dense buffer over its whole dimension** when only part of
  it can have been written. That is pure bandwidth. If the narrow clear leans
  on an invariant, assert the invariant.
- **The touched-list scatter** is the standard sparse-accumulation idiom: a
  dense value array plus a list of the indices written, so clearing costs
  O(nnz) instead of O(n). If a codebase grows two or three variants of this
  for different callers, unify them — the divergence is where bugs live.
- **Read-mostly data separate from write-mostly data.** Even single-threaded,
  it improves line utilisation; under threads it is the difference between
  working and false sharing.

## Branches

- Predictable branches are nearly free. Spend effort on unpredictable ones.
- **Branchless selection** where both sides are cheap and side-effect free:
  `x = c ? a : b` becomes `cmov` under `-O2` when operands are in registers.
  Do not force it with bit tricks; measure, and prefer the readable form.
- `__builtin_expect` earns its place only on a branch that is both hot and
  reliably one-sided. Most are neither.
- **Hoist loop-invariant conditions out of loops.** A flag tested per element
  is a branch per element; tested once outside, it is one — and it lets the
  compiler specialise the body.

## Sorting, deterministically

- **Always impose a total order.** Comparing on a floating-point key alone
  leaves ties, and an arbitrary tie-break repeated at a degenerate point is
  how an iterative method cycles. Break ties on an index. This is not
  pedantry: it is the difference between terminating and not.
- **Comparison sort:** introsort (quicksort with a heapsort fallback and an
  insertion-sort cutoff) is the right default. Library `qsort` pays an
  indirect call per comparison; a hand-rolled sort over parallel arrays with
  an inlined comparator is measurably better on a hot path and no harder to
  get right if you test it.
- **Small n:** insertion sort below ~16 elements, sorting networks below ~8.
  Measure the distribution of n before choosing — hot sorts in solvers are
  very often tiny, and then everything above is irrelevant.
- **Radix sort on doubles** is available: flipping the sign bit for positives
  and all bits for negatives turns an IEEE double into a monotone unsigned
  key, so an LSD radix pass sorts in O(n) with a stable, deterministic order.
  Worth it only for large sets, and it needs the (key, index) pair to keep
  the total order.
- **Do not sort when you need only the extremum or the k smallest.**
  Quickselect is O(n); a single minimum is a scan. A great many "sort then
  take the first" loops are a scan wearing a costume.

## Allocation

- **Allocate once per solve, not per iteration**, and keep buffers across
  solves when the caller will re-solve the same model repeatedly. Growth by
  doubling, never by increments.
- `calloc` gets lazily-zeroed pages from the OS and can beat
  `malloc` + `memset` for large buffers that are written sparsely.
- Allocation failure must be a status, not an abort, in a library.

## Compiler leverage — GCC, C23

- **`restrict` on non-aliasing pointer parameters is the highest-value
  annotation in numeric C.** Without it the compiler must assume a store
  through one pointer may have changed what another points at, and reloads
  after every write. Only claim it where it is true; it is a promise, and
  breaking it is undefined behaviour that appears as a wrong answer under
  `-O2` and a right one under `-O0`.
- **`static` on every file-local function** enables inlining and dead-code
  removal, and shrinks the surface a reader has to hold.
- Prefer loops whose trip count the compiler can see. A data-dependent
  `break` blocks vectorisation — sometimes that is the right trade, and
  sometimes the condition can be hoisted or the loop split.
- `-O2` versus `-O3`, LTO and PGO are real and this project cannot yet
  measure them, because they move wall-clock time and not the deterministic
  work counter. They need a pinned measurement host. Until then they are
  proposals, not findings — and whatever else they do, **they must not move a
  single result digest.**
- `-g` in release costs nothing at run time. Keep it.

## Integer arithmetic

- Division is 20–40 cycles and does not pipeline. Strength-reduce a
  loop-invariant integer divisor. **Do not** replace a floating-point
  division by a reciprocal multiply: it changes the result bits.
- C23's `<stdckdint.h>` gives checked integer arithmetic. Index arithmetic
  over sparse structures is exactly where overflow hides, and it hides
  silently.
- Signed overflow is undefined and the compiler exploits it for loop
  induction. That is usually in your favour; never rely on wraparound.

## What a performance claim looks like

A per-instance table against the committed baselines on every instance set,
saying what improved, what regressed and where the cost landed. Load
`jaos-measure` before running anything. "It should be faster" is not a
finding, and neither is a summary line.
