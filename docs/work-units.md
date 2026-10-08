# Work units

A work unit is counted in the kernels and never derived from a clock. The
same model consumes the same number of units on every machine, so
`jaos_set_work_limit` stops a solve at the same point everywhere. Read
`jaos_work_units` after a solve to see what it cost.

A simplex solve stopped by a work limit, a time limit or a callback parks
its state on the model: the factorisation, its update chain, the pricing
weights, the shifts and the phase. Raising the limit and calling
`jaos_solve` again continues from that state. An LP stopped and resumed
ends on the uninterrupted answer to the bit, work and iterations included
(`bench/measurements/02-247/`). A stopped solve has no answer to read, and
`jaos_basis` says so. An edit, a new basis, or a changed algorithm or
tolerance drops the parked state, and the next solve starts warm from the
basis the stop left. The barrier, PDLP and the conic interior point have no
basis to keep, so a run of one of them cut off by a budget starts again
from its starting point.

## The weights

Defined in `src/jaos_internal.h`. They are drafts until calibrated. The
definition becomes public contract at 1.0, and after that the ratios change
only at a major version.

| Constant | Weight | Event |
|---|---|---|
| `JM_WORK_NONZERO` | 1 | A nonzero touched in a solve, in pricing, or in an update |
| `JM_WORK_ELIMINATED` | 2 | A nonzero eliminated, in a factorization or in a basis update |
| `JM_WORK_UPDATE` | 64 | Fixed cost of one basis update |
| `JM_WORK_FACTOR` | 4096 | Fixed cost of one refactorization |

An elimination does the same axpy inside a factorization and inside a
Forrest-Tomlin update, so it has one weight in both.

The two fixed costs exist because both operations have an O(dim) floor
that does not depend on how much they change: three full passes in an
update, the setup in a factorization. Without them the budget would
promise a run far cheaper than the one it buys.

## Where it is charged

The kernels bill directly: `src/lu.c`, `src/chol.c`, `src/presolve.c`,
`src/aggregate.c`, `src/simplex.c`, `src/barrier.c`, `src/pdlp.c`,
`src/conic.c`, and one pass in `src/ranging.c`. The searches built on them
(the branch and bound, the conic tree, the concurrent solve, the IIS and
the feasibility relaxation) add up what their solves billed and charge
their own passes on top. Most charges follow one rule: one unit per
position touched.

**Presolve** (`jm_presolve_run`) charges `JM_WORK_NONZERO` per nonzero a
round visits while it computes a reduction: each entry of a column being
fixed, once, and each live entry of a row whose activity range is
computed. The range charge is paid on every live row of every round,
because the round reads the ranges to decide whether there is a reduction
at all (`bench/measurements/02-04/`). A round that finds nothing still
bills the whole live matrix once. The implied free column singleton pays
the range charge like every other reader of a row's activity. When it
fires it pays it a second time, because the substitution walks the row
again to move the eliminated column's cost onto the other live columns. A
candidate that is examined and declined pays once.

Presolve charges the same `jm_work` that the reduced model's solve then
continues: `jm_dual_simplex` seeds the simplex's accumulator with
presolve's total before `sx_init` runs. So `jaos_set_work_limit` sees one
total for the whole solve, and a solve stopped and resumed keeps that one
total. A work figure taken with presolve on cannot be compared with one
taken with it off, because the two solves billed different models.
Nothing else in `src/presolve.c` bills anything (see "What is outside the
budget").

**Aggregation** (`jm_aggregate`, `src/aggregate.c`) charges
`JM_WORK_NONZERO` per nonzero of the model it copies into its row and
column lists, per entry it reads while it tests a row's candidates, per
entry it looks up or changes while it substitutes a column, and once more
per nonzero of the reduced model it builds. It bills onto the same
`jm_work` as presolve. It runs on every continuous model solved cold by
the dual simplex outside a tree. It does not run on a model with a
starting basis, cones or quadratic rows. A solve that publishes a basis
stores it as the next start, so a second solve of the same model does not
aggregate. Where it runs and substitutes nothing, it still pays one pass
over the nonzeros, and a pass per candidate column over each equality row
of two to `AGG_ROW_MAX` entries. A solve that substituted something and
then ends with a numerical error is solved once more without the
aggregator, under what is left of the work and time limits. The failed
attempt's work and iterations are added to the answer's. When the failed
attempt ends with a complete basis, postsolve maps it to the model and the
second solve starts from it. The caller's own start comes back if the
second solve publishes no basis.

**Restarts inside the dual simplex** keep their work. A warm start that
reaches no answer restarts cold. A solve whose early cost perturbation is
followed by a full stall restarts from the same start without it. In both
cases the work of the abandoned attempt stays on the counter.

**Factorization** (`jm_lu_factor`): `JM_WORK_FACTOR` once on entry, plus
`JM_WORK_ELIMINATED` per nonzero the elimination produces.

**Triangular solves** (`jm_lu_ftran`, `jm_lu_btran`): `JM_WORK_NONZERO`
per entry visited (the entries of each L column used, each U column used,
and each Forrest-Tomlin eta). In the hyper-sparse form, which both
directions take on a sparse right-hand side, they also charge one per edge
the reach walk examines and one per word and per entry of the pattern
sort. Both directions charge the same way. The reach walk is billed,
although it replaces an unbilled pass over every slot, so on hyper-sparse
instances the counter can read slightly above the full pass while fewer
instructions run (`FTRAN_HYPER_DEN` in `tolerances.md`).

**Basis update** (`jm_lu_update`): `JM_WORK_UPDATE` for the floor, plus
`JM_WORK_ELIMINATED` per entry of the eliminated row.

**Pricing** (`src/simplex.c`): the pricing row `rho' M_v` charges the
nonzeros of column `v`: one for a logical, `nnz` for a structural. The
row-wise pass over `rho` charges `touched + nrow`, because it reads every
row whether it skips it or not. It reads the solver's own row-wise copy,
whose nonbasic entries come first in each row, and `touched` counts those
entries only. Keeping that copy in step charges the nonzeros of each
column that enters or leaves the basis, and rebuilding it after a new
basis charges `nnz + nrow`. The row scan that picks which infeasibility to
repair charges one per row. Scanning the candidates charges one per live
candidate, and the Harris two-pass over them charges two.

**Ordering the pricing row's pattern** charges one per position the
scatter recorded, one per bitmap word the read-back looks at, and one per
distinct position handed back. It is charged only on the iterations that
take the sparse path. Without this charge the counter would show a gain
for reading `alpha` through a pattern of any size, including one that
costs more than the scan it replaces.

**Ratio test and bookkeeping**: building the candidate set charges one per
variable it looks at. When the pricing row is read densely, that is the
nonbasic set, because the scan never reaches a basic variable. Otherwise
it is the size of the row's pattern. The dual update follows the same
rule, but its dense form walks every variable to read its status, so it
charges one per variable. It also sweeps every variable on the first
iteration after anything rewrites a reduced cost outside a pivot.

The steepest-edge weight update charges one per row. The exact weight that
feeds it charges one per slot it adds up, and each swap attempted while
settling up charges two per row. After a weight drifts, the dual prices by
Devex. The Devex update charges one per slot of the pivot row it reads for
the row's reference weight and one per row of the entering column it
updates, and its reset charges one per row. Devex solves no second column,
which is where it costs less than the steepest-edge update. A solve that
ends under Devex, or that perturbed its costs, refines the duals it
publishes once: one per nonzero of the basic columns for the residual, one
BTRAN, and one per nonzero of every column for the reduced costs.

The primal's steepest-edge weights are reset when the entering column's
carried weight has drifted past `DSE_DRIFT` from its exact one. A reset
writes the slack basis's weights at `nnz + rows`. In phase 2, after
`PSE_CHEAP_RESTARTS` such resets, the weights are rebuilt exactly for the
current basis instead: one FTRAN per variable, each billed by the solve
itself, plus one per row per variable for the squares.

**The primal phase 1.** Building the cost vector (`primal_phase1_costs`)
charges the number of positions the last call set, plus one per row it
scans to find this call's. At most `nrow` positions can be set, because
only a basic variable can be infeasible and a basis holds distinct
variables. Pricing charges one per variable, as phase 2 does. The ratio
test charges one per row, because phase 1 must know which basics would
cross a declared bound in either direction. A bound flip charges one per
row and nothing else, because no basis changes. The phase-1 duals are
billed inside `compute_duals`, like every other call of it.

**Ending a solve** is the largest single charge most solves make outside
the iterations. Optimality is not accepted on carried values. When the
loop believes it is finished, the point is recomputed from a fresh
factorization and priced again: one `JM_WORK_FACTOR` plus its
eliminations, two triangular solves and a pricing pass. On a small model
this can be most of the total, so it matters when choosing a small work
limit.

**Reading the unbounded verdict** charges an FTRAN and one per row for
each column still resting on a bound phase 1 lent it. Most solves charge
nothing here.

**The sparse Cholesky** (`src/chol.c`) bills four passes. It is the only
kernel outside `src/lu.c` that charges `JM_WORK_FACTOR`.

*The ordering* (`jm_chol_symbolic`, first pass) charges `JM_WORK_NONZERO`
per adjacency entry it reads: the variable and element lists of the pivot
when the new element is formed, and the element and variable lists of
every variable in that element when their degrees are recomputed. A scan
that skips a dead entry still pays for reading it.

*The symbolic factorisation* (same call, second pass) charges
`JM_WORK_NONZERO` per entry of the permuted upper triangle and one per
node of every row's reach in the elimination tree, which is one per
nonzero of `L` below the diagonal.

*The numeric factorisation* (`jm_chol_numeric`) charges `JM_WORK_FACTOR`
once on entry, `JM_WORK_NONZERO` per input entry gathered and per row of
`L` produced, and `JM_WORK_ELIMINATED` per multiply-add in the column
updates. That last term is the flop count of the factorisation and
dominates on anything but a tree.

*The solve* (`jm_chol_solve`) charges `JM_WORK_NONZERO` per entry of `L`
in each direction, the diagonal included, plus one per row for each of
the two permutations.

*The quasi-definite LDL* serves the barrier's augmented system and the
whole conic interior point. Its factorisation (`jm_ldlt_numeric`) charges
`JM_WORK_FACTOR` once, plus the entries gathered and the multiply-adds, as
the Cholesky does. Its solve (`jm_ldlt_solve`) charges `2 * nnz + 3 * n`:
one more per row than the Cholesky solve, for the divide by the diagonal.

*On threads*, the numeric factorisation works in blocks (`CHOL_BLOCK`) on
`--threads` lanes. Each lane counts what it gathered and eliminated, and
the counts are summed, so the charge is the one-thread charge at any
thread count.

**The barrier** (`src/barrier.c`) is billed on top of the Cholesky.
Forming the normal matrix `A Θ A^T` charges `JM_WORK_NONZERO` per
multiply-add (the sum over the columns of the square of their length),
plus one per entry of the pattern read out. A column with more than
`BARRIER_DENSE_FACTOR` times the average count is left out of that matrix.
The Sherman-Morrison-Woodbury correction that puts it back charges
`k * nr + k * dnz + k^3 / 6 + k^2` per factorisation and
`k * nr + dnz + k^2` per solve, for `k` dense columns holding `dnz`
entries. Every product with `A` or `A^T` charges one per nonzero plus one
per row, and a product with a quadratic objective's `Q` two per entry. The
augmented system, when the barrier takes it, is factored and solved by the
LDL above. Every sweep over the variables (the residuals, the scaling, the
two directions, the step lengths, the neighbourhood check and the update)
charges one per variable. Nothing is charged per iteration beyond what the
iteration touches.

*The QP push* (`qp_push`) finishes a quadratic model's point. Each round
factors and solves its own system at the LDL's rates and charges one per
entry it sweeps. Its sign test charges `2 * nvar`. When it settles, the
exact duals it sets on inactive rows and single-row columns charge
`nrow + ncol`, and the sign test that judges them another `2 * nvar`. Its
polish by conjugate gradients charges `6 * nr` per step, plus a product
pair with the rows and a solve with their factor per step, for up to
`QP_PUSH_CG` steps. The early hand-off at `BARRIER_MU_DEAD`, and the walk
that goes on after a push that does not settle, bill onto the same counter
as the walk.

*The crossover* charges the sort of the basis guess at one per variable
per pass of a comparison sort, `nvar * (2 + floor(log2 nvar))`, then the
LU factorisation of the guess at the factorisation's own rate, and two per
row for each repair pass that swaps an unpivoted position for a logical.
The push then moves every nonbasic column onto a bound. It charges
`nnz + nvar` to start, then for each column it moves: its entries, an
FTRAN at the LU's rate, `2 * nr` twice, and an LU update at the update's
rate or a refactorisation. The primal simplex finishes from the pushed
basis and is billed as any warm-started solve, on the same counter, so
`jaos_work_units` reads the whole path from the starting point to the
vertex.

**PDLP** (`src/pdlp.c`): every product with `A` or `A^T` charges one per
nonzero plus one per entry of the vector it writes, and each Ruiz round of
the preconditioning is one such pass. Every sweep over the iterates (the
step, the running averages, the restart test, the KKT error, and the ray
test every `PDLP_CHECK_EVERY` iterations) charges one per entry it reads.
The barrier's crash basis, billed as above, and the dual simplex from it
finish the solve. PDLP does not run the push.

**The conic interior point** (`src/conic.c`): each product with the matrix
charges one per nonzero plus one per entry written. Each sweep over the
iterate charges one per entry (twelve per variable and row for the step's
bookkeeping), and each pass of iterative refinement one per entry of the
system. The product with a cone's scaling block (`mul_h`) inside that
refinement charges two per row of the cone block, one read and one
written. The Newton finish charges `(CONIC_REFINE + 2) * u` per step,
where `u` is the entries of its system, whatever number of refinement
passes ran. It runs up to `CONIC_NEWTON_ROUNDS` times. When the checker
refuses its point, the settle step runs the same system for a projection
and a dual refit, up to twice, at the same rate. The scan for rows that
hold at every point of their columns' boxes charges one per nonzero when
it drops any. The ray polish charges `(it + 2) * (2 * at + n + nrow)` for
its conjugate gradients. The ray probe is a full LP solve, and the
sub-solves on a reduced model are full solves; the work of each is added.
Its factorisations go through `src/chol.c` and are billed there. An
infeasibility certificate the checker refuses is searched again before it
is given up. The coordinate climb makes at most `CONIC_CERT_CALLS` checker
calls. The tilt ladder before it makes at most 130 calls per outer round,
for two rounds. Each checker call charges one pass over the model,
`nnz + cols + rows + 1`.

**Ranging** (`src/ranging.c`) refactors the published basis and bills the
factorisation and its solves at the LU's rates, plus one unit per entry of
each pattern it walks.

**The branch and bound** (`src/mip.c`) has no kernel of its own. Each
node's relaxation is solved on the tree's copy of the model and billed as
any solve, and the tree adds that bill to its total. Every other pass it
makes (a cut separator, a heuristic, probing, the clique table, orbital
fixing, propagation) adds one unit per entry of the model it reads, and a
round of Gomory cuts adds the tableau rows it reads. Four passes read the
model more than once and bill that: a MIR round
`(nnz + nc + nr) * (MIP_MIR_DELTAS + 1)`, a clique round `nm * nm` per
clique of `nm` members, a stalled pump round `MIP_PUMP_FLIPS * nc`, and
orbital fixing `ngen * nfix + kept * nc`. The probe of a root round's
aggregated MIR cuts bills its copy of the root LP at `nnz + nr + nc` and
its solve as any solve. Symmetry detection (`src/symmetry.c`) searches
under a cap and bills what it spent of it. Under `--tree-batch N` above 1,
each node of a round is solved on its own copy of the tree's LP with
`(work_limit - work) / n` of the budget, and its work is added. The tree
then solves each node again from the round's basis, and that solve is
billed too. So the work at N above 1 differs from the work at 1
(`bench/measurements/02-290/`), but it does not change with the thread
count. `jaos_work_units` after a MIP solve is that total, and the work
limit applies to it.

**The conic tree** (`src/conictree.c`) adds up the work of every conic
solve it runs: the nodes, the rounding and the dive. Each solve gets the
budget that is left, and under `--tree-batch N` a round's nodes share it.

**The concurrent solve** (`src/concurrent.c`) bills the sum over its three
arms, in the rounds the one-thread schedule runs them, so the total is the
same at any thread count. A simplex arm stopped by its slice parks its
state and resumes from it in the next round. `settle_arm` bills a resumed
arm only what it added since the last round, and the next round's budget
caps the arm's whole walk. Under a work limit, a resumed arm may add at
most what is left of it.

**The IIS** (`src/iis.c`) adds the work of every re-solve it runs, and
each re-solve gets the whole work and time limit. **The feasibility
relaxation** (`src/relax.c`) adds the work of every solve of its elastic
copy, and caps every box round after the first at `RELAX_ROUND_WORK`
times the first round's work.

## What is outside the budget

**Model loading.** Reading a file or calling `jaos_load_lp` costs no
units. The budget is a solve budget, and a caller who loads once and
solves repeatedly should not see the load in every figure.

**Scaling.** A solve computes a Curtis-Reid scaling when the model has
none. That computation, a Jacobi-preconditioned conjugate gradient over
the matrix, is real work that no unit counts.

**The pricing row's clear.** The clear of `alpha` and the reset of its
basic entries are not billed. On an iteration that ends up reading
`alpha` densely, the part of a pattern it recorded and then discards (at
most a quarter of the variables) is not billed either.

**Presolve's bookkeeping and the reduced model.** The classification pass
that decides which column is fixed reads every column once per round,
whether it fires or not. Building the reduced model copies every surviving
column's nonzeros into a new CSC. Both are real work with no measured rate,
so neither is billed. A model presolve barely reduces pays nearly all of
this and is billed almost nothing for it.

**The nonbasic bitmap's words.** The dense candidate scan reads one
machine word per 64 variables to find the bits that are set, and only the
bits it finds are charged. On a model whose nonbasic set is a small
fraction of its variables, the scan pays `nvar/64` reads and bills almost
nothing.

**The clock.** A time limit is read once every 64 iterations in the
simplex and PDLP, once per iteration in the barrier and the conic interior
point, and once per node in both trees. It can stop a solve. It never
chooses a pivot. This is why the work limit and the time limit are
separate calls, and why only the work limit is reproducible.

## There is no per-iteration constant

An iteration is charged entirely through the events above: the nonzeros
its solves touch, the variables its bookkeeping sweeps, the rows its
pricing scans and the update it ends with. An attribution of every unit
to the phase that spent it (D32) showed that each part of an iteration's
cost scales with a dimension or a count of nonzeros, with no floor. A
fixed charge per iteration would bill a second time for work the counter
already sees. Where the instructions of a dual simplex solve go, function
by function, is read in `bench/measurements/02-281/`.

## Determinism

Every charge above is a fixed integer added at a fixed point in a
fixed-order loop. No charge depends on a value, a timing, an address or
an allocation, so two runs of the same model consume identical totals.
The test suite pins one model's total exactly, which catches a kernel
that stops charging.
