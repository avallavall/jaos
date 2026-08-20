# 02-79 — the published objective was neither summed carefully nor summed over the point it came with

D169. One new function in `src/model.c`, called from the three places a
solution is published.

## Two defects, and `jaos.h` states the promise both of them broke

> *Objective value of the solution held by the model, including the constant
> term.*

**1. The sum was naive.** `c'x` is a sum of many terms of differing magnitude
taken in column order, so a large term arriving before many small ones drops
all of them.

**2. It was not a sum over the point the caller reads.** The two presolve
postsolve paths reported the REDUCED model's objective, or the accumulated
offset alone. Neither is a sum over the values in `orig->sol_col`.

## The constructed model — `objective.txt`

```
costs in column order:  +1e16,  1 (k times),  -1e16
every column fixed at 1, one row holding them all and binding nothing
```

The answer is `k`. One ulp of 1e16 is 2, so each of the `k` ones is below half
an ulp of the running total and none of them moves it; the `-1e16` then brings
it to zero.

| | k = 64 | k = 256 |
|---|---|---|
| parent `ba69a88`, shipping build | **0** | **0** |
| parent, `-DJAOS_NO_PRESOLVE` | **0** | **0** |
| D169, both builds | 64 | 256 |
| `jaos_check_solution`, all four | 64 | 256 |

The checker accumulates in `long double` and is printed beside it on every
line: **the same library answered the same question about the same point two
ways, and the two disagreed by 100%.**

## The 94 — `objective-vs-checker.txt`

The checker is the oracle for this: it judges the model as loaded, in the
original space, independently of any solver bookkeeping. So the measurement is
`|jaos_objective − jaos_check_solution's primal_objective|` on the same
published point.

| | parent | D169 |
|---|---|---|
| **exact agreement with the checker** | 34 of 94 | **81 of 94** |
| closer | — | 57 |
| further | — | 4 |
| unchanged | — | 33 |

The four that move the wrong way are `sierra` 0 → 1.86e-09 (1.2e-16 of the
objective), `kb2` 0 → 2.27e-13, `afiro` 0 → 5.68e-14 and `tuff` 0 → 5.55e-17.
All four are at the last bit, and each is a naive sum whose errors happened to
cancel onto the long-double value.

**Against the manifest's published reference the same 94 read closer on 54,
further on 8, and exact on 53 against 21.** That is a weaker measure than the
one above and is reported second on purpose: the reference is the true optimum
of the model, which the published point is only near, so agreement with it
mixes this defect with how good the point is. `finnis` is where the two
measures disagree — see below.

## What is left, and what it is — `split-the-error.txt`

`finnis` has the worst cancellation in the set: the terms sum to 1.7e5 while
their magnitudes sum to 3.2e12, and the largest single term is 6.5e11. Four
numbers for the same point:

| | `finnis` |
|---|---|
| naive `double` | 172791.0657497762 |
| Neumaier `double` (what D169 publishes) | 172791.06569834377 |
| the same ROUNDED products, added in `long double` | 172791.06569833743 |
| the products themselves in `long double` (the checker) | 172791.06567182826 |

**The accumulation is now exact to 6.3e-09 on the worst instance in the set**,
against 5.1e-05 for the naive sum. What is left, 2.65e-05, is entirely the
rounding of each `c_j * x_j` to a double: one term of 6.5e11 rounds by up to
7.2e-05 on its own. **No accumulator can reach that** — it needs a two-product
(Dekker's split, or `fma`), which is its own change with its own decision, and
it is written up in `TODO.md` rather than done here.

This is also why `finnis` reads *further* from the manifest reference (4.93e-05
→ 1.03e-04) while being *closer* to the objective of its own point. The
parent's number came from the reduced model and was nearer the true optimum by
luck; `finnis` publishes a point with `row = 8.44e-07` and `gap_positive =
1.05e-04`, and the objective of that point is what D169 reports.

## The gate — `gate-diff.txt`

`make netlib netlib-infeas netlib-kennington J=12`, all three `gate: PASS`
with `0 regressed, 0 improved, 0 new`, 139 of 139 `objective=ok checker=ok`.

| set | instances | bit-identical | moved | **digest changes** |
|---|---|---|---|---|
| netlib | 94 | 32 | 62 | **0** |
| netlib-infeas | 29 | 29 | 0 | **0** |
| netlib-kennington | 16 | 3 | 13 | **0** |

**Every moved instance moved on `obj` and on nothing else.** No work unit, no
iteration count, no digest, no basis. `bench/run.c`'s digest covers x and y, so
zero digest changes across 139 instances is the statement that this change
touched the reported objective and no part of the solve — the cleanest
attribution available here.

## The tests, and what makes them evidence

Both are in `tests/test_model.c`, which is where the objective's contract
lives.

- `test_the_objective_is_summed_from_the_values_it_publishes` is the model
  above at k = 256, asserting the exact bits of 256. Built against the parent's
  `src/` it **fails on both the shipping and the reference build**; with D169
  it passes on both.
- `test_the_objective_keeps_its_constant_term_and_its_sense` is the control,
  and it is the one a compensated sum can get wrong: a MAXIMIZE model with an
  objective constant of 100, asserting 112 exactly and asserting the same
  number again recomputed from the published `x`. Dropping the offset or
  minimising by accident shows up here and would move nothing in the gate. It
  passes on the parent and on D169, which is what a control does.

**Two positive tests started failing under `-DJAOS_PRESOLVE_FAULT_OFFBYONE`
and the diff guards them.** That is the fault build doing its job: it corrupts
a postsolved value, and until D169 the objective did not read those values, so
tests designed to be broken by it passed. `solved_objective` in
`tests/test_model.c` is guarded at the helper, the way `solve_and_verify` is in
`tests/test_simplex.c`; `test_a_maximised_empty_column_is_not_unbounded_downwards`
is guarded with its now-unused model builder.

`make configs` exits 0 — all five configurations build and pass.

## What this does not close

- **The product rounding above.** It is the dominant term now and it is
  bounded by `eps` times the sum of the term magnitudes, which is 7.1e-04 on
  `finnis`. A two-product would remove it.
- **`settled_objective` in `src/simplex.c` is a fourth accumulation of the same
  shape** and is untouched. It compares the objective across rounds inside the
  solve and never reaches the caller, so it decides a trajectory rather than a
  published number. Left for its own model.
- **presolve's `obj_offset` is still accumulated naively** as columns are
  removed. Nothing reads it for the answer any more — both postsolve paths now
  sum the published values instead — but it is still the number
  `p->reduced.obj_offset` carries, and the simplex's own objective on the
  reduced model starts from it.
