# 02-222 — PDLP_RUIZ_ROUNDS, 0, 5 and 20 against the shipped 10

Sets `PDLP_RUIZ_ROUNDS` (`docs/tolerances.md`). Taken 2026-09-09 at 9863d9a,
the commit that added Ruiz equilibration and the Pock-Chambolle pass to
the first-order method.

Before that commit the first-order method ran on the Curtis-Reid scaling
alone and `bench/results/pdlp.txt` read 3 of the 94 inside 10x the dual's
work (commit 3fc772f). The preconditioning is the reference's: rounds of
Ruiz equilibration, each dividing every row and column of the matrix with
its slack columns by the square root of its largest entry, then one
Pock-Chambolle pass with α = 1. The question is the round count.

## What is here

| file | what it is |
|---|---|
| `ruiz.sh` | builds a worktree of the named commit per count with only the constant changed and runs the first-order campaign, the standard 94 at 10x the dual's work |
| `pdlp-ruiz-0.txt`, `pdlp-ruiz-5.txt`, `pdlp-ruiz-20.txt` | those campaigns; the 10-round reading is `bench/results/pdlp.txt` at 9863d9a |

## What it says

| rounds | inside 10x | past it | disagreeing |
|---|---|---|---|
| none (3fc772f) | 3 | 91 | 0 |
| 0 (Pock-Chambolle alone) | 18 | 76 | 0 |
| 5 | 17 | 77 | 0 |
| 10 | **22** | 72 | 0 |
| 20 | 20 | 74 | 0 |

The sets overlap but move: 0 rounds has fit2d and lotfi that 10 lacks;
10 has boeing1, fit1d, gfrd-pnc, grow15, grow7 and pilotnov that 0 lacks.
The count is what the campaign measures and 10 leads it by two. The geometric mean of
work over the agreeing instances is not comparable across settings,
because the sets differ. The ten small instances without a limit are in
`docs/tolerances.md`, before against after at 10 rounds.

**10**, the reference's count, ships.
