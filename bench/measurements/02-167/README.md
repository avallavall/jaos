# 02-167 — ranging over the two feasible sets

D258. The three ranging calls on the published basis of every instance
the solver answers, with the checks `jaos.h` promises: the call
succeeds, no interval end is NaN, every current number lies inside its
own interval, a nonbasic column's cost interval ends exactly where its
reduced cost says, and the two bounds of a row or column never cross
inside their intervals. A refusal is the unscaled refactorization
calling the published basis singular, which is honest; a failed check
would be a wrong interval, which is what the driver exists to find.

## What is here

| file | what it does |
|---|---|
| `ranging-population.c` | solves each `.mps` in a directory, ranges it, runs the checks, prints one line per instance and one total |
| `run-ranging-population.sh` | builds the driver against the working tree in a temp dir outside the repository and runs it on `bench/instances` and `bench/instances-kennington`; writes `ranging-population.txt` beside itself |
| `ranging-population.txt` | the reading |

## The reading

| set | solved | ranged | refused | failed a check | ranging seconds, all | slowest |
|---|---|---|---|---|---|---|
| netlib, 94 | 94 | **93** | 1, `finnis` | **0** | 4.8 | `dfl001`, 1.4 |
| Kennington, 16 | 16 | **16** | 0 | **0** | 65.8 | `ken-18`, 41.5 |

`finnis` is refused because four of its columns are published nonbasic at
a bound the solve lent them (D19), `[0, inf)` boxes at 1e10 to 4e10 with a
zero reduced cost; `TODO.md` carries it as the solve's to fix. The
`nonbasic-end gap` column compares ranging's end for a nonbasic column's
cost, computed from its own unscaled factorization, with the published
reduced cost: 4.07e-10 or below everywhere but the pilot family, where
`pilot-ja` reads 2.14, `pilot-we` 2.25e-3, `perold` 1.32e-5 and `pilot87`
1.59e-6, the conditioning of an unscaled basis. Every interval on every
ranged instance holds its current value.

## The second reading, with the solve's scaling (D260)

The same driver on the tree that factors the basis scaled as the solve
scales it. netlib: 93 ranged, `finnis` refused as before, 0 failed
checks, and the gap column collapses: `pilot-ja` 1.39e-6, `pilot`
1.4e-7, `wood1p` 7.18e-8, `dfl001` 1.78e-8, `pilot-we` 1.42e-8, nothing
else above 1e-8. `ranging-population.txt` is this reading; the first is
in the D258 entry and in the table above.
