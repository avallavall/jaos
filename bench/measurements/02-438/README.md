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

## The gate

`make test` and `make sanitize` pass. `make maros-meszaros` shows 0
regressed and gives 2ba1ad3's lines, liswet10 and liswet11 included.
`make barrier` gives the same lines. The whole reading on this change
follows in the next commit.
