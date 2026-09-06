# 02-207 — why `bell5` does not finish under the dive, and the length rule that does not fix it

D316 left one thing standing: the plain dive reads 0.934x over the 23
instances it finishes, none past 2x, and `bell5` alone has no ratio. That
is the whole of D289 now. This directory is the diagnosis and the one
repair it pointed at.

## The diagnosis (`open-set-and-bound.txt`, `probe-bell5.sh`)

`bell5` and `egout`, each with the dive off and on, 150 s cap, one at a
time, tree 30c14b5. `egout` is the control and it is not decoration: it is
an instance the dive **helps**, so if the two read the same shape the
probe is measuring the log and not the dive.

The published bound is the smallest key any open node has. The open set is
the reading:

| | open set, sampled across the run | bound |
|---|---|---|
| `bell5`, no dive | 155 → 16183 → **22659** → 20789 → 2391 | closes, 140595 nodes, 15 s |
| `bell5`, dive | 57 → 190355 → 341819 → 472109 → **580535** | 8961385 to 8963431 in a million nodes, no proof |
| `egout`, no dive | 47 → 177 → 363 → 923 → 403 | closes, 6841 nodes |
| `egout`, dive | 37 → 311 → 455 → 471 → 77 | closes, 6547 nodes |

Without the dive `bell5`'s open set peaks and comes back down, which is
what a search that is closing looks like. With the dive it grows without
stopping and the bound crawls: the dive rarely prunes on this instance, so
its own children pile up while the oldest open nodes go on holding the
bound down. `egout` never passes 500 either way, so the shape is the
instance and not the instrument.

## The repair the diagnosis pointed at, and its reading (`dive-length.txt`, `try-length.sh`)

If the open set grows because the dive never comes back, bound how far it
may go: a dive takes at most `N` children in a row, then the search
returns to best-bound order and the child it did not take joins the open
set, so no node is lost. Implemented, measured, and **reverted** — the
code is not in the tree, and this reading is why.

| | length 0 | 2 | 5 | 10 | 25 |
|---|---|---|---|---|---|
| `bell5` work | 30.3 G | 26.1 G | **25.4 G** | 30.7 G | 30.8 G |
| `bell5` status | time limit | time limit | time limit | time limit | time limit |
| `bell5` first incumbent | node 1849 | **none at all** | 1372 | 1607 | 1849 |
| `egout` work | 59.9 M | **45.3 M** | 77.4 M | 59.9 M | 59.9 M |

**`bell5` does not finish at any length.** The best setting saves 16% of
the work on an instance that needs a factor of at least thirteen, and the
shortest setting costs `bell5` its incumbent entirely: at length 2 the
search runs 240 s and finds no answer, where the plain dive finds one at
node 1849. On `egout` the curve is not a curve — 2 helps, 5 hurts, and 10
and 25 are byte-identical to no length at all, because a dive there is
already shorter than ten children.

## What this closes

The dive has now been measured in seven forms: plain (D289), four child
rules (D295), a backtrack budget (D308), a resume bounded by the gap
(D311), a resume bounded by the child's fall from its parent (D316), and
a length on the dive itself (D317, here). `bell5` finishes under none of
them. D317 closes the question rather than writing another reopen
condition, and says what would have to change for it to be worth
reopening: the instance, not the rule.
