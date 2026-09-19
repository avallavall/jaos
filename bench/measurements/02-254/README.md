# 02-254 — wide cones, and CBLIB

02-253 left the conic interior point writing each second-order cone's
scaling block dense in its Newton system: a cone of `d` members cost `d²`
entries, and the factor carried a dense `d × d` block. CBLIB 2014, the
published conic set, has cones of 1000 to 99998 members (sched, nb_L2,
chainsing-*-2 and -3). On it the old code took 316 s and 1.05e12 work
units on sched_100_50_scaled, 545 s on sched_100_50_orig, and was not run
on the 17885- and 99998-member cones of sched_200_100 and
chainsing-50000-3, whose dense blocks alone are 2.5 GB and 80 GB. Eight
of the optima it gave failed the checker. This reading is what drove the
changes below, each measured on the 29 continuous CBLIB 2014 instances
under 70 MB and on the 3000 generated models of 02-253.

## What changed

1. **Each cone's scaling block is the identity and two rank-one terms.**
   The Nesterov-Todd point `w` has `w0² - |w1|² = 1`, and the block
   `W²/η² = 2ww' - J` has the eigenvalues `λ± = (w0 ± |w1|)²` on
   `q± = (1, ±w1/|w1|)/√2` and 1 on the rest. So
   `W²/η² = I + (λ+ - 1) q+q+' - (1 - λ-) q-q-'`, and the system takes two
   extra variables per cone, `p = η u'dz` with `u = sqrt(λ+ - 1) q+` on the
   positive side and `r = η v'dz` with `v = sqrt(1 - λ-) q-` on the
   negative side. It stays quasi-definite, because `I - vv'` has the
   smallest eigenvalue `λ- > 0`. Every cone takes it, whatever its width.
   - A first split, `D = diag(d1, 1, ..., 1)` with the rank-one terms along
     `(u0, α w1)` and `(0, β w1)`, satisfies the same algebra but leaves the
     head's diagonal at `d1 < 1/(1 + 2|w1|²)`, which near the boundary is
     below the regularisation. The factor then carried pivots of 1e17 and
     the walk stalled at 1e-10 on a cone of 71 members, where the dense
     block converged in 7 iterations. The split above converges in 7.
   - The dense block also fails where the split does not. On a cone of
     three members near its boundary (a relative determinant of 3e-11) its
     entries reach 2e10 around an eigenvalue of 1e-7, the factorization
     cancels to noise, and sched_100_50_orig's walk ended on a replaced
     pivot and a solve of 1e132. With the split for every cone, CBLIB
     reads 29 optima against 27 for the split only above 64 members (the
     variant table below).
2. **The ordering.** Minimum degree picked the smallest index of the
   lowest degree by scanning the degree's whole bucket, which is quadratic
   when many nodes share a degree. A heap on `(degree, index)` picks the
   same node: over 3000 random patterns with hubs, ordered by the old
   `chol.c` and the new one side by side, the same permutation, fill and
   work units, and the Maros-Meszaros set, which the barrier solves
   through this ordering, reads byte-identical. And on request a node with more
   than `max(16, 10 sqrt(n))` neighbours is ordered last (Amestoy, Davis
   and Duff's AMD rule): the two extra variables of a cone touch every
   member, and without the rule the ordering rescanned their lists at
   every elimination. The conic calls ask for it and the barrier's do not.
3. **Compensated sums** for a cone's inner products and determinants
   (Ogita, Rump and Oishi's Dot2). chainsing-50000-3's cone of 99998
   members reached a relative determinant of 2e-12, below the rounding of
   a plain sum of 99998 squares, and the walk took its point for one
   outside the cone (a NaN direction at iteration 10).
4. **The walk's gap includes the complementarity.** The gap was the
   difference of the homogeneous objectives, which the residuals can
   offset: nql60 stopped at 5e-14 by it with a complementarity `s'z` of
   5.6e-7 of the objective, and the checker refused the point. The gap is
   now the larger of the two.
5. **A walk that holds a point within 1e-6 stops after `CONIC_STALL_ITERS`
   iterations without progress.** With the complementarity in the gap, the
   nql walks went on to 113 and 200 iterations, the primal residual
   climbing from 1e-10 to 1e-5, and answered from their iteration-19
   point anyway.
6. **The Newton finish** writes each cone's map member by member, with the
   same floating-point operations as the dense matrix it replaces, so the
   02-253 digests did not move. Before, it allocated a `d × d` map per
   call and its Hessian cost `d³`. A cone wider than `CONIC_NEWTON_WIDE`
   enters its system as a diagonal and one rank-one term held by one
   extra variable. Its entry sort now breaks ties by insertion order, so
   entries at one position are summed in the same order under any C
   library's `qsort`, which does not promise an order for ties.
7. `jaos_add_cone` checks for a repeated member in linear time.

## Readings

`conic.c` of 02-253 prints a digest of every answer's bits since this
reading, so a change can be shown to leave the generated set untouched.

**The generated set** (02-253's `conic.sh`, seeds 1 to 3): changes 2, 6
and 7 alone leave the three digests as they were. With everything, 8
fail (10 before): six rays the projection cannot bring inside the ray
checker (2e-6 to 9e-2 past a side) and two walks that stall. 1679
optima, all taken by the checker; 713 rays, all confirmed; 600 planted
infeasibilities, all found. 34559 iterations (34597 before), 457753888
work units (429475776), worst dual violation 2.2e-16, longest walk 36.

**One cone of `W` members** (`wide.sh`, `-a'x` over the unit ball), work
units:

| members | dense block | new block |
|---|---|---|
| 8 | 60484 | 55534 |
| 16 | 109572 | 71166 |
| 32 | 303554 | 106478 |
| 64 | 1548866 | 215210 |
| 128 | 8862547 | 389767 |
| 256 | 59761831 | 700452 |
| 1024 | 3374581402 | 2814373 |
| 4096 | 301069600019 | 10945797 |

Every width solves to 1e-14 or better against `-||a||`. The ordering at
50000 members took 6.7 s before the dense-last rule and 0.24 s after;
200000 members solve in 1.0 s.

**CBLIB 2014**, the 29 continuous instances under 70 MB (`make cblib`),
with the final code:

| instance | status | checker | iterations | work units | objective | CBLIB reference | seconds |
|---|---|---|---|---|---|---|---|
| chainsing-1000-1 | optimal | yes | 17 | 187906419 | 30.18015748 | 30.18015749 | 0.14 |
| chainsing-1000-2 | optimal | yes | 14 | 145573736 | 30.18015748 | 30.18015799 | 0.11 |
| chainsing-1000-3 | optimal | yes | 18 | 155204882 | 30.18015748 | 30.18015678 | 0.10 |
| chainsing-10000-1 | optimal | yes | 16 | 1790765238 | 302.6106783 | 302.6106786 | 1.75 |
| chainsing-10000-2 | optimal | yes | 14 | 1507054044 | 302.6106784 | 302.6106885 | 1.31 |
| chainsing-10000-3 | optimal | yes | 16 | 1513999924 | 302.6106784 | 302.6106694 | 1.11 |
| chainsing-50000-1 | optimal | yes | 16 | 8910406946 | 1513.412993 | 1513.412995 | 12.18 |
| chainsing-50000-2 | optimal | yes | 13 | 8002179920 | 1513.412993 | 1513.413058 | 8.83 |
| chainsing-50000-3 | optimal | yes | 15 | 7527407208 | 1513.412992 | 1513.412947 | 7.20 |
| nb | optimal | yes | 21 | 2073940098 | -0.05070309465 | -0.05070309474 | 0.67 |
| nb_L1 | optimal | yes | 19 | 2077737072 | -13.01227068 | -13.01226111 | 0.70 |
| nb_L2 | optimal | yes | 14 | 2598823695 | -1.628971981 | -1.628971971 | 1.03 |
| nb_L2_bessel | optimal | yes | 13 | 1452292246 | -0.1025695112 | -0.1025695099 | 0.52 |
| nql30 | optimal | yes | 23 | 772142211 | -0.9460285015 | -0.9460240521 | 0.27 |
| nql60 | optimal | yes | 22 | 4793266778 | -0.9350529498 | -0.9350419057 | 1.43 |
| nql90 | optimal | yes | 27 | 19229222536 | -0.9313831627 | -0.9313565092 | 6.22 |
| nql180 | optimal | yes | 33 | 197893652921 | -0.927728617 | -0.9276439442 | 64.98 |
| qssp30 | optimal | yes | 19 | 445459445 | -6.496675735 | -6.496658827 | 0.22 |
| qssp60 | optimal | yes | 25 | 4567123774 | -6.56270647 | -6.562690123 | 1.62 |
| qssp90 | optimal | yes | 27 | 15444839661 | -6.594401689 | -6.594247629 | 5.61 |
| qssp180 | optimal | yes | 29 | 145484644192 | -6.639610842 | -6.639113368 | 49.45 |
| sched_50_50_orig | optimal | yes | 29 | 294702575 | 26673.00098 | 26673.00176 | 0.18 |
| sched_50_50_scaled | optimal | yes | 28 | 188264629 | 7.85203844 | 7.85203844 | 0.13 |
| sched_100_50_orig | numerical error | - | 40 | 946320685 | - | 181889.9519 | 0.47 |
| sched_100_50_scaled | optimal | yes | 35 | 694794277 | 67.1650311 | 67.1650311 | 0.37 |
| sched_100_100_orig | numerical error | - | 38 | 1734589503 | - | 717367.8234 | 0.94 |
| sched_100_100_scaled | optimal | yes | 40 | 1486656248 | 27.33078559 | 27.33078559 | 1.68 |
| sched_200_100_orig | numerical error | - | 40 | 4673167637 | - | 141360.4521 | 2.56 |
| sched_200_100_scaled | optimal | yes | 36 | 3707057236 | 51.81196103 | 51.81196103 | 2.14 |

The chainsing objectives agree with the references to the references'
own error (their primal errors run to 1.3e-7).

The old code, where it ran: nql and qssp ran about as fast as now (their
cones have 3 and 4 members), and the checker refused nql60, nql90,
nql180, qssp60 and qssp180; sched_50_50_orig and _scaled took about a
minute each against 0.18 and 0.13 s, sched_100_50_scaled 316 s
against 0.37, sched_100_50_orig 545 s against 0.47, chainsing-1000-3
10 s against 0.10. The sched_*_scaled optima it gave failed the checker;
now they agree with the library's reference to ten digits.

**The references of nql, qssp and nb_L1 are not the optimum.** JAOS's
objectives are below them, the checker takes JAOS's points on both sides,
and `cbfeval.py`, which reads the CBF file on its own, finds them feasible
(`evaluate.sh`):

| instance | JAOS | CBLIB reference | below by, relative | worst cone violation |
|---|---|---|---|---|
| nql30 | -0.946028502 | -0.946024052 | 4.7e-6 | 5.0e-12 |
| nql60 | -0.935052950 | -0.935041906 | 1.2e-5 | 1.1e-12 |
| nql90 | -0.931383163 | -0.931356509 | 2.9e-5 | 1.6e-13 |
| nql180 | -0.927728617 | -0.927643944 | 9.1e-5 | 1.3e-13 |
| qssp30 | -6.496675735 | -6.496658827 | 2.6e-6 | 2.2e-16 |
| qssp60 | -6.562706470 | -6.562690123 | 2.5e-6 | 7.3e-15 |
| qssp90 | -6.594401689 | -6.594247629 | 2.3e-5 | 2.4e-15 |
| qssp180 | -6.639610842 | -6.639113368 | 7.5e-5 | 8.6e-14 |
| nb_L1 | -13.012270675 | -13.012261114 | 7.3e-7 | 2.0e-13 |

The library's own dual residual on these is 4e-8, which a primal of norm
1e4 turns into a gap of the size seen, so its dual does not bound the
optimum either.

**The variants** (`CONIC_*` values as named; "optimal" and "checked" on
CBLIB):

| variant | 02-253 failed | 02-253 work | CBLIB optimal | checked | CBLIB work |
|---|---|---|---|---|---|
| dense block to 64 members, above it the new block and compensated sums | 10 | 429475776 | 26 | 18 | 322050294164 |
| the same, compensated sums for every cone | 10 | 423850318 | 27 | 18 | 319543809907 |
| the new block for every cone, compensated sums above 64 members | 9 | 468306709 | 29 | 19 | 415950940743 |
| the new block and compensated sums for every cone | 8 | 462136810 | 29 | 19 | 441859044207 |
| B: that, the complementarity in the gap, replaced pivots at ±2e-7, `CONIC_STALL_ITERS` 5 | 8 | 465105221 | 26 | 26 | 467289091449 |
| B without the stop after no progress | 8 | 468815833 | 26 | 26 | 1693438600760 |
| B without the complementarity | 8 | 459684785 | 28 | 19 | 354746010002 |
| B without the dense-last ordering | 8 | 465105221 | 25 | 25 | 564185386354 |
| B with replaced pivots at ±1e-13, as before | 8 | 464133444 | 26 | 26 | 467289091449 |
| final: B at ±1e-13 and `CONIC_STALL_ITERS` 3 | 8 | 457753888 | 26 | 26 | 440299195736 |

The ±2e-7 pivots are refused (`bench/refusals.txt`, `conic-pivot-swap`):
with the new block no pivot is replaced, and CBLIB reads the same work
units either way. `CONIC_STALL_ITERS` at 1 and 2 reads the same verdicts
at 411 and 426 thousand million work units on CBLIB, but changes answers
on 02-253; `CONIC_NEWTON_WIDE` at 16 and 256 reads as at 64.

The three numerical errors of the final code are sched_100_50_orig,
sched_100_100_orig and sched_200_100_orig. Their walk reaches 3e-8 on the
primal residual, then loses it as `mu` falls, and neither its best point
nor the Newton finish passes the checker; the library's own solutions
carry a primal error of 2e-6 to 9e-6 on them. Without the complementarity
in the gap they end `OPTIMAL` with the checker refusing them, so the
final code trades three unchecked answers for three honest errors and
seven more checked optima.

## Files

- `wide.c`, `wide.sh`: the one-cone reading.
- `cbfeval.py`, `evaluate.sh`: the independent evaluation of a point
  against a CBF file.
- `bench/cblib.manifest`, `bench/cblib.baseline`: the set and its record.
