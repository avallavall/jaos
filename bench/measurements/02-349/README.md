# 02-349 — two follow-ups to 02-347, both refused

Taken on 2026-10-05 on the tree of ebf79c2, for TODO row J7. MIPLIB 3 at
J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model. Both arms
are read against the readings of ebf79c2 (MIPLIB 3 and a 2017 gap sum of
14.606, 4 solved).

## The aggregation's gain bar at 1e-3 (`agg1m3`)

`agg-gain.py` reads `MIP_MIR_AGG_GAIN` from `JAOS_AGGGAIN`. At 1e-3 the
aggregated cuts are kept for more root rounds on `timtab1`, and its bound
at the limit goes from 528764 to 533784. MIPLIB 3 reads 1.032x
(`khb05250` 2.125x, its tree 93 to 23 nodes, `dcmulti` 0.975x), and the
2017 gap sum 14.613, with `tr12-30`'s incumbent 131367 to 131793 and
`beasleyC3`'s 782 to 789. The bar stays at 1e-2.

## The row's slack in the aggregation's row choice (`aggslack`)

On `ic97_potential` JAOS's cuts leave the root at 3868.46, where SCIP's
c-MIR alone reaches 3903.76 with six aggregation steps (3868 with one).
The Python copy of 02-347 (`aggproto.py`, here with `slack` and `tight`)
stays at 3868 too, and rebuilds
each of the three most violated of SCIP's cuts from the rows that hold
their integer columns. Those rows are tight at the point. 02-347's row
choice counts the bound distance of the aggregate's columns but not the
slack of the row it adds. Adding |lambda| times that slack to the measure
takes the copy to 3877 on `ic97_potential` and from 488883 to 499466 on
`timtab1`; starting only from rows within 0.1 of tight leaves
`ic97_potential` at 3868.

In JAOS (`row-slack.py`), `timtab1`'s root goes from 427178 to 437120 and
its bound at the limit from 528764 to 536959, and `ic97_potential` does
not move: its first aggregated round lifts the bound by 0, so the probe of
`MIP_MIR_AGG_GAIN` ends aggregation for the solve. The network models pay:
`p200x1188c`'s root spends the whole 1e10 work units and ends with no
bound, `beasleyC3` holds 831 over 748 (782 over 739), `tr12-30` 132746
(131367) and `neos-3627168-kasai` 1016229 (1003649). The gap sum reads
16.67. MIPLIB 3 reads 0.991x, `egout` 0.796x the only move. Outside
network mode alone it would move only `timtab1`'s dual gap, by 0.011.

The SCIP side runs under Windows Python with pyscipopt 6.1.0:
`scip-sepa.py` of 02-347 for the root bounds, `scdump.py` for SCIP's cut
rows, then `cutwhy2.py` for the rebuild, after `aggproto.py` has written
its final point to `protox.json`.

Refused as `mir-agg-row-slack` in `bench/refusals.txt`.
