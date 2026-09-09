# 02-221 — PDLP_TOL, 1e-4 against 1e-6

Sets `PDLP_TOL` (`docs/tolerances.md`). Taken 2026-09-09 at 10d000f, the
commit that shipped the first-order method at 1e-6.

The first-order method stops at a relative KKT error and hands its point
to the crossover, so the tolerance trades first-order iterations against
the quality of the basis guess the dual simplex starts from. The
reference's default is 1e-4.

## What is here

| file | what it is |
|---|---|
| `tol.sh` | builds a worktree of the named commit with the constant at 1e-4 and runs the first-order campaign, the standard 94 at 10x the dual's work |
| `pdlp-tol-1e-4.txt` | that campaign; the 1e-6 side is `bench/results/pdlp.txt` at 10d000f |

The ten small instances below were run by hand with the shipped CLI under
`--algorithm pdlp` and no work limit, at each setting, and the counts are
the first-order iterations the log reports before the crossover.

## What it says

At the 10x work limit the two settings agree on every verdict: the same
three inside the limit (pilot and pilot87 by the hand-off at the
iteration cap, truss by converging), 91 past it, none disagreeing. Only
truss moves: 31168 iterations and 3.30x the dual's work at 1e-4 against
38912 and 3.99x at 1e-6, and the crossover from the looser point takes
8540 dual iterations against 8903, so the guess is no worse.

Without a limit, the looser tolerance converges everywhere the tighter
one does and on two more:

| instance | 1e-4 | 1e-6 |
|---|---|---|
| afiro | 448 | 768 |
| sc50a | 896 | 1856 |
| adlittle | 3648 | 10112 |
| sc105 | 4672 | 6784 |
| kb2 | 15168 | 73088 |
| blend | 20032 | 23168 |
| stocfor1 | 46976 | 89792 |
| scagr7 | 74176 | past the 200000 cap |
| share2b | 104320 | 148096 |
| israel | 123773 | past the 200000 cap |

Every answer at either setting is the dual's objective to the last digit,
because the crossover publishes a vertex. **1e-4** is what shipped in the
following commit.
