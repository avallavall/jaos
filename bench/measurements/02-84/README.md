# 02-84 — `pilot`'s 2.31e-05 is `DUAL_TOL`, and what tightening it would cost

D174. No source change. The diagnosis D173 handed to `TODO.md`, and the sweep
that says what the obvious repair is worth and what it breaks.

## The mechanism, from the source

`DUAL_TOL` is not only "the width of the Harris window", which is all
`docs/tolerances.md` says about it. It is **what the solve calls zero for a
reduced cost**: `dual_breach`, `published_breach`, `settled_dual_violation`
and `points_outwards` all read `s->dual_tol`, and the comment beside
`dual_breach` states the rule — "a breach this solver's own arithmetic cannot
distinguish from zero is not evidence of anything".

A reduced cost is a rate, not a value. What a column is still worth is `d_j`
times the distance it would travel, and `DUAL_TOL` bounds `d_j` alone. On
`pilot` a rate under 1e-7 in scaled space is worth **2.31e-05** of objective,
which is the whole gap.

## Presolve is not involved, and neither is `PRIMAL_TOL`

`tolerance-sweep-pilot.txt`, two binaries built in separate directories so
neither can be the other's objects.

| | `-DJAOS_NO_PRESOLVE` | shipping |
|---|---|---|
| default | 2.312e-05 | 2.312e-05 |
| `dual_tol = 1e-11` | **0** | **0** |

`PRIMAL_TOL` moves nothing: 1e-13, 1e-12, 1e-11, 1e-10, 1e-9, 1e-8, 1e-6 and
1e-5 all publish the same 2.312e-05 on the shipping build.

## `pilot` against the dual tolerance alone

| `dual_tol` | gap to Koch | work |
|---|---|---|
| 1e-6 | **`numerical error`** | — |
| **1e-7 — the default** | **2.312e-05** | 1.0000x |
| 1e-8 | 2.312e-05 | 0.9932x |
| 1e-9 | -5.266e-09 | **0.9134x** |
| 1e-10 | **0** | 1.5811x |
| 1e-11 | -5.266e-09 | 2.4777x |

The default sits one step from a cliff on the loose side: at 1e-6 `pilot` and
`pilot87` both fail outright.

## All four of D173's instances, at 1e-9 — `dual-tol-sweep-netlib.txt`

| | gap at the default | gap at 1e-9 | work at 1e-9 |
|---|---|---|---|
| `pilot` | 2.312e-05 | -5.266e-09 | **0.9134x** |
| `pilot87` | 1.044e-07 | **0 — exact** | **0.9202x** |
| `scsd6` | 1.118e-09 | **0 — exact** | 1.0807x |
| `etamacro` | 1.315e-08 | -1.137e-13 | **0.9934x** |

Three of the four cost **less** work. In units of `eps * sum |c_j x_j|`, which
is the scale D173 established, `pilot` goes 1.87e+08 → 4.25e+04, `pilot87`
1.53e+06 → 0, `scsd6` 9.97e+04 → 0, `etamacro` 2.74e+04 → 0.24. **No other
instance on the set changes materially in either direction.**

`pilot87` is `TODO.md`'s D92 backlog row, "suboptimality bound, not
understood". At any tolerance from 1e-8 to 1e-13 it publishes Koch's optimum
exactly, and every one of those settings costs less work than the default.

## The control, first, because nothing below means anything without it

`jaos_set_dual_tolerance(m, 1e-7)` names the built-in default. Over all 94
netlib instances the two rows agree on **status, objective, iterations, work
and digest, with 0 differing**. The setter reaches what the solve reads, and
the sweep is not measuring one binary seven times — the trap that broke three
of five build configurations for a session (D154). It needs no rebuild at all
because `DUAL_TOL` is one of the two constants a caller owns (D64).

## The whole cost, over all three sets

netlib, over the instances optimal at both settings:

| `dual_tol` | fails | digests moved | work geomean | past the gate's 2.0x |
|---|---|---|---|---|
| 1e-6 | `pilot`, `pilot87` | 29 | 0.9840x | 2, worst `grow22` 2.14x |
| **1e-7 default** | none | 0 | 1.0000x | 0 |
| 1e-8 | none | 32 | 1.0344x | 5, worst `d2q06c` 4.98x |
| **1e-9** | **none** | **35** | **1.0339x** | **5, worst `d2q06c` 5.32x** |
| 1e-10 | `dfl001` | 35 | 1.0561x | 6, worst `d2q06c` 5.06x |
| 1e-11 | `dfl001`, `wood1p` | 37 | 1.0183x | 5, worst `d2q06c` 5.05x |

`netlib-infeas`: **all 29 still refused at every setting**, work geomean
1.0070x at 1e-9 and inside 0.7% everywhere.

Kennington: no failures at 1e-8 or 1e-9, **3 digests move**, work geomean
0.9803x at 1e-8 and 1.0976x at 1e-9 — where the whole of it is `pds-20` at
**4.815x**, against 0.953x for the same instance at 1e-8.

**1e-8 does not fix `pilot`.** It leaves the gap at 2.312e-05, unchanged. So
the cheap setting is not a smaller version of the repair; 1e-9 is the first
one that reaches this defect at all.

## Why the 2.0x bar cannot arbitrate this, and why it still blocks it

Six instances across the three sets cross the gate's per-instance work bar at
1e-9, so **the gate would report `6 regressed` and go red.** That is the fact.

What the ratios are made of is a separate question, and the sweep answers it:
they are not monotone in the tolerance.

| | 1e-6 | 1e-8 | 1e-9 | 1e-10 | 1e-11 |
|---|---|---|---|---|---|
| `grow22` | **2.14x** | 3.00x | 1.49x | **0.22x** | 0.22x |
| `greenbea` | 1.06x | 2.53x | **1.15x** | 3.28x | **0.99x** |
| `nesm` | 1.24x | 1.10x | **2.17x** | 1.21x | 1.00x |
| `pds-20` | — | **0.953x** | **4.815x** | — | — |
| `d2q06c` | **2.01x** | 4.98x | 5.32x | 5.06x | 5.05x |

`grow22` and `d2q06c` cross 2.0x when the tolerance is **loosened** to 1e-6,
where the answers get worse — no accuracy argument explains that. `greenbea`
swings by a factor of three between adjacent settings. **The bar is detecting
a changed pivot sequence, and this change moves 38 of them.** Only `agg3`,
`d2q06c`, `perold` and `pilot-ja` stay high once the tolerance is tight, and
they are the part of the cost that is real.

## What is NOT proposed here

**Nothing in `src/` was changed and the default stays at 1e-7.** Lowering a
shipped default is a change to the contract every caller already has (D64),
it turns the gate red on six instances, and 1e-9 sits one step from `dfl001`
failing at 1e-10. The sweep is what a decision needs, not the decision.

`TODO.md` carries the candidate with what it needs. `docs/tolerances.md`
gains the sweep, because `DUAL_TOL` was one of the constants shipping without
one.

## Reproducing

```
bench/measurements/02-84/run-tolerance-sweep.sh pilot     # ~15 min, 2 builds
bench/measurements/02-84/run-dual-tol-sweep.sh            # netlib, ~35 min
bench/measurements/02-84/run-dual-tol-sweep.sh bench/netlib-infeas.manifest
bench/measurements/02-84/run-dual-tol-sweep.sh \
    bench/netlib-kennington.manifest 0,1e-8,1e-9          # ~45 min
```

Both probes are sequential on purpose: work units are deterministic, so
parallelism would buy wall clock only, and a shared stderr under `-j` is how
three readings of one counter came out different.

## A defect in the probe, found and fixed before anything read it

`jaos_solve_status_str` returns `"numerical error"`, with a space, so the
failing rows carried **one extra field and every column after it shifted**.
The first netlib and infeasible runs are discarded; both sets were re-run with
a single-token status. Nothing in this record comes from the shifted rows.
