# 02-211 — the exact proof file over the reference sets: 18 of 29 certificates hold with no tolerance, and all 28 optimum proofs do

What decided D328. `certsweep.sh` solves each instance, writes the exact
proof file with `jaos solve --proof`, and judges it back with `jaos check
--proof`, which reads the model and the file and nothing else. Two sets:
the 29 pinned infeasibles of `bench/netlib-infeas.manifest` and the 94 of
`bench/netlib.manifest`.

Each line is `name status verdict [where it failed]`.

## The infeasibles: 18 hold, 11 broken (`infeas.txt`)

Every certificate is written, so the vector always exists. **18 of the 29
are certificates exactly**, with no tolerance anywhere.

**All eleven failures are the same thing, and it is always a column.**
Each broken line names an `at_col` and never an `at_row`: a column whose
`(A'y)_j` is exactly nonzero while the bound on the side that
multiplier points at is infinite, so the supremum of `y'Ax` over the box
is infinite and no gap can be proved.

That is the term `jaos_check_certificate` deliberately ignores. Its rule
is that a sum of doubles cannot place a zero more finely than the
magnitudes that went into it, so a term below `tol` times its own traffic
is read as the zero it is trying to be (D254). The exact checker has no
such rule, because it has no tolerance: a rounding away from zero is not
zero. **So the two checkers disagree on exactly these eleven, and the
exact one is the strict one.** D256 reads 28 of 29 certifying at 1e-7;
this reads 18 of 29 certifying at nothing at all.

Neither number is wrong. They answer different questions: "is this vector
a certificate to the precision doubles can express?" and "is this vector
a certificate?".

## The optima: 28 hold, 0 broken, 66 with no proof (`netlib.txt`)

An optimum's proof needs `jaos_verify` first, and 66 of the 94 are
refused a priori by its Hadamard bound before a limb is allocated (D273,
D274) — those write no file and are counted apart, since a file that was
never written is not a file that failed.

Of the 28 that do get a proof, **every one passes the file checker, and
none is broken.** That is worth more than the count. The two paths share
no code: `jaos_verify` proves a basis by exact block elimination and
reports the coordinates, while `jaos_check_proof` reads the coordinates
out of a file, knows nothing about any basis, and re-derives primal
feasibility, dual feasibility and complementary slackness from the model.
Two independent routes to the same verdict, agreeing 28 times out of 28.

## What this does not measure

The eleven broken certificates are not shown to be wrong answers. The
models are infeasible — the gate says so on every one of the 29 — and the
solver's own verdict is not in question here. What is measured is whether
the *published vector* proves it with no tolerance, and on eleven of them
it does not.
