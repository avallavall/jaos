# Literature on the last three presolve families

Sources read in full (text extracted from PDF, formulae unreliable):
Achterberg, Bixby, Gu, Rothberg, Weninger, *Presolve Reductions in Mixed
Integer Programming*, INFORMS JoC 32(2):473-506, 2020, DOI
10.1287/ijoc.2018.0857 (read as ZIB-Report 16-44); Galabova, *Presolve, crash
and software engineering for HiGHS*, PhD thesis, Edinburgh 2023, DOI
10.7488/era/2974; Gemander, Chen, Weninger, Gottwald, Gleixner, Martin,
*Two-row and two-column mixed-integer presolve using hashing-based pairing
methods*, EJCO 8:205-240, 2020, DOI 10.1007/s13675-020-00129-6.

## What bears on whether to build them at all

**HiGHS omits parallel rows and parallel columns on purpose.** Galabova
§3.2.3, quoted: the rules were left out, performance on the LP test set was
satisfactory without them, and "many of the parallel rows and columns were
eliminated due to a combination of other elimination rules, applied in a
suitable order." JAOS's field value is measured against HiGHS (D81).

**Gurobi's own measured effect, on MIP, against their default:** parallel and
nearly parallel rows 3% overall; parallel columns 4% on the ">=10 s" tranche;
dominated columns 1% overall. MIP numbers, not LP, and not a dual simplex.
Against D81's 1.42x for presolve as a whole these three are the tail.

## What has no published source, and would be ours to derive

1. The **dual postsolve for parallel rows**. Not in the ZIB report; not in
   Galabova because HiGHS omits the rule. The constraint is
   `s*y_q + y_r = y_m`, and complementary slackness fixes the split, but the
   presolve has to record which row supplied each surviving endpoint. Under
   `s < 0` two endpoints come from different rows and both must be stored.
2. The **primal split rule for merged duplicate columns**. The ZIB report says
   the value can be split and does not say how.
3. The **parallel-with-mismatched-cost case**. Definition 6 of §6.4 carries no
   scale factor, so it does not capture it. No published rule found.

## No published tolerance

None of the three documents gives a tolerance for deciding two rows or two
columns are parallel. The normalisation (max-norm 1, sign fixed by requiring a
positive coefficient on the lowest-index variable) says what space the
comparison lives in. The threshold is not published and would need a sweep.

The transferable pattern is the Markowitz-style test the same report uses for
implied-free substitution: relative to the row's largest coefficient, not
absolute. `|a_qc - s*a_rc| <= tau * max_c' |a_qc'|`.

**Failure mode**: a row is dropped that the other does not imply. The
polyhedron grows and the solver returns a point infeasible in the original,
possibly with a better objective than the optimum. **This family changes the
answer, not the path.**

## Constants the ZIB report does give (prose, not tables)

feasibility tolerance 1e-6 default; infinity 1e30; minimum bound change in
propagation 1e3 * eps; maximum modulus of a new bound 1e8; Markowitz-style
criterion for implied-free substitution `|a_ij| >= 0.01 * max_k |a_ik|`, the
0.01 rising to 0.5 or 0.9 under high numerical focus.

Gemander: `|lambda| <= 1000` on the scale factor, "as too large or too small
scaling factors can lead to numerical problems".

## The reductions, for when they are built

**Parallel rows** (§5.2). Rows q, r parallel if `A_q. = s * A_r.`, s != 0.
Three cases. Both equalities: drop r if `b_q = s*b_r`, infeasible otherwise.
One equality: drop r if `b_q <= s*b_r` and s>0, or `b_q >= s*b_r` and s<0.
Both inequalities, s>0: drop the weaker. **Both inequalities, s<0: the two
merge into a range row** `s*b_r <= A_q.x <= b_q`, or an equality when they
meet. That case creates a row rather than removing one.

**Duplicate columns** (§6.3). Parallel if `A_.k = lambda * A_.j`. Merge needs
`c_k = lambda * c_j` — that is the duplicate/dominance boundary, and it is
written in the source. `y := x_j + lambda*x_k`, bounds by Minkowski sum with
the ends crossing when lambda<0, and `c_y = c_j`. The "requirements on
variable types" in the source are **entirely about integrality**; in
continuous LP there is no bound condition, because the Minkowski sum of two
real intervals is an interval. Copying the integer condition would restrict
the reduction for no reason.

Dual postsolve is forced, with no choice: `d_j = d_y` and `d_k = lambda * d_y`.

Worked example from the source, usable as a test: `min 2x1+4x2+x3` s.t.
`-x1-2x2-x3 <= -10`, `x1 in [0,3], x2 in [0,4], x3 in [0,5]`; lambda=2,
`y in [0,11]`, `y* = x3* = 5`, and the same `y*` admits `(0, 2.5)` and
`(1, 2)`. The ambiguity is the whole difficulty in one line.

**Dominated columns** are two different techniques sharing a name. Form A,
dual fixing (§4.4): with all rows equalities or `<=` and the variable in no
equality, `c_j >= 0` and `a_ij >= 0` for all i fixes `x_j := l_j`; with
`l_j = -inf` and `c_j > 0` the problem is unbounded or infeasible; with
`c_j = 0` the variable **and every row it touches** are removed. Form B,
pairwise (§6.4 Definition 6 and Theorem 1): `x_j` dominates `x_k` if
`c_j <= c_k` and `a_ij <= a_ik` for all i. Four cases, each needing one
infinite or **implied** bound.

## Detection

Parallel rows and columns: two-level hashing, attributed to Andersen and
Andersen. First hash on the support; second on coefficients normalised to
max-norm 1 with the sign fixed by the lowest-index variable. Buckets that stay
large are sorted lexicographically and only direct neighbours compared. The
authors report neither needs work limits.

**The second hash lives in floating point.** Two exactly parallel rows do not
produce bit-identical normalised values after a division in double, and the
source does not say how the hash is quantised. Safe form: hash the support
only, compare numerically inside the bucket.

Dominated columns: only columns with an infinite or implied bound are
candidates to dominate; the candidates to be dominated come from the shortest
row where the dominating column has a coefficient of the right sign; a 32-bit
support signature (bit b set if the column touches a row `i = b mod 32`) rules
out most pairs with one bitwise operation.

## Two consequences for JAOS beyond the algorithm

- **"Unbounded or infeasible" must exist as a single status.** Both sources
  say a presolve cannot distinguish them. Galabova: "dual infeasibility does
  not imply primal unboundedness, since the problem could also be primal
  infeasible. Hence, there is only one status for infeasible or unbounded."
  That is a public API question, not an algorithm one.
- **Determinism has four free choices** to pin: bucket traversal order (sort
  by signature and index, never iterate a hash table), the pivot that computes
  `s` or `lambda`, the dual split between two active parallel rows, and the
  primal split in a column merge. None needs randomness.

## Verified citations not read in full

Andersen & Andersen, *Presolving in linear programming*, Math. Prog.
71(2):221-245, 1995, DOI 10.1007/BF01586000 — the origin of all three
families, confirmed by both sources that were read. Tomlin & Welch, ORL
5(1):7-11, 1986, DOI 10.1016/0167-6377(86)90093-3 (Crossref has the author as
"L.A. Tomlin", an Elsevier metadata error). Gondzio, INFORMS JoC 9(1):73-91,
1997, DOI 10.1287/ijoc.9.1.73, **with its addendum** Meszaros & Gondzio 13(2):
169-170, 2001, DOI 10.1287/ijoc.13.2.169.10519, which corrects Propositions 1
and 2 — read the addendum before implementing dominance from Gondzio.

Dead end, so nobody repeats it: Ploskas & Samaras, *Linear Programming Using
MATLAB*, ch. 4, covers eleven presolve methods and none of these three.
