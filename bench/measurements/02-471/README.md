# 02-471 — the items search with an LP over the columns that link rows

Taken on 2026-11-17 on ec95542 with this change, for TODO row J32.
`read.sh` takes every reading from a copy of HEAD, one solve at a time:
- `miplib3/landed.txt`;
- `full/landed.txt` and `full/sum.txt`;
- `budget.txt`, from a second copy with `trace.py`'s prints;
- `prototype.txt`, from `prototype.sh`.

`full/before.txt` is 02-469's record.

## The shape

02-469's search covers the models of 02-463's items family whose
continuous columns each sit in one row. Four are left, and in each the
columns outside the items link rows. At the root, after presolve:

| model | partitioning rows | LP columns | of them integer | LP rows |
|---|---|---|---|---|
| `glass4` | 36 | 19 | 0 | 360 |
| `graphdraw-domain` | 45 | 74 | 20 | 340 |
| `assign1-5-8` | 26 | 26 | 0 | 130 |
| `csched007` | 50 | 299 | 0 | 299 |

- `glass4` places 9 rectangles: each pair's row picks a relative position,
  and big-M rows tie the pair's coordinates. Its 19 coordinates and length
  sit in 9 to 90 rows each.
- `graphdraw-domain` picks one of four sides for each of 45 pairs of
  nodes, with 20 integer coordinates in 1 to 34, 34 distances between
  nodes and 20 distances to the centre.
- `assign1-5-8` assigns 26 points to 5 groups; each point's cost column
  sits in one row per group.
- `csched007` picks a start for each of 50 jobs; a resource profile runs
  through a chain of equality rows.

Given the items, those columns' cheapest values depend on each other, so
02-469's closed form does not price them. A bound propagation would when
no cost is negative and each row, read as a lower bound, raises at most
one of those columns, and read as an upper bound, at most one: the least
point is then the optimum. That holds on `assign1-5-8` and `csched007`, but
221 of `glass4`'s big-M rows raise two coordinates at once, and 17 of
`graphdraw-domain`'s rows two distances. So the search here prices each
move with an LP over those columns.

TODO's first idea, a run of moves with those columns held and an LP after
the run, was not built. Held at a vertex, the columns leave rows tight
that most moves would break, which is why 1-opt changed almost nothing
(`one-opt`, 02-468); that was not measured here.

## The prototype

`prototype.py` solves one LP per move through the Python binding. The
first phase starts from each row's largest item in the relaxation and walks
on the violation; the second searches by threshold accepting from the
first point; the best assignment is then fixed and the model solved, a MIP
when general integers are left. `prototype.txt`, seeds 1 to 3, the
objective after that last solve:

| model, optimum, JAOS at 1e10 before | 500 LPs | 1000 LPs | 4000 LPs |
|---|---|---|---|
| `glass4`, 1.2000e9, 5.900e9 | 2.60e9, 2.67e9, 2.27e9 | 1.93e9, 2.33e9, 2.00e9 | 1.83e9, 1.90e9, 1.88e9 |
| `graphdraw-domain`, 19686, 28362 | 24174, 25095, 26814 | 23190, 22508, 26806 | 23717, 23018, 22848 |
| `assign1-5-8`, 212, 212 | 214, 214, 216 | 212, 215, 217 | 212, 214, 216 |
| `csched007`, 351, 462 | no point | no point | no point |

The first phase reaches zero after 106 to 253 LPs on `glass4`, 135 to 293
on `graphdraw-domain` and 37 to 56 on `assign1-5-8`. On `csched007` it
stays at 6, and at 10 to 12 from a greedy start that tries each item of
each row in a seeded order. On `graphdraw-domain` the LP's coordinates
come out fractional, and the MIP over them adds 0 to 214 to the LP's
objective.

## What was built

`link_for_point` in `src/mip.c` runs once at the root of the main tree,
after 02-469's search, never in a sub-MIP.
- **Detector:** 02-469's, with two changes. A general integer column in
  no partitioning row is allowed, and continuous columns may sit in any
  number of rows. Some continuous column must sit in two or more rows, or
  some such integer column exist, so the two searches never share a model.
- **The LP:** the free columns other than the items, integers relaxed,
  over the rows they sit in, with the items' activity moved into the row
  bounds. One elastic column per finite row bound prices the violation in
  the first phase and is fixed at zero in the second. Each solve starts
  from the basis of the last optimum, without presolve. Rows with no such
  column are hard and priced in closed form, as in 02-469.
- **Start:** the incumbent, or else each row's largest item in the
  relaxation.
- **First phase**, with no incumbent: a move is taken when the hard rows'
  violation plus the elastic LP's optimum does not rise. The phase gives
  up after `MIP_LINK_STALL` LPs per partitioning row without a fall.
- **Second phase:** 02-469's moves and threshold, the first threshold the
  median rise of `MIP_LINK_SAMPLE` moves. A move that raises the hard rows'
  violation is refused before its LP.
- **Budget:** `MIP_LINK_WORK` times the model's size, the LPs' work
  included.
- **Result:** the best assignment has its items fixed and the rest solved,
  as a sub-MIP over the general integer columns when there are any.

`budget.txt` traces the run at each root:

| model | start | first point | best | LPs | work | last point at |
|---|---|---|---|---|---|---|
| `glass4` | the relaxation, zero after 83 LPs | 3.200e9 | 1.875e9 | 2,259 | 1.27e8 | 0.94 of the budget |
| `graphdraw-domain` | the jump's 28362 | 26263 | 20486, 20616 after the sub-MIP | 4,584 | 1.86e8 | 0.63 |
| `assign1-5-8` | the jump's 224 | 224 | 214 | 18,890 | 2.02e8 | 0.09 |
| `csched007` | the relaxation, 10 after 221 LPs | none | | 221 | 4.0e6 | |

An LP costs about 56,000 units on `glass4`, 41,000 on `graphdraw-domain`
and 11,000 on `assign1-5-8`, the run's work over its LPs. The run takes
92%, 82% and 57% of their roots' work at `--node-limit 1`.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: 1.005x, no instance changes
status, and none has the shape. `bell5` (1.00x, 21,034 to 20,971 nodes)
and `misc07` (1.02x, 9,485 to 9,557) end at other nodes, moved by the
work the detector adds at the root.

MIPLIB 2017 at 1e10, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-469) | 6.5397 | 6.5166 | 13.0562 |
| this change | 5.9623 | 6.5139 | 12.4762 |

The models of the shape:
- `glass4`: incumbent 5.900e9 to 1.875e9, 1.0 to 0.56 of the optimum
  away, the root's point;
- `graphdraw-domain`: 28362 to 20616, 0.44 to 0.047, and its dual gap
  0.304 to 0.306;
- `assign1-5-8`: the same incumbent, its optimum, and its dual gap 0.091
  to 0.090;
- `csched007`: no point at the root, and its tree, moved by the 4.0e6
  units the first phase spends, ends at 539 instead of 462: 0.32 to 0.54,
  its dual gap 0.148 to 0.144.

The other instances move only by the work the detector adds:
`neos-3754480-nidda` 0.053 to 0.083, `neos-3046615-murg` 0.077 to 0.083,
`supportcase26` 0.011 to 0.008, `tr12-30` 0.0004 to 0.0006.

`glass4` and `graphdraw-domain` give -0.83, `csched007` +0.22 and the
rest +0.03.

## On top of 02-470 and 02-472

These readings were taken on ec95542. The change landed after 02-470's
moves over big-M disjunctions and 02-472's search over a schedule's
starts. In the root's order it runs after 02-470's moves, from the
incumbent they leave. Both sets were taken again on that code, one solve
at a time:
- `miplib3/combined.txt`, against `bench/miplib.baseline` as 02-472
  re-based it;
- `full/combined.txt`, against `full/before-combined.txt`, 02-472's
  record.

MIPLIB 3 reads 0.998x. Four instances end at other nodes, all within
0.98x to 1.04x: `flugpl` 1.03x, `bell5` 1.01x, `misc07` 0.99x and
`p0282` 0.99x.

MIPLIB 2017's gap sum goes from 10.9142 to 10.8494:

| instance | primal gap | dual gap |
|---|---|---|
| `glass4` | 0.500 to 0.250 | 0.333 to 0.250 |
| `graphdraw-domain` | 0.056 to 0.100 | 0.303 to 0.299 |
| `assign1-5-8` | | 0.091 to 0.091 |
| `csched007` | 0.154, the same, from 02-472's root point | 0.138 to 0.139 |

The other instances move only by the work the detector adds:
- worse: `timtab1` 0.429 to 0.689;
- better: `mas74` 0.020 to its optimum, `neos-3046615-murg` 0.058 to
  0.046, `supportcase26` 0.018 to 0.016.

## The verdict

Landed, on by default with the rounding heuristics. `--no-heuristics`
turns it off. TODO row J32 keeps the cost of its LPs.
