# 02-82 — the per-term rounding a compensated sum cannot reach, and Dekker's split

D172. One source change, in `src/model.c`: the published objective recovers
what each `c_j * x_j` lost when it rounded.

## What was left after D169

D169 made the objective a compensated sum, which removed the ACCUMULATION
error. Each product is still rounded once before it is added, and **no
accumulator reaches an error that is already inside a term**. The bound is
`eps` times the sum of the term magnitudes.

## The minimum model — `two-product.txt` §1, `minimal-model.c`

Two columns, one row, and it needs no instance:

```
c0 = 2^27 + 1,  x0 fixed at 2^27 + 1     exact product 2^54 + 2^28 + 1
c1 = -1,        x1 fixed at 2^54 + 2^28   exact product -(2^54 + 2^28)
```

One ulp at 2^54 is 4, so the first product rounds to `2^54 + 2^28` and leaves a
residue of exactly 1. **Every accumulator over the rounded products therefore
sums to 0, however carefully it adds**, and the true objective is 1.

| | shipping build | `-DJAOS_NO_PRESOLVE` |
|---|---|---|
| parent `39a49f6` | **0** | **0** |
| `jaos_check_solution`, both | 1 | 1 |
| D172 | **1** | **1** |

The library disagreed with itself, and the disagreement was the whole answer.

## Where the error was — `two-product.txt` §2

`finnis` is the worst cancellation in the set: the terms sum to 1.7e5 while
their magnitudes sum to **3.2e12**, and the largest single term is 6.5e11. Four
numbers for one point, at the parent:

| | `finnis` |
|---|---|
| naive `double` | 172791.0657497762 |
| Neumaier `double` (what D169 published) | 172791.06569834374 |
| the same ROUNDED products, added in `long double` | 172791.06569833743 |
| the products in `long double`, rounded to 64 bits (the checker) | 172791.06567182826 |

The accumulation was already exact to 6.3e-09. The 2.65e-05 that remained is
entirely the per-term rounding: one term of 6.5e11 rounds by up to 7.2e-05 on
its own.

## The repair, and why Dekker rather than `fma`

Dekker's split, with `SPLIT = 2^27 + 1`. **What keeps it exact is
`-fno-associative-math`, the default, and NOT `-ffp-contract=off`** —
measured across six flag sets, contraction on or off gives identical bits,
because every product inside the split is exact and a fused multiply-add
rounds to the same value the separate operations do. What would delete
`ca - (ca - a)` is reassociation, which only `-ffast-math` and `-Ofast`
enable and the Makefile uses neither (`numerics-reviewer`). Beyond a factor magnitude of `2^996` the split itself
would overflow, so the function reports a zero residue there and the sum falls
back to the plain product it already had; `SPLIT * 2^996` is `2^1023`, one
binade under `DBL_MAX`.

**`fma()` would also work and the flag is not what rules it out**
(`numerics-reviewer`, D169): `-ffp-contract=off` stops the COMPILER contracting
`a*b+c` on its own and says nothing about an explicit call, which IEEE-754
requires to be correctly rounded and which is therefore deterministic across
machines in a way `log` and `exp` are not. The split is preferred because it
needs no claim about libm at all, and D34's list is about what this tree may
rely on.

## The 110 — `two-product.txt` §3

`|jaos_objective − jaos_check_solution's primal_objective|` on the same
published point, which is `jaos.h`'s promise measured.

| | parent | D172 |
|---|---|---|
| **netlib, exact agreement** | 83 of 94 | **93 of 94** |
| netlib: closer / further / unchanged | — | 11 / **0** / 83 |
| **Kennington, exact agreement** | 15 of 16 | **16 of 16** |
| Kennington: closer / further / unchanged | — | 1 / **0** / 15 |

**Nothing moves the wrong way on either set.** D169's own reading had four
netlib instances going further at the last bit, which is what a compensated sum
does when a naive one got lucky; recovering the products leaves nothing for
luck to do.

**The one that is left is `finnis`, at 2.2992e-08, and it is below the
oracle's own floor.** `(long double) c * x` is **not** an exact product: a
binary64 product needs 106 bits and a `long double` mantissa holds 64, so each
term carries up to `2^-64 |t|`. Over `finnis`'s `sum|t| = 3.2e12` that is
**1.73e-07** from the product roundings alone, and `src/check.c` then sums
those naively, which over 614 columns is worse again. The observed 2.2992e-08
is 7.5 times below the tighter of those floors, so **the comparison is
exhausted**: neither number can be called more right than the other
(`numerics-reviewer`).

**And the better oracle says the remaining `finnis` gap is the POINT, not the
sum.** D172 moved 0 digests, so the published `x` is bit-identical and every
`obj` move is pure summation. Against Koch's exact-rational optima the eleven
netlib movers read **8 closer, 3 further, and exact matches 3 → 6** — `25fv47`,
`80bau3b`, `afiro`, `maros`, `ship04l` and `truss` now match the reference to
the last bit, while `bandm`, `scagr25` and `tuff` each went from exact to about
one ulp, which is not a cost. `finnis` goes 1.027e-04 → 7.624e-05: improved by
25% and still **2.6 million ulps**, where `ulp(172791.06) = 2.9e-11`. A
compensated sum of exact products cannot leave that behind. **The published `x`
is not the exact optimal vertex**, which is a different question and belongs in
`TODO.md`.

## The gate — `gate-diff.txt`

`gate: PASS` on all three sets with `0 regressed, 0 improved, 0 new`, 139 of
139 `objective=ok checker=ok`, 29 of 29 correctly refused.

| set | instances | bit-identical | moved | **digest changes** |
|---|---|---|---|---|
| netlib | 94 | 83 | 11 | **0** |
| netlib-infeas | 29 | 29 | 0 | **0** |
| netlib-kennington | 16 | 15 | 1 | **0** |

Every moved instance moved on `obj` and on nothing else — no work unit, no
iteration count, no basis. The diff writes `m->objective` and nothing else, so
this confirms the reasoning rather than discovering anything, which is the
right shape for a change of this kind.

## The test

`test_the_objective_recovers_what_a_rounded_product_dropped` in
`tests/test_model.c` is the minimum model above, asserting the exact bits of
1.0 and asserting that `jaos_check_solution` reads the same number off the same
point. Built against the parent's `src/` it **fails on both the shipping and
the reference build**; with D172 it passes on both. It also asserts that
`big * big` really does round to `prod`, because the model only says what it
says while that holds.

`make configs` exits 0 — all five configurations.

## What this does not close

- **`settled_objective` is still a naive sum**, and D169's review corrected its
  severity: `take_best_if_better` restores the saved best point and `publish()`
  writes it, so that sum selects **which point gets published**. It is not
  reached by this change, which repairs the objective only after the point is
  chosen.
- **presolve's `obj_offset` is still accumulated naively.** Nothing reads it for
  the answer since D169.
