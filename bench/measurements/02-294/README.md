# 02-294 — columns that touch nothing, and the conic tree's node order

Taken on 2026-09-22 on the tree of 2ed4237, for TODO row C5.

## Columns that touch nothing

`tests/data/g_cone_badbox.mps` holds fifteen bound-only columns of
QPLIB_9002, two of them near 1e9 against `Q` entries of 4e-11, beside a
cone the walk cannot leave out. The walk ran 37 iterations and more with
`tau` falling to 1e-13 and the gap swinging between 1e-4 and 0.5, and
ended at a certificate the checker refused.

None of the fifteen touches a row, a cone or another column of `Q`. Such
a column's best value does not depend on the rest of the model: it is
`-c/q` clamped to its box, or the bound its cost points at when `q` is 0.
The conic interior point now deletes these columns from a copy, adds their
terms to the objective's constant, solves the copy, and sets each at its
own minimiser, its reduced cost `c + q x`. A fixed column stays in the
walk, since nearly every CBF file carries one in no row and taking it out
moves the walk for nothing.

- g_cone_badbox: `OPTIMAL` at 73622258.83, taken by the checker.
- `make cblib J=4`: 29 of 29, the results file unchanged to the byte.
- CBLIB's 80 mixed-integer instances at 1e10 and QPLIB's 27 convex QCQPs
  (types LCD, LMC and LMD) at 1e10: every line the same as on 2ed4237.

With x11 and x13 in a row of their own the two are no longer alone, the
walk sees the box and still fails. `tests/test_conic.c` keeps that model
for the tree's failed leaf.

## The conic tree's node order

`conic-tree-estimate.diff` gives the conic tree the MIP tree's order since
ae25a70: when a plunge ends, the open node with the best estimate (its
bound plus each fractional column's smaller pseudocost gain) is taken,
and the best bound every fifth pick. `--node-select bound` keeps the old
order, and reads the 80 instances to the bit.

`cblib-mip-1e10-main.txt` and `cblib-mip-1e10-estimate.txt`, CBLIB's 80
mixed-integer instances at 1e10 work units, compared by `cmpgap.py` (an
instance's gap is `|incumbent - bound| / max(|incumbent|, |bound|, 1)`,
capped at 1, and 1 with no incumbent):

| | best bound | estimate |
|---|---|---|
| `OPTIMAL` | 30 | 28 |
| with an incumbent | 78 | 78 |
| gap sum | 12.300 | 13.025 |

The estimate loses sssd-strong-25-4 and sssd-weak-25-4 and leaves the
sssd bounds behind (sssd-weak-30-4 from a gap of 0.0008 to 0.36). It
finds better incumbents on turbine07_lowb (gap 0.60 to 0.16) and on four
sssd-weak files. Refused (`conic-tree-estimate`).
