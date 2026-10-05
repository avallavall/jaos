# 02-347 — the aggregation's row chosen by what the aggregate keeps

Taken on 2026-10-05 on the tree of ad5d787, for TODO row J7 (`timtab1` has
no incumbent and a bound of 414399 against the reference 764772).
MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work units a model.

## Why timtab1's root is low

`timtab1` is periodic timetabling in cycle form: 171 equality rows, each
with one integer column of coefficient -60 and three to ten continuous
columns of coefficient +-1. JAOS's root ends at 245047. HiGHS's root
reaches 609536 and SCIP's 457552.

SCIP with presolve off and only its c-MIR separator reaches 458724
(`scip-sepa.py`). With at most one aggregation step at the root it reaches
369256, and with none 214269. So the strength comes from the aggregation.
JAOS's root without aggregation (`--mir-aggregate 0`) reaches 238901, close
to SCIP's.

`aggproto.py` repeats the MIR rounds in Python on the LP relaxation, with
the aggregation's choices as switches:

| column picked | row picked | bound after the rounds stop |
|---|---|---|
| largest coefficient | first in column order (JAOS) | 228510 |
| largest bound distance | first in column order | 238302 |
| largest bound distance | fewest entries | 252002 |
| largest coefficient | least bound distance kept | 421429 |
| largest bound distance | least bound distance kept | 488883 |

The prototype's final point (third line) violates 138 of the 219 c-MIR cuts
SCIP's root keeps, up to an efficacy of 0.648. Each of the four most
violated comes out of the prototype's own MIR when it is given the cut's
source rows. The rounding is the same; the rows combined differ. The most
violated cut sums two rows that share three continuous columns, and a walk
that takes the first row holding the picked column never pairs them.

## The change

The row taken to substitute out the picked column is the one that leaves
the aggregate with the least sum of |coefficient| times bound distance over
its continuous columns, a column with no finite bound counted first. The
first such row in column order wins a tie. Outside network mode the column
picked is the one farthest from its bounds (Marchand and Wolsey's rule,
refused alone as `mir-agg-distance`); in network mode it stays the largest
coefficient.

`timtab1`'s root goes from 245047 to 427178 for 1.22e8 work units
(1.28e7 before).

## The arms

The three arms ran from an environment switch that is not in the code.

| arm | MIPLIB 3 work | 2017 solved | 2017 gap sum |
|---|---|---|---|
| base (ad5d787) | 1 | 3 | 14.85 |
| `aggpick`: both rules everywhere | 0.962x | 4 | 15.70 |
| `aggA`: both rules outside network mode | 0.999x | 3 | 14.69 |
| `aggB`: the row rule everywhere, the column rule outside network mode | 0.982x | 4 | 14.61 |

`aggpick` solves `p200x1188c` and takes `egout` to 0.436x, but
`beasleyC3`'s incumbent goes from 831 to 2175 and `tr12-30`'s from 132496
to 139748. `aggB` is the change: `p200x1188c` solves, `beasleyC3` holds
782 over 739, `tr12-30` 131367 over 130044, `neos-3627168-kasai` 1003649
over 960451, and `timtab1`'s bound at the limit is 528764 (still no
incumbent). `egout` reads 0.655x; no MIPLIB 3 model is worse.
