# 02-418 — exact solving over the decimals a file wrote

Taken on 2026-10-22 on the tree of 6642d88, with the change. `read.sh`
takes the reading.

## The change

The exact arithmetic read each number of the model as its double, exactly.
An MPS value of 0.1 was then 3602879701896397/36028797018963968, and a
model whose rows hold only in decimal, such as `x = 0.1`, `y = 0.2`,
`x + y = 0.3`, was infeasible over the rationals. 02-358 found `sierra`
and `scorpion` infeasible that way.

`jaos_set_exact_decimal` (option `exact_decimal`, `--exact-decimal`) makes
the exact arithmetic read each number as the shortest decimal that rounds
to it, found in exact arithmetic with no library formatting: 0.1 is 1/10
and 4.35 is 87/20. The basis solver scales each row by a power of five as
well as a power of two, so its matrix stays integer. The proof file
carries `data decimal`, and `jaos_check_proof` reads the model the way the
file says.

## The reading

The 94 Netlib models under `solve --exact`, 300 seconds each
(`binary.txt`, `decimal.txt`):

| reading | proved optimal | infeasible | a number outgrew the limbs |
|---|---|---|---|
| binary | 59 | 2 | 33 |
| decimal | 74 | 0 | 20 |

`sierra` and `scorpion` prove optimal. 13 models whose proof outgrew the
limbs in binary now prove: `80bau3b`, `bandm`, `bnl1`, `bnl2`, `bore3d`,
`brandy`, `capri`, `cycle`, `e226`, `maros`, `stocfor3`, `truss` and
`wood1p`. Their decimal numbers are much shorter than their doubles'.
Every model that proved in binary proves in decimal, and none needs an
exact pivot, where 9 needed 1 to 82 in binary: the floating-point basis
is already the exact optimum of the decimal model.

On the doubles, the tool with the change reads every line the same as the
tool of 6642d88 (`binary-head.txt`).
