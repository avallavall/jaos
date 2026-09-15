# 02-244 — the primal and dual tolerances

SPECS row 149, primal and dual tolerances, said `done` and said nothing
else (`TODO.md` row 6). `jaos_set_primal_tolerance` and
`jaos_set_dual_tolerance`, `--primal-tol` and `--dual-tol`, the options
`primal_tolerance` and `dual_tolerance`: how far a variable may sit
outside its bound and count as feasible, and how far a reduced cost may
sit on the wrong side of zero; both in the scaled space the simplex
works in, the defaults `PRIMAL_TOL` 1e-7 and `DUAL_TOL` 1e-9 in
`docs/tolerances.md`. `tol.c` reads four properties.

## The models

1000 per seed, six seeds, 02-237's generator, LPs only. Every model is
solved once for the reference, then with each tolerance set explicitly,
loosened and tightened. Two rows over one column are added, `x_j >= v`
and `x_j <= v - d`, with `v` the planted point's value there, so at
d = 0 the model keeps a point and at d = 1e-9 it has none.

## The properties

1. a negative, NaN or infinite tolerance is refused; zero is taken
2. the defaults set explicitly, 1e-7 primal and 1e-9 dual, give the
   reference to the bit
3. the pinned column at d = 0 is taken and published at `v`; at
   d = 1e-9 the model is refused as infeasible at the default and at
   1e-4 alike, because presolve folds the two rows into the column's box
   and asks whether it is inverted beyond rounding
   (`PRESOLVE_ROUND_ULPS`), not beyond the tolerance; and a primal
   tolerance of 1e-9, 1e-7, 1e-5 or 1e-3 on the model itself gives an
   optimum whose worst violation, read by the checker with a 1e-12
   window, is at most ten times the tolerance
4. at a dual tolerance of 1e-11, 1e-9, 1e-7, 1e-5 or 1e-3 the checker's
   `max_dual_violation` on the published answer is at most ten times the
   tolerance, and the objective is within that violation times the box
   widths of the reference

Every 50th model is written out and the tool runs on it: `--primal-tol
1e-5` and `--opt primal_tolerance=1e-5` give the same output above the
`time` line, `--dual-tol 1e-9` gives the output of no flag, and
`--primal-tol -1` exits 5.

## The reading

| seed | worst primal violation at 1e-9 / 1e-7 / 1e-5 / 1e-3 | worst dual violation over its tolerance, 1e-11 to 1e-3 | CLI |
|---|---|---|---|
| 1 | 7.4e-15 / 7.4e-15 / 7.4e-15 / 9.0e-4 | 0 at every one | 20 of 20 |
| 2 | 7.1e-15 / 7.1e-15 / 7.1e-15 / 7.1e-15 | 0 at every one | 20 of 20 |
| 3 | 7.1e-15 / 7.1e-15 / 7.1e-15 / 7.1e-15 | 0 at every one | 20 of 20 |
| 4 | 1.2e-14 / 1.2e-14 / 1.2e-14 / 1.2e-14 | 0 at every one | 20 of 20 |
| 5 | 7.1e-15 / 7.1e-15 / 7.1e-15 / 7.1e-15 | 0 at every one | 20 of 20 |
| 6 | 1.4e-14 / 1.4e-14 / 1.4e-14 / 1.4e-14 | 0 but 0.42 at 1e-3 | 20 of 20 |

**No defect, and one stale line in the docs.** Every property holds on
all 6000 models; on seed 1, 2000 pinned columns were taken at their
value and 2000 contradictions of 1e-9 refused, none oddly. The
`--dual-tol` row of `docs/cli.md` said the default was 1e-7; the code's
default is `DUAL_TOL`, 1e-9, since 2026-08-25 (`docs/tolerances.md`),
and setting 1e-9 explicitly gives the reference to the bit on every
model; the row says 1e-9 now. Setting 1e-7 gives the same answers on
these models too, so the reading cannot tell the two apart; the code
can.

**What the reading cannot show.** On these models the tolerances barely
act: an optimum published at 1e-3 violates a bound once in 6000, by
9e-4, and the dual side reads zero at every tolerance but once, 4.2e-4
at 1e-3 with the objective 1.4e-3 off. The models are small with
integer data and land on exact vertices. What the sweep proves is the
refusals, the defaults, and that loosening never breaks the stated
bounds; a control that made the simplex ignore the model's tolerance
would not fire here, and is not claimed.

## The pass is not vacuous

One one-line break was measured on 200 models of seed 1, then reverted:
the setter taking a negative tolerance. It fired property 1 twice on
every one of the 200 models, and the CLI's `--primal-tol -1` exited 0
instead of 5 on all 4 files.

## How to run

```
make all cli
bench/measurements/02-244/tol.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-244/tol.sh   # one short seed
```
