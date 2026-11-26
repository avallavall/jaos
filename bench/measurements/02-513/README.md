# 02-513 — the dual's primal infeasibilities: a row a step leaves alone keeps its own, and each row keeps its basic variable's bounds

Taken on 2026-11-25 and 26 on 76917545, for J36's root LP: the same
pivots in less time.

## Where the time went

02-510's gprof of `ex9`'s root LP (`../02-510/rootlp-read.txt`) puts 9.4%
of the time in `pviol_set`: 575,845,909 calls over 14,756 pivots, 39,000 a
pivot on 33,846 rows. A pivot whose column is dense, and the bound flips'
dense pass, set every row's infeasibility again, and each set reads the
basic variable's bounds through `basis[i]`, scattered over every column
and row.

## The change

`src/simplex.c`:

- In a pivot whose column is dense and in the flips' dense pass, a row
  whose entry is 0 keeps its infeasibility. Its value moves by a signed
  zero at most, which sets no infeasibility bit, and `price_row` reads a
  row's infeasibility and side only where its bit is set. The debug check
  in `price_row` compares the bit on every row and the value and side on
  the rows that have it.
- Each row keeps the bounds of its basic variable (`blo`, `bup`), written
  where the infeasibilities are rebuilt and at the pivot's row, and
  `pviol_set` reads them in row order. Every other write to the bounds or
  the basis in the dual clears `pviol_ok`, so the next pricing rebuilds
  them; debug builds check the kept bounds against the variable's at every
  set.

Before the gates, on copies of the tree, MIPLIB 2017's manifest at a work
limit of 3e8 and Netlib read the same records as 76917545 but for the
seconds.

## The readings

`build.sh` builds 76917545 (`base`), the change (`pvb`) and the change with
`udiv.py` (`pvu`, below). `time.sh` runs the three at once on each model,
the machine otherwise idle, three rounds (`time-read.txt`): every round of
every model takes the same iterations and work units in all three.
Seconds summed over the rounds:

| model | iterations | base | change | ratio |
|---|---|---|---|---|
| `chromaticindex1024-7`, root LP to 4e9 work units | 31,400 | 83.80 | 78.65 | 1.065 |
| `ex9`, root LP | 20,347 | 111.17 | 109.74 | 1.013 |
| `ken-18` | 54,600 | 48.36 | 48.40 | 0.999 |
| `pds-20` | 34,445 | 31.47 | 31.37 | 1.003 |
| `osa-60` | 6,433 | 36.30 | 36.65 | 0.990 |
| `osa-30` | 2,645 | 5.30 | 5.07 | 1.044 |
| `cre-b` | 7,813 | 2.47 | 2.57 | 0.960 |

`cre-b`'s three rounds take under a second each. `small.sh` runs `base`
and the change at once on six of Kennington's short LPs, ten rounds
(`small-read.txt`): `cre-b` 1.014x, `cre-d` 1.022x, `ken-11` 1.009x,
`cre-a` 1.000x, `pds-10` 0.997x and `ken-13` 0.993x.

`root.sh` runs 12 of 02-505's root LPs that do not end in 300 s for 120 s,
the three builds of two models at once, six processes, as `run.sh -j 5`
loads the machine (`root-read.txt`). Iterations reached against `base`:
1.034x geometric over the 11 that reach the limit,
`chromaticindex1024-7` 1.115x, `cryptanalysiskb128n5obj14` 1.086x, `ex10`
1.053x, `irish-electricity` 1.047x, `neos-631710` 1.036x, `s100` 1.024x,
`supportcase19` 1.022x, the other four within 0.3%. `ns1116954` ends its
root LP in all three.

## The division skipped

`udiv.py`: the dense forward solve with U skips the division at a zero
entry and gives the zero the sign the division would. Where it runs the
factor has full rank and an update refuses a tiny diagonal, so no
diagonal is 0 and every answer is the same. 02-510's profile put 18.9% of
`ex9`'s time in that solve. Against the change alone it reads 0.952x on
`chromaticindex1024-7` idle (82.63 s against 78.65, slower in each round),
0.996x on `ex9`, and 0.999x geometric under load (`s100` 0.976x). It is
left out (`ftran-u-skip-zero-division` in `bench/refusals.txt`).

## The gates

`make test` and `make sanitize` pass. `netlib`, `netlib-infeas`,
`netlib-kennington` and `miplib` read 0 regressed, 0 improved, 0 new, 0 at
other nodes, and their records and those of `primal barrier pdlp
concurrent warm` are byte-identical.
