# 02-485 — exact numbers whose storage follows their size

Taken on 2026-11-18 on the tree of 1235543, with the change (TODO J19).
`head.sh` builds a tool, `read.sh` takes the reading, `summary.py` reads
it, `ab.sh` times two tools back to back, `blocks.sh` reads the shape of
the bases and `trace.sh` the cost of their dense blocks.

## The change

Every `jm_nat` held 128 limbs of 32 bits in place, 512 bytes, whatever
its size. A copy moved all of them, a dense block of `k` rows took
`k (k + 1)` times 528 bytes before its first product, and a number past
4096 bits ended the proof. With decimal data 20 of the 94 Netlib models
outgrew that (02-418); builds of 512 and 1024 limbs proved four of them
in 19 to 260 seconds, and the wider numbers made the blocks of `25fv47`
and others too large to hold (02-419).

A number now holds two limbs in place and the rest on the heap, as many
as its size needs, and grows as it grows, up to `JM_EXACT_LIMBS`, now
4096 limbs (131072 bits). It starts zeroed, is freed when done, is copied
only on purpose, and an operation may write over one of its operands. The
arithmetic takes the textbook algorithms its old shape did without:
Knuth's Algorithm D for long division (TAOCP vol. 2, 4.3.1), where the old
code went a bit at a time; Lehmer's gcd (4.5.2, Algorithm L), also a bit
at a time before; Knuth's sum and product of fractions, which take the gcd
of the small pieces (4.5.1); and for a quotient known to be exact,
Jebelean's division from the low end (J. Symbolic Computation 15, 1993),
checked modulo 2^32 - 1 and 2^32 + 1. The borrows are branch-free.

In the dense elimination, `VERIFY_BLOCK_BYTES` now counts what the block
holds, its table at 32 bytes a number and every limb as it grows;
`VERIFY_BLOCK_WORK` (2^40) caps its limb products; an update whose two
operands are zero is skipped. A refusal names its budget: a number, a
block's memory or a block's work. The feasibility relaxation's lattice
proof keeps the old 4096 bits (`RELAX_LATTICE_BITS`), so it skips the
same blocks as before.

The default solve paths do not reach the exact arithmetic, and
`make netlib netlib-infeas` reads byte for byte as before.

## The reading

`read.sh` solves the 94 with `--exact --exact-decimal`, once with the
tool of 1235543 and once with the tool of the change, the two runs of a
model back to back, four models at a time, an hour a run, on a machine
shared with other jobs (load 7 to 19). `read.txt` holds the records and
`summary.py read.txt` reads them.

| tool | proved optimal | a number outgrew the limbs | a block reached its work budget | over an hour |
|---|---|---|---|---|
| 1235543 | 74 | 20 | 0 | 0 |
| the change | 91 | 0 | 3 | 0 |

The 74 that proved before prove again with the same `objective_exact`,
and all 94 report the same work units, since the exact arithmetic
charges none and the floating-point solve is unchanged. None of the 74 is
slower: together they take 17.4 seconds where they took 221.6 (0.079x).
The longest drop the most: `degen3` from 59.4 to 2.8 seconds, `truss`
from 27.6 to 2.7, `wood1p` from 19.0 to 1.0; of those over a second, the
least is `fit2p`, from 2.4 to 1.2.

The 20 that outgrew the limbs, with the size of each basis's largest
block and its Hadamard bound (`blocks.txt`, from the tool of 1235543):

| model | largest block | bound bits | before | after | peak MiB |
|---|---|---|---|---|---|
| `grow7` | 98 | 4369 | 5.9 s | proved, 0.8 s | 5 |
| `scsd8` | 149 | 12089 | 3.9 s | proved, 1.3 s | 9 |
| `modszk1` | 174 | 11201 | 90.5 s | proved, 3.6 s | 9 |
| `grow15` | 202 | 9248 | 19.1 s | proved, 7.7 s | 13 |
| `grow22` | 239 | 12425 | 32.3 s | proved, 21.7 s | 18 |
| `25fv47` | 364 | 13084 | 69.9 s | proved, 22.4 s | 24 |
| `stair` | 324 | 9875 | 138.0 s | proved, 28.2 s | 27 |
| `greenbea` | 412 | 34027 | 34.8 s | proved, 29.9 s | 38 |
| `nesm` | 278 | 21745 | 12.8 s | proved, 42.2 s | 24 |
| `pilotnov` | 366 | 23023 | 13.2 s | proved, 44.7 s | 44 |
| `pilot4` | 279 | 14160 | 38.2 s | proved, 76.0 s | 33 |
| `greenbeb` | 513 | 38981 | 53.3 s | proved, 118.7 s | 77 |
| `maros-r7` | 104 | 54117 | 7.7 s | proved, 195.9 s | 73 |
| `perold` | 428 | 21603 | 38.1 s | proved, 226.2 s | 86 |
| `pilot-we` | 519 | 26600 | 33.5 s | proved, 278.7 s | 132 |
| `pilot-ja` | 544 | 29530 | 46.9 s | proved, 730.9 s | 169 |
| `pilot87` | 1487 | 65136 | 6.6 s | work budget, 371.7 s | 564 |
| `pilot` | 1103 | 42653 | 2.7 s | work budget, 433.6 s | 644 |
| `d2q06c` | 892 | 50150 | 62.2 s | work budget, 449.6 s | 287 |
| `dfl001` | 3129 | 8192 | 20.9 s | proved, 2177.2 s | 348 |

"Before" is the time the old tool took to give up. 17 of the 20 prove,
15 of them within 300 seconds, the limit 02-418 and 02-419 used; the 512-
and 1024-limb builds of 02-419 proved four (`grow7`, `scsd8`, `modszk1`,
`grow15`) in 19 to 260 seconds. `dfl001` alone needs exact pivots: its
floating-point basis is 6 pivots from the exact optimum of the decimal
model, and each pivot solves the basis again, so it takes 36 minutes. A
refusal now takes up to 450 seconds where the old ones took at most 138:
the old arithmetic stopped at 4096 bits, and the new one runs until a
block passes its budget.

## The budgets

`trace.sh` reads the cost of the blocks with `trace.patch`, which prints
each block's limb products and bytes, on a build without a work budget and
before the zero skip (`trace.txt`). The costliest solve of a block that
proves is `pilot-ja`'s transposed one, 8.0e11 limb products on 544 rows;
`perold`'s is 2.0e11 and `pilot-we`'s 2.8e11. `VERIFY_BLOCK_WORK` is
2^40, 1.1e12, the power of two above it. In that build `d2q06c`, `pilot`
and `pilot87` reached `VERIFY_BLOCK_BYTES` at 2.6e11 to 7.1e11: each
update swapped the elimination's scratch number into the table, so an
entry that stayed zero held the storage of a large one. With the zero
skip they stay small, and the three reach the work budget instead, with
at most 644 MiB resident. `dfl001`'s block of 3129 rows costs 4.5e9 limb
products a solve, but holds 321 MiB, 299 MiB of it its table, and sweeps
3129 cubed updates of numbers a limb or two long, so its time is in the
sweep and not in the products.

`JM_EXACT_LIMBS` is 4096: twice the largest Hadamard bound of the 94,
65136 bits on `pilot87`, so a product of two numbers of that size fits.
No proof of the 94 reaches it.

## What was measured and dropped

Karatsuba's product (TAOCP vol. 2, 4.3.3) from 32 limbs up gave no gain
on `grow15`, `grow22` and `25fv47`, alternating with the tool without it
(`ab-karatsuba.txt`), and was taken out. A profile of `grow7` with
callgrind put 50% of the instructions in long division and 42% in
schoolbook products, so the division got the two changes that paid: the
branch-free borrow, 4% to 16% on `grow7`, `modszk1`, `grow15` and `grow22`
(`ab-borrow.txt`), and the exact division from the low end, 6% to 26% on
top of it on those and `scsd8` (`ab-divexact.txt`).

`storage-20.txt` is the first reading of the 20, 600 seconds a run, with
the storage, Algorithm D and Lehmer's gcd only: 14 proved, and
`pilot-ja`, `pilot-we`, `dfl001` and `pilot` ran out of time.

## What is left

`d2q06c`, `pilot` and `pilot87` need a fraction-free elimination that
does not sweep the whole block, and `dfl001` one that does not hold and
sweep a 3129-row table of mostly small numbers on every pivot. A proof
solves its largest block at least four times (the primal and dual
systems, then again to publish the point), and one factorization would
serve them all. Dixon's p-adic lifting
(`docs/research/exact-verification.md`) solves with word-sized numbers
and lifts, and needs neither.
