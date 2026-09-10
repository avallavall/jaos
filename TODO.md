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
   The fix: propagate the rows onto the freed columns and take the finite
   bounds where they exist. The same propagation runs at every node, so it
   needs a reading over the MIP set.
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

2. **`make configs` cannot pass.** `make test` runs `windows-test`, and
   `tests/windows.sh` builds the Windows side through cmake with its own
   toolchain file, so `EXTRA_CFLAGS` never reaches it. Under
   `-DJAOS_NO_PRESOLVE` the Linux binary has presolve off and the Windows
   binary has it on, and seven checks fail comparing one against the other.
   The fix: pass the flags through to the cmake build, or skip
   `windows-test` when `EXTRA_CFLAGS` is set.
   Measured 2026-09-10, the same seven failures at 3bf0585 and at its child.

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
