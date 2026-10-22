# 02-438 — 02-436's push rule against the wider QP sets

Taken on 2026-10-22. `read.sh` is 02-416's reading with 8458cbd, the
tree before 02-436, as the base: 02-365's and 02-366's circulations,
02-248's 6,000 generated QPs, Maros-Meszaros, QPLIB's 11 convex QPs and
its 17 convex MIQPs.

## 02-436's rule breaks seven circulations

02-436 (2ba1ad3) was gated on `make maros-meszaros` alone. On the
circulations (`*-2ba1ad3.txt`) seven models that ended optimal and
checked on 8458cbd broke:

- `nq` 12-30-230, 12-30-1710, 40-100-9, 40-100-1004 and 40-100-1118 end
  `numerical_error`. On 230, 8458cbd's push that drops the duals' shift
  and frees a pin in each broken row (`push_free`) settled in 7 rounds;
  02-436 stopped it at round 6, when a single release was undone at
  once.
- `nqb` 12-30-1220 and 12-30-1314 end optimal at points the checker
  refuses. The first push, freeing one pin a round, settled on an
  active set whose point has a duality gap of 5.3e-7 (1220). The
  barrier trusts a settled push on its duals (`bx_holds` asks for
  primal feasibility and dual feasibility or a settled push), so the
  point was published.

No J24 model moved: 10, 19, 1 and 1 still end `numerical_error`.

## The change

- The one-pin rule and its stop apply only to the first push, the one
  with none of the plain, free or margin variants. Those take the steps
  they took on 8458cbd.
- A push that settled after freeing one pin a round is not trusted on
  its duals: its point must pass the checker's dual test too, and when
  it does not, the barrier goes on as it does after any refused push.

On the seven models all end optimal and checked; six take less work
than on 8458cbd (1220: 183,443 against 747,981) and 40-100-1118 more
(1,498,112 against 1,282,240). 02-436's model, its two test models and
liswet10 and liswet11 keep 02-436's answers and work.

## The reading on 403079f

| set | against 8458cbd |
|---|---|
| 02-365, 12 nodes, seeds 1 to 400 and 1001 to 1800 (`nq-12*.txt`) | no status moves; 10 and 19 still `numerical_error`; work 0.88x and 0.95x |
| 02-365, 40 nodes, seeds 1 to 120 and 1001 to 1200 (`nq-40*.txt`) | no status moves; 1 and 1 still `numerical_error`; work 0.87x and 0.87x |
| 02-366, 12 nodes, 1,200 seeds (`nqb-12*.txt`) | all optimal and checked on both; work 0.86x and 0.90x |
| 02-248's 6,000 generated QPs (`gen-*.txt`) | the same lines |
| Maros-Meszaros (`mm-cmp.txt`) | the same statuses; liswet10 and liswet11 cheaper (0.65x on liswet10), the rest the same |
| QPLIB's 11 convex QPs (`cqp-*.txt`) | the same lines |
| QPLIB's 17 convex MIQPs at 1e10 (`miqp-*.txt`) | the same statuses; QPLIB_4270 stops at the limit with an incumbent 1.6e-9 apart, and QPLIB_5543 still ends `numerical_error` |

The work is the geometric mean over the models optimal on both trees.
02-434's `flip.c` and 02-435's `mirror.c` on 403079f (`flip-*.txt`,
`mirror-*.txt`) have 0 disagreements in every mode.

## The gate

`make test` and `make sanitize` pass. `make maros-meszaros` shows 0
regressed and gives 2ba1ad3's lines, liswet10 and liswet11 included.
`make barrier` gives the same lines.
