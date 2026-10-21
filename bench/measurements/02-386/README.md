# 02-386 — J21's model rescaled by hand

Taken on 2026-10-21 on the tree of d9f6119, for TODO row J21. No code
changed.

`bench/measurements/02-382/seed4-533.mps` ends `numerical_error`: a
rotated cone `2·C1·C2 ≥ C5² + 16` and the quadratic row R4 hold C1 near
0.007, so C2 sits near 1100 and the objective near 3.19e5, and the walk's
tau falls to 1.3e-7. Two scalings were tried by hand on the file, each
keeping the model's meaning.

## The cone's heads

`rescale.py` sets `C1 = C1'/k` and `C2 = k·C2'`, which leaves the rotated
cone as it is and brings both heads near 1 at k = 1000.

| k | status | iterations |
|---|---|---|
| 1 | `numerical_error` | 26 |
| 30 | `numerical_error` | 32 |
| 1000 | `numerical_error` | 25 |
| 30000 | `numerical_error` | 92 |

## R4's multiplier

Raising C1 a little lets C2 fall a lot, so R4's multiplier is large.
`rowscale.py` multiplies R4 (linear part, quadratic part and side) by s,
which divides that multiplier by s.

| s | status | the walk's last tau |
|---|---|---|
| 1 | `numerical_error` | 1.3e-7 |
| 1e3 | `numerical_error` | 4.0e-9 |
| 1e6 | `numerical_error` | 2.7e-3, primal residual 1.2e-2 |
| 1e8 | `numerical_error` | 8.1e-5, primal residual 7.2e-6 |

Neither the heads' balance nor R4's scale is the cause on its own.

## The checker's reduced-cost test

At the point the walk leaves, read with a print that is not kept, the
row multipliers are 4.46e7 on R1 and -2.53e7 on R4. C1 sits off its
bounds at 0.0094, and its reduced cost is -4.56, a sum of terms near
3.7e7: 1.2e-7 of their size. The checker's test on a reduced cost is
absolute, 1e-7 whatever the size of its terms, so this point would need
15 to 16 exact digits. The row test, by contrast, is relative to the
row's traffic.
