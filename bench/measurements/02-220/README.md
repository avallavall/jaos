# 02-220 — how far the barrier's iterate grows, converging and not

Sets `BARRIER_DIVERGE` (`docs/tolerances.md`). Taken 2026-09-09 at the tree
`growth.txt` names, with the rule still at its placeholder 1e10 so that
every run shows its whole walk.

The barrier cannot certify infeasibility or unboundedness. What it can see
is its own iterate: on a model with no optimum the dual iterate (an
infeasible primal) or the primal iterate (an unbounded one) grows without
bound while the residuals stall. The question is at what multiple of the
data that growth can be called divergence without touching a run that
converges.

## What is here

| file | what it is |
|---|---|
| `growth.sh` | solves the standard 94 and the infeasible 29 under `--algorithm barrier --log detail`, 12 at a time, and reads the per-iteration `iterate P/D of the data` line |
| `growth.txt` | per instance: the status, the barrier's iteration count, the largest primal and dual ratio seen, and the first iteration at which either passed 1e4, 1e6, 1e8 and 1e10 |

Run `make cli` first and have both sets fetched. The script finds the
repository from its own path.

## What it says

- All 94 converging runs stay under **3.8e4** (`recipe`, dual side,
  passing 1e4 at its iteration 6 and converging at 9). The next largest
  is `pilot` at 1.4e3; the primal side never passes 1.5e2 (`greenbeb`).
  So 1e4 is refused: it would hand `recipe` to the dual simplex.
- Of the 29 infeasible instances, 10 are refused by presolve and never
  reach the barrier. Of the 19 that do, 18 pass 1e6 between iteration 3
  and 95; at 1e10 five of them (`cplex2`, `ex72a`, `ex73a`, `qual`,
  `vol1`) never cross and run to the 200-iteration cap. `cplex2`'s
  iterate stays under 1.5e2 at every iteration, so no multiple of the
  data catches it; it ends at the cap under any setting.
- Between 1e6 and 1e8 the saving is iterations on the same verdicts:
  `bgprtr` 7 against 94, `gosh` 17 against 55, `vol1` 25 against 37,
  `qual` 23 against 33.

**1e6** is two decades above the largest converging ratio and one decade
below the smallest useful threshold that is not. Measured on the standard
and infeasible sets only; the Kennington set has no barrier reading.
