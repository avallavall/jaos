# 02-162 — a fixed column never joins the dual ratio test

D252. `admit_candidate` refuses `lo == up` (the primal pricing sites
already did), so the walk never retires a width-zero flip, `apply_flips`
never toggles a fixed column's published label, and Harris never picks
one as the entering variable. Equality-row slacks are fixed columns too,
so the trajectory moves on most models; the campaigns below say what
that cost, per instance, against the baselines committed at `3f60df2`.

## What is here

| file | what it is |
|---|---|
| `record-diffs.txt` | `record_diff.py` and `geomean.py` on all four records, candidate vs `3f60df2`, as read for the verdict |

## Re-deriving

The before is every `bench/results/*.txt` and `bench/*.baseline` at
`3f60df2`; the after is the same files in D252's own commit. The tests
that pin the behavior are `test_a_fixed_column_is_not_a_flip_candidate`
(fails on the parent, label toggled) and
`test_a_row_repairable_only_by_fixed_columns_is_infeasible`, both under
`EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`.

The `record_diff.py` in the diffs below is the repaired one: its RSUB
floor said 1e-9 where `bench/run.c` has read 1e-16 since 2026-08-24, and
before the repair it reported 2 of the runner's 6 flags on this same
record.
