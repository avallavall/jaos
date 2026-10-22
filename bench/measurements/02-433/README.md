# 02-433 — a maximisation against its negated minimisation

Taken on 2026-10-22 on the tree of fe013a2. `read.sh` takes the reading
below with the library given, `perm.sh` takes 02-431's permutation
reading again on the change, and `gate.sh` the gate of the change in
`src/conic.c`.

## The harness

`flipcone.c` is 02-431's `permcone.c` with the second model in the same
order, its costs negated and its sense the other one. Minimising `c'x` is
maximising `-c'x`, and negating a double is exact, so a solver that
treats the two the same way ends both at the same point, with
objectives of opposite sign. The statuses must match and the two
objectives must sum to within 2e-6 of (1 + |a|). It draws the same
models as `permcone.c` on the same seed. Seeds 1 to 10, 20,000 models a
seed, in mixed and in continuous mode.

## The reading on fe013a2

| mode | compared | disagree |
|---|---|---|
| mixed | 198,376 | 42 (`mixed-head.txt`) |
| continuous | 200,000 | 203 (`continuous-head.txt`) |

The continuous mode has no tree, so the conic interior point solves both
models. In the first disagreement taken apart (seed 1, model 446) the
two logs are the same line for line through all 36 iterations of the
walk and the first Newton finish. Then the finish lets go of each column
whose reduced cost pushes it off its bound, and it read the published
reduced cost, which carries the user's sense. On a maximisation every
sign was the other way, so it let go of the columns whose sign was right
and kept the ones whose sign was wrong.

With that fixed, 28 continuous disagreements were left. In the six taken
apart, the minimisation ran the quadratic programs on the quadratic
rows' linearisation (`cm_sqp`) and the maximisation did not: that step
returned at once on a maximisation.

## The change

- The Newton finish judges a reduced cost's sign with the sense applied.
- `cm_sqp` runs on a maximisation too. It turns the row duals into the
  minimisation's convention on the way in, reads the objective times
  the sense sign in each program and in the reduced costs, and turns the
  duals and reduced costs back on the way out.

On a minimisation the sense sign is 1, so every value is the one
fe013a2 computes. `tests/data/qc_max_polish.mps` (model 446) and
`tests/data/qc_max_sqp.mps` (seed 1's model 7550 in continuous mode) end
`numerical_error` on fe013a2. They now end optimal at the same point as
their negated minimisation, bit for bit.

## The reading on the change

| mode | compared | disagree |
|---|---|---|
| mixed | 198,393 | 0 (`mixed-new.txt`) |
| continuous | 200,000 | 0 (`continuous-new.txt`) |

More models get an answer in the plain order too: the pairs where the
first model ends with no answer fall from 637 to 616 in mixed mode and
from 847 to 738 in continuous mode.

`perm.sh` takes 02-431's permutation reading again on the change
(`perm-mixed.txt`, `perm-continuous.txt`). The four objectives that
02-431 took apart are still there; each other disagreement has one order
with no answer.

| mode | tree | compared | disagree |
|---|---|---|---|
| mixed | fe013a2 (02-432's `mixed.txt`) | 198,371 | 41 |
| mixed | the change | 198,380 | 26 |
| continuous | fe013a2 (02-431's `continuous-new.txt`) | 200,000 | 168 |
| continuous | the change | 200,000 | 155 |

## The gate

`gate.sh` is 02-432's on fe013a2 against the change.

| set | fe013a2 | the change |
|---|---|---|
| 02-255, 8000 models | | the same answers and nodes (`mip.txt`); work 3,104,879,000 to 3,076,895,040 |
| 02-326, 6000 models | | the same answers and nodes (`dis.txt`); work 2,447,856,952 to 2,422,418,415 |
| CBLIB's 80 mixed-integer instances at 1e10 | | the same lines |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | | the same lines |
| 02-427's `warmqc.c`, both modes, seeds 1 to 8 | 02-427's `warmqc.txt` | the same lines |
| 02-425's `warmcone.c`, misocp, seeds 1 to 3 | 02-432's `warmcone.txt` | the same counts, P1, P2, P3 all 0 |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

In 02-255 and 02-326 only the work lines move: 0.9% and 1.0% less. The
models there draw both senses, and only a maximisation's solve can
change. `warmqc.c` and `warmcone.c` generate minimisations only.
