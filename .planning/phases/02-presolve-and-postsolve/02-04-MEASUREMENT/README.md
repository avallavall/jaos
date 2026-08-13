# 02-04's raw readings

A plan whose deliverable is a constant is a plan whose deliverable is a
verdict, and CLAUDE.md requires the readings behind such a verdict to be
committed rather than left in a scratchpad. Everything here is re-derivable
without trusting the summary.

## How to reproduce

```sh
bash .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/sweep.sh eps
bash .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/sweep.sh rounds 1e-9
bash .planning/phases/02-presolve-and-postsolve/02-04-MEASUREMENT/summarise.sh
```

`sweep.sh` runs `make clean` between every setting and builds `canary.c`
against the library it just built, so the canary reading and the campaign
beneath it always come from the same binary. `summarise.sh` rebuilds both
tables from the records in this directory.

Nothing here writes to `bench/results/` or to any baseline: the runner is
invoked with `-o` pointing into this directory, and `-b` only reads a
baseline. `02-07` owns the baseline rewrite.

## The canary, and why there are two of them

`canary.c` carries one model per constant, and neither sweep is evidence
without its own.

- **The round cap's canary** is a chain of 200 singleton rows, each round
  resolving exactly one link. It reads 1, 2, 4, 8, 16, 32, 64, 128 across
  the grid, so the constant demonstrably reaches the binary at every
  setting.
- **The epsilon's canary** is a singleton-row fold that conflicts with its
  column's own bound by 1e-8. It reads INFEASIBLE at 1e-12 through 1e-9 and
  SOLVED at 1e-8 through 1e-4, flipping inside the grid.

The first version of the epsilon canary measured bound tightening, and when
that family was removed the canary went flat — which is exactly the failure
the canary exists to detect, applied to itself. The epsilon sweep was rerun
with a canary that measures what the constant still governs.

## What each file is

| file | what it holds |
|---|---|
| `sweep.sh` | the sweep, both modes; `make clean` between settings is in the script and not in the operator's memory |
| `canary.c` | the two canary models |
| `summarise.sh` | rebuilds both tables from the records below |
| `netlib-eps-*.txt` / `.gate` | the standard set at each epsilon setting, record and runner output |
| `netlib-rounds-*.txt` / `.gate` | the standard set at each round-cap setting |
| `infeas-*.txt` / `.gate` | the infeasible set at the same settings |
| `canary-*.txt` | the canary reading for each setting, kept beside the campaign it validates |
| `build-*.log` | the build for each setting, so a silent build failure cannot look like a flat line |
| `final-netlib*.txt` / `.gate` | all three sets against their committed baselines, on the tree this plan ships |
| `attribution-netlib-02-03.txt`, `attribution-netlib-02-04.txt` | the standard set built at `8425acc` (the tree 02-04 started from) and at 02-04, so every checker rejection has an owner |
| `attribution-kennington-02-03.txt` | the same for the Kennington set |
| `isolate-no-*.txt` / `.gate` | one activity-range family disabled at a time, which is how bound tightening was identified as the family that refuses feasible models |

## What the readings say

Both tables are in `docs/tolerances.md` and beside their constants in
`src/presolve.c`. The three findings that are not in either:

1. **The standard set was already failing before this plan.** `8425acc`
   reads 78 checker ok of 94 and the Kennington set 8 of 16, against 93 and
   16 in the records committed at 02-01. 02-03 scoped its own verification
   to the infeasible set and said so; nobody ran the other two. This plan
   ships 79 and 12.

2. **Bound tightening refuses feasible models, in every variant built.**
   Six designs, nine epsilon settings; `pilot`, `pilot87`, `agg` and `maros`
   come back INFEASIBLE. The family is not shipped and `src/presolve.c`
   carries the table.

3. **`bgindy` moves 2.0x on the infeasible set** and is the only baseline
   movement there. Verdict and determinism are unaffected; it joins the two
   02-03 named.
