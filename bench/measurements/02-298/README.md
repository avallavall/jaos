# 02-298 — the three cut families on the 2017 set

Taken on 2026-09-22 on the tree of 0e7e8b1, for TODO row E1. The families
were measured on MIPLIB 3 before the aggregator, the node order and the
parking of a failed node, and never on the 2017 set, where the tree
spends its whole budget and a cut that lifts the bound shows.

Each arm is `make miplib2017 J=4 MIPLIB2017_ARGS='-O <option>'`, 30
instances at 1e10 work units, and its result file is here. `gapsum.py`
reads them: an instance's primal gap is `|incumbent - reference| /
max(1, |reference|)`, capped at 1 and 1 with no incumbent, its dual gap
the same from the bound, and the gap sum is their total against the
default's.

| arm | option | incumbents | at the reference | primal | dual | gap sum |
|---|---|---|---|---|---|---|
| default | | 17 | 4 | 0.5988 | 0.3464 | 1.000x |
| flow cover | `mip_flow_cover_rounds=1` | 17 | 4 | 0.5942 | 0.3406 | 0.989x |
| zero-half | `mip_zero_half_rounds=1` | 17 | 4 | 0.5988 | 0.3462 | 1.000x |
| lifted covers | `mip_cover_lift=true` | 19 | 4 | 0.5988 | 0.3469 | 1.001x |

The pay rule milestone B used is a gap sum at or under 0.95x with no
fewer solved and no fewer incumbents. None of the three reaches it.

- Flow covers move both halves a little, the dual gap from 0.3464 to
  0.3406, and come closest at 0.989x. The set has few variable upper
  bounds, which is what the family reads.
- Zero-half changes almost nothing here: 1.000x, the dual gap 0.3462
  against 0.3464.
- Lifted covers find two more incumbents, 19 against 17, and both are
  far enough from the reference that the capped primal gap does not
  move; the dual gap rises from 0.3464 to 0.3469 for the work the
  lifting takes.

All three stay off (`cuts-2017-reading` in `bench/refusals.txt`).
