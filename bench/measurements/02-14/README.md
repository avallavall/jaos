# What greenbeb's and scfxm3's post-D106 overcost is made of

The `TODO.md` §1d question, measured 2026-08-17. D108 is the decision it
closed. Two readings: a split of the committed records, which needed no new
run, and an instruction-count attribution under callgrind, which needed two
binaries and no source change.

## The record split — the three do not share a mechanism

Owners: `bench/results/netlib.txt` at HEAD (post-D106) and at `a7d8699^`
(pre-D106, written at `b40fe74`). The work ratios are the ones §1d already
carries; the iteration split is new.

| instance | iterations | work | work per iteration |
|---|---|---|---|
| `greenbeb` | 8124 → 11194, **1.3779x** | 1.5126x | 1.0978x |
| `scfxm3` | 1354 → 1427, 1.0539x | 1.3557x | **1.2864x** |
| `forplan` | 182 → 194, 1.0659x | 1.1648x | 1.0928x |

`greenbeb` pays in iterations. `scfxm3` pays per iteration, on a model that
shrank. `forplan` is small and mixed. One label ("three firings costing
51%") covered two different phenomena.

## The instrument

`run-callgrind.sh`. Two trees: HEAD, and a worktree at `b40fe74`, the commit
whose tree produced the pre-D106 record. One diagnostic build each,
identical flags (`-O2 -g -DNDEBUG -ffp-contract=off`, no LTO so function
boundaries survive), built by the same script.

Calibration before anything is profiled: each binary must reproduce its own
record's `iters=` and `work=` exactly, per instance. All six matched —
`greenbeb` 11194/573519868 and 8124/379164967, `scfxm3` 1427/11414560 and
1354/8419409, `forplan` 194/1246118 and 182/1069794 — so the profiled
trajectories are the recorded ones, not an artifact of the diagnostic
flags. Work units are flag-independent, which is what makes this check
possible.

Then `valgrind --tool=callgrind` per instance per side. Instruction counts
are deterministic for a given binary and input. Flat tables in `profiles/`;
totals include the reader and the checker, which are identical on both
sides, so solve-side ratios are slightly above the quoted totals.

## What the profile says

Program totals, instructions retired:

| instance | pre | post | ratio | per iteration |
|---|---|---|---|---|
| `greenbeb` | 22.91e9 | 32.59e9 | 1.423x | **1.032x** |
| `scfxm3` | 898.3e6 | 1040.0e6 | 1.158x | 1.099x |
| `forplan` | 66.2e6 | 71.4e6 | 1.079x | 1.012x |

**`greenbeb`: every kernel scales with the iteration count.** Per-function
ratios sit between 1.34x and 1.57x around the 1.378x iteration ratio; per
iteration nothing moved more than ~14% (`ftran_prefix` 1.14x, `jm_lu_factor`
1.11x the largest, both consistent with slightly denser factors along the
new path). The overcost is the path, not the arithmetic.

**`scfxm3`: the growth is localized in the ratio-test path.** With
iterations at 1.054x: `update_dual` 1.71x, `shift_to_feasible` 1.68x,
`admit_candidate` 1.57x, `pivot` 1.48x — against `jm_lu_update` 1.05x,
`jm_lu_factor` 1.07x, `jm_lu_btran_sparse` 1.11x. Each dual iteration
admits and processes more candidates than before. The work-unit rise per
iteration (1.286x) is larger than the instruction rise (1.099x) because the
billed quantities live exactly in the kernels that grew.

**`forplan`: nothing is localized** (largest mover `jm_dual_simplex` 1.093x)
and the whole effect is inside what two changed trajectories produce.

## What this refutes

- One mechanism. `greenbeb` and `scfxm3` pay through different machinery.
- A site-local predictor. Both overcosts are downstream consequences of an
  exact substitution that removed 3–5 rows per instance; nothing at the
  reduction site distinguishes these three instances from the 14 the same
  family made cheaper (set geometric mean 0.9527x, D106). §2 already showed
  the same firings halving `grow15` while inflating `grow22` sevenfold.
