# 02-442 — a zero-cost column boxed in [-a, a] starts where its rows say

Taken on 2026-11-11 on the tree of e603905 with this change to
`src/simplex.c`. 02-440's `read.sh` with `OUT=~/read442` takes the mirror
reading; its records are the set files here. `variants.sh` takes
`variants.txt`.

## What parted

After 02-441, `pilot87` of Netlib and `qual`, `refinery` and `vol1` of the
infeasible set still took another path when mirrored. Each has zero-cost
columns boxed in [-a, a]: 9 in `pilot87` and 117 in each of the others. The
two bounds are as near zero, so the slack basis put the column at -a both
ways, and the mirrored solve started from another point.

## The change

`build_initial_basis` calls `break_start_ties` when the model has such a
column. It adds up each row's value over the other nonbasic columns. Then
it takes the tied columns in order. For each, it reads its rows at -a and
at a:

1. the sum of the rows' infeasibilities. The column starts at the bound
   where the sum is larger.
2. when the sums are equal, the sum of each feasible row's distance to its
   nearer finite bound. The column starts at the bound where it is smaller.
3. when both are equal, the lower bound, as before.

The column's value then enters the rows' values, and the next tied column
reads them. A mirror negates the column's entries and a negated row swaps
its bounds, so every term at -a becomes the same term at a, in the same
order. The mirrored column starts at the mirrored bound. Both bounds keep
the start dual feasible, so no answer depends on the choice. The pass is
billed: one work unit per entry of a nonbasic column away from zero, and
two per entry of a tied column.

The first key alone settles `pilot87`. On the three infeasible models two
or three columns stayed tied on it. Each sits in equality rows that the
other columns leave exact, with value 0 and side 0, and in one row of its
own that holds either way. In `qual`, column 166 has -1.639 in row 53,
an equality at 0, and -1 in row 250, which is at most 112. Only the room
left in row 250 tells the two bounds apart, and that is the second key.
Deciding these columns after all the others did not help: row 53 still
read 0.

`test_a_mirrored_column_in_a_symmetric_box_starts_mirrored` in
`tests/test_simplex.c` solves a four-column LP and its mirror. On e603905
the two count 4,302 and 4,289 work units; with the change both count the
same.

## The direction of the two keys

Each key can go either way and the mirror still holds. `variants.sh` reads
the four with `variants.patch` (work units; `J28V` in the first column, 9
is the lower-first rule of e603905 with the pass billed):

| rule | pilot87 dual | pilot87 primal | qual | refinery | vol1 |
|---|---|---|---|---|---|
| 9: lower first | 3,159,056,119 | 14,260,178,725 | 1,457,732 | 774,873 | 1,171,354 |
| 0: more infeasible, less room | 3,432,019,590 | 12,487,421,692 | 1,301,570 | 906,631 | 1,247,315 |
| 1: less infeasible, less room | 3,602,323,119 | 23,710,774,385 | 1,516,963 | 840,527 | 1,573,387 |
| 2: more infeasible, more room | 3,432,019,590 | 12,487,421,692 | 1,414,940 | 968,679 | 1,560,704 |
| 3: less infeasible, more room | 3,478,075,431 | 23,677,538,128 | 1,576,277 | 973,709 | 1,394,977 |

Rule 0 landed. The bound with less infeasibility takes `pilot87`'s primal
solve from 27,259 iterations to about 63,000.

## The mirror reading

| set | files | same | path | DIFF | 02-441 |
|---|---|---|---|---|---|
| Netlib | 94 | 94 | 0 | 0 | 93 / 1 / 0 |
| Netlib infeasible | 29 | 29 | 0 | 0 | 26 / 3 / 0 |
| Kennington | 16 | 16 | 0 | 0 | 16 / 0 / 0 |
| Maros-Meszaros | 138 | 116 | 22 | 0 | 116 / 22 / 0 |
| CBLIB continuous | 29 | 29 | 0 | 0 | 29 / 0 / 0 |
| CBLIB mixed-integer | 80 | 31 | 48 | 1 | 31 / 48 / 1 |
| MIPLIB 3 | 24 | 0 | 22 | 2 | 0 / 22 / 2 |

Every LP file now takes the same path both ways. The sets that do not move
hold QPs, which the barrier solves, conic models, and MIPs, whose trees
part for their own reasons (TODO J29).

## The gates

`make netlib netlib-infeas netlib-kennington primal barrier pdlp concurrent
warm miplib`, each against its baseline: 0 regressed. `bench/results/` is
rewritten by these runs. What moves:

| solve | e603905 | with the change |
|---|---|---|
| `pilot87` dual | 9,490 it, 3,159,044,877 | 9,708 it, 3,432,019,590 |
| `pilot87` primal | 27,259 it, 14,260,167,483 | 26,411 it, 12,487,421,692 |
| `pilot87` warm after the branch | 217 it, 464,180,443 | 222 it, 416,568,661 |
| `pilot87` cold after the branch | 9,363 it, 3,097,717,529 | 10,584 it, 4,020,885,361 |
| `qual` | 357 it, 1,456,975 | 359 it, 1,301,570 |
| `refinery` | 287 it, 774,145 | 317 it, 906,631 |
| `vol1` | 332 it, 1,170,597 | 335 it, 1,247,315 |
| `p0033` | 13 nodes, 1,365,132 | 21 nodes, 2,571,744 |
| `gen` | 541 it, 3,930,556 | 568 it, 4,237,775 |
| `misc03` | 43,280,922 | 42,965,044 |
| `misc07` | 3,805,119,769 | 3,798,011,981 |

`bell5`, `lseu`, `p0201` and `p0282` keep their iterations and nodes and
move by under 0.02% in work units. No other line of Netlib, its infeasible
set, Kennington or MIPLIB 3 changes.
