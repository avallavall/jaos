# 02-348 — the root sub-MIP at any fixing share when no incumbent is known

Taken on 2026-10-05 on the tree of ebf79c2, for TODO row J7 (five 2017
models end with no incumbent). The 2017 set at 1e10 work units a model,
MIPLIB 3 at J=2 and 4 GB a solve.

The root's sub-MIP (RENS) fixes the integer columns its relaxation leaves
integral and runs only when they are `MIP_SUBMIP_FIXED` (0.3) of the
integer columns. On `timtab1` the root relaxation leaves 34 of 171
integral, so it never runs. HiGHS finds its first `timtab1` point with a
sub-MIP after its root (816264).

Run at any share, with its work cap and node limit raised, the sub-MIP on
`timtab1`:

| node limit | work cap | work spent | point |
|---|---|---|---|
| 2000 | 2e9 | 5.64e8 | none |
| 10000 | 2e9 | 1.43e9 | none |
| 50000 | 2e9 | 2.00e9 | 833156 |
| 1000000 | 1e9 | 1.00e9 | none |

The reference is 764772. On the other four models the sub-MIP's box holds
no feasible point, and the sub-MIP proves it: `ic97_potential` in 2.1e5
work units, `csched007` in 1.8e7, `glass4` in 4.5e7 and `csched008` in
1.72e9.

The arm `nirens` (`noinc-rens.py`, `JAOS_NIFIX=0 JAOS_NICAP=0.25`) runs
the root sub-MIP at any share with a cap of a quarter of the work limit and
the usual 500 nodes. Nothing changes but `timtab1`'s bound at the limit
(528764 to 527994) and `csched008`'s (171 both, to the last digits);
MIPLIB 3 reads 1.000x. The point needs a sub-MIP tree of over 10000 nodes,
which is a second tree search.

Refused as `noinc-rens-any-share` in `bench/refusals.txt`.
