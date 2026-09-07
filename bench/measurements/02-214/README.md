# 02-214 — what the MIP basis repair costs

Closes the cost half of **D334**. The correctness half is
`bench/measurements/02-212/mip-basis-count.txt`, which reads 5 of 24
before the repair and 24 of 24 after.

## What is here

| file | what it is |
|---|---|
| `cost.py` / `cost.txt` | the baseline's work against the new record's, per instance, with the iterations and node counts beside them |

## What it says

```
geometric mean of the work ratios: 1.001163x
largest single ratio             : 1.013031x
instances whose work did not move: 5 of 24
instances whose tree moved       : none
```

**The repair costs 0.12% on the geometric mean and 1.3% at worst.** It is
one LP over a model whose integer columns are all fixed, so it is small
next to the tree that produced them.

**No tree moved.** Every instance's iteration count and node count are
identical to the baseline's, which is what the repair's own shape
predicts: it runs after the search has finished and cannot reach any
decision the search made.

**The five that pay nothing are the five that never had the defect.**
`enigma`, `flugpl`, `l152lav`, `misc07` and `stein45` publish a basis of
the right size without help, the count is asked before the re-solve, and
they never enter it.

The runner's own line reads `0 regressed, 0 improved, 0 new`, and it is
not what this measures: that line only says no predicate flipped and no
instance passed 2.0x work.

## The baseline

`bench/miplib.baseline` is rewritten with this run, deliberately, after
the numbers above were read. Only the work column moves; the statuses,
iteration counts and node counts are the ones already there.

## Reproducing

```
make cli
make miplib J=12
python3 bench/measurements/02-214/cost.py
```
