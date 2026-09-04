# Exact verification of a final basis — what the literature says

Background for `SPECS.md` section 5, whose verifier is still missing.
Produced by `literature-scout` on 2026-09-04, on four questions: which
elimination, what the published verifiers actually do, whether anything
avoids a full exact solve, and how those papers report an instance they
could not certify. It is a reading list with mechanism, not a design.

## Read this before trusting a number below

Each citation in §7 carries its own status. **VERIFIED** means the record
was checked against the publisher, Crossref, dblp or an institutional
archive. **TEXT REACHED** means the agent read the text. Most entries are
verified records whose text was not reached, and two are marked
SEMI-VERIFIED because the repository host refused the request.

One number set is explicitly shaky: the LPex netlib table was read through
a lossy PostScript converter. Re-take it with `pdftotext` before quoting
it. `docs/research/primal-simplex.md` records that `pdftotext` 24.02 works
from WSL, so this is a re-run and not a limit.

**A correction the search produced.** The SODA 2003 author list carried
into the search request was wrong. Schirra and Skutella are not authors;
Seel and Schulte are. Checked twice, against dblp key
`conf/soda/DhiflaouiFKMSSSW03` and against Mehlhorn's own publication list.

## 1. Which elimination — Bareiss, on a block-triangularised basis

The literature does not treat this as a close call.

At step k, for i, j > k:

    a_ij^(k) = ( a_kk^(k-1) a_ij^(k-1) - a_ik^(k-1) a_kj^(k-1) ) / a_(k-1,k-1)^(k-2)

with `a_00^(0) = 1`. Every division is exact. That is Bareiss's theorem and
it follows from Sylvester's determinant identity. Each `a_ij^(k)` is exactly
a (k+1)x(k+1) minor of the **original** matrix, so Hadamard bounds it.

Growth, with m basis rows, σ the largest entry and δ the most nonzeros in a
column:

| method | bound on entry size |
|---|---|
| Bareiss, dense | `log2 det B <= (m/2) log2 m + m log2 σ` |
| Bareiss, sparse (column-norm Hadamard) | `<= m (log2 σ + 0.5 log2 δ)` |
| rational GE, no normalisation | bit length roughly doubles per step |
| rational GE, gcd at every step | bounded, but a gcd per operation |

The last two rows are the reason not to build the obvious thing. Escobedo
2022 (TEXT REACHED) states the second: exact rational arithmetic "generally
cannot solve SLEs in time proportional to arithmetic work because they must
constantly carry out GCD operations to prevent exponential growth". The
"doubles per step" claim is folklore-with-consensus in the fraction-free
literature; no page-level citation was reached for it.

**What Bareiss gives this project for free.** The solution comes out over
**one common denominator, `det B`**. No per-entry normalisation at all: one
big integer denominator and m big integer numerators. That is a much better
fit for `src/exact.c`'s fixed-capacity, no-allocation model than a vector of
independently normalised `jm_rational`.

The solve, from Escobedo 2022 (TEXT REACHED): scale to `x' = det(B) x`;
integer forward substitution with the same fraction-free recurrence
(`y_i = (l_kk y_i - l_ik y_k) / l_(k-1,k-1)`); integer backward
substitution; one division at the end, `x_i = x'_i / l_nn` with
`l_nn = det B`.

**The step before the elimination is the lever.** LPex's first module is
block-structure discovery, and it is what makes exact solving of an LP basis
affordable. Two deterministic stages: permute columns so every diagonal
entry is nonzero (maximum transversal, Duff 1981), then take strongly
connected components of the digraph with an edge i→j when `b_ij != 0` and
order them topologically (Pothen & Fan 1990). The matrix becomes block
triangular and each block's determinant is small. LP bases are usually very
reducible.

**Two-step (multistep) Bareiss** is Bareiss 1968's actual subject and
roughly halves dense multiplications and exact divisions. After block
triangularisation the blocks are small, so it is not where to start.

## 2. What the published verifiers do

**Dhiflaoui et al. 2003, LPex** (TEXT REACHED, lossy). Given an LP and a
basis, decide primal feasible and dual feasible; both means optimal.
Modules: block-structure discovery, sparse LU by Bareiss, sparse solving by
Wiedemann over finite fields with Chinese remaindering, a feasibility test,
and exact primal, dual and criss-cross solvers for repair. Table 1 covers
**20 netlib instances**, not the set: 0.07 s to about 2697 s, relative
objective errors 0 to about 2.93e-11. Two CPLEX bases were feasible but not
optimal, `etamacro` and `scsd6`, both repaired in seconds. Re-take these
numbers before quoting them.

**Koch 2004** (ABSTRACT ONLY). Claims exact optimal objective values for all
netlib problems, by a program using exact rational arithmetic. The method
and its caveats are in a PDF that was not reached.

**Applegate, Cook, Dash, Espinoza 2007, QSopt_ex** (TEXT NOT REACHED). Later
authors call the method **incremental precision boosting**: solve in
floating point, take the basis, check it exactly, and on failure re-solve at
higher precision and repeat.

**Gleixner, Steffy, Wolter 2016** (ABSTRACT ONLY) and **Gleixner & Steffy
2020** (TEXT REACHED). LP iterative refinement: solve a sequence of LPs
sharing the constraint matrix and differing only in objective, right-hand
side and bounds; exact arithmetic computes and stores the transformed
problems and floating point solves them. The 2020 paper defines a
limited-precision oracle and gives two algorithms, refinement alone and
refinement interleaved with rational reconstruction. Basis verification is
explicit: build B from the basis columns, solve exactly, check primal and
dual feasibility in rational arithmetic, discard and continue on failure.

**Eifler, Nicolas-Thouvenin, Gleixner, arXiv 2311.08037** (TEXT REACHED).
The clearest statement of the procedure found: rational LU of the basis
matrix, rational triangular solves for exact `(x*, y*)`, check `A x* = b`
and `x* >= l`, check `c - A' y* >= 0`, both hold so certified optimal. It
also reports that **the factorization approach clearly outperformed the
reconstruction approach**.

## 3. Is there a way round the exact solve

Four candidates, none of which replaces it.

- **Rational reconstruction** from a refined floating-point solution
  (Gleixner & Steffy 2020, Algorithm 2). Refine to about 2^-k, reconstruct
  each component by continued fractions under a denominator bound, verify
  exactly. Their own successor paper reports the factorization beat it.
  **Do not build this first.**
- **Safe bounds by directed rounding** (Neumaier & Shcherbina 2004). No
  exact arithmetic: interval arithmetic on an approximate dual gives a
  rigorous bound on the optimal objective. Needs finite variable bounds. It
  proves a bound, not that a basis is optimal — a different deliverable.
- **Althaus & Dumitriu 2009/2012.** Certifies feasibility and an objective
  value using rational arithmetic as little as possible, by finding a
  feasible point far from any tight inequality. Mechanism not reached.
- **Exact residual only.** For a point already held exactly, checking is
  exact evaluation, which JAOS has (`jm_exact_evaluate`, D267). It is the
  checker after the elimination, not a replacement for it.

## 4. How a paper reports what it could not certify

- LPex 2003 published 20 netlib instances, one at about 45 minutes. That was
  the practical limit of that generation.
- Koch 2004 claims all of netlib; the caveats are in the unreached PDF.
- Applegate 2007 and Gleixner 2020 do not report "could not certify". Their
  failure mode is precision exhausted, answered by boosting. SoPlex caps
  boosting at 1000 bits and the 2023 paper reports the cap was never
  reached.
- The 2023 paper reports 79 of 84 on lplib and 91 of 91 on cutlib under a
  7200 s limit, with the 5 listed as unsolved.

**A first-class "could not prove this one" is not standard in this
literature.** That is a gap, not a failed search.

What is citable and worth building for the budget question: compute the
column-norm Hadamard bound on `det B` **before allocating anything**. It is
`sum_j 0.5 log2( sum_i b_ij^2 )`, evaluated in floating point rounding
upward. Past the limb capacity, refuse a priori. That is a proof rather than
a guess and it costs one pass over B. Block triangularisation may cut it by
a lot, so it runs first.

## 5. The constants each technique brings

**Bareiss brings none.** No threshold, no tolerance, no interval. The only
choice is the pivot, and a nonzero pivot is a correctness requirement rather
than a tuned number. A Markowitz threshold for sparsity is the one number it
would add, and this project already sweeps one for the floating-point LU.

Everything else carries constants, all of them SoPlex defaults measured on
the authors' own sets and none transferable: iterative refinement's scaling
factors and stall test, precision boosting's schedule (start 64 bits, first
boost 192, growth 1.5x, cap 1000) and its tolerance scaling, and rational
reconstruction's denominator bound and growth.

## 6. Where this collides with the premises

- **Wiedemann is randomized.** Its analysis needs the randomness, so seeding
  it does not make it deterministic in the sense the reproducibility rule
  means. LPex used it; JAOS may not. Bareiss and Dixon p-adic lifting are
  deterministic. Chinese remaindering with early termination is randomized;
  a-priori CRT against a Hadamard bound is not.
- **Block triangular form is deterministic only if the matching's tie-break
  is fixed by index.** Worth writing down before it is built.
- **No dependency is needed.** All of the above builds on `src/exact.c`.
  SPEX and SuiteSparse would be a dependency, and their source is out of
  bounds under the no-reading-other-solvers rule regardless.
- **It does not change any answer.** The verifier is a read-only pass over a
  published basis and stays out of the simplex's control flow.
- **An outcome no paper names.** D267 evaluates the *published point*
  exactly. The verifier computes the exact *basic point*, a different
  vector. They can disagree, and the disagreement is a real report: the
  basis is optimal and the published point is not its exact basic solution.

## 7. Citations

1. **Bareiss, E.H.** "Sylvester's identity and multistep integer-preserving
   Gaussian elimination." *Mathematics of Computation* 22(103):565–578,
   1968. DOI 10.1090/S0025-5718-1968-0226829-0. VERIFIED, TEXT NOT REACHED.
2. **Bareiss, E.H.** "Computational Solutions of Matrix Problems Over an
   Integral Domain." *IMA J. Applied Mathematics* 10(1):68–104, 1972.
   DOI 10.1093/imamat/10.1.68. VERIFIED, TEXT NOT REACHED.
3. **Dhiflaoui, M., Funke, S., Kwappik, C., Mehlhorn, K., Seel, M.,
   Schömer, E., Schulte, R., Weber, D.** "Certifying and repairing solutions
   to large LPs: how good are LP-solvers?" *SODA 2003*, pp. 255–256.
   VERIFIED (dblp and Mehlhorn's list). TEXT REACHED, lossily.
4. **Koch, T.** "The final NETLIB-LP results." *Operations Research Letters*
   32(2):138–142, 2004. DOI 10.1016/S0167-6377(03)00094-4. Preprint
   ZIB-Report 03-05. VERIFIED. ABSTRACT ONLY.
5. **Applegate, D.L., Cook, W.J., Dash, S., Espinoza, D.G.** "Exact
   solutions to linear programming problems." *Operations Research Letters*
   35(6):693–699, 2007. DOI 10.1016/j.orl.2006.12.010. VERIFIED,
   TEXT NOT REACHED.
6. **Gleixner, A.M., Steffy, D.E., Wolter, K.** "Iterative Refinement for
   Linear Programming." *INFORMS J. Computing* 28(3):449–464, 2016.
   DOI 10.1287/ijoc.2016.0692. VERIFIED. ABSTRACT ONLY.
7. **Gleixner, A.M., Steffy, D.E., Wolter, K.** "Improving the accuracy of
   linear programming solvers with iterative refinement." *ISSAC 2012*,
   pp. 187–194. DOI 10.1145/2442829.2442858. VERIFIED, TEXT NOT REACHED.
8. **Gleixner, A., Steffy, D.E.** "Linear programming using
   limited-precision oracles." *Mathematical Programming* 183:525–554, 2020.
   DOI 10.1007/s10107-019-01444-6. arXiv:1912.12820. VERIFIED,
   **TEXT REACHED**.
9. **Eifler, L., Nicolas-Thouvenin, J., Gleixner, A.** "Combining Precision
   Boosting with LP Iterative Refinement for Exact Linear Optimization."
   arXiv:2311.08037. **TEXT REACHED**. A journal version exists
   (DOI 10.1287/ijoc.2023.0409) whose volume, issue and pages were not
   confirmed: cite the arXiv entry.
10. **Cook, W.J., Steffy, D.E.** "Solving Very Sparse Rational Systems of
    Equations." *ACM TOMS* 37(4), Article 39, 2011. DOI 10.1145/1916461.1916463.
    VERIFIED. ABSTRACT ONLY. **The one paper that directly compares methods
    on LP basis matrices** — rational LU, Wiedemann, Dixon and Wan — and its
    text was not reached. Read this first.
11. **Lourenço, C., Escobedo, A.R., Moreno-Centeno, E., Davis, T.A.** "Exact
    Solution of Sparse Linear Systems via Left-Looking Roundoff-Error-Free
    LU Factorization in Time Proportional to Arithmetic Work." *SIAM J.
    Matrix Analysis and Applications* 40(2):609–638, 2019.
    DOI 10.1137/18M1202499. VERIFIED. ABSTRACT ONLY. The sparse Bareiss paper.
12. **Escobedo, A.R., Moreno-Centeno, E.** "Roundoff-Error-Free Basis Updates
    of LU Factorizations for the Efficient Validation of Optimality
    Certificates." *SIAM J. Matrix Analysis and Applications* 38(3):829–853,
    2017. DOI 10.1137/16M1089630. VERIFIED. ABSTRACT ONLY. Directly about
    updating an exact factorization across basis changes.
13. **Escobedo, A.R.** "Exact Matrix Factorization Updates for Nonlinear
    Programming." arXiv:2202.00520v2, 2022. **TEXT REACHED**. Preprint, no
    journal version confirmed. Source of the bit-length bounds and the
    substitution recurrences above.
14. **Lourenço, C., Chen, J., Moreno-Centeno, E., Davis, T.A.** "Algorithm
    1021: SPEX Left LU." *ACM TOMS* 48(2), Article 20, 2022.
    DOI 10.1145/3519024. VERIFIED. ABSTRACT ONLY.
15. **Dixon, J.D.** "Exact solution of linear equations using P-adic
    expansions." *Numerische Mathematik* 40(1):137–141, 1982.
    DOI 10.1007/BF01459082. VERIFIED, TEXT NOT REACHED.
16. **Wiedemann, D.** "Solving sparse linear equations over finite fields."
    *IEEE Trans. Information Theory* 32(1):54–62, 1986.
    DOI 10.1109/TIT.1986.1057137. VERIFIED. Randomized — excluded here.
17. **Duff, I.S.** "On Algorithms for Obtaining a Maximum Transversal."
    *ACM TOMS* 7(3):315–330, 1981. DOI 10.1145/355958.355963. VERIFIED,
    TEXT NOT REACHED.
18. **Pothen, A., Fan, C.-J.** "Computing the block triangular form of a
    sparse matrix." *ACM TOMS* 16(4):303–324, 1990. DOI 10.1145/98267.98287.
    VERIFIED, TEXT NOT REACHED.
19. **Neumaier, A., Shcherbina, O.** "Safe bounds in linear and
    mixed-integer linear programming." *Mathematical Programming*
    99(2):283–296, 2004. DOI 10.1007/s10107-003-0433-3. VERIFIED,
    TEXT NOT REACHED.
20. **Althaus, E., Dumitriu, D.** "Certifying feasibility and objective
    value of linear programs." *Operations Research Letters* 40(4):292–297,
    2012. DOI 10.1016/j.orl.2012.03.004. VERIFIED. The SEA 2009 conference
    version's page range is UNVERIFIED.
21. **von zur Gathen, J., Gerhard, J.** *Modern Computer Algebra*, 3rd ed.,
    Cambridge University Press, 2013. DOI 10.1017/CBO9781139856065.
    VERIFIED. The chapter covering fraction-free elimination was not
    confirmed, so cite no section number from this entry.
22. **Espinoza, D.G.** PhD thesis, Georgia Tech, 2006. Handle 1853/10482.
    SEMI-VERIFIED (repository returned 403). TEXT NOT REACHED.
23. **Steffy, D.E.** "Topics in Exact Precision Mathematical Programming."
    PhD thesis, Georgia Tech, 2011. Handle 1853/39639. SEMI-VERIFIED
    (host unreachable). TEXT NOT REACHED. Likely the best single description
    of exact sparse rational linear algebra for LP bases.

## 8. What was deliberately not opened

No solver source. The SoPlex Doxygen source pages, the SPEX / SuiteSparse
repository and the `BasisLIB_INT` repository were all left unopened. The
last is a dataset of LP basis matrices rather than source and may be useful
as test data; it was still not opened, because that call is the
maintainer's.

## 9. The build order this suggests

1. Block triangular form of B: maximum transversal, then SCCs, tie-break by
   index. Deterministic, cheap, and it decides whether the rest is
   affordable.
2. Column-norm Hadamard bound per block. This is the refusal test and the
   capacity test in one. **Measure it over all 139 gate instances before
   writing any elimination**, which answers "how many of the 139 can it
   prove" in advance and at almost no cost.
3. Bareiss one-step over the blocks, with the common denominator `det B`.
4. The two solves, then the three exact checks: `A x* = b` with the bounds,
   `c - A' y* >= 0`, and complementary slackness.
5. Report the first violating row when it fails.
