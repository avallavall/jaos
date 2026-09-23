# 02-307 — coefficient tightening reads a pulling column's slack at 1

Taken on 2026-09-23 for TODO row H2, on the tree of e80565d.

The root's coefficient tightening (`MIP_TIGHTEN`, on by default) shrinks
the coefficient of a binary column in a one-sided row when the row cannot
reach its bound through that column. For a column whose coefficient pulls
the row away from its bound (negative in a `<=` row, positive in a `>=`
row) it read the row's slack with the column at 0. That case fires only
on a row that is redundant with the column at 0 and at 1. Savelsbergh's
rule, applied to the complemented column, reads the slack with the
column at 1: `x - 10 y <= 0` with `x <= 3` becomes `x - 3 y <= 0`, and
the unit test built on that row solves at the root with no cut, where the
old rule needed two.

MIPLIB 3 (`make miplib J=2`) against the results of e80565d:

| instance | work | nodes |
|---|---|---|
| p0033 | 0.272x | 289 to 33 |
| gen | 0.721x | 5 to 3 |
| lseu | 0.911x | 7147 to 6315 |
| p0282 | 0.956x | 3248 to 3355 |
| p0201 | 0.975x | 354 to 390 |
| the other 19 | 1.000x | byte-identical |

0.928x over the 24, none past 2x.

On the three fixed-charge networks of the 2017 set that HiGHS and SCIP
close at the root (`sp150x300d`, `p200x1188c`, `exp-1-500-5-5`) the rule
changes nothing: their `u` equals the column bound of `x`, so the
tightening needs the bounds the flow rows imply, which the root does not
compute.

On the whole 2017 set (`make miplib2017 J=2`, 1e10 work units) the gap
sum reads 1.000x against d6245e0's reading, with the same 17 incumbents
and 8 at the reference; `neos-2657525-crna` and `ic97_potential` take
other paths through their trees and end with the same bounds and no
incumbent.
