# 02-447 — a MIP tree and the signs of its columns and rows

Taken on 2026-11-11 on the tree of 3a81a08 with this change to
`src/mip.c` and `src/symmetry.c`, for TODO row J29. `modes.patch` gives
02-440's `mirrorfile.c` the variable `MIRROR`: `rows` negates rows only,
`cols` mirrors columns only, `int` and `cont` mirror the integer or the
continuous columns only. The `*-head.txt` records are 3a81a08's library,
the others the tree's, each on MIPLIB 3 under 2e9 work units with seed 1.
`miplib.txt` is 02-440's `read.sh`, columns and rows together.

## Which part moves a tree

On 3a81a08, of MIPLIB 3's 24 models:

| what is mirrored | models whose node count differs |
|---|---|
| rows negated | 7 |
| columns mirrored | 22 |
| integer columns only (`miplib-int-head.txt`) | 22 |
| continuous columns only (`miplib-cont-head.txt`) | 4 |

`bell5` takes 20,089 nodes as it is and 185,040 with its integer columns
mirrored, and the same 20,089 with its rows negated or its continuous
columns mirrored. A mirrored binary column lies in [-1, 0] and a mirrored
integer column swaps its down child with its up child.

With rows negated, `stein27` takes 3,888 nodes against 1,235, `rgn` 2,105
against 143 and `p0201` 879 against 351. The symmetry search finds 8
generators on `stein27` as it is and none with rows negated: it coloured a
row by its two sides and an entry by its value, so a negated row matched
none of its twins.

## The change

**Columns (`src/mip.c`).** The MIP presolve mirrors a column whose lower
bound lies farther from zero than its upper bound: the tree's copy of the
model takes `-x`, with the bounds, the cost and the entries negated
(`mp_contract`). The answer, the solution pool and a MIP start cross the
mirror (`mp_postsolve`, `mp_build`). A model with such a column now runs
on a copy even when no equality row is substituted, and its optimum is
solved again on the model as loaded, as after a substitution. A column in
[0, u] is left as it is, so a model in that form runs as before.

**Rows (`src/symmetry.c`).** `sg_build` reads each row with one sign
(`sg_row_sign`): the finite side above when only one side is finite, the
side of larger magnitude above when both are, a positive right-hand side
for an equality, then a positive sum of the entries, then a positive
first entry. The row's colour and its entries' labels are taken with that
sign. A row and its negation get the same colour and labels, so every
symmetry found before is still found, and rows that differ only by a
negation now match.

`test_a_mirrored_column_gives_the_same_tree` in `tests/test_mip.c` and
`test_a_negated_row_is_coloured_as_its_twin` in `tests/test_symmetry.c`
fail on 3a81a08. `overflowing_down_branch`, the model of
`test_a_node_whose_relaxation_fails_is_set_aside`, had its integer column
in [-3, 0]; it is now in [-3, 4], which the presolve does not mirror, and
the test's tree is the one it was.

## The reading

MIPLIB 3, models whose node count differs:

| what is mirrored | 3a81a08 | tree |
|---|---|---|
| rows negated (`miplib-rows*.txt`) | 7 | 4 |
| columns mirrored (`miplib-cols*.txt`) | 22 | 0 |
| both (`miplib.txt`; 02-441 for 3a81a08's count) | 22 | 4 |

`bell5` takes 20,089 nodes both ways, at 205,954,598 and 205,970,528 work
units: the mirrored run pays for the solve on the model as loaded. Every
line of `miplib-cols.txt` still says `path`, for that work.

The four left are `enigma` (895 nodes against 15,642), `l152lav` (1,438
against 1,838), `misc03` (248 against 253) and `misc07` (7,192 against
7,139), each with rows negated. Their roots, clique tables and first cuts
are the same. On `enigma` the first 39 cuts match bit for bit, and the two
runs part inside a node's LP before the fortieth: the right-hand side of
an FTRAN has 14 nonzeros as it is and 15 with rows negated.

02-440's `read.sh` on the other sets reads as 02-442: Netlib 94, its
infeasible set 29, Kennington 16 and CBLIB's continuous 29 the same both
ways, Maros-Meszaros 116 and 22, CBLIB's mixed-integer 31, 48 and 1.

## The gates

`make test`, `make sanitize` and `make python-test` pass. `make netlib
netlib-infeas`: 0 regressed. `make miplib`: 0 regressed, and its file
does not move. `make miplib2017`: 5 solved and 28 incumbents as before,
the gap sum 13.4820 against 13.4804 (02-328's `m17sum.py`); 12 lines move
in work units. The QPLIB MIQP gate reads as its
record apart from work units and QPLIB_4270, which has differed from that
record since 02-415.
