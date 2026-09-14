# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

A row says what is wrong and what the fix needs. The reading behind it is in
the commit that took it, named here by hash.

## Milestone: reach and polish

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

6. **Sixteen SPECS rows say `done` and say nothing else.** A row with an
   empty description is a feature nobody has written down, and once on
   2026-09-10 it was also a feature nobody had read. Row 72, the solution
   pool, held one point twice (02-227). Rows 54 and 120, the basis file and
   the solution file, came back clean (02-228). Rows 45, 51 and 117,
   postsolve to the caller's indices, copy a model and direct load from
   arrays, came back clean (02-229). Row 74, the incumbent callback, came
   back clean (02-231). Row 151, the progress callback, had two defects a
   MIP caller met on nearly every call (02-232). Row 57, the presolve
   report, counted what presolve had discarded and missed what a forcing
   row fixed (02-233). Row 96, bit-identical across machines, was not: the
   mingw libm's `round` fixed a column at 1 where glibc fixed it at 0, on
   one model in 300 (02-234). Row 102, the certificates: a MIP whose root
   relaxation was infeasible published none (02-235). Row 104, the IIS
   written as a model, came back clean (02-236). Five of the twelve were
   worth the sweep.
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
   an answer to check, rather than a shape: exact
   rational values (99), and a basis or point another solver produced
   (106, 107).
