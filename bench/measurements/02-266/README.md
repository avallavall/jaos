# 02-266 — a model whose cones all go leaves the walk

`TODO.md` row 5 listed a badly scaled box: 15 bound-only columns of
QPLIB_9002, two of them near 1e9 against `Q` entries of 4e-11, with one
free column alone in a cone of one member. The conic walk ended it at a
certificate the checker refuses, `NUMERICAL_ERROR` since 2026-09-19,
while the barrier solves the same model without the cone at 73622257.83.

## What was wrong

That cone is idle: its head is free above, costs nothing and sits in no
row, so 02-262 already leaves it out of the walk. What was left was a
quadratic model with no cone at all, and the walk kept walking it. The
walk is built for a cone problem; the model's own algorithm, the barrier
with its push, is built for this one.

## The change

When every cone is left out and no row is quadratic, `conic_solve` copies
the model, fixes the columns the cones held (the dead cones' at 0, an
idle head at its lower bound or 0), deletes the cones and calls
`jaos_solve` on the copy, which picks the algorithm the model would have
had without cones. The answer comes back with the cones' parts rebuilt as
02-262 rebuilds them: an idle head at the norm of its cone's rest, a dead
cone's dual from its columns' reduced costs, its certificate from their
coefficients, and a ray with those columns at 0.

## The reading

- The box ends `OPTIMAL` at 73622257.830107108, every violation 0 and
  the checker taking both sides (`tests/data/g_cone_dropbox.mps`).
- The same box beside one cone the walk cannot leave out still ends
  `NUMERICAL_ERROR` on the refused certificate
  (`tests/data/g_cone_badbox.mps`), which is the walk's own trouble with
  the scaling and not this route's.
- 02-253's 3000 generated models: no numerical error, no checker failure
  and no wrong verdict, as 02-265 left them. Seed 1's digest moves, where
  a model with nothing conic left now takes the other algorithm, and its
  work falls from 155605285 to 155526627 units; seeds 2 and 3 do not
  move at all.
- 02-255's 3000 mixed-integer models: the same nodes and answers, the
  work 0.3% lower (380455280, 409375636, 389513738 against 381844928,
  410873124, 390184528), since a node with nothing conic left is cheaper
  that way.
- CBLIB's 80 mixed-integer instances at 1e10 work units: the same to the
  bit. `make cblib`, the 29 continuous instances: 0 regressed, 0
  improved, 0 new.
