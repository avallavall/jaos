# 02-148 — four `presolve.c` contracts become asserts

D235. Four of `presolve.c`'s prose contracts, from
`bench/measurements/02-121/presolve.c.md`.

## What is here

| file | what it does |
|---|---|
| `run-presolve-population.sh` | the four asserts over all 139 instances, with a canary |
| `run-presolve-controls.sh` | the edit that fires each one, over the 94 standard instances |
| `population.txt` | the population run, as run |
| `controls.txt` | the arms, as run |

Both derive the repository root and run from anywhere (D217). Neither is a
gate tool.

**Every solve stops as soon as presolve returns.** These are presolve
asserts, presolve runs once at the top of every solve, and the simplex is
what makes an assert-enabled Kennington cost fifty minutes (02-145). With the
patch it is about a minute. The runner then reports every instance as failed,
because a solve that returns after presolve has no solution; that is expected
and is not the signal. **The only signal is whether an assert fired**, which
is why both scripts carry a canary.

## The four

| the contract | the check |
|---|---|
| `rounds` never passes the structural backstop | `p->counts.rounds <= nr + nc + 1` at loop exit |
| `row_traffic` is a magnitude | `row_traffic[i] >= 0.0` at every row, at the end |
| an already-infinite row end is not subtracted from | `lo_absorbs \|\| !isfinite(cur_rl[i])` after the singleton-col shifts |
| the empty row's bound-scale fallback is unreachable | `isfinite(row_traffic[i])` before the ternary |

## One of them was written wrong, and the population run caught it

The third started as an **equality**: `lo_absorbs == isfinite(cur_rl[i])`,
on the reasoning that this branch has finite column bounds so the shift keeps
a finite end finite. That reasoning is wrong. `!free_col` means at least one
column bound is finite, not both, so a column boxed at `[0, +inf)` makes one
of `cmin`/`cmax` infinite and turns a finite row end infinite.

**It fired 58 times on the first population run.** The contract only ever
claimed the other direction — an end that was *already* infinite is not
subtracted from — and that is what the assert says now. The false converse is
recorded beside it with its firing count.

## The fourth needed an inverted assert, not a defect

`isfinite(row_traffic[i])` states that a fallback is unreachable. An assert
that is never evaluated is also never violated, so a quiet run would have
proved nothing. The `fallback-canary` arm flips it to
`!isfinite(row_traffic[i])` and requires that to fire.

**It fires on 5 of the 94 standard instances.** So the branch is reached, the
budget is finite every time it is, and the comment D155 left behind is a
measurement now rather than a claim.

## The arms

| arm | firings | what fired |
|---|---|---|
| `intact` | 0 | — |
| `rounds` | 94 | the backstop |
| `traffic-sign` | 37 | the magnitude |
| `absorbs` | 12 | the one-way implication |
| `fallback-canary` | 5 | the inverted fallback assert |
| `restored` | 0 | — |

The arms run the 94 standard instances; the population run covers all 139.

## Reproducing

```
bash bench/measurements/02-148/run-presolve-population.sh   # ~3 min
bash bench/measurements/02-148/run-presolve-controls.sh     # ~8 min
```
