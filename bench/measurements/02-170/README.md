# 02-170 — what the relative row window is still carrying, after the loans went

D263. No source change. Three probes over the standard set, each run
against the parent tree and the candidate, because D261 moved the instance
`docs/tolerances.md` used as its worked example everywhere.

## What is here

| file | what it does |
|---|---|
| `row-traffic.c` / `run-row-traffic.sh` | one instance's rows: the traffic `sum_j \|a_ij x_j\|`, the activity, the residue, one ulp of the traffic, and the checker's whole report. Named rows on the command line |
| `row-census.c` / `run-row-census.sh` | the same figures for every instance of a set, one line each; writes `row-census-<tag>.txt` |
| `window-need.c` / `run-window-need.sh` | the question itself: how many rows would an ABSOLUTE complementary-slackness window refuse that the relative one admits? Writes `window-need-<tag>.txt` |

## The reading

`run-window-need.sh` over the 94 standard instances, at `tol = 1e-6`:

| tree | instances with a row the absolute window refuses | worst |
|---|---|---|
| parent, `642f71a` | **1** — `finnis`, row 0 and row 3 | 3.389 windows, row 0 |
| candidate | **0** | 0 |

`finnis` row 0 sat 3.38884e-06 from its bound on a row carrying 8e+10, with
a multiplier of 16.17; row 3 sat 1.51815e-06 from its bound on 4e+10, with a
multiplier of 27.996. Both are the numbers `docs/tolerances.md` was written
from, and both are the recomputed activity of a row whose terms include four
columns published at 1e10 on bounds the solve had lent them (D261).

After the retirement `finnis` row 3 carries 7733.97 and rests exactly on its
bound. **No instance on the standard set now demonstrates the relative
window.**

## What that does and does not settle

It does not argue for removing the window. The window exists because a row
activity is a sum whose terms cancel, which is a fact about arithmetic and
not about this population, and D24 is the precedent: a quantity that is
quiet on today's instances is not evidence that the guard against it is
dead. What it settles is that the *worked example* in
`docs/tolerances.md` had to move to the past tense, and that
`run-window-need.sh` is what re-asks the question on any future tree.

## The two other figures the doc read from `finnis`

| what the doc said | parent | candidate | the instance that carries it now |
|---|---|---|---|
| the gap against its two halves | gap 2.21e-10 over 1.05e-04 and 2.89e-05 | gap 1.20e-16 over 6.44e-11 and 1.63e-11 | **`grow22`**, gap 1.99e-13 over 6.41e-05 and 1.24e-07 |
| the worst absolute row residue, against the 1e-6 bar | 8.44e-07, 16% of margin left | 1.58e-13 | **`greenbea`**, 4.66e-08 on a row carrying 6.5e+05, 95% of margin left |

`row-census-parent.txt` against `row-census-candidate.txt` differs on
**one line**, `finnis`, so nothing in this table is credited to a commit
that did not do it.
