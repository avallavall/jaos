# 02-120 — `pilot87`'s phase-1 refusal is an absolute pivot floor reading FTRAN noise

## The question

`pilot87` is the one instance of the standard 94 whose forced-primal solve
ends in the solver's own words: *"column 478 prices at 0 in row 790 of the
primal phase 1 on a freshly built factorization; this is a JAOS defect"*. Two
computations of the same number, `(B^-1 A_q)_r`, disagree: the ratio test read
it off the FTRAN column and accepted row 790 as blocking; `build_pricing_row`
read it off the BTRAN pricing row and found it below `PIVOT_MIN`. On a fresh
factorization nothing can be rebuilt, so the solve refuses. Which of the two
is wrong, and why?

## Step one — `diag-pilot87.sh`, `diag-pilot87.txt`

The two numbers at the refusal, plus a census of `|alpha[q]|` over every
phase-1 iteration that reached the pricing row:

| iteration | `col[r]` (FTRAN) | `alpha[q]` (BTRAN) | `n_updates` |
|---|---|---|---|
| 7873 | -1.03e-09 | -8.37e-10 | 10 |
| 17165 | **-1.59e-07** | **0** (exactly) | **0** |

The first is an edge: both sides agree to 24% and the threshold splits them;
the factorization had updates, so the rebuild path recovered it. The second is
the refusal. Over 14785 phase-1 iterations the census shows **one** exact
zero, this one. An exact zero from a dot product of doubles is a term that was
never added, or a structural zero. Not rounding. `ganges`, the control, shows
neither.

## Step two — `diag2-pilot87.sh`, `diag2-pilot87.txt`

At the refusal, with its own buffers: ρ recomputed by the dense BTRAN, column
478 recomputed, and the three ways of forming `alpha[q]` compared.

```
rho_nonzero=1  rho_not_in_pattern=0  rho_dense_vs_sparse_differ=0
col_nonzero=12 dropped_terms=0
alpha_dense=0  alpha_over_pattern=0  alpha_from_sparse_rho=0
```

**Row 790 of B⁻¹ is a singleton, the sparse pattern is complete, and none of
column 478's twelve nonzeros sits on that row.** So the pricing row's zero is
structurally exact: there is no term. The hyper-sparse BTRAN (D38) is not at
fault and neither is `price_all`'s row walk (D35).

That leaves the FTRAN. `(B^-1 A_q)_790` is exactly zero and the triangular
solves returned -1.59e-07 for it: a rounding residue on a structurally zero
entry, on a column whose terms are of order 1e9 (D86 measured `pilot87`'s
dual steps at 1e213 on a stale factorization; this one is fresh, and 1e-7 is
what one ulp of 1e9 looks like). The residue is 160 times `PIVOT_MIN`.

## What the defect is

`primal_phase1_ratio` and `primal_ratio_test` accept a row as blocking when
`|col[i]| >= PIVOT_MIN`, an **absolute** 1e-9. A residue that scales with the
magnitudes in the column passes that test whenever the column is large. The
pricing row then reports the truth, the two disagree, and the refusal fires on
a pivot that was never there.

The dual side already knows this shape: `can_move` rejects a breach below
`NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v)`, a floor relative to the
terms that produced the number. The primal ratio tests have no such floor.

## What is not concluded here

Not the repair. A relative floor in the two primal ratio tests is the shape,
and its constant needs a sweep on both sides (the rule every number here
follows); the candidate must be measured on the primal campaign and the gate
before it is believed, and `jaos-measurer` judges it. `TODO.md` section 0
carries it. Nothing in `src/` changed for this record.
