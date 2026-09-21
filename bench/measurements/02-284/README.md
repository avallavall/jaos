# 02-284 — the MIPLIB 2017 reading, and JAOS against HiGHS and SCIP on MIP

Taken on 2026-09-21 on 3086162 (TODO B7). Until this reading the MIP tree
had only been measured on the 24 MIPLIB 3 instances.

## The set

`bench/miplib2017.manifest` pins the 30 smallest instances of the MIPLIB
2017 benchmark set that have a proven optimum in `miplib2017-v31.solu`.
`make miplib2017` solves each one with a work limit of 1e10 units
(`MIPLIB2017_WORK`, the runner's `-L`). The reading is not a gate and has no
baseline.

Two numbers describe an instance that stops at the limit. Both are capped
at 1:

- the primal gap: `|incumbent - reference| / max(|reference|, 1)`, and 1
  when there is no incumbent;
- the dual gap: `|reference - bound| / max(|reference|, 1)`.

`gap.sh` prints the set's means from a result file. `compare.sh` lists the
instances whose status, incumbent or bound differ between two files.

## The default

`bench/results/miplib2017.txt`: 0 of 30 finish inside 1e10 work units. 15
have an incumbent, and 1 of them is at the reference (gen-ip002). The mean
primal gap is 0.7132 and the mean dual gap 0.3432.

## The components that are off, one at a time

The rule was set before the first run. A component pays on this set when it
solves more instances than the default, or when its mean primal gap plus its
mean dual gap reads at or under 0.95x the default's 1.0564. It must also keep
at least as many solved instances and incumbents as the default. Each run is
`make miplib2017 J=6 MIPLIB2017_ARGS='-O <option>'`, and its result file is
here.

| component | option | solved | incumbents | at the reference | primal gap | dual gap | gap sum |
|---|---|---|---|---|---|---|---|
| none (the default) | | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |
| reliability branching (D293) | `mip_reliability=1` | 0 | 15 | 1 | 0.7131 | 0.3232 | 0.981x |
| bound propagation (D324) | `mip_propagate=1` | 0 | 15 | 2 | 0.6799 | 0.3407 | 0.966x |
| reduced-cost fixing (D323) | `mip_rcfix=1` | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |
| root probing | `mip_probing=1` | 0 | 15 | 1 | 0.7132 | 0.3432 | 1.000x |
| flow cover | `mip_flow_cover_rounds=1` | 0 | 15 | 1 | 0.7086 | 0.3366 | 0.989x |
| zero-half | `mip_zero_half_rounds=2` | 0 | 15 | 1 | 0.7127 | 0.3429 | 0.999x |
| lifted cover (D307) | `mip_cover_lift=1` | 0 | 17 | 1 | 0.7132 | 0.3447 | 1.001x |
| RINS (D315) | `mip_rins=50` | 0 | 15 | 1 | 0.6997 | 0.3432 | 0.987x |
| node dives (D289) | `mip_dive=1` | 1 | 20 | 4 | 0.5497 | 0.3540 | 0.855x |

Only the node dive pays. It finishes sp150x300d, reaches the reference on
pk1, neos5 and assign1-5-8, and finds incumbents on five instances that had
none. Its dual gap is worse than the default's, and gen-ip002 leaves the
reference (-4773.15 against -4783.73).

What the others do:

- Reliability branching raises 21 of the 30 bounds and lowers 3. The dual gap falls
  to 0.3232, but no instance finishes.
- Bound propagation takes markshare_4_0 to the reference; that one instance
  is most of its 0.966x.
- RINS improves three incumbents, pk1 196 to 37, mas76 47559.8 to 40662.1
  and mas74 17337.6 to 14769.7. The primal gap is capped at 1, so pk1's move
  does not show.
- Lifted cover finds incumbents on neos-911970 and mad, each more than 100%
  from the reference, so the capped gap does not move.
- Reduced-cost fixing and root probing move a few bounds in the last digits.

## JAOS against HiGHS and SCIP

`bench/compare/run-mip.sh`, 20 s per instance, one thread, all three
stopping at the relative gap 1e-6 (`bench/compare/README.md` has the rules):

| set | JAOS | HiGHS 1.15.1 | SCIP 10.0 | JAOS / HiGHS | JAOS / SCIP |
|---|---|---|---|---|---|
| MIPLIB 3, 24 instances | 23 solved, 1.43 s | 24, 0.70 s | 24, 0.64 s | 1.43x | 1.48x |
| MIPLIB 2017, 30 instances | 0, 20.00 s | 8, 14.38 s | 7, 12.87 s | 1.37x | 1.51x |

The times are shifted geometric means with a shift of 1 s, and the ratio is
the ratio of the shifted means. HiGHS and SCIP finish enlight_hard,
sp150x300d, neos-911970, exp-1-500-5-5, neos-3381206-awhea, beasleyC3 and
p200x1188c inside 20 s, and HiGHS binkar10_1 too.

The first run (`mip-*-unequal.txt`) left HiGHS on its own thread count and
its gap of 1e-4, and SCIP at a gap of 0: HiGHS read 0.91 s on MIPLIB 3 and
13.48 s on 2017, SCIP 0.66 s and 12.42 s. The equal settings are the ones
the table quotes.

JAOS overran the limit on csched008, 40.7 s and 42.4 s in the two runs,
with one node. `TODO.md` B16 has it.

## The node dive against MIPLIB 3's bar

The dive pays on the 2017 set, so B7 checked it against MIPLIB 3's bar on
this tree. Its refusal (`bench/refusals.txt`, D289-dive) rests on bell5,
which never finished under it. `miplib3-bell5-dive.txt` is bell5 with
`-O mip_dive=1` and a work limit of twice its baseline work, 6.3e9 units:
it stops at the limit after 385473 nodes (190741 without the dive), with its
incumbent 8966413.71 above the optimum 8966406.49. The bar allows no
instance past 2x, so the dive stays off.
