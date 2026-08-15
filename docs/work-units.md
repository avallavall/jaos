# Work units

The currency of the reproducible budget (D16). A work unit is counted in
the kernels, never derived from a clock, so the same model consumes the
same number of units on every machine — which is what makes
`jaos_set_work_limit` mean something a wall-clock limit cannot.

Read `jaos_work_units` after a solve to see what it cost.

**A budget that stops can be started again (D70).** A solve cut off by a work
or time limit keeps the basis it stopped on, so raising the limit and calling
`jaos_solve` again continues from there instead of walking back from the slack
basis. There is no answer to read in between — the run did not produce one, and
`jaos_basis` says so, because a stopping point is not a solution. Until that
landed, a budget was a way to abandon work and nothing else, which is a strange
thing to have built a deterministic counter for.

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

Everything below is in `src/lu.c`, `src/presolve.c` and `src/simplex.c`.
Nothing else counts.

**Presolve is one-way, and this is the door (D-14, 02-02).** A work figure
read before presolve existed and one read after are not comparable on any
model presolve actually reduces — the model a solve billed then is not the
model a solve bills now. Every historical figure in `DECISIONS.md` and every
figure in the three committed baselines was taken before this paragraph
existed, on every one of the 26 standard-set instances 02-01's own reduction
already touches. Read them as figures about two different problems, because
on those instances they are. The rewrite that acknowledges this in the
baselines themselves is `02-07`'s own task, deliberately not a side effect
of landing this section.

**Presolve** (`jm_presolve_run`): `JM_WORK_NONZERO` per nonzero a round
actually visits while computing a reduction — the entries of a column being
fixed, visited once each while its cost and its matrix contribution shift the
rows it touches, and one per live entry of a row whose activity range is
computed. The range charge is the one that scales differently from the rest:
it is paid on every live row of every round, not once per reduction, because
the range is what the round reads to decide whether there is a reduction at
all (02-04). A round that finds nothing therefore still bills the whole live
matrix once, which is the honest figure — that scan is the work.

The implied free column singleton (D106) pays that range charge like every
other reader of a row's activity, and then pays it a **second** time when it
fires: the substitution walks the row again to push the eliminated column's
cost onto every other live column in it. Two passes over the same row, billed
as two, because two is what it does. A candidate that is examined and declined
pays once.

Charged onto
the same `jm_work` the reduced model's own solve then
continues (`jm_dual_simplex` seeds `sx`'s accumulator with presolve's total
before `sx_init` runs), so a caller's `jaos_set_work_limit` sees one total
for the whole solve and not two — the reason D-14 exists at all: an
inflated or omitted total is compared against the same budget that decides
where a solve stops, and phase 1 (D93's amendment) is the standing example
of what that costs when it goes silently wrong. Nothing else in
`src/presolve.c` bills anything; see "What is outside the budget" below for
what that leaves.

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
variable it looked at — the nonbasic ones when the pricing row is read
densely, because that scan walks the nonbasic set and never reaches a basic
variable at all (D93), and the size of its pattern when it is not (D40). The
dual update charges by the same rule and arrives at a different number: its
dense form still walks the whole range, reading every variable's status to
find out whether it has a cost to step, so every variable is one it looked
at. Two charges that no longer match, from the same rule applied to two
loops, one of which was taught to skip. The dual update also sweeps every
variable on the first iteration after anything rewrites a reduced cost
outside a pivot, because that sweep is repairing rather than stepping
(D41). The
steepest-edge weight update charges one per row, the exact weight that feeds
it charges one per slot it adds up rather than one per row (D42), and each
swap attempted while settling up charges two per row.

**Ordering the pricing row's pattern** charges one per position the scatter
recorded, one per bitmap word the read-back looked at, and one per distinct
position handed back. It is charged only on the iterations that take the
sparse path, and it is what makes that path's saving honest: without it the
counter would show a gain for reading `alpha` through a pattern of any size,
including one large enough to cost more than the scan it replaces.

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

**Neither pricing form bills its own sweep over the variables**, and that
predates D40: the clear of `alpha` and the reset of its basic entries are
real work no unit counts, in the row-wise form as in the column-wise one it
replaced. D40 makes the first of those much smaller on a sparse iteration
without making it visible. On an iteration that ends up reading `alpha`
densely it also records part of a pattern it then discards — bounded by a
quarter of the variables, and unbilled for the same reason the clear is.

**Presolve's own bookkeeping is not billed, and neither is building the
reduced model (D-14, 02-02).** The classification pass that decides which
column is fixed reads every column once regardless of whether it fires —
an O(rows + cols) floor per round, the same shape the two fixed factorization
and update costs above rest on, but with no rate chosen for it: this phase's
one round fires or it does not, and inventing a per-round constant from a
single round would be fitting a number to one instance, which is the
mistake this project's first rule exists to prevent. The reduced model's own
construction — the CSC prefix and the copy of every surviving column's
nonzeros into it — is a one-time structural cost and is unbilled for the
same reason: it is real work, it is not a reduction being computed, and no
measurement exists for what a rate on it should be. Both floors are real and
invisible in the counter the same way the `nvar/64`-read floor below is; a
model presolve barely reduces pays nearly all of this and is billed almost
nothing for it.

**The nonbasic bitmap's words are not billed either.** The dense candidate
scan reads one machine word per 64 variables to find the bits that are set,
and only the bits it finds are charged (D93). A word is not a variable, the
rule above is one per variable looked at, and a second currency for the
skipping would need a rate — which is a number with no measurement on either
side of it. The consequence is worth stating rather than leaving to be
discovered: on a model whose nonbasic set is a small fraction of its
variables the scan bills almost nothing while still paying `nvar/64` reads,
so the floor under that charge is real and invisible. It is stated here for
the same reason the unbilled `alpha` sweep above is.

**Pricing does bill its walk over the pricing row**, which is worth stating
because the charge is easy to misread. The row-wise pass charges
`touched + nrow`; the second term was one per logical column in the
column-wise form it replaced, and here only `nnz(rho)` logicals are written.
What it matches instead is the walk over `rho` itself, which reads every row
whether it skips it or not. On the Kennington set that single charge is 27%
of everything billed.

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
dual update and the steepest-edge weights — which came to exactly
`nvar + 2*nrow` per iteration, in every one of the 110 solves. D41 replaced
the `nvar` in that with the size of the pricing row's pattern on the
iterations that have one, so the sum is no longer fixed by the dimensions;
what has not changed is that every term of it is still a dimension or a
count, which is the point the figure was making.

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

Those shares are as of D32 and predate D40, D41 and D93. The first two took
the ratio test's candidate scan and the dual update off the first two rows
wherever the pricing row is sparse — 1.895x less total work on the
Kennington set — and D93 takes that same scan down on the iterations where
the row is read densely, which are exactly the ones the other two do not
reach. The ranking has certainly moved; the figures are left as measured
rather than rescaled by arithmetic, and the next attribution run replaces
them.

## Determinism

Every charge above is a fixed integer added at a fixed point in a
fixed-order loop. No charge depends on a value, a timing, an address or an
allocation, so two runs of the same model on the same input consume
identical totals — and the test suite pins one model's total exactly,
which is what catches a kernel that stops charging.
