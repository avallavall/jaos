# 02-357 — MIR cuts measure a column from its other bound

Taken on 2026-10-07 and 2026-10-08 on the tree of 02adc41, for TODO row
J7. MIPLIB 3 at J=2 and 4 GB a solve, the 2017 set at 1e10 work units a
model.

## What SCIP's c-MIR does on neos-911970

`neos-911970` assigns 35 jobs to 24 machines; its 48 capacity rows read
`sum a_ij x_ij - s_j <= 6.5` with `x` binary and an overflow column `s_j`
in the objective. JAOS's root reached 45.42 after 708 cuts. SCIP 10's root
with c-MIR alone reaches 51.81 (`scip-neos-911970.txt`, the scripts of
02-356), the reference optimum is 54.76.

JAOS's single-row MIR (`mir_side`) measures each column from its nearer
bound and tries as divisors the row's coefficients on fractional integer
columns. Marchand and Wolsey's c-MIR adds two steps after the best divisor:
the divisor halved up to three times, and each integer column strictly
inside its bounds measured from its other bound in turn, kept when the
cut's efficacy rises. `mird.diff` reads them from `JAOS_MIRD` (1 halves,
2 flips, 4 limits the flips to rows with a continuous column); it was cut
from a copy that lacks 02adc41's guard against an empty integer range.

| `JAOS_MIRD` | `neos-911970` root | work units |
|---|---|---|
| 0 | 45.42 | 1.06e9 |
| 1 | 43.82 | 1.28e9 |
| 2 | 51.56 | 1.14e9 |
| 3 | 51.47 | 1.15e9 |

## The arms

| arm | MIPLIB 3 | 2017 incumbents | 2017 gap sum |
|---|---|---|---|
| base (02adc41) | 1.000x | 27 | 14.19 |
| `mird2`, flips on every row | 1.090x | 28 | 13.48 |
| `mird6`, flips on rows with a continuous column | 1.000x | 28 | 13.48 |

On the 2017 set both arms change the same models: `csched007` finds its
first point (474, the reference 351; bound 294.4 to 298.3), `neos-911970`
ends at 55.60 over 51.93 (55.85 over 51.80), `mad` at 0.330 (0.377),
`binkar10_1`'s bound goes to 6718.3, and `markshare2` ends at 154 (113)
inside its capped gap. Under `mird2` the pure integer knapsacks of
MIPLIB 3 change: their roots are the same or higher (`p0033` 2962 to 2984,
`mod008` 299.1 to 299.7, `gt2` the same) but their trees grow (`gt2` 447 to
819 nodes, `p0201` 351 to 627, `mod008` 683 to 1569). Under `mird6` every
MIPLIB 3 model keeps its tree; `blend2`'s work differs by 24 units.

## The test

`flipsearch.py` generates assignments of 4 to 7 items to 2 or 3 bins, each
bin `sum a_i x_i - s <= cap` with `s` costed, and solves each with only the
MIR rounds on and the heuristics off, the flips off and on. Of 3000, seven
close at the root only with the flips. `flip-2479.lp` is the smallest:
without the flips its root reads 0.364 and the tree takes 7 nodes, with
them the root reads the optimum 0.5. It is
`test_a_mir_cut_on_a_mixed_row_measures_a_binary_from_its_far_bound` in
`tests/test_mip.c`.
