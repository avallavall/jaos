# 02-466 — the root's dive and pump held to half the root's work while no incumbent exists

Taken on 2026-11-16 on dd33215 with this change, for TODO rows J7 and J32.
`read.sh` takes the readings one solve at a time:
- `csched008-before.txt`: each root heuristic's work on HEAD's
  `csched008`, from `heuristics.py`, which patches a copy of `src/mip.c`
  to print it;
- `miplib3/share*.txt`: MIPLIB 3 at shares of 0.25, 0.5 and 1, with no
  floor;
- `full/share*.txt`: MIPLIB 2017 at shares of 0.5 and 0.25, with no floor;
- `miplib3/landed.txt`, `full/landed.txt`: the landed share and floor.

`full/before.txt` is 02-464's record.

## What the root spent

`csched008` stayed at its root for the whole 1e10 (02-459). Each root
heuristic's work:

| heuristic | work | point |
|---|---|---|
| lock rounding, twice | 55,424 each | none |
| feasibility jump | 150,680,661 | none |
| dive heuristic | 1,418,281,920 | none |
| feasibility pump | 5,500,988,387 | none |

The root relaxation with its cuts took about 2.8e9. The pump had 20
rounds and no work cap while there was no incumbent, and each of its
rounds solves this root's LP again. The jump has its share of the root
(`MIP_FJ_ROOT`). The dive and the pump had none: they took a stop only once
an incumbent existed.

## What was built

At the root, while there is no incumbent, the dive heuristic and the pump
each stop once they have spent `MIP_HEUR_ROOT` (0.5) times the work the
root took before them. The stop is never less than `MIP_HEUR_FLOOR`
(20000) times (nonzeros + columns + rows), the measure of the jump's own
budget. The stops already set with an incumbent are unchanged.

`tests/test_mip.c` reads it on `tests/data/mip_noinc_propagate.mps` at
node limit 1: 2000 and 4000 rounds of the pump both read 6,091,939 work
units. On HEAD they read 23,430,013 and 44,558,013.

## The share, without the floor first

MIPLIB 3, against `bench/miplib.baseline` (02-464):

| share | total work | past 2x | moved past 10% |
|---|---|---|---|
| none (HEAD) | 1.000x | none | |
| 0.25 | 0.955x | none | `misc03` 0.85x, `misc07` 0.85x |
| 0.5 | 0.933x | none | `l152lav` 0.71x, `misc03` 0.85x, `flugpl` 0.86x |
| 1 | 0.975x | none | `flugpl` 0.77x, `l152lav` 0.87x |

MIPLIB 2017, 02-328's `m17sum.py`:

| share | primal | dual | sum |
|---|---|---|---|
| none (02-464) | 7.0004 | 6.5150 | 13.5153 |
| 0.5 | 6.8781 | 6.5251 | 13.4032 |
| 0.25 | 7.2284 | 6.5345 | 13.7629 |

At 0.5:
- `csched007`'s incumbent comes from 0.46 to 0.21 of its optimum, and
  `mas74`'s from 0.081 to 0.047.
- `neos-3754480-nidda` goes from 0.056 to 0.114, `timtab1` from 0.53 to
  0.61, and `beasleyC3` from 0.028 to 0.060.
- `csched008` reaches 79 nodes instead of 1 and still finds no point. A
  node there costs about 6e7 work units.

## The floor

Without the floor, half the root is too little on a small model.
`tests/test_mip.c`'s four pump tests failed: on those models the pump
needs a few rounds, and each round costs about what the root's whole
solve cost. The MIPLIB 3 gains above came the same way: the small models'
dive and pump were cut short.

With the floor (`miplib3/landed.txt`, `full/landed.txt`):
- MIPLIB 3 reads its baseline exactly, 1.0000x, every instance within 2%.
- MIPLIB 2017's gap sum is 13.3607, against 13.5153.

Three instances move, and the other 27 read the same:
- `timtab1`'s incumbent comes from 0.53 to 0.43 of its optimum.
- `csched007`'s comes from 0.46 to 0.40.
- `csched008` reaches 79 nodes instead of 1, and still finds no point.

## The verdict

Landed at a share of 0.5 with the floor. MIPLIB 3's baseline is
unchanged. `bench/results/miplib2017.txt` is `full/landed.txt`.
