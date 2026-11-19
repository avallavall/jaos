# 02-505 — where a dense root LP's iteration goes, and U's columns in one block

Taken on 2026-11-19 on ccef8dba and fd97794e, for J36's next step: a JAOS
iteration on the dense root LPs costs 3.9x to 4.7x HiGHS's (02-501).

## The profile

`prof.sh` builds ccef8dba with `-pg -fno-inline`, solves `mzzv11`'s root LP
alone for 60 s (02-503's root-only settings), and reads gprof
(`gprof-mzzv11.txt`, by line in `gprof-mzzv11-lines.txt`). It ran 26,615
iterations:

| function | share | calls |
|---|---|---|
| `jm_lu_factor`, its own lines | 24.1% | 410, one every 65 iterations, 20 ms each |
| `ftran_u_dense` | 23.6% | 73,261, 2.75 an iteration |
| `jm_lu_btran_in`, `btran_l_pattern`, `btran_u_pattern` | 13.2% | |
| `ftran_prefix` | 6.0% | |
| `price_all` | 5.2% | |
| `compact_pivot_row`, under the factorization | 4.4% | 3,894,590 |

Inside the factorization the time is the elimination's pass over each
active column the pivot row meets (lines 639 to 666). `compact_pivot_row`
spends its share in a linear search of the column, because the positions
the rows keep go stale when the elimination compacts a column. Inside
`ftran_u_dense` it is the scatter `y[idx] -= val * z`. U holds 47,376
entries over 9,499 rows after a factorization, and each update adds a
spike column that holds 56% to 69% of the rows.

The refactorization interval sits near its best for this factorization's
cost. Take an update's spike, S entries at c seconds an entry over 2.75
solves an iteration, and a factorization of F seconds. The interval T that
minimises the per-iteration cost is about sqrt(F / (1.4 S c)). With
F = 20 ms, S = 5,000 and c = 0.53 ns that is near 70, against
`REFACTOR_EVERY`'s 64. Fewer updates between factorizations pay only once
a factorization costs less.

## The change: U's columns in one block

U's columns were each their own pair of arrays. A factorization now lays
them out in one block, in the order the dense forward solve walks them,
with each column's entries in the order they had (`arena.py` applies it
to a tree). An update's spike column, and any column an update makes
grow, moves to arrays of its own. The solves read the same entries in the
same order, so every answer and work unit is the same bit for bit.

The root LP alone, both builds at once (`root3-read.txt`):

| instance | iterations, the same for both | before | after |
|---|---|---|---|
| `mzzv11` | 63,258 | 67.5 s | 61.2 s |
| `dano3_3` | 45,213 | 21.0 s | 20.3 s |
| `qap10` | 22,721 | 5.68 s | 5.48 s |

On the 27 of 02-503's 33 whose root LP is still unfinished at 300 s
(`root27.txt`: the 29 left after the four that 02-503 finished, less
`neos-3402454-bohle` and `square47`, which do not start under a 3 GB cap),
`root.sh` runs both builds of each at once for 300 s. Since the two walk
the same path, the iterations each reaches are its speed
(`root27-read.txt`). After against before reads 1.138x geometric, from
1.003x (`supportcase19`) to 1.418x (`cryptanalysiskb128n5obj14`).
`rail01`'s root LP now ends inside 300 s.

The whole solve at 300 s, 02-501's `pair.sh` on `root27.txt` with both
legs side by side under a load of 2 to 10 (`pair-record.txt`,
`pairsum.txt`): no instance leaves the root or finds an incumbent in either
leg, and where both hold a bound it is the same. `rail01`'s after leg first
ended on out of memory. The build it ran sized the block from
`us_start[dim]`, which a factorization of no rows never writes, and the
landed code sizes it from the entries it holds. Run again on that code,
both legs reach bound -92.0873.

## The gates

On the change over fd97794e: `make test` and `make sanitize` pass.
`netlib`, `netlib-infeas`, `netlib-kennington` and `make miplib` read 0
regressed, 0 improved, 0 new, 0 at other nodes. Every record that
`primal barrier pdlp concurrent warm`, `make miplib` and `make miplib2017`
write is byte-identical to the committed one, and `m17sum.py` reads 11
solved, 29 incumbents and sum 9.2647 before and after.
