# 02-449 — the permutation reading after the day's changes

Taken on 2026-11-11 on the tree of 32f2863, for TODO row J27. 02-431's
`read.sh` with `OUT=~/read449` takes `continuous.txt` and `mixed.txt`. No
code changed.

## The reading

02-431's `permcone.c`: each model is solved as it is and with its rows
and columns reordered, 10 seeds of 20,000 models. A `STATUS` line is a
model that ends with an answer in one order and `numerical_error` in the
other; an `OBJ` line is two optima that differ.

| mode | models compared | STATUS | OBJ | before |
|---|---|---|---|---|
| continuous | 200,000 | 71 | 0 | 168 on 02-431's change, 155 after 02-433 |
| mixed | 198,391 | 17 | 2 | 37 and 4 on 02-432's change, 22 in all after 02-433 |

The changes between are 02-443 (a reduced cost judged against its
terms), 02-444 (certificates through quadratic rows), 02-445 (the
refit's weights) and 02-446 (a quadratic row with no interior).

## Two of the models left

02-431 keeps five of seed 1's continuous models as files. Three now end
the same in both orders: `c25` and `c2123` through 02-446's held row, and
`c3431` as an optimum with duals. Two do not:

- `c1905a.mps`. R1 is `-C1 - 2 C3 - C4 - 0.75 C5² - 0.25 C6² >= 0` with
  C1, C3 and C4 at 0 or more, so the only feasible point is the origin,
  which is the tip of both cones. R1 has no interior, but the solve that
  02-446 runs to find that, R1 as the objective over the rest of the
  model, itself ends `numerical_error`: its optimum is at the cones'
  tip.
- `c2939a.mps`. R1's largest value is 2 above its side, so the row has
  an interior. The walk ends after 56 iterations with no answer. 02-431
  reads it as an optimum that is a limit no point reaches.
