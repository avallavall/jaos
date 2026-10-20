# 02-375 — why QPLIB_3312's iterations cost what they do

Taken on 2026-10-21 on the tree of ac77ea2. No code changed.

QPLIB_3312 stops at 1e11 work units after 23 conic iterations, about
4.3e9 per iteration. CBLIB's `nql90`, of a similar size, takes 7.2e8 per
iteration and solves in 27.

## The factor

Callgrind over one iteration puts 61% of the instructions in
`jm_ldlt_numeric`. The augmented system's symbolic factor, read with a
print that is not kept:

| model | system order | entries of the system | entries of L | operations (minimum degree) |
|---|---|---|---|---|
| QPLIB_3312 | 124094 | 762462 | 7.73e6 | 1.93e9 |
| nql90 | 113641 | 582659 | 2.77e6 | 2.58e8 |

## What it is not

- The disc rows. Each of QPLIB_3312's 8281 quadratic rows is
  `x² + y² ≤ 1` over two columns of its own. Written instead as a cone
  `(h, x, y)` with `h` fixed at 1, the model costs the same: 1.23e10 work
  units for 3 iterations against 1.22e10.
- The ordering's handling of dense nodes. The objective's column touches 91
  rows across the grid. Nested dissection never finishes under minimum
  degree's cost, on QPLIB_3312 or on nql90, with nodes of more than 3522
  (the current rule), 200, 64, 32 or 16 neighbours set aside.

## What it is

The rows. QPLIB_3312 has 15842 equality rows of 12 entries, 8281 of 2,
8281 of 3 and a few hundred of 6 to 9. Each 12-entry row joins 12 columns
in the system, and minimum degree's factor carries 2.8 times nql90's
entries and 7.5 times its operations.
