# Scaling

Real instances often have coefficients that span many orders of
magnitude, for example because the modeller mixed units. With that spread
a pivot can look acceptable when it is not, and several Netlib instances
cannot be solved without scaling.

## What JAOS computes

Row factors `rho_i` and column factors `gamma_j` such that the scaled
magnitudes `rho_i * |a_ij| * gamma_j` cluster around 1.

**The stored matrix is never modified.** The factors live beside it, and
the independent checker judges against the original matrix. The solver
builds a scaled working copy from the factors and runs the whole solve on
it. The checker keeps working in original space.

## What the solver does with them

A solve that finds no scaling on the model computes Curtis-Reid. From
there the working copy is the scaled problem throughout: matrix values,
column bounds divided by their factor, row bounds multiplied by theirs,
costs multiplied by the column factor. This is the change of variable
`x_j = gamma_j * xhat_j`, so it is exact, and the solver's tolerances
apply in that space.

Answers come back in the caller's units. Column values carry their
factor, and row activities divide theirs out. The duals go the other way:
row duals multiply by the row factor and reduced costs divide by the
column factor, because a dual is a rate per unit of the thing it prices.

Every LP and QP solve uses Curtis-Reid: the dual and the primal simplex,
the barrier, PDLP and ranging all call `jm_model_scale` with
`JM_SCALE_CURTIS_REID`. No option or API call chooses another mode. The
conic interior point does not use it. It scales its own copy by Ruiz
equilibration (`CONIC_RUIZ`, `docs/tolerances.md`), and so does every
node of the conic tree.

## Powers of two, always

Every factor is an exact power of two. Multiplying a double by a power of
two changes only its exponent field and leaves the mantissa intact, so
scaling adds no rounding error of its own. A factor such as `1/3.0` would
round every scaled value.

Exponents are clamped to ±20 (`EXP_LIMIT` in `src/scale.c`; its reason is
in `docs/tolerances.md`). A factor beyond `2^20` changes the model's
units, and the residual tests in scaled space can then miss errors in the
original model.

## Curtis-Reid (default)

Chooses exponents minimising

```
sum over nonzeros of (log2|a_ij| - r_i - c_j)^2
```

The normal equations form a symmetric positive semi-definite system in
`[r; c]`, solved by Jacobi-preconditioned conjugate gradients. They run at
most `CR_MAX_ITER` (30) iterations and stop sooner when `r'z` falls to
`CR_TOL^2` times its start (`CR_TOL` is 1e-8), or when `p'q` is not
positive (`src/scale.c`). Each iteration is one fixed-order pass over the
CSC copy, so results are bit-identical across runs. A test recomputes the
factors and compares the raw bytes.

Across machines the results depend on `log2`, the one libm result that
IEEE does not pin down. A different C library could in principle tip one
of the roundings. Over the 139 reference instances, no scale factor moves
until `log2` is off by about 4x10^8 ulps.

The system is singular: adding `k` to every `r_i` and subtracting it from
every `c_j` changes nothing. It is also consistent, so CG from a zero
start converges, and the scaled magnitudes do not depend on that free
shift. The tests therefore assert scaled magnitudes. They assert a factor
only where the free shift cannot move it: an empty row or column, the
clamp at `2^-20`, and a repeated run that must give the same bytes.

When the matrix is an exact power-of-two scaling of a uniform matrix, the
least-squares residual reaches zero and every scaled magnitude comes out
at exactly 1.0. A test pins that.

Empty rows and columns carry no information, and their factor stays 1.

Reference: A.R. Curtis, J.K. Reid, "On the Automatic Scaling of Matrices
for Gaussian Elimination", IMA J. Applied Mathematics 10(1):118–124, 1972.

## Geometric-mean equilibration (internal)

Alternating passes set each factor to `1/sqrt(min * max)` over the row or
column. The passes stop when the row spread, in log2 units, improves by
less than `GEO_TOL` (1e-3), when the spread is 0, or after `GEO_MAX_PASS`
(20) passes. It reacts more to a few extreme outliers, which Curtis-Reid
averages over.

It is `JM_SCALE_GEOMETRIC` in `src/scale.c`, and only
`tests/test_scale.c` reaches it. As the simplex's default it measured
worse than Curtis-Reid (`bench/measurements/02-279/`, `scale-geometric`
in `bench/refusals.txt`).
