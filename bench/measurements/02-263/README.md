# 02-263 — refused certificates trimmed, failed leaves set aside

## What went wrong

After 02-262, turbine07_lowb still split 57 of its first 1094 nodes at
1e10 work units, each one a node whose walk ended at an infeasibility
certificate the checker refused (47 before 02-262). At 1e11 one such
node had every integer column fixed, so it could not be split, and the
tree ended `NUMERICAL_ERROR` after 7423 nodes, where HEAD had reached
the work limit.

Two nodes dumped from the tree show the certificate's fault. The head
`x0` of the two-member cone `(x0, x1)` is free, costs 1 and sits in 14
rows, each tying the head of a disk cone to a multiple of `x0`. The
certificate hardly touches that group: every term of column 0 and of
the 14 disk heads is below 4e-9, against a largest entry of 2.2e5, and
column 0's terms add up to 7.9e-10, the whole of its traffic. The
checker needs a free column's coefficient to vanish to the primal
tolerance, 1e-7, of its traffic, so it refuses. The cleanup that
followed zeroed every entry below `CONIC_RAY_ZERO` (1e-7) times the
largest, 0.022 here, and that broke columns 3 to 10 (-0.0348 against a
traffic of 1930 on column 3). Solving with the objective cleared, or
stopping the walk at `CONIC_TOL_INFEAS` 1e-10, 1e-12 or 1e-14 instead of
1e-8, does not help, and at 1e-14 the walk ends `OPTIMAL` at 3.4e12, a
wrong answer.

## The change

**A narrower cleanup first** (`cm_cert_trim` in `src/conic.c`). A column
the checker refuses, free in the direction of its coefficient, whose
terms add up to no more than `CONIC_RAY_ZERO` times the certificate's
largest entry, loses what touches it and is itself that small: its rows'
multipliers below that size, its member parts, and its cone when it is a
head and every part of the cone is below that size. One pass, then the
checker decides. A first version that took any column of small traffic,
the checker's verdict aside, zeroed row 0 through a member of one of the
disk cones and broke columns 3 to 10 by up to 3.4e-3. When the checker
still refuses, the old cleanup runs on the certificate as the walk gave
it, so every certificate the old code published is still published.

**A failed leaf set aside** (`src/conictree.c`). A node whose relaxation
fails with every integer column fixed, or whose integral relaxation
fails its fixed solve, cannot be split. It no longer ends the tree: its
bound (the parent's, or its own relaxation's) is kept, the search goes
on, and the tree ends `OPTIMAL` only when its incumbent is within the
gap of every bound so kept, `NUMERICAL_ERROR` with the first such node's
reason otherwise. The bound reported at a limit includes them.
`tests/test_conic.c` builds the badly scaled box of `TODO.md` row 5 with
an integer column that touches nothing: the root and both leaves fail,
and the tree now ends after all three nodes where it stopped at the
second.

## CBLIB's 80 mixed-integer instances

`bench/measurements/02-255/cblib.sh`, with HEAD (2af94b9's code) and with
02-262 and this reading together (`cblib-1e10-final.txt`,
`cblib-1e11-final.txt`).

At 1e11 work units:

| | HEAD | after |
|---|---|---|
| `OPTIMAL`, taken by the checker | 40 | 43 |
| work limit, each with an incumbent | 39 | 37 |
| numerical error | 1 | 0 |

- 75 instances end the same to the bit.
- turbine07 (15 nodes) and turbine54 (7 nodes) end `OPTIMAL`, as at 1e10.
- turbine07_lowb ends `OPTIMAL` at 0.89931143 in 7588 nodes and 5.94e10
  work units, where HEAD reached the limit after 15318 nodes with an
  incumbent of 1.867 and a bound of 0.868. The library's reference is
  0.89930073, 1.2e-5 below it.
- turbine07_lowb_aniso reaches the limit with its incumbent at 1.585
  against 2.247 and its bound at 1.392 against 1.105, the reference being
  1.395, in 4202 nodes against 17956.
- The work over the 80 goes from 4.621e12 to 4.479e12 units.

At 1e10 work units the trim alone moves one instance,
turbine07_lowb_aniso: the same incumbent, a bound of 1.176 against 1.053
and 1444 nodes against 1559. The other 79 are as 02-262 left them.

## The generated models

- 02-253's 3000 conic models, seeds 1 to 3: the same digests, work and
  iterations as HEAD.
- 02-255's 3000 mixed-integer models: every answer still agrees with
  brute force, and the digests of seeds 1 and 2 do not move. The search
  does: nodes 2408, 2409, 2482 to 2410, 2488, 2486, and the work over the
  three 1.172e9 to 1.189e9, 1.5% more. Certificates the checker now takes
  prune nodes whose children the tree had explored. Seed 3's digest moves
  with them.
- `make cblib`, the 29 continuous instances: 0 regressed, 0 improved,
  0 new against the baseline.
- The badly scaled box of `TODO.md` row 5 still ends `NUMERICAL_ERROR`.
