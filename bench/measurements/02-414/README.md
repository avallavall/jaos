# 02-414 — where TODO row J24's circulations stop

Taken on 2026-10-21 on the tree of 39c3861. `sort.sh` takes the reading
(`sort.txt`). No code changed.

## The reading

After 02-413, 33 of 02-365's circulations still end `numerical_error`:
31 at 12 nodes (seeds 1 to 400 and 1001 to 1800) and 2 at 40 nodes
(seeds 1 to 120 and 1001 to 1200). `sort.sh` solves each with the
summary log. The barrier stops in one of four ways:

| where the barrier stops | models |
|---|---|
| its iterate grows past 1e10 times the data, within 4 to 30 iterations | 16 |
| 200 iterations without convergence | 3 (127, 1104, 1551) |
| it converges, and no push settles, the last push of 02-413 included | 9 |
| it converges, a push settles, and the checker still refuses the point | 5 (25, 1458, 1504, 1607, 1710) |

On seed 25 the push that settles is the last push of 02-413. On the other
four an earlier push settles, so the last push does not run.

Then each goes to the conic interior point. On 32 of the 33 it reports an
improving direction that the ray checker does not confirm. Every arc of
these models has a positive curvature, so none is unbounded. On seed 1551
it ends at a point the checker refuses, with duals off by 6.4e3.
