# Scaling

Real instances arrive with coefficients spanning many orders of magnitude,
often because the modeller mixed units. That spread is what makes a pivot
look acceptable when it is not, and several Netlib instances are unsolvable
without addressing it — which is why scaling sits inside the first milestone
rather than after it (PLAN.md 2.5).

## What JAOS computes

Row factors `rho_i` and column factors `gamma_j` such that the scaled
magnitudes `rho_i * |a_ij| * gamma_j` cluster around 1.

**The stored matrix is never modified.** The factors live beside it, and the
original matrix remains the authority the independent checker judges
against. The solver will build its scaled working copy from these factors;
the checker keeps working in original space.

## Powers of two, always

Every factor is an exact power of two. Multiplying a double by a power of
two changes only its exponent field, leaving the mantissa bit-for-bit
intact, so scaling cannot introduce rounding error of its own. That is the
whole point: fix the exponent range without disturbing the digits. A factor
of `1/3.0` would improve the spread on paper and corrupt the data slightly
in practice.

Exponents are clamped to ±512, comfortably inside the double range.

## Curtis-Reid (default)

Chooses exponents minimising

```
sum over nonzeros of (log2|a_ij| - r_i - c_j)^2
```

The normal equations form a symmetric positive semi-definite system in
`[r; c]`, solved by Jacobi-preconditioned conjugate gradients. Each
iteration is one fixed-order pass over the CSC copy, so results are
bit-identical across runs and machines (D8) — verified by a test that
recomputes and compares raw bytes.

The system is singular: adding `k` to every `r_i` while subtracting it from
every `c_j` changes nothing. It is also consistent, so CG from a zero start
behaves; and the quantity that matters, the scaled magnitude, is invariant
under that freedom anyway. Tests therefore assert scaled magnitudes, never
individual factors.

When the matrix *is* an exact power-of-two scaling of a uniform matrix, the
least-squares residual reaches zero and every scaled magnitude comes out at
exactly 1.0. There is a test pinning that.

Empty rows and columns carry no information; their factor stays 1.

Reference: A.R. Curtis, J.K. Reid, "On the Automatic Scaling of Matrices for
Gaussian Elimination", IMA J. Applied Mathematics 10(1):118–124, 1972.

## Geometric-mean equilibration (option)

Alternating passes setting each factor to `1/sqrt(min * max)` over the row
or column, stopping when the spread stops improving. Kept as an option
because it responds differently to a handful of extreme outliers, which
Curtis-Reid averages over.

## What is not settled yet

Which mode is the better default across Netlib is a question for the
campaign, decided by measurement rather than by preference. Both modes exist
so the comparison can be run.
