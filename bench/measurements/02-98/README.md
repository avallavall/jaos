# 02-98 — no mapped basis arrives long in 101 calls, so the refusal holds and the cheap route to §2 is closed

D186. No source change. A census over both gate sets with 02-90's probe.

## The question

`build_warm_basis` refuses a long count with a premise written into the source:

> "A LONG count is still refused: **no long map has been measured**, and a
> demotion rule for an unmeasured case would be a constant fitted to nothing."

Nobody had counted. This project has had a refusal's premise expire unnoticed
three times (D24, D94, D101), so a premise of the form "no X has been measured"
is worth measuring.

**The hypothesis it was measured against.** §2's rank argument is needed at
POSTSOLVE, which has no factorization, and that is what has made the item look
expensive. `build_warm_basis` runs inside the solver, and its own comment says
rank stays where it already lives — `repair_singular_basis`, which runs
downstream of it. So a demotion HERE would need no new rank machinery, and D179
had already measured the supply it would draw on: 19 of 24 instances covered.

## The answer: there is nothing to demote

| set | calls | exact | short | **long** |
|---|---|---|---|---|
| netlib | 90 | 35 | 55 | **0** |
| Kennington | 11 | 6 | 5 | **0** |
| **both** | **101** | 41 | 60 | **0** |

**The premise holds.** No mapped basis arrives long anywhere, so a demotion rule
in `build_warm_basis` has no population to act on and the route above is closed.

## The published basis and the mapped basis move in opposite directions

D179 counted 24 netlib instances publishing a basis one or more members **too
long**. This counts **0** mapped bases too long, and 55 too short.

`fit1p` is the clearest single case: it publishes a basis **over by 21** and its
map arrives **short by 241**. The two are different objects with different
mechanisms — `SINGLETON_COL` adds a BASIC at postsolve without freeing a slot,
while presolve's mapping drops every stored-basic member presolve removes again
— and nothing that repairs one touches the other.

## What the census gives that nobody had: the cap's price per instance

| set | short past `WARM_REPAIR_MAX_SHORT = 4` | share |
|---|---|---|
| netlib | **35 of 90** | 39% |
| Kennington | **0 of 11** | — |

**35 netlib warm starts fall back to a cold solve** because their shortfall is
past the cap. Kennington loses none: all five of its short maps are within it,
which is D151's own finding that Kennington does not vote on the value.

The worst shortfalls:

| instance | mapped short by |
|---|---|
| `sctap3` | **596** |
| `sctap2` | 432 |
| `dfl001` | 343 |
| `seba` | 331 |
| `fit1p` | 241 |
| `fit2p` | 237 |

**One figure disagrees with D149 and it is recorded rather than resolved.** That
entry refused the blanket repair on `dfl001` "paying 172x for a **596**-short
repair". Today `dfl001` is 343 short and `sctap3` is 596. D149 is 2026-08-19 and
many `src/` commits have landed since, so the tree may have moved under it;
re-running at that tree is what would tell, and it was not done here.

## Reproducing

```
bench/measurements/02-98/run-mapped-census.sh      # both sets, -j 1, ~35 min
```

`-j 1` is load-bearing: the DIAG lines go to stderr and belong to the instance
named just before them. The script aborts when no `DIAG-MAPPED` line appears at
all, because a table of zeros from a probe that never ran reads exactly like
"no long maps".
