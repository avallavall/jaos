# 02-261 — pseudocost branching in the conic tree

The conic branch and bound of `src/conictree.c` branched on the most
fractional integer column. 02-258 refused handing QPLIB's MIQPs to it
because that rule could not close the trees the MIP tree's pseudocosts
close, and `TODO.md` row 5 lists its reach: 43 of CBLIB's 80 mixed-integer
instances at the work limit.

## The change

Each node remembers the column its parent branched on, the direction and
the distance moved. When the node's relaxation solves (a rough node
excepted), the gain over the parent's bound per unit of that distance is
added to the column's pseudocost for that direction. A node branches on
the column with the largest product of its two estimated gains, each the
column's mean so far or, before it has one, the mean over the columns
that have; each factor has a floor of `CT_PC_EPS` (1e-6), the MIP tree's
`MIP_PC_EPS`. At the root nothing is known, every estimate is the same,
and the product picks the most fractional column, as before.
`--branching most-fractional` keeps the old rule throughout.

Breaking a tie in favour of the more fractional column, as the MIP tree
does, was read on the instances below that moved: no change on the
robust, classical and shortfall files, and sssd-strong-20-4 and
sssd-weak-20-4 slower (947 to 1019 and 1602 to 1952 nodes). Ties go to
the lower index.

## CBLIB's 80 mixed-integer instances at 1e10 work units

`bench/measurements/02-255/cblib.sh 10000000000`, the library before
(`cblib-1e10-before.txt`) and after (`cblib-1e10-after.txt`).

- `OPTIMAL` goes from 26 to 28: sssd-weak-20-4, sssd-weak-25-4 and
  sssd-strong-25-4 are new, and shortfall_50_1 reaches the limit with its
  incumbent 7e-4 above the old optimum.
- The sssd family's bounds close in. sssd-weak-15-4 goes from a bound of
  113178 under an incumbent of 397401 to 327644 under 328419;
  sssd-strong-15-8 from 476882 to 621516; sssd-weak-30-4's incumbent from
  464590 to 264225.
- turbine07_lowb_aniso gets an incumbent, 2.247, where it had none, and
  uflquad-nopsc-20-100's falls from 1264 to 761.
- Some incumbents at the limit rise: sssd-weak-25-8 by 19%,
  uflquad-nopsc-10-150 by 6.4%, uflquad-nopsc-10-100 by 1.6% and
  turbine07_lowb by 0.8%. The three classical files, robust_100_3 and
  shortfall_50_2 end a little worse on both incumbent and bound, and
  shortfall_50_3 a little better on both.
- Of the 25 both runs solve, 16 take within 3% of the same work; estein5_C
  takes 0.63x, pp-n10-d10 0.78x, sssd-strong-20-4 0.84x, estein5_B 0.85x,
  estein5_nr1 0.89x and turbine54GF 0.94x; robust_50_1, 2 and 3 take
  2.1x, 3.3x and 4.2x (29 to 70, 33 to 121 and 33 to 171 nodes). Nodes
  over the 25: 5589 to 5168; work 3.75e10 to 4.22e10, the robust files
  being the difference.

## The generated models of 02-255

Seeds 1 to 3, 1000 models each, against brute force: the same answers
(the digests match), 0 failed, and nodes 2408, 2415, 2486 to 2408, 2409,
2482. These models branch 2.4 times on average, so the rule has little to
learn.

## The checker's split

The test built for this, a weighted rounding model in
`tests/test_conic.c` (39 nodes with pseudocosts, 63 with the most
fractional column), stopped the dev build on the assertion that the
complementarity terms' positive and negative sums are not negative
(`src/check.c`). A term whose compensated sum cancelled to 0 in its
leading part, with a compensation left over, went to the negative side as
the negation of that compensation. The side is now chosen by the term's
full value, and a term whose two parts agree in sign is added exactly as
before, so the checker's figures do not move elsewhere.
