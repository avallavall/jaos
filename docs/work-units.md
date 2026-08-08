# Work units

The currency of the reproducible budget (D16). A work unit is counted in
the kernels, never derived from a clock, so the same model consumes the
same number of units on every machine — which is what makes
`jaos_set_work_limit` mean something a wall-clock limit cannot.

Read `jaos_work_units` after a solve to see what it cost.

## The weights

Defined in `src/jaos_internal.h`. Drafts until calibrated (PLAN.md 2.7);
the definition becomes public contract at 1.0, and after that the ratios
change only at a major version.

| Constant | Weight | Event |
|---|---|---|
| `JM_WORK_NONZERO` | 1 | A nonzero touched in a solve, in pricing, or in an update |
| `JM_WORK_ELIMINATED` | 2 | A nonzero eliminated, in a factorization or in a basis update |
| `JM_WORK_UPDATE` | 64 | Fixed cost of one basis update |
| `JM_WORK_FACTOR` | 4096 | Fixed cost of one refactorization |

An elimination is charged by what it does, not by which routine runs it:
the same axpy costs the same inside a factorization and inside a
Forrest-Tomlin update. That is why there is one weight for it and not two.

The two fixed costs exist because both operations have an O(dim) floor
independent of how much they change — three full passes in an update, the
setup in a factorization — and a budget that ignored the floor would
promise a run far cheaper than the one it buys.

## Where it is charged

Everything below is in `src/lu.c` and `src/simplex.c`. Nothing else counts.

**Factorization** (`jm_lu_factor`): `JM_WORK_FACTOR` once on entry, plus
`JM_WORK_ELIMINATED` per nonzero the elimination produces.

**Triangular solves** (`jm_lu_ftran`, `jm_lu_btran`): `JM_WORK_NONZERO` per
entry actually visited — the entries of each L column used, each U column
used, and each Forrest-Tomlin eta. Both directions charge the same way,
which is why a BTRAN-heavy iteration is not cheaper than an FTRAN-heavy one
in the budget any more than it is on the machine.

**Basis update** (`jm_lu_update`): `JM_WORK_UPDATE` for the floor, plus
`JM_WORK_ELIMINATED` per entry of the eliminated row.

**Pricing** (`src/simplex.c`): the pricing row `rho' M_v` charges the
nonzeros of column `v` — one for a logical, `nnz` for a structural — and
the row scan that picks which infeasibility to repair charges one per row.
Scanning the candidates charges one per live candidate, and the Harris
two-pass over them charges two.

**Ratio test and bookkeeping**: building the candidate set charges one per
variable, the dual update charges one per variable, the steepest-edge
weight update charges two per row, and each swap attempted while settling
up charges two per row.

**Ending a solve** is the largest single charge most solves make outside
the iterations themselves, and it is worth knowing about before choosing a
work limit. Optimality is not accepted on carried values (D20), so when the
loop believes it is finished the point is recomputed from a fresh
factorization and priced again: one full `JM_WORK_FACTOR` plus its
eliminations, plus the two triangular solves and the pricing pass that
follow. On a small model that can be most of the total — a three-row model
in the test suite went from 4411 units to 8517 when this was introduced —
and on anything the size of a real instance it disappears into the noise.
It is charged rather than exempted because it is work the machine actually
does, and a budget that hid it would promise a run cheaper than the one it
buys.

**Reading the unbounded verdict** charges an FTRAN and one per row, for
each column still resting on a bound phase 1 lent it. Most solves charge
nothing here, because most models need no lent bounds and most that do are
not held by them at the end.

## What is outside the budget

**Model loading is not charged.** Reading a file or calling `jaos_load_lp`
costs no units, deliberately: the budget is a solve budget, and a caller
who loads once and solves repeatedly should not see the load in every
figure.

**Scaling is not charged either**, which is a smaller and less deliberate
statement. A solve computes a Curtis-Reid scaling when the model has none,
and that computation — a Jacobi-preconditioned conjugate gradient over the
matrix — is real work that no unit currently counts. It is stated here
because it is true, not because it was decided.

**The clock is never involved.** A time limit is read at most once every 64
iterations and can only stop a solve; it can never choose a pivot (D8).
That separation is why the two budgets are separate calls with separate
meanings, and why only one of them is reproducible.

## There is no per-iteration constant, and that is measured

An iteration is charged entirely through the events above — the nonzeros its
solves touch, the variables its bookkeeping sweeps, the rows its pricing
scans, the update it ends with. There is no fixed charge for the iteration
itself, and D32 is the measurement that settled it rather than an omission.

Two things came out of attributing every unit to the phase that spent it,
both worth knowing before choosing a work limit. **The basis update is 1.8%
of an iteration**, not the bulk of it: over the standard 94 and the 16
Kennington, the non-update work of an iteration runs from 4.4x the update's
cost on the smallest model to 1450x on the largest. And **more than half of
all work is the pricing row and the ratio test**, with another 27.5% in the
dual update and the steepest-edge weights — which come to exactly
`nvar + 2*nrow` per iteration, in every one of the 110 solves.

That last figure is why the constant is zero rather than small. Every part
of an iteration's cost scales with a dimension or with a count of nonzeros;
none of it is a floor. A fixed charge would bill a second time for work the
counter already sees.

| Quantity | Where the units go |
|---|---|
| pricing row and ratio test | 53.09% |
| dual update and steepest-edge weights | 27.52% |
| the two FTRANs of a pivot | 6.80% |
| the row scan that picks the infeasibility | 5.62% |
| refactorization and the refreshes | 5.07% |
| the basis update | 1.79% |
| everything outside the solve loop | 0.11% |

## Determinism

Every charge above is a fixed integer added at a fixed point in a
fixed-order loop. No charge depends on a value, a timing, an address or an
allocation, so two runs of the same model on the same input consume
identical totals — and the test suite pins one model's total exactly,
which is what catches a kernel that stops charging.
