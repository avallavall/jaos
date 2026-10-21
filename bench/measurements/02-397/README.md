# 02-397 — a hull cut starts from an empty row

Taken on 2026-10-21 on the tree of 334ac27. `read.sh` takes the reading.

## The generator

No reading ran the MIP tree under its own options on small models.
`mipopts.c` takes 02-235's models with integer marks (8 to 24 columns, 6
to 16 rows, infeasible and unbounded models beside the feasible ones). It
solves each with the default options, then under each of 49 variants that
set one or two options: cut rounds, each cut family off, heuristics,
branching and node rules, presolve and probing. Each answer must have the
default's status (P1), and an optimum must have the default's objective
within 1e-5 of (1 + |default's|), with integral integer columns and a
point `jaos_check_solution` finds feasible (P2).

## What it found

On HEAD, 9 of the 3000 models break under `mip_flow_cover_rounds=0`, and
under no other variant (`head.txt`): 6 end infeasible where the default is
optimal, and 3 end optimal at a worse objective. HiGHS 1.15.1 agrees with
the default on all 9.

On seed 1's model 306 the default proves -62. With flow covers off, a hull
cut at the root cuts that point off. The cut carries entries of a Gomory
cut found earlier in the same round, on columns outside the hull cut's
row. The root's cut families build each cut in one shared buffer, and each
clears it before it writes, except `hull_round` in `src/mip.c`, which
wrote only its row's columns. By default `flow_cover_round` runs just
before it and leaves the buffer clean, so only that variant showed it.

## The change

`hull_round` clears the buffer before its first row.
`tests/data/mip_hull_stale.mps` is seed 1's model 306, solved with flow
covers off.

## The readings

| set | HEAD | the change |
|---|---|---|
| `mipopts.c`, seeds 1 to 3, 1000 models each, 49 variants | 9 broken, all under `mip_flow_cover_rounds=0` (`head.txt`) | none (`new.txt`) |
| `make miplib J=2` | | 24 of 24, the same results file |
