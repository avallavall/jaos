# 02-272 — the barrier picks its system by the factor's cost

`TODO.md` row 4 said QPLIB's large convex QPs "fill badly: one iteration of
QPLIB_8785 (10399 columns) costs 2.8e10 work units, its factor 9.1 million
nonzeros. The augmented system costs 1.6e9 there ... so a choice of system
by the symbolic factor's size would pay on some."

## Why the choice was live

`src/barrier.c` takes the augmented system when the model has a quadratic
objective in `q_nz`, and the normal equations otherwise. A diagonal
quadratic objective is held in `col_quad`, not in `q_nz`, because it folds
into the normal matrix's diagonal. So a model like QPLIB_8785, whose `Q` is
diagonal, went to the normal equations and paid their fill.

## The change

When the model has a diagonal quadratic objective and the normal matrix's
factor costs at least `BARRIER_AUG_TRY` operations, the barrier also builds
the augmented system's pattern and keeps the cheaper of the two. The cost
read is the factor's operation count, `Σ_j h_j²` over the columns' heights,
not its nonzero count. The loser's factor and pattern are freed. The
symbolic work of both is billed.

## The reading

QPLIB's convex QPs at a work limit of 1e11, the ones `TODO.md` names.

| instance | before | after | system |
|---|---|---|---|
| QPLIB_8785 | 7 iterations | 59 | augmented |
| QPLIB_10038 | 8 | 16 | augmented |
| QPLIB_10034 | 84 | 169 | augmented |
| QPLIB_8500 | 2 | 2 | normal |

All four still reach the limit. QPLIB_8785's factor falls from 9081630
nonzeros to 3029103 and its cost per iteration from 2.8e10 work units to
1.7e9.

`make maros-meszaros`: 0 regressed, 0 improved, 0 new over the 138.
`make test` and `make sanitize` pass. A model with no quadratic objective
never reaches the second pattern, so every LP is untouched.

## Two readings that set the constants

**The cost has to be the operation count, not the nonzero count.** With the
nonzero count, `make maros-meszaros` regressed 6 instances, boyd1 85.7x.
Its normal factor holds 132 nonzeros in 1.13e3 operations and its augmented
one 4.67e6 operations, and the nonzero count picked the wrong one.

**The second analysis has to be earned.** With the operation count and no
floor, the same 6 regressed and boyd1 kept the normal system anyway: the
regression was the second symbolic analysis itself, 2.0e10 work units
against the 2.4e8 the whole solve costs. `BARRIER_AUG_TRY` stops the
barrier from looking when the normal factor already costs less than 1e8
operations, and the gate is then clean.
