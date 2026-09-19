# 02-260 — no infeasible verdict on a refused certificate without quadratic rows

The conic interior point published `INFEASIBLE` without a certificate
when its walk ended at one the certificate checker refused. That was
meant for quadratic rows, whose curvature the checker takes linearly, so
a true infeasibility can fail the check there. On a model with no
quadratic row the check is exact, and a refused certificate is no
evidence at all.

## How it showed

QPLIB_9002 routed to the conic walk ended `infeasible` in 50 iterations,
the certificate checker reporting "columns reach 0, rows need 0", while
the barrier finds a point 8.9e-7 from feasible. Shrinking the model while
the walk kept that verdict (rows and columns deleted in halving chunks,
the verdict tested after each) left 0 rows and 15 columns: 13 in
`(-inf, 0]` with `q = 2`, and two in `[1736510, 21706300]` and
`[1838820000, 22985200000]` with `q` of 4.6e-8 and 4.4e-11. A box is
feasible; its optimum is 73622257.83, with the two large columns on their
lower bounds, and the barrier finds it. With a free column in a cone of
one added in front, `jaos_solve` sends the model to the conic walk
directly, and it ended `infeasible` there too.

## The change

`src/conic.c`: when the walk ends infeasible, is not relaxed, and the
certificate checker refuses the certificate, a model with no quadratic
row ends `NUMERICAL_ERROR`. A model with a quadratic row keeps the old
verdict. The 16-column model is `test_a_refused_certificate_is_no_verdict_without_quadratic_rows`
in `tests/test_conic.c`.

## The reading

`ab.sh` against the library before and after, 1000 models per seed:

| harness | seed 1 | seed 2 | seed 3 |
|---|---|---|---|
| 02-253, continuous conic models | same | same | same |
| 02-255, mixed-integer conic models | same | differs | differs |

On 02-255 every tree verdict still agrees with brute force, "failed
models 0" and "tree numerical errors 0" on every seed. What moved is the
brute force's own verdict: some of its fixed-integer solves ended
`infeasible` on a refused certificate and end `numerical_error` now, so 3
models of seed 2 and 2 of seed 3 go from counted verdicts to "brute force
unsure" (optimal 599 to 598 and infeasible 200 to 198 on seed 2,
infeasible 199 to 197 on seed 3). The tree splits such nodes instead of
pruning them: 2406 to 2415 nodes on seed 2 and 2485 to 2486 on seed 3.

`make cblib`: the 29 continuous CBLIB instances give the same lines, none
of them infeasible.
