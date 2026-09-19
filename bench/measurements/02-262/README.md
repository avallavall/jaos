# 02-262 — cones the conic walk leaves out

## What went wrong

The 1e11 reading of CBLIB's 80 mixed-integer instances after 02-261 ended
turbine07 `NUMERICAL_ERROR`. Since 6e6f0c5 a node whose walk ends at a
certificate the checker refuses is no longer called infeasible. The
conic tree splits such a node at its widest integer column, and a node
whose integer columns are all fixed cannot be split, so the tree ended
there. 957 of its 1413 nodes had been split that way.

The nodes were infeasible. turbine07's first cone, `(x0, x1, ..., x8)`,
has a head that is free above, costs nothing and appears in no row and
no other cone. Such a cone never binds, so its dual has to be 0, and an
interior point walk cannot reach that from inside. On an infeasible node
the walk's certificate keeps a small multiplier on that head, 1.5e-11
against a certificate of order 1e5, and the checker needs the free
column's coefficient to vanish. The cleanup that zeroes entries below
`CONIC_RAY_ZERO` of the largest then breaks another column (0.6 against
a traffic of 4190). With that one cone deleted, the three failing nodes
dumped from the tree end `INFEASIBLE` with a certificate the checker
takes.

A second kind of cone showed up on the same nodes: where the branching
puts an integer head's upper bound at 0, the cone holds all its columns
at 0 and has no interior point.

## The change

`conic_solve` (`src/conic.c`) leaves two kinds of quadratic cone out of
the walk:

- **idle**: the head is free above, costs nothing, has no matrix entry
  and no quadratic term, and appears in no other cone. The walk holds the
  head at its lower bound, or at 0 when it has none. After the walk the
  head is set to the larger of its lower bound and the norm of the rest,
  and the cone's dual is 0. A ray's head is the norm of the rest.
- **dead**: the head's upper bound is 0, every member's box holds 0, and
  no member is the head of a cone. The walk fixes all its columns at 0.
  Its dual is rebuilt: each member takes its column's reduced cost, and
  the head takes the norm of those, raised to the head's reduced cost
  when the head sits at its upper bound with room below. A certificate is
  rebuilt the same way: each member cancels its column's coefficient, and
  the head takes the norm, raised so that its own coefficient is not
  negative.

Rotated cones and a member that heads another cone are left to the walk.
`--log summary` counts both kinds.

## CBLIB's 80 mixed-integer instances at 1e10 work units

`bench/measurements/02-255/cblib.sh 10000000000` with HEAD (2af94b9's
code, `cblib-1e10-head.txt`), with the change (`cblib-1e10-after.txt`)
and with the idle rule alone (`cblib-1e10-idle-only.txt`).

| | HEAD | after |
|---|---|---|
| `OPTIMAL`, taken by the checker | 28 | 30 |
| work limit | 51 | 50 |
| numerical error | 1 | 0 |

- 76 instances end the same to the bit.
- turbine07 ends `OPTIMAL` at 2 in 15 nodes and 2.21e7 work units,
  where it failed after 1413 nodes and 1.50e9.
- turbine54 ends `OPTIMAL` at 3 in 7 nodes and 6.69e7 work units, where
  it reached the limit after 2436 nodes with a bound of 1.66.
- turbine07_aniso ends at the same optimum with 0.96x the work.
- turbine07_lowb reaches the limit both times, its incumbent 2.536
  against 1.867 before (the reference is 0.899) and its bound 0.7444
  against 0.7489. The idle rule alone takes the same path, so the
  difference is the search's, from node 160 on. Its failed nodes, 47
  before and 57 now, are a certificate problem of another kind, below.
- The work over the 80 goes from 5.775e11 to 5.66e11 units.

The idle rule alone gives the same outcomes. The dead rule saves 5.7%,
3.6% and 3.3% of the work on turbine07, turbine07_aniso and turbine54,
and turbine54 ends at 3 rather than 2.9999999999993592.

## CBLIB's 80 mixed-integer instances at 1e11 work units

`cblib-1e11-after.txt`, against HEAD the same day
(`cblib-1e11-head.txt`, 2af94b9's code):

| | HEAD | after |
|---|---|---|
| `OPTIMAL`, taken by the checker | 40 | 42 |
| work limit, each with an incumbent | 39 | 37 |
| numerical error | 1 | 1 |

turbine07 and turbine54 end `OPTIMAL` as at 1e10, and 76 instances end
the same to the bit. turbine07_lowb ends `NUMERICAL_ERROR` after 7423
nodes, where HEAD reached the limit after 15318: its search now meets a
node with every integer column fixed whose certificate the checker
refuses, and such a node cannot be split. 02-263 takes that up.

## Other readings

- 02-253's 3000 generated conic models, seeds 1 to 3: the same digests,
  work and iterations as HEAD. The 8 numerical errors are the same 8.
- 02-255's 3000 generated mixed-integer models: the same digests and
  nodes (2408, 2409, 2482), the work 0.998x (382930378 to 382101375,
  395827388 to 395300590, 393044021 to 392547008).
- `make cblib`, the 29 continuous instances: 0 regressed, 0 improved,
  0 new against the baseline.

## What is left: turbine07_lowb

Its failed nodes end at certificates too, of another kind. The head of
the two-member cone `(x0, x1)` costs 1 and sits in 14 rows, so the cone
is not idle. The certificate leaves that free column a coefficient of
7.9e-10, the whole of its traffic, and the cleanup then leaves column 3
at -0.0066 against a traffic of 272. `TODO.md` row 5 has it.
