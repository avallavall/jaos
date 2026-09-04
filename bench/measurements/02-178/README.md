# 02-178 — the bit budget once the basis is made integral (D273)

D271 said an exact verifier could prove 97 of 110 gate bases at today's
capacity. D272 said block triangular form takes that to 110 of 110. Both
numbers count the basis entries as if they were already integers.

They are not. Bareiss is a fraction-free elimination and it is exact because
its intermediate entries are minors of an **integer** matrix. A basis entry
like `1.06` is a dyadic rational whose odd mantissa needs 53 bits and whose
exponent is -52. To hand a row of those to an integer elimination you scale
the row by `2^-s`, with `s` the smallest exponent in it. That is exact and it
carries no rounding, but it is not free of bits:

    det Z  =  det B * 2^(-sum_i s_i)

and it is `Z`, not `B`, that the elimination holds.

## What was run

```
bash bench/measurements/02-178/run-integral.sh
python3 bench/measurements/02-178/budgets.py > bench/measurements/02-178/budgets.txt
```

`integral.c` is D272's `blocks.c` from the basis build through the SCCs,
taken verbatim so both instruments read the same matrix and find the same
blocks, plus the scaling. Same population: the 110 gate instances that solve
OPTIMAL and have a basis to read.

## The answer

**86 of 110, not 97 and not 110.**

| | bases inside 4096 bits |
|---|---|
| D271, whole basis as read | 97 |
| D272, largest block as read | 110 |
| **D273, largest block once integral** | **86** |

Twenty-four fall out. The worst:

| instance | rows | block as read | block once integral | row scale | rows needing one |
|---|---|---|---|---|---|
| pilot87 | 2030 | 1734.3 | 81771.3 | 72 | 1786 |
| pilot | 1441 | 302.1 | 61430.1 | 72 | 1172 |
| d2q06c | 2171 | 3061.5 | 42807.6 | 65 | 1432 |
| pilot-ja | 940 | 3140.3 | 29002.0 | 71 | 661 |
| greenbeb | 2392 | 297.2 | 27666.1 | 67 | 1332 |
| truss | 1000 | 54.6 | 6760.0 | 54 | 970 |

`pilot87` is out by a factor of 47. `truss` by 124: its blocks are tiny and
almost every row carries decimal data, so the scaling is the whole cost.

**The prediction held.** 19 of the 110 need no scaling at all, and they are
every `ken`, every `pds`, and `d6cube`, `degen2`, `degen3`, `fit2d`,
`recipe`, `sctap1`, `sctap2`, `sctap3`, `seba`, `shell`, `sierra`. Their
entries are integers already, so D271's and D272's figures stand for them
unchanged. The row scale where one is needed is 53 to 72 bits, which is the
53 of a double's mantissa plus the exponent of the smallest entry.

## The second budget, which nobody had costed

A verifier that eliminates a block of `k` rows holds `k*k` numbers at once
and forms about `k**3/3` products of them. So block **size** limits it as
hard as block width, and that limit is not in D271 or D272 at all.

`budgets.py` joins the block size from `02-177/blocks.txt` with the width
from `integral.txt` and prints both. Neither file can answer alone, and
neither number is restated: the script does the arithmetic so the table is
re-derivable.

At a ceiling of 1024 MiB held and 1e10 products formed, **the second budget
costs exactly one instance**: `dfl001`, whose largest block is 3159 rows.
Its entries are cheap (2280.5 bits, 72 limbs) but 3159 rows dense is 2817
MiB and 1.05e10 products. Every other wide block belongs to an instance the
width budget already refused, and `pds-20` passes both: 1542 rows at 27
limbs is 263 MiB and 1.22e9 products.

**So the verifier's reach is 85 of 110**, and both refusals are a priori:
one pass over the basis gives the width, and the SCC pass gives `k`.

## What this changes

- `SPECS.md` section 5 carried D271's 97 as the budget answer. It is not the
  budget the verifier has to meet.
- The build order in `TODO.md` said "the elimination first, on the 97, with
  the Hadamard refusal in front of it; blocks second". Blocks are not
  optional and the refusal test is the integral bound, not D271's.
- The 24 that fall out are one family. Every `pilot`, `greenbe*`, `grow*`,
  `maros*` and `d2q06c` is in it. Raising `JM_EXACT_LIMBS` to cover
  `pilot87` means 2556 limbs, and a dense 1488-row block at that width is
  21 GiB. That instance is not reachable by widening the constant, and
  `bench/refusals.txt` should say so.

## Files

- `integral.c` — the instrument
- `run-integral.sh` — builds and runs it over all three sets
- `integral.txt` — the reading
- `budgets.py` — joins the two owners and prints both budgets
- `budgets.txt` — that join
