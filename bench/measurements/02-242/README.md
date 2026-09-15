# 02-242 — the work limit and the time limit

SPECS row 148, work limit and time limit, said `done` and said nothing
else (`TODO.md` row 6). `docs/cli.md` promises that `--work-limit N`
stops the solve after N deterministic work units with the outcome
`work_limit`, that two runs at one limit give the same output byte for
byte, and that a stopped solve, solved on, reaches the answer; and that
`--time-limit` stops on the clock with the outcome `time_limit`.
`limits.c` reads six properties.

## The models

1000 per seed, six seeds, 02-237's generator with a third of the models
carrying integer marks: 4006 LPs and 1994 MIPs. Every model is solved
once with no limit for the reference: status, objective, point, work W
and iterations. Then, for each limit L in {1, W/4, W/2, W-1, W, 2W} that
is positive and distinct, a fresh copy is solved at L, a second fresh
copy at L again, and the first is solved on with the limit lifted.

## The properties

1. L at or above W: the solve ends optimal and is the reference to the
   bit, status, objective, point, work and iterations
2. L below W: the solve ends `work_limit` with its work at or past L (it
   never stops early); the overshoot past L is read, and a solve that
   ends optimal past L without stopping is counted as `past`
3. two solves at one L agree to the bit
4. the stopped solve, solved on with no limit, ends optimal at the
   reference objective to 1e-9; a bit-identical objective is counted
5. a time limit of 1e-9 s ends `time_limit` or optimal, never anything
   else, and solving on reaches the reference; a time limit of 1000 s
   ends optimal and is the reference to the bit
6. a NaN time limit is refused; a work limit of 0 or below means no
   limit and gives the reference to the bit

Every 50th model is written out and `solve --work-limit W/2` runs on it
twice: both exit 3 with `status work_limit` and agree above the `time`
line, or, on a MIP, both end optimal with the work at or past the limit,
which is counted.

## The reading

| seed | MIPs | resumed | exact | LP stopped | LP overshoot, mean / max | LP past | MIP stopped | MIP overshoot, mean / max | MIP past | max past | timed stopped | CLI |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 324 | 2950 | 2927 | 2028 | 1946 / 4609 | 676 | 922 | 39660 / 644099 | 374 | 34221 | 1000 | 20 of 20 |
| 2 | 323 | 2947 | 2924 | 2031 | 1945 / 4615 | 677 | 916 | 36792 / 702004 | 376 | 36488 | 1000 | 20 of 20 |
| 3 | 338 | 2939 | 2903 | 1986 | 1942 / 4609 | 662 | 953 | 38091 / 700743 | 399 | 33196 | 1000 | 20 of 20, 2 past |
| 4 | 328 | 2944 | 2920 | 2016 | 1945 / 4630 | 672 | 928 | 38399 / 704985 | 384 | 75566 | 1000 | 20 of 20, 1 past |
| 5 | 344 | 2947 | 2918 | 1968 | 1959 / 4648 | 656 | 979 | 36263 / 817447 | 397 | 23921 | 1000 | 20 of 20 |
| 6 | 337 | 2937 | 2903 | 1986 | 1948 / 4610 | 666 | 951 | 32587 / 704590 | 397 | 25293 | 999 | 20 of 20 |

`past` is the count of limited solves that ended optimal past their
limit; `max past` is how far past, in work units, the furthest of them
ran. The 4009 LP `past` solves are all at L = W-1, ending one unit past,
except one at seed 6 that ended 78 past, the polish after optimality.

**Every property holds on the 6000 models**: no stop before its limit,
every pair of runs identical, every resumed solve at the reference
objective (17495 of 17664 to the bit, the rest in the last bits, which
`TODO.md` row 0b holds), every generous time limit and every lifted
limit the reference to the bit. A 1e-9 s time limit stopped 5999 of 6000
solves before their first iteration; one finished first.

**One finding, on the MIPs.** An LP stops within one iteration's step
of its limit, 4648 units at most on these models. A MIP does not: the
tree reads the limit only between node solves, and each sub-solve it
starts runs under the caller's whole limit on its own. The overshoot
averages 37000 units, reaches 817447, and 2327 of the 7976 limited MIP
solves finished optimal past their limit without stopping at all, one
of them 75566 units past. On the CLI the same thing shows as 3 of the
120 runs ending `optimal` with exit 0 where `work_limit` and exit 3 were
asked for. The SPECS row is `partial` for this and `TODO.md` row 7
holds the fix.

## The pass is not vacuous

Five one-line breaks were measured, one per property that a break could
reach, each on 200 models of seed 1 (60 for the tool's), then reverted:

| break | where | fires |
|---|---|---|
| an unknown MPS section accepted | `src/mps.c`, the `unsupported section` refusal | 02-240: P2 on 200 files, every D1 |
| content after `End` accepted | `src/lpfmt.c` | 02-240: P2 on 200 files, every L1, and 2 of 8 CLI runs |
| `diff` ignores a cost that differs | `cli/jaos.c`, `cmd_diff` | 02-241: 4 of 60 edits, every cost edit drawn |
| `show` prints the upper bound as `lower` | `cli/jaos.c`, `cmd_show` | 02-241: 60 of 60 shows |
| the simplex stops at half the limit | `src/simplex.c`, the three `work_limit` checks | 02-242: P1 on 143 solves, P2 on 310 |

## How to run

```
make all cli
bench/measurements/02-242/limits.sh            # six seeds
RUNS=200 SEEDS=1 bench/measurements/02-242/limits.sh   # one short seed
```
