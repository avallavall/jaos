# 02-437 — a row held twice

Taken on 2026-10-22 on the tree of 2ba1ad3. `read.sh` takes the reading
and `probe.sh` sorts its disagreements.

## The harness

`dupcone.c` is 02-435's `mirrorcone.c` with the second model the first
plus a copy of one of its rows, its sides and its quadratic part
included, added last. A row held twice is the same model, so the status
must match and the objective with it. The rows are then no longer
independent, which the interior point methods have to bear. The copied
row comes from a second random stream and is never the indicator row,
so the models are 02-431's on the same seed. Seeds 1 to 10, 20,000
models a seed.

The first version of this harness, and 02-435's `mirrorcone.c` and
`mirror.c`, built the first model with an array of ones that was filled
only for the columns, and it served as the row signs too, so a row past
the last column could take a value left on the stack. Each now fills the
array whole, and 02-435's readings are taken again below.

## The reading

| mode | compared | disagree | 02-435's mirror, same models |
|---|---|---|---|
| mixed | 190,512 | 27 (`mixed.txt`) | 17 |
| continuous | 200,000 | 227 (`continuous.txt`) | 132 |

The mixed mode compares fewer models: 8,534 are not built, because each
of 8 draws of the row to copy fell on the indicator row (in nearly all
of them it is the only row), and 954 stop at the work limit.

A copied row costs answers but finds no new kind of failure. `probe.sh`
dumps the 21 mixed status disagreements and the 48 continuous ones of
seeds 1 and 2, and moves every quadratic side and cone head out by 1e-4
(`coneout-*.txt`):

- all 21 mixed ones and 46 of the 48 continuous ones then end optimal
  at the same objective in both models, so they are TODO J27's kind;
- the other two, seed 1's models 9388 and 17871, end `numerical_error`
  without the copy, even moved out, and optimal with it. The interior
  point stops "without an answer" and the ray checker finds no
  improving direction, as on 02-431's c2939, whose optimum no point
  reaches: J27's dual form.

The six mixed objectives that differ include 02-431's three of J27's
kind (seed 1's 2952, seed 9's 7011 and seed 10's 8215); the other three
differ by 5.8e-5, 8.8e-5 and 2.2e-6 of (1 + |a|).

## The mirror readings again

On 2ba1ad3, 02-435's `read.sh` and `readlin.sh` with the corrected
harnesses (`mirrorcone-*.txt`, `mirror-*.txt`) give the bytes of
02-435's cone records and of 02-436's linear records. The values left on
the stack were the ones from earlier models, so no record moved.
