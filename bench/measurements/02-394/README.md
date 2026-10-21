# 02-394 — generated MILPs against HiGHS

Taken on 2026-10-21 on the tree of 09523fc. `read.sh` takes the reading.
No code changed.

## The comparison

Brute force cannot reach MILPs with 20 to 120 integer columns, and the
checker passes an incumbent's feasibility, not its optimality. So
`mipcmp.py` solves each generated model with JAOS (`--check`, 2e9 work
units) and with HiGHS 1.15.1 (`mip_rel_gap = 0`, 300 s, one thread), and
compares the statuses, and the objectives within 1e-5 of (1 + |HiGHS's|).
Every coefficient, side and cost is an integer. Seven families take turns
by seed: multi-row knapsacks, set covering, fixed-charge transport,
general-integer models with equality rows and some continuous columns,
bin packing, larger knapsacks, and larger general-integer models.

On seeds 1 to 700, 100 models a family (`mipcmp.txt`): 662 agree, 38 are
skipped, and none disagree. Every answer JAOS gives, an optimum or
infeasibility, is HiGHS's.

## Where JAOS stops short

The 38 skips are 36 larger general-integer models, one larger knapsack
(seed 446) and one general-integer model (seed 24). On 34 of them JAOS
reaches 2e9 work units where HiGHS proves the optimum; on 4 HiGHS reaches
300 s too. Each `biggeneral` model has 30 to 60 columns bounded in [0, 8],
85% integer, and 15 to 30 rows, half of them equalities. On seed 27 HiGHS
proves -936 in 31.7 s after restarts, and JAOS stops with -935 and a bound
of -969.95 after 171701 nodes.

`general` seed 24 shows the pattern: 27 columns (21 integer), 14 rows.
HiGHS proves -105 in 7 s and 72919 nodes, after restarts. JAOS reaches 2e9
work units after 318097 nodes with its bound at -158.6 and its incumbent
at +4.0, first found at node 311489. Rounding, the dive heuristic, RINS
and the pump find nothing. From node 1000 the tree dives
(`MIP_NOINC_DIVE_AFTER`), resuming 100 times a dive, and still finds no
point until node 311489. With `--dive` from the start the first point
comes at node 9948, and the optimum -105 is found; with
`--dive-backtrack` 16, 100 or 1000 at nodes 18577, 41120 and 61420. Both
use the same child rule, the nearer side first.

Seed 24 is the outlier. `dive.sh` runs the 34 models JAOS left at 2e9 and
HiGHS proved, by default and with `--dive`, at 5e8 work units
(`dive.txt`): the default finds a point on 22, `--dive` on 16. Where both
find one, `--dive`'s is better on 9, the same on 2 and worse on 1, and
`--dive` proves 2 optimal (seeds 174 and 314). No switch fixes the family; it goes to TODO
row J7.

RENS, tried from outside the tool (`rens.py`): solve the relaxation, fix
each integer column whose value is integral, box the others between floor
and ceil, and solve that MIP at 1e8 work units. On all 33 general-integer
models the sub-MIP is infeasible at its root; on `bigknap` seed 446 it
finds 2286, where the tree already has 2332.
