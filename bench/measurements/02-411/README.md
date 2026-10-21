# 02-411 — the same LP scaled by powers of ten

Taken on 2026-10-21 on the tree of 758b21c. `read.sh` takes the reading.
No code changed.

## The harness

`scaled.c` solves each of 02-235's LPs, then scales it: each row by
10^a, each column by 10^b (`x = 10^b x'`) and the objective by 10^c, with
a, b and c drawn from -K to K, and solves the scaled model with the dual,
the primal and the barrier. The status must stay, an optimum divided by
10^c must match the unscaled one within 1e-6, and the checker must pass
the point and duals at 1e-7. For each refusal the harness also prints the
checker's certified suboptimality.

## The reading

Seeds 1 to 3, 1000 models each, at K = 2, 4 and 6 (`scaled.txt`). A
coefficient then spans up to 1e8, 1e16 and 1e24 from its unscaled value.

| K | solves of an optimal model | status changed | objective off | checker refuses |
|---|---|---|---|---|
| 2 | 4734 | 0 | 0 | 0 |
| 4 | 4734 | 1 | 0 | 103 |
| 6 | 4734 | 4 | 0 | 1070 |

Every optimum reached matches the unscaled one. Every refusal is the
checker's absolute reduced-cost test (1e-7): among the refusals printed
(the first six per algorithm and seed, and every one without a certified
gap), no row is off by more than 1.5e-16 of its traffic and the largest
relative suboptimality is 2.3e-14, so the refused points are optimal to
rounding. Their reduced costs are differences of terms up to 1e12, and
1e-7 of them is below a double's rounding.

The five status changes are solves that end `numerical_error`: the
simplex refuses to publish a settled point whose unscaled reduced cost
breaches the dual tolerance by 1.2e-9 to 4.8e-7, the same absolute test
in the simplex itself (`published_breach` against `DUAL_TOL`). At 1e-7
that tolerance once let four Netlib models publish points that were not
optimal (`DUAL_TOL` in `docs/tolerances.md`). Both are TODO row J21's
question: an absolute reduced-cost test against one relative to its
terms.
