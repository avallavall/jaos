# 02-422 — a first point by lattice rounding

Taken on 2026-10-22 on the tree of c356045, from outside the tool. No
code changed.

## The idea

TODO row J7 lists 34 generated MILPs (02-394) with general integers and
equality rows, where JAOS finds a first point late or never. The integer
points of the equality rows whose columns are all integer form a lattice,
x0 + L z. `lattice.py` solves the LP relaxation, finds x0 and L by column
operations to a Hermite form, reduces L by LLL, and rounds the
relaxation's point to the nearest lattice point (Babai). It fixes the
integer columns there and solves the LP over the continuous ones. When
that LP is infeasible, it steps along lattice vectors and tries again, up
to 60 LPs a model.

## The reading

In 1500 seconds the prototype reached 7 of the 34 models (`lattice.txt`)
and found a point on none. These models have 1 to 8 equality rows whose
columns are all integer, out of 15 to 30 rows; the lattice has 20 to 40
dimensions. The rounded points hold those rows, and the fixed LP then
fails on the equality rows with a continuous column, on the inequalities
or on the bounds. The rows a lattice can hold are not the ones that keep
these models from a first point (`bench/refusals.txt`:
`lattice-rounding-first-point`).
