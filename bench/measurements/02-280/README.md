# 02-280 — the dual perturbs its costs after a shorter stall

Taken on 2026-09-21 on edc7aa8 with the Devex fallback of 02-278 (TODO
B10). Every ratio here is against that tree.

## Before

The dual perturbs its costs (`DUAL_PERTURB`) the first time the best total
primal infeasibility has not improved for `STALL_FACTOR * (nrow + ncol +
1)` iterations, ten times the model's size, and turns to Bland's rule after
a second such plateau. On grow22 (1387 rows and columns) that is 13870
iterations, nearly its whole solve of 14176.

## Perturbing before the first iteration, refused

The rivals perturb from the start on a degenerate model. Done so outside
the MIP tree (`start-*.txt`): netlib 0.9586x, Kennington 0.9579x, the
infeasible set 0.9575x, with grow22 at 0.071x. The netlib gate fails on
pilot at 2.49x, and five answers' suboptimality bounds rise past 2x
(etamacro from 3e-16 to 6e-11); cre-c does on Kennington. Refining the
published duals after a perturbed solve left four of them past 2x. Inside
the MIP tree the same change grew one MIPLIB run past 8 GB, until the OOM
killer stopped it. The share of zero-cost columns does not tell the models
that gain from those that lose: wood1p (1.00) and grow22 (0.93) gain,
pilot (0.99) and greenbea (0.88) lose. `bench/refusals.txt` has it as
dual-perturb-at-start.

## Perturbing after a plateau of the model's size

Outside the MIP tree, the perturbation now comes after
`PERTURB_STALL_FACTOR * (nrow + ncol + 1)` iterations without progress,
and a solve that perturbed refines the duals it publishes once, as a solve
under Devex does. Swept over netlib (`stall-*.txt`; `netlib.txt` is 1):

| `PERTURB_STALL_FACTOR` | work | moved |
|---|---|---|
| 0.3 | 0.9955x | pilot 7.94x, nesm 3.93x: the gate fails |
| 0.5 | 0.9656x | pilot 1.11x, truss 1.09x, d2q06c 1.04x among the losses |
| **1** | **0.9688x** | four instances, all better |
| 2 | 0.9829x | grow22 alone |
| 10 | 1.0000x | the old behaviour |

At 1:

| instance | iterations | work | suboptimality bound |
|---|---|---|---|
| grow22 | 14176 to 2014 | 0.113x | 9.5e-14 to 2.4e-16 |
| grow15 | 1871 to 1197 | 0.508x | 7.1e-16 to 2.3e-16 |
| grow7 | 614 to 542 | 0.891x | 8.6e-16 to 1.7e-16 |
| truss | 17732 to 17667 | 0.995x | 8.3e-16 to 7.7e-16 |

The four gates pass with 0 regressed: netlib 0.9688x, the infeasible set
1.0002x (klein1 1.006x), and Kennington and the MIP set byte-identical.
The plateau at 1 is below truss's longest, 1.67 of its size, which is why
truss is the one instance besides the grow family that moves.

## What it does to presolve's price on the grow family

`bench/measurements/02-11/` found presolve making grow22 11.16x and grow7
8.56x more expensive, from twenty singleton-column firings whose widened
rows leave the dual a long degenerate plateau. The same tree built with
`-DJAOS_NO_PRESOLVE` against the shipping one, work (`grow-*.txt`):

| instance | with presolve | without | ratio |
|---|---|---|---|
| grow22 | 45283251 | 34109520 | 1.33x |
| grow15 | 18949180 | 29794445 | 0.64x |
| grow7 | 7490415 | 8486831 | 0.88x |

The plateau is where the earlier perturbation acts, so presolve's price on
grow22 falls under 2x and on the other two it pays (TODO B5).
