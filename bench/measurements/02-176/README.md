# 02-176 — how much of the gate an exact verifier could prove, asked before it is built

D271. `TODO.md` has asked since D266 for "a measurement of how many of the
139 it can prove", and until now the only way to answer it was to build the
verifier and see. It is not: the answer comes from one pass over the basis,
in floating point, before a single limb is allocated.

## Why one pass is enough

An exact verifier factors the final basis `B` over the integers. Bareiss's
entries are minors of `B`, so Hadamard bounds every one of them, and the
largest thing the factorization ever holds is `det B`:

    log2 |det B|  <=  sum_j log2 ||b_j||_2

Past the limb capacity the verifier can refuse **a priori** and say which
instance and by how much, which is what this project wants instead of
running out of memory halfway through a factorization. A slack column is a
unit vector and contributes zero to the sum, which is why the answer is not
simply "no".

## What is here

| file | what it does |
|---|---|
| `hadamard.c` / `run-hadamard.sh` | solves each gate instance, reads the published basis, and sums the log2 column norms. One line per instance: rows, basic structurals, basic slacks, the bound in bits, the largest single column, and whether it fits |
| `hadamard.txt` | its output |

## The reading

**97 of 110 fit in 4096 bits. 13 do not.** The other 29 of the 139 are the
infeasible set and have no basis to bound.

So the verifier is buildable on 88% of the gate at today's capacity, with no
block-structure work at all. That is the number D266 asked for.

## The thirteen, and what their shape says

| instance | rows | basic structurals | bits | largest column |
|---|---|---|---|---|
| `ken-18` | 105127 | 88775 | 52523 | **0.79** |
| `stocfor3` | 16675 | 12169 | 36847 | 9.85 |
| `pds-20` | 33874 | 27909 | 15682 | **0.79** |
| `ken-13` | 28632 | 25257 | 14708 | **0.79** |
| `woodw` | 1098 | 863 | 8643 | 10.40 |
| `ken-11` | 14694 | 13459 | 7855 | **0.79** |
| `pds-10` | 16558 | 13278 | 7416 | **0.79** |
| `d2q06c` | 2171 | 1780 | 6596 | 11.18 |
| `osa-60` | 10280 | 2913 | 6396 | 4.09 |
| `cre-b` | 9648 | 4669 | 5198 | 7.16 |
| `stocfor2` | 2157 | 1536 | 4505 | 9.42 |
| `dfl001` | 6071 | 5834 | 4230 | 1.90 |
| `pds-06` | 9881 | 7517 | 4187 | **0.79** |

**Read the last column.** Six of the thirteen — every `ken` and every `pds`
— have a largest column of exactly **0.79 bits**. Their entries are tiny.
What puts them over the capacity is the **count**: `ken-18` has 88775 basic
structural columns, each contributing about 0.59 bits on average, and 88775
times almost nothing is still 52523.

That distinguishes two remedies and says which one is right:

- **Raising `JM_EXACT_LIMBS`** buys them. `ken-18` wants 1642 limbs where
  there are 128, so a `jm_nat` goes from 512 bytes to 6.6 KB and a
  `jm_rational` to 13 KB. Every operation is linear or quadratic in the limb
  count, so this trades a refusal for a cost that grows with the square of
  the model.
- **Block triangular form** is the one the shape argues for. The bound is a
  sum over columns, so splitting `B` into blocks splits the sum, and each
  block gets its own much smaller bound. That is exactly the case where the
  count is the problem and the entries are not.

`stocfor3` at 9.85 bits per column and `d2q06c` at 11.18 are the other
family: those have genuinely large entries as well as many of them, and
blocks alone may not be enough for them.

## What this does not say

It does not say the verifier is easy, only that it is not blocked on
capacity for 97 of 110. The Hadamard bound is an upper bound and a loose
one: an instance that fails it may still factor inside the budget. Nothing
here has factored anything.

It also assumes the whole basis is one block. Every figure above is what a
verifier with no block-structure discovery would face.
