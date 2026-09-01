# 02-150 — two bound contracts at the start of a solve

D237. Two contracts from `bench/measurements/02-121/simplex.c.md`.

## What is here

| file | what it does |
|---|---|
| `run-bound-controls.sh` | six arms, each over all 139 gate instances |
| `controls.txt` | the arms, as run |

Derives the repository root and runs from anywhere (D217). Not a gate tool.

## The two

| the contract | where | the check |
|---|---|---|
| infinities survive: no bound changes side | `sx_init` | `isfinite(s->lo[v])` matches the model's, per variable |
| a loan only ever replaced an infinity | `build_initial_basis` | a faked end always holds the artificial value |

The first is what makes every downstream `isfinite` test mean what it says.
Every scale factor is a positive power of two (D223), so scaling moves a
bound's magnitude and never its finiteness. **A factor that reached zero would
turn a real bound into an absent one and the solve would quietly stop
enforcing it**, with no gate predicate able to see it.

## They cost a minute to judge, not fifty

Both run once per solve and before any iteration, so every solve stops as soon
as the starting basis is built. An assert-enabled Kennington costs about fifty
minutes when the simplex runs too (02-145); this is about one.

## The canary caught the stop point being in the wrong place

The first version stopped after `sx_init` returned. The loan assert fired
nothing — **and so did its inverted canary**, which is the signature of a check
that is never reached rather than one that holds. `build_initial_basis` runs
later, from the warm/cold choice inside `jm_dual_simplex`'s retry loop.

Without the canary this would have been written up as "holds on 139
instances" for an assert no instance executed. The stop point is past the
basis build now.

## The arms

| arm | firings | what it says |
|---|---|---|
| `intact` | 0 | both hold on all 139 |
| `canary-scale` | 129 | `sx_init`'s loop is reached on 129 instances |
| `canary-loan` | 129 | `build_initial_basis`'s loop is too |
| `break-scale` | 129 | column 0 scaled by zero |
| `break-loan` | 1 | the loan recorded without moving the bound |

`break-loan` reaches only one instance because it needs a column with an
absent lower bound and a positive cost, which is the shape the loan exists
for. One is enough: the arm proves the assert catches it.

## Reproducing

```
bash bench/measurements/02-150/run-bound-controls.sh   # ~8 min
```
