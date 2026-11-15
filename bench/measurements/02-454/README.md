# 02-454 — the MIP presolve's mirror on a quadratic objective

Taken on 2026-11-15 on the tree of c7b970e with this change to
`src/mip.c`, for TODO row J29. `read.sh` is 02-440's `mirrorfile.c` with
02-447's `modes.patch` on QPLIB's 17 convex MIQPs (02-258's list), each
file as it is and mirrored under 2e9 work units with seed 1's masks, in
five modes: rows negated and columns mirrored (`both-*.txt`), rows only,
columns only, integer columns only and continuous columns only. The
`*-head.txt` records are c7b970e's library, the `*-new.txt` the change's.
`miqp.sh` reads the 17 at 1e10 work units, as 02-415 does, with both
tools (`miqp-*.txt`).

## Which part moves a MIQP's tree

On c7b970e, files whose mirror is not `same`:

| what is mirrored | `path` | of which with other nodes or iterations |
|---|---|---|
| rows negated | 1 | 0 |
| columns | 16 | 11 |
| integer columns only | 16 | 11 |
| continuous columns only | 4 | 1 |

QPLIB_10050 takes 135 nodes as it is and 128 with its integer columns
mirrored, QPLIB_3913 19 and 17, and QPLIB_3980 302 iterations and 17 at
its root. Most of the 17 are still at their root at 2e9 work units, and
their root's iterations or work move. The MIP presolve, which mirrors a
column whose lower bound lies farther from zero than its upper bound
(02-447), did not run on a model with a quadratic objective.

QPLIB_5924's line with rows negated differs in its work alone,
2,529,294,989 against 2,529,294,983 units, at the same 44 iterations and
1 node.

## The change

`mp_eligible` takes a model whose quadratic part is in its objective
alone. On such a model `mp_contract` mirrors and merges no columns, and
`mp_build` gives the tree's copy the objective's `Q` with each term that
pairs a mirrored column with one that is not negated (a mirrored pair
and a diagonal term keep their sign), as 02-451's conic tree does. The
answer, the pool and the start cross the mirror as for a MILP, and an
optimum is solved again on the model as loaded with its integer columns
fixed.

`test_a_mirrored_column_gives_the_same_miqp_tree` in `tests/test_mip.c`
solves a four-column MIQP with cross terms, and its copy with two integer
columns mirrored: the same objective and nodes, the mirrored point, and
the presolve's line saying 2 columns are mirrored. It fails on c7b970e,
where no presolve runs.

## The reading

Files whose mirror is not `same`, c7b970e against the change:

| what is mirrored | c7b970e (of which with other nodes or iterations) | the change | of which with other nodes or iterations |
|---|---|---|---|
| rows and columns | 16, 11 | 14 | 0 |
| rows negated | 1, 0 | 1 | 0 |
| columns | 16, 11 | 14 | 0 |
| integer columns only | 16, 11 | 14 | 0 |
| continuous columns only | 4, 1 | 9 | 0 |

On the change every mirrored file takes the nodes and iterations of the
file as it is, with the same incumbent. The `path` lines differ in their
work units alone: the mirrored model pays for the presolve's copy, 565 to
338,844 units of 2e9 (5,065 to 158,990 with continuous columns only), as
02-447's MIPLIB 3 lines do. That cost is also why seven files that read
`same` on c7b970e with only their continuous columns mirrored read `path`
on the change, where no presolve ran either way before; QPLIB_5527 and
QPLIB_5543, whose roots moved, now read `same`.

## The 17 at 1e10 work units

`miqp.sh`, c7b970e against the change (`miqp-*.txt`). QPLIB as
published mirrors no column of 14 of the 17, and those 14 read the same
status, incumbent, bound and nodes, at 565 to 158,990 more work units
for the presolve's pass. Three have columns to mirror:

| instance | columns mirrored | c7b970e | the change |
|---|---|---|---|
| QPLIB_5527 | 4,591 | numerical_error, 9,080,655,810 | work_limit, 10,643,686,333 |
| QPLIB_5543 | 4,638 | numerical_error, 7,860,697,103 | work_limit, 10,884,899,272 |
| QPLIB_5577 | 1,152 | work_limit, 10,670,567,775 | work_limit, 10,187,650,925 |

None of the three has an incumbent or a finite bound either way.

On QPLIB_5527 and QPLIB_5543 the mirrored tree ends `numerical_error`,
and the presolve then runs the tree again on the model as loaded, as it
has since 02-447. That second tree took the whole work limit again, so a
first form of the change ended both at `numerical_error` after
18,056,415,662 and 15,125,905,849 work units, 1.8 times the limit. The
second tree, and the one run when the optimum cannot be solved again on
the model as loaded, now take the limit less the work already spent
(`bb_run_after`). No MILP of the gates reaches either.

## The gates

`make test` and `make sanitize` pass. `make netlib netlib-infeas`,
`netlib-kennington` and `miplib`: 0 regressed, and no line of
`bench/results/` moves. The mirror reading on the final tree is the
first form's line for line.
