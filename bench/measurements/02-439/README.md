# 02-439 — the sense flip on the benchmark files

Taken on 2026-10-22 on the tree of 0c74a87. `read.sh` takes the reading
over the files and `sym.sh` the one over generated models, with the
library given. `*-head.txt` are 0c74a87's records and `*-new.txt` the
change's; a set with one record gave the same bytes on both.

## The harness

`flipfile.c` is 02-433's sense flip on files. Each model is solved as it
is, then with its costs, its quadratic objective and its offset negated
and its sense the other one. Negating a double is exact, so both solves
should take the same path: the same status, objectives of opposite sign,
the same iterations, work units and nodes. A line says `same` when every
number matches, `path` when the status and objective match but a count
differs, and `DIFF` otherwise. Each solve stops at 2e9 work units.

`gensym.py` writes integer programs with 2 to 4 groups of 2 to 6
interchangeable columns. The columns of a group share their cost, upper
bound and coefficients, and the groups' costs differ. `sym.sh` runs
`flipfile.c` on 2000 of them at 2e8 work units each.

## The reading on 0c74a87

| set | files | same | path | DIFF |
|---|---|---|---|---|
| Netlib | 94 | 94 | 0 | 0 |
| Netlib infeasible | 29 | 29 | 0 | 0 |
| Kennington | 16 | 16 | 0 | 0 |
| Maros-Meszaros | 138 | 138 | 0 | 0 |
| CBLIB continuous | 29 | 29 | 0 | 0 |
| CBLIB mixed-integer | 80 | 80 | 0 | 0 |
| MIPLIB 3 | 24 | 22 | 2 | 0 |
| `gensym.py` | 2000 | 1873 | 125 | 2 |

`gen` and `p0201` end at the same objective with the same iterations
and nodes, and their work units differ: 3,930,556 against 3,875,884, and
60,943,518 against 61,010,828. With each MIP switch turned off in turn,
only `mip_orbital` off makes the two counts equal.

The symmetry search gives each column a colour from its cost, bounds,
diagonal quadratic term and kind, numbered in ascending order of those
keys, and the quadratic terms' labels the same way. It then splits the
first cell with more than one member, in colour order. Negated costs
reverse that order, so the maximisation splits another cell first and
the search counts other work. On `gen` and `p0201` it ends with the same
generators. On the generated models it does not always: of the 125
`path` lines, 53 take a different number of nodes (`sym0214` takes 1703
nodes as it is and 323 negated), 14 a different number of iterations
and 58 only different work. The two `DIFF` lines are `sym0362` and
`sym1152`, where one solve proves the optimum and the other stops at
the work limit.

## The change

The symmetry graph reads each cost, diagonal quadratic term and
quadratic objective term times the sense sign, which is the value the
minimisation sees. On a minimisation the sign is 1 and every key is the
one 0c74a87 builds, so a minimisation takes the same path as before.
`test_a_maximisation_searches_the_symmetry_as_its_minimisation` in
`tests/test_mip.c` is `sym0933`: on 0c74a87 the maximisation takes 4
nodes and the minimisation 2.

## The reading on the change

Every set but MIPLIB 3 gives 0c74a87's bytes. MIPLIB 3 is 24 `same`
(`miplib-new.txt`), and the 2000 generated models are 2000 `same`
(`sym-new.txt`). On all 2000 the minimisation's iterations, work units
and nodes are 0c74a87's.

## The gate

`make test` and `make sanitize` pass. `make miplib J=2` solves 24 of 24
with 0 regressed, and `bench/results/` is byte for byte the same.
