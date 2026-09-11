# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

## Milestone: reach and polish

0. **`relax --cols` does not finish on a model with no integer point.**
   The elastic copy frees every column, so an integer column freed that way
   hands the tree an unbounded space, and a model whose rows plus
   integrality admit no point at all has to exhaust it to say so. The node
   count grows with the width of the box.
   `relax --work-limit N` stops the runaway and the copy ends `work_limit`,
   which `jaos_feasrelax` reports as a refusal with its reason. That is an
   escape hatch and not the fix.
   **Propagating the rows onto the freed columns does not fix it.** Read
   off the model below. `r3` is a singleton and fixes `x3` at 2. After
   that every remaining row still holds two or more freed columns, so no
   finite bound follows from any of them. What refuses the model is parity:
   substitute `x2` out and `r1` becomes `6 x1 + 4 x4 = -19`, whose left
   side is even for every integer pair. The tree does not see that.

   The fix: give the freed columns a finite box and grow it. Hold every
   freed column in `[lo - M, hi + M]` and solve. When the total move `V`
   comes out at or below `M`, that `V` is the answer for the free box too.
   Any point cheaper than `V` would have to hold a column more than `M`
   outside its own box, and that alone costs more than `M`. Otherwise
   double `M` and solve again. Every round then ends, so a work limit stops
   one bounded search.

   Start `M` from the elastic copy solved with the integer marks dropped.
   That value is a lower bound on `V`, so `M = max(1, 2 V_lp)` usually ends
   in one round. A model with no integer column keeps the free box and its
   single solve, so no LP answer moves.

   A model whose rows plus integrality admit no point still never
   terminates. That stays a limit of `relax --cols`, and SPECS row 104
   already says it.

   It changes `relax.c` only, so no gate applies. It needs a reading over
   generated models: no answer may move, and the cost of the extra rounds
   has to be read.
   Model: `tests/data/relax_runaway.mps`. Reading: 980c565.

0b. **A resume does not follow the path the uninterrupted run took.**
   Two runs at the same work limit agree exactly, which is what
   `docs/cli.md` promises. A run stopped and then finished does not always
   reach the answer the straight run reaches. Nothing documented is broken;
   what is missing is the stronger property a caller expects, that a stop
   does not change the answer.
   The cause is the re-entry: `sx_init` builds the factorisation again and
   the pricing weights with it, so the walk after the stop is not the walk
   that would have happened.
   The fix: carry the state across the stop — the LU, its update chain, the
   steepest-edge weights and the Harris pass's state. It changes
   `simplex.c`, so it needs the four gates. Measure the cost of holding that
   state as well as the benefit, because a solve that never stops pays it
   too.
   Reading: 4738ebd.

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.

3. **Primal simplex: 5 of the 94 standard instances run past 10x the dual's
   work** (`bench/results/primal.txt`): d6cube, dfl001, fit1d, fit2d, seba.
   None disagrees. The SPECS row stays partial until the count is zero.

   **Do not re-measure these.** Five ideas are refused with their reopen
   conditions in `bench/refusals.txt`: `primal-bound-perturbation`,
   `primal-tie-hash`, `primal-cost-perturbation`, `primal-noise-floor`,
   `primal-expand-step`. Steepest edge, Devex and Dantzig are all already
   read over the set; the SPECS row records them. Cost per iteration was
   read and paid 7.6% over the set for nothing here, because the gap is the
   iteration count.

   **The five are two faults, not one**, so a remedy aimed at either reads
   as noise over the set unless it is measured on its own group (d5a43e9).

   - d6cube and degen3 stand still. d6cube holds one vertex for 8508
     consecutive phase-2 bases. They need a remedy that leaves a vertex.
     EXPAND's growing tolerance schedule and its periodic reset are the
     named candidate; the step alone is measured and halves d6cube without
     clearing the overrun (77dcabf), and neither the schedule nor the reset
     is built.
   - seba and fit1d do not stall at all. They leave for a new point at
     nearly every pivot, so degeneracy is not what holds them and the
     target is the entering column. What holds them is still unnamed.

   The walk revisits no basis, so there is no cycle for an anti-cycling
   rule to break, which is why Bland's rule never pays here (f954aee).

4. **`ranging` and `verify` solve the model before they refuse it.**
   `jaos_cost_ranging` refuses a MIP and a QP by name before it needs an
   optimum, which is right. `cmd_ranging` in `cli/jaos.c` calls
   `solve_for_report` first and only then calls the library, so
   `jaos ranging` on a MIP pays for the whole tree to be told the command
   does not apply. `cmd_verify` has the same order.
   Under either presolve fault build the tree on `tests/data/g_sos.mps`
   never settles, so the command runs for ever. That was one of the four
   reasons `make configs` could not pass, and it is the only one this
   session did not close: `tests/cli.sh` skips the check under a fault
   build until this lands.
   Measured 2026-09-10: the hang is the same at 74005b9 and at its child,
   so it is older than that session.
   The fix needs a decision first. The CLI cannot ask the library whether a
   model is a MIP, because `jm_model_has_integer` is internal. Either
   publish that question as a call, or have the CLI ask
   `jaos_num_sos`, `jaos_col_integer` and `jaos_col_semicontinuous` itself.
   The second repeats the rule that `semi_live` holds, that a
   semi-continuous column counts only where its lower bound is above zero,
   and a copy of a rule is what let the OSiL defect of 02-226 through.

5. **The pool can hold one point twice when a continuous column differs in
   the last bits.** 02-227 closed the two ways an exact duplicate got in.
   Four pools of 24000 still hold two entries whose integer columns agree
   exactly and whose continuous column does not:

       x[4]  1.5000000000000266  against  1.5000000000000178
       objectives 9.5000000000001332 and 9.5000000000000888

   A caller who asked for the two best points got one point twice, so it is
   the same complaint as the two that are fixed. The fix is not the same,
   because `spool_offer` cannot tell one vertex reached twice from two
   vertices of one optimal face without a tolerance, and JAOS has not got a
   constant for "these two points are the same point".
   What it needs, in order: decide whether identity in the pool is the
   integer assignment alone or the whole point; if the whole point, set the
   tolerance from a measurement and write it into `docs/tolerances.md`; then
   compare on it. Deciding by the integer columns alone is the cheaper
   answer and it matches what `docs/cli.md` promises, "the K best integer
   points", but it makes two vertices of one optimal face one entry, and a
   caller reading the pool for a spread of answers wants both.
   Reading: `bench/measurements/02-227/`.

6. **Twenty-two SPECS rows say `done` and say nothing else.** A row with an
   empty description is a feature nobody has written down, and once on
   2026-09-10 it was also a feature nobody had read. Row 71, the solution
   pool, held one point twice (02-227). Rows 54 and 119, the basis file and
   the solution file, came back clean (02-228). Rows 45, 51 and 116,
   postsolve to the caller's indices, copy a model and direct load from
   arrays, came back clean (02-229). One of the six was worth the sweep.
   The shape that works: pick a row, write down the properties its answer
   must satisfy, generate models, and check them. Then fill the row in with
   what the feature is, and break the code on purpose to prove the sweep
   would have seen it.
   **A fault build is not always a control.** Under
   `JAOS_PRESOLVE_FAULT_OFFBYONE` the library dies on the second model
   02-229 generates, before it publishes anything, so the run reports no
   count and proves nothing about the checks. What proved them was a
   one-line edit to `jaos_solution` that publishes the point one column out
   of step and changes nothing else. Write the control that breaks the step
   the property is about.
   `grep -n '^| .* | \*\*done\*\* | |$' SPECS.md` lists them. The ones with
   an answer to check, rather than a shape: presolve statistics (57), the
   incumbent callback (73), bit-identical across machines (95), exact
   rational values (98), the certificates (101), the IIS written as a model
   (103), a basis or point another solver produced (105, 106), and the
   progress callback that can stop (150).
