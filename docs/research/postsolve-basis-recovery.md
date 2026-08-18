# Basis recovery through postsolve — what is published

Assembled 2026-08-18 for D137, after D131–D136 measured that `jaos_basis`
publishes a basic count that is not `num_row` on 70% of solves.

`literature-scout` searched and could not open a single PDF. **The thesis
below was then read directly**, with `pdftotext -layout` in WSL, which turned
the scout's central "no source states this" into a citation. Everything marked
**READ** below was read this session; everything marked **derived** is
reasoning, not citation, and must not be cited as literature.

## The one source that states the rules

**Galabova, I. *Presolve, crash and software engineering for HiGHS*.
University of Edinburgh, 2023.** Handle `1842/39725`, DOI `10.7488/era/2974`.
PDF at `https://era.ed.ac.uk/bitstreams/a7ae4781-0250-4104-a47c-92a96eaedf37/download`.
**READ** — 2542 lines of `pdftotext -layout`, §3 "Basis" and "Postsolve".

### The obligation

> In addition to satisfying the KKT conditions in Section 2.1, a point returned
> by postsolve must also be a basic feasible solution (BFS) in order to
> hot-start the simplex algorithm.

### The counting rule, in print

> **At each step of postsolve where a new row is introduced, a variable must be
> identified as basic.**

That is the accounting identity D132 derived and D133 measured — one basic
variable per restored row — stated by a published source. `literature-scout`
reported that no source states it; the scout could not open this PDF.

### The status rules

> Basic variables can have primal values between their lower and upper bounds
> but must have a zero dual value. Nonbasic variables must be at a bound but
> can have nonzero duals. The sign of the dual value must correspond to the
> bound at which the primal value is fixed.

That is D136's rule, derived there from complementary slackness and confirmed
here as the published one.

### Interior implies basic

> If the eliminated variable is strictly between bounds it must be ensured that
> it is basic in the postsolved problem.

That is D134's argument, in print.

### The fallbacks HiGHS uses

> If `x_j^r` is nonbasic it must become basic in the postsolved solution.
> During postsolve an attempt is made to do that. If the dual value of the
> eliminated variable `x_k` is calculated to be infeasible, the dual value is
> transferred to the row by making the row basic.

> In the cases when `x_k` can be both basic and nonbasic and the current values
> do not fall into one of the special cases above, an attempt is made to set it
> to basic. If that assignment of values is infeasible, the remaining column
> `x_j` is selected for the basis.

**Attempt, then fall back.** HiGHS does not derive the assignment in closed
form; it tries one and takes the other when the first is infeasible.

### And HiGHS turned this family off

> The zero cost column singleton rule was added to HiGHS, however, enabling it
> led to a reduction in elimination counts for some test problems, and it was
> disabled by default.

> Thus confirming that the zero cost column singleton rule should not be
> included in the default presolver list.

> However, allowing the generalized version of the rule may lead to incorrect
> presolve elimination steps for some models.

**This is the family responsible for 5902 of netlib's basis errors here**
(D133). HiGHS measured it, found it did not pay, and disabled it. That is not
an argument to disable it in JAOS — D95 and D106 are this project's own
measurements and they say something different about its value — but it is a
published datum about the same rule, and it belongs beside them.

## The per-family table

**Derived, not cited.** `literature-scout` produced it from the counting rule
above and it is reproduced here because it is useful, not because a paper says
it. The classical presolve literature — Brearley/Mitra/Williams 1975, Tomlin &
Welch 1983, Fourer & Gay 1994, Andersen & Andersen 1995, Gondzio 1997,
Achterberg et al. 2020 — recovers values and duals and does not assign basis
status per reduction.

| family | rows restored | columns restored | must add | the rule this forces |
|---|---|---|---|---|
| empty row | 1 | 0 | +1 | logical BASIC, always |
| redundant row | 1 | 0 | +1 | logical BASIC, always |
| singleton row | 1 | 0 | +1 | exactly one of {logical, folded column} becomes basic |
| forcing row | 1 | k | +1 | the k columns nonbasic at forced bounds; one degenerate basic member still needed |
| empty column | 0 | 1 | 0 | column NONBASIC at a bound |
| fixed column | 0 | 1 | 0 | column NONBASIC at the fixed value |
| free / implied-free column singleton | 1 | 1 | +1 | column BASIC, row's logical NONBASIC |
| cost-0 bounded column singleton | 0 | 1 | **0** | column NONBASIC, or a swap |

JAOS measures the first six at drift 0 and the last two wrong (D133), which is
what this table predicts.

## The swap, for the cost-0 bounded column singleton

**Derived.** If the restored column lands strictly inside its own bounds, the
reduced activity was strictly inside the widened row bounds, so **row `i`'s
logical was basic in the reduced solve** — a nonbasic logical sits on a bound
by definition. It is the only variable the record touches, so the leaving
variable is forced rather than chosen. The exchange removes `e_i` and inserts
`A_{·j}`, whose only nonzero is `a_ij`, so the pivot is `a_ij` itself and
presolve already required it non-zero: no rank test, no fallback.

And it moves no numbers. `c_j = 0`, so a basic `x_j` needs `d_j = -y_i a_ij =
0`, so `y_i = 0` — which the reduced solve already had, because row `i`'s
logical was basic there.

**D135 measured the prediction and it nearly holds**: the row's logical is
basic on 5822 of netlib's 5902 firings and on all 482 of Kennington's. The 80
exceptions contradict the derivation and are unexplained. Note that D135 read
the *published* status, not the status in the reduced solve, and those differ
if a later record overwrote it — so the 80 need re-measuring before they are
believed to be a second defect.

## The lower bar

Galabova, and Gurobi's own user documentation, both describe postsolving and
then re-optimising:

> Additional simplex iterations after postsolve ensure that the solution
> returned to the user is feasible within the desired tolerances.

If the postsolved basis only has to be a **valid starting** basis rather than
the optimal one, it must have `num_row` members and be nonsingular, and it does
not have to reproduce the duals. JAOS's consumer is `build_warm_basis`, which
re-optimises anyway.

## Sources checked and found not to carry a basis rule

Verified by `literature-scout` against publisher pages or Semantic Scholar,
**none read as full text**: Andersen & Andersen 1995 (`10.1007/BF01586000`),
Brearley/Mitra/Williams 1975 (`10.1007/BF01580428`), Tomlin & Welch 1983
(`10.1007/BF02591947`), Fourer & Gay 1994 (`10.1007/978-1-4613-3632-7_8`),
Gondzio 1997 (`10.1287/ijoc.9.1.73`, abstract read), Megiddo 1991
(`10.1287/ijoc.3.1.63`), Bixby & Saltzman 1994 (`10.1016/0167-6377(94)90074-4`),
Andersen 1999 (`10.1287/ijoc.11.1.95`, abstract read), Gleixner/Gottwald/Hoen
2023 (arXiv:2206.10709, HTML read — a footnote only), Achterberg et al. 2020
(MIP, not applicable).

**A new lead nobody here has read**: Tomlin & Welch, *A pathological case in
the reduction of linear programs*, Operations Research Letters 1983,
`10.1016/0167-6377(83)90036-6`. The title is a reduction that goes wrong.

**No solver source was read**, per this project's rule. The PaPILO and HiGHS
facts above come from published descriptions by their authors.
