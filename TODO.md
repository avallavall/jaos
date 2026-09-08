# TODO — the current milestone

Rows from `SPECS.md`. A line leaves this file in the commit that lands it.
When the file is empty, pick the next rows from SPECS and fill it again.

## Milestone: reach and polish

1. **Windows build, the rest.** The shim is in (`src/jaos_sys.h`),
   mingw-w64 builds the library and the tool, wine gives the Linux answers,
   and the Python binding knows `jaos.dll`. Missing: a native Windows run
   and clang-cl, both needing a machine this repository has not got.
2. **Primal simplex: the 6 of 94 standard instances that run past 10x the
   dual's work** (`bench/results/primal.txt`): d6cube, degen3, dfl001,
   fit1d, fit2d, seba. Down from 17 once phase 2 stopped shifting costs
   and from 9 with steepest-edge pricing (02-31: iterations primal/dual
   1.43x to 1.23x, work 2.75x to 2.65x, pilot87 solves). What is left is
   the long degenerate phase 2 of d6cube, degen3 and dfl001 and the cost
   of the weight update itself on the dense ones; bound perturbation and
   a hashed choice at a tie in the ratio test were refused
   (`bench/refusals.txt`, primal-bound-perturbation and primal-tie-hash,
   the second in four gates, the grow family refusing every one and the
   six not moving in any), and so was a cost perturbation on the pricing
   side after a run of zero steps (primal-cost-perturbation, 1.001x).
   Every named reopen is measured; what is left is reading the walk
   itself. The row in SPECS stays partial until the count is zero.

## Milestone: symmetry

10. **Symmetry detection at the root.** The model as a coloured graph:
    one vertex per column, coloured by cost, bounds and kind, one per row,
    coloured by its bounds, an edge per nonzero coloured by the
    coefficient. Colour refinement to an equitable partition, then a
    partition-backtracking search that individualises a vertex of the
    first non-singleton cell, refines, and compares leaves, under a work
    cap; every automorphism found is a generator, and the column orbits
    come from the generators by union-find. A subgroup is enough: every
    use below is valid on a subgroup. Logged at the root (generators,
    orbits, the largest); `jaos_mip_report` says how many orbits. No
    change to the tree, so every gate stays byte-identical. Tests on a
    model with a known group and on stein27's log.
11. **Orbital fixing at the nodes** behind `--orbital-fixing`: at a node,
    the generators that fix pointwise every binary the branching set to 1
    generate a subgroup; in each of its orbits that holds a binary the
    branching set to 0, every binary is fixed to 0 (Ostrowski, Linderoth,
    Rossi and Smriglio). Measured on the MIP set; lands on or off by the
    reading.



