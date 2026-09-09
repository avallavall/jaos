# 02-31 — the primal's degenerate six and the MIP root features, 2026-09-08

Raw readings behind seven lines of `bench/refusals.txt` and two defaults in
`docs/tolerances.md`. All taken on 2026-09-08, on the trees each file names
in its header. The directory carried this id in the refusal lines before it
was committed; the id is kept so the lines still resolve.

## Primal simplex (refusals `primal-tie-hash`, `primal-cost-perturbation`)

Each file is one run of `make primal` over the standard 94, the same format
as `bench/results/primal.txt`: dual and primal iterations and work per
instance, then the geometric means. No script: the arms were source patches
that were not kept, and the refusal lines describe each one.

| file | arm | work primal/dual |
|---|---|---|
| `primal-tie-hash-iters.txt` | leaving row by hash of variable and iteration at every zero step | 2.781x |
| `primal-tie-hash-var.txt` | hash of the variable alone | 2.827x |
| `primal-tie-hash-stall1.txt` | hash only after a sweep without a gain in dual infeasibility | 2.653x |
| `primal-tie-hash-zero100.txt` | hash only after 100 zero steps in a row | 2.707x |
| `primal-cost-perturb-zero100.txt` | cost perturbation in phase 2 after 100 zero steps in a row | 2.657x |

The plain primal at that tree read 2.65x. The six overruns (d6cube, dfl001,
fit1d, fit2d, seba, degen3) do not move in any arm.

## MIP root features, on the 24-instance MIP set

Each `sweep-*.sh` solves the set with the shipped CLI, 12 at a time, 240 s
each, and writes the `.txt` beside it: per instance the status, work and
node count per arm, then per arm the geometric mean of work arm/off, the
count past 2x and the count unfinished. Run `make cli` first.

| file | what it measured | verdict |
|---|---|---|
| `sweep-flow-cover.txt` | flow cover cuts at 1, 2, 4 rounds | 1.012x, 1.017x, 1.017x; refused (`mip-flow-cover`) |
| `sweep-zero-half.txt` | zero-half cuts at 1, 2, 4 rounds | 1.210x, 1.182x, 1.219x; refused (`mip-zero-half`) |
| `sweep-clique-fix.txt` | node fixing by the clique table, alone and with probing feeding the table | 1.026x, 1.094x; refused (`mip-clique-fix`) |
| `sweep-probing.txt` | root probing of the fractional binaries at caps 0.5x, 1x, 2x and none of the root's work | 1.108x to 1.109x; refused (`mip-probing-root`) |
| `sweep-conflicts.txt` | Farkas conflict rows at infeasible nodes, on against off | 0.924x; shipped, `MIP_CONFLICTS` on |
| `sweep-orbital.txt` | orbital branching and fixing, on against off, `MIP_SYMMETRY_WORK` 250 | 0.835x; shipped, `MIP_ORBITAL` on |
| `sweep-orbital-cap100.txt` | the same at `MIP_SYMMETRY_WORK` 100 | 0.943x |
| `sweep-orbital-cap250.txt` | the same at 250, the run that set the default | 0.835x |

The three orbital files share the clique-fix header line; the columns are
the orbital counts the script greps.
