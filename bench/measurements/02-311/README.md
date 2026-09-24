# 02-311 — the rest of the barrier on threads (TODO H7)

Taken on 2026-09-24 on the tree of 1ed6e14, on a 12-thread machine with
nothing else running (5% to 17% CPU before the run). Wall-clock times are
one run each. Answers and work units are exact.

## The patch

`normal-threads.patch` forms the barrier's normal matrix in bands of rows,
one band per thread, when the matrix holds at least
`BARRIER_NORMAL_LANE_MIN` entries. Each band has its own work vector and
writes only its own rows of the matrix, so every entry is the same sum in
the same order as on one thread. The work count is the sum over the bands,
the same number as the one-thread loop.

It is bit-identical. With the threshold at 1e5, `make barrier` and `make
maros-meszaros` wrote the committed files at one thread, and the Netlib
barrier reading and Maros-Meszaros at four threads wrote the same files as
at one thread. `tests/test_barrier.c` in the patch solves a 400 by 800 LP,
whose normal matrix holds about 159000 entries, at 1 and 3 threads and
compares the work, the iterations and every value bit for bit.

## The timing

`timing.sh` builds the tool with the threshold at 1e12 (never threaded, the
control), 0, 1e4, 1e5 and 1e6, and solves five LPs with `--algorithm
barrier`, the control at 1 and 4 threads and the others at 4
(`timing.txt`):

| model | 1 thread | control, 4 | 0 | 1e4 | 1e5 | 1e6 |
|---|---|---|---|---|---|---|
| maros-r7 | 3.17 s | 2.25 s | 3.30 s | 2.20 s | 2.31 s | 2.28 s |
| dfl001 | 50.88 s | 27.22 s | 27.48 s | 26.12 s | 26.31 s | 26.35 s |
| d2q06c | 1.88 s | 1.91 s | 1.87 s | 1.97 s | 1.87 s | 1.86 s |
| pilot87 | 17.81 s | 15.63 s | 16.62 s | 15.52 s | 16.68 s | 15.54 s |
| pds-06 | 4.20 s | 3.56 s | 4.54 s | 3.53 s | 3.56 s | 3.55 s |

Every line of a model has the same objective and work units. The best gain
is 1.1 s on dfl001 (4%), and the thresholds of 1e4, 1e5 and 1e6 read within
0.23 s of each other there. On the other four models the lines fall on both
sides of the control. A threshold of 0 costs up to 1 s.

## Why: forming and the solves are small

`profile.sh` runs callgrind on ken-11 and pds-06 under `--algorithm
barrier` at one thread (`profile.txt`). On pds-06 the numeric Cholesky
(inlined into `form_normal`) takes 53.1% of 77.2e9 instructions, the
forming loop itself (`normal_lane_run`) 0.18% and the triangular solves
(`jm_chol_solve`) 1.00%. On ken-11 the forming loop takes 1.04% and the
solves 1.61%. The factor already runs on threads (02-288), and what is left
of the barrier on one thread is too small to pay for starting threads.

So neither part is worth threads on these models: the forming loop was built
and refused (`barrier-normal-threads` in `bench/refusals.txt`), and the
solves were not built.
