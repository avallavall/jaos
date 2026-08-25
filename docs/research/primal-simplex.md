# The primal simplex — what the literature says, and how far it was checked

Background for `TODO.md` §0. Produced by `literature-scout` on 2026-08-25, on
the five questions §0 names. It is a reading list with mechanism, not a design:
the design lives in §0.

## Read this before trusting a formula below

**The agent reached no PDF, and that limit was its own rather than this
machine's.** Its `Read` needs `pdftoppm`, which is not installed on the Windows
side, and `WebFetch` hands raw PDF bytes to a reader that cannot decompress
them. Everything below that is not marked otherwise is a citation checked
against Crossref — publisher-deposited metadata — with the text not reached.

**Five PDFs were then read from WSL, where `pdftotext` 24.02 and `curl` both
work** (2026-08-25). Freely available preprints and a university-hosted thesis
only; nothing paywalled was fetched. What they changed is marked **READ AT
SOURCE** where it appears, and one of them refuted a restatement the agent had
carried — see Megiddo in §5.

- Koberstein's thesis (Paderborn's own repository). **It also confirms the
  institution at the title page**, which is the correction made to
  `crash-basis.md`.
- Hall & McKinnon 2004, arXiv:math/0012242.
- Ge et al., Smart Crossover, arXiv:2102.09420.
- Megiddo 1991, the author's copy at Stanford.
- Huang et al. 2021, arXiv:2111.03376.
- **Maros, DTR01-3**, Imperial's own copy — the pricing report §3 wanted.
- arXiv:1803.05167, for the steepest-edge score from first principles.
- Huangfu & Hall, arXiv:1503.01889, and Hall & Huangfu, ERGO-11-007.

**The Devex weight-update recurrence and its reset threshold appear in none of
the nine.** They were searched for in every one. Harris (1973) is paywalled and
is still required before that recurrence is coded. Section 8 lists what is left.

**Every entry below says how far it was checked.**

So the citations are good and several of the formulas are not verified at
source. The ones marked **CHECK AT SOURCE** are written in the form the
technique is standardly stated in, and a sign or a `max()` in the wrong place
there produces a solver that works and is slow. That is the hardest kind of
defect to find here.

**No solver source was opened.** One search result was a SoPlex repository
file. It was not opened and nothing here comes from it (D12).

**Two corrections were made to this report before it landed**, both by
re-checking Crossref from the main session rather than taking the agent's word:
the Galabova & Hall title the agent gave was truncated, and the venue it
corrected was right. See `crash-basis.md`.

---

## 1. Bounded-variable primal pivoting

The canonical implementation-level statement is **Maros (2003), Chapter 9, "The
Primal Algorithm", pp. 161–260**. The upper-bounding technique originates with
**Dantzig (1955)**. There is no separate famous paper for bounded primal
pivoting; it is textbook material that the paper literature builds on.

### The iteration

`min c'x`, `Ax = b`, `l <= x <= u`, logicals in `A`. Basis `B` of `m` columns,
nonbasic set `N`. Every nonbasic sits at one of its bounds, or at zero if free.

1. **Price.** `y = B^-T c_B` (BTRAN). `d_j = c_j - y'a_j` for `j` in `N`.
2. **Eligibility, and this is where the bounds enter.** A nonbasic at its lower
   bound may enter only if `d_j < -tol`. At its upper bound only if
   `d_j > +tol`. A free nonbasic may enter if `|d_j| > tol`. A fixed variable
   (`l = u`) never enters.
3. **Direction.** `sigma = +1` when `q` enters from its lower bound, `-1` from
   its upper bound. For a free nonbasic, `sigma = -sign(d_q)`.
4. **FTRAN.** `alpha_q = B^-1 a_q`. The basics move as
   `x_B(t) = bbar - t*sigma*alpha_q`, `t >= 0`.
5. **Ratio test over basic rows.**
   - `sigma*alpha_iq > 0`: `t_i = (bbar_i - l_B(i)) / (sigma*alpha_iq)`
   - `sigma*alpha_iq < 0`: `t_i = (bbar_i - u_B(i)) / (sigma*alpha_iq)`
   - a free basic does not limit anything.

   `t_max = min t_i`. **Also compute the bound-flip limit `t_flip = u_q - l_q`.**
6. **Three outcomes.**
   - `t_flip <= t_max` and finite: **bound flip.** No basis change. `q` moves to
     its other bound, `bbar := bbar - sigma*t_flip*alpha_q`, the objective moves
     by `sigma*d_q*t_flip`. `B`, the LU, `y`, `d` and the pricing weights are all
     untouched. This is the cheapest iteration the primal has.
   - `t_max < t_flip`: **basis change.** Row `r` leaves, its variable set
     *exactly* on the bound it hit; `q` becomes basic at position `r`.
   - both infinite: **unbounded**, a ray.
7. **After a basis change**, update `bbar`, then
   `d_j := d_j - (d_q/alpha_rq)*alpha_rj` using the pivot row
   `alpha_r = (B^-T e_r)'N`, then update the LU factors.

### What this costs JAOS

`bbar`, `d`, and which bound each nonbasic rests on: the dual maintains all
three. The bound flip is arithmetic on `bbar` and needs no new machinery. The
pivot row is what the dual's ratio test already computes.

### Citations

- Dantzig, G.B. "Upper Bounds, Secondary Constraints, and Block Triangularity in
  Linear Programming." *Econometrica* 23(2), 174–183 (1955).
  DOI 10.2307/1907876. **Crossref-verified; end page 183 is from secondary
  sources and is UNCONFIRMED. Text not reached.**
- Maros, I. *Computational Techniques of the Simplex Method.* Springer US,
  Boston MA, 2003. DOI 10.1007/978-1-4615-0257-9. Ch. 9 "The Primal Algorithm",
  pp. 161–260, DOI 10.1007/978-1-4615-0257-9_9. **Crossref-verified including
  chapter title and page range. Text not reached.**
- Maros, I. "A generalized dual phase-2 simplex algorithm." *EJOR* 149(1), 1–16
  (2003). DOI 10.1016/S0377-2217(02)00448-4. **Crossref-verified. Text not
  reached.** Dual-side, but it is where Maros sets out the variable-type
  taxonomy his primal chapter uses.

---

## 2. The primal ratio test

### A correction to the premise §0 was written on

**Harris's two-pass ratio test was published for the PRIMAL simplex.** Harris
(1973) describes pivot selection in the Devex code, and Devex is a primal code.
The dual form JAOS already has is the transposition of the original. The primal
statement is the older one.

### Harris two-pass, primal form

**Pass 1.** Relax every basic bound outward by `delta` and take the largest step
that keeps every basic inside the *relaxed* bounds:

- `sigma*alpha_iq >  tau_pivot`: `t_i = (bbar_i - l_i + delta) / (sigma*alpha_iq)`
- `sigma*alpha_iq < -tau_pivot`: `t_i = (bbar_i - u_i - delta) / (sigma*alpha_iq)`

`t1 = min t_i`, capped by `t_flip`.

**Pass 2.** Among the rows whose *exact-tolerance* ratio is `<= t1`, take the one
with the **largest `|alpha_iq|`**.

**What it buys.** The largest pivot among near-ties, so the LU update is more
stable and the factors grow less. At a degenerate vertex the relaxed bound lets
`t` be strictly positive, so the objective moves.

**What it costs.** Two passes over `alpha_q`. **The accepted step can push a
basic up to `delta` outside its true bound**, and that residual has to be
absorbed — normally by snapping the leaving variable exactly onto its bound.

Two constants, both needing a sweep here: `delta` (relaxed primal feasibility)
and `tau_pivot` (minimum acceptable pivot). **No values are carried from Harris;
the paper was not read.**

### EXPAND (Gill, Murray, Saunders, Wright 1989)

Read in Hall & McKinnon's restatement, **not in GMSW itself**. Re-check the
symbols at source.

- The working tolerance **grows every iteration**: `delta_k = delta_0 + k*tau`,
  with `tau = (delta_final - delta_initial)/K`.
- `K` is the reset interval; Hall & McKinnon's restatement gives about 10000.
- Two passes as in Harris, against the expanding `delta`.
- **A minimum step is enforced**: `t = max(t_min, t_harris)`,
  `t_min = tau/|alpha_rq|`. This is the part that stops stalling.
- Every `K` iterations, reset `delta` and push any drifted basic back onto its
  bound.

**What it buys.** GMSW's abstract (read) claims stalling cannot occur in exact
arithmetic, validated on the first 53 netlib problems.

**What it does not buy.** Hall & McKinnon (2004) construct explicit small LPs
where **EXPAND still cycles**; their abstract says it "is not guaranteed to
prevent cycling". A cycle detector with a Bland fallback is still needed, which
is the shape JAOS already has on the dual side (D26).

**D8: EXPAND is safe.** The tolerance depends on the iteration counter alone.
No clock, no randomness.

### The primal has no phase-2 long step, and looking for one wastes time

In the *dual* ratio test one row is scanned across many nonbasics, so there are
many breakpoints and the step can walk past several. In the *primal* ratio test
**only one variable moves** — the entering one. The only flip available is that
variable reaching its own opposite bound, which is outcome (a) in section 1 and
is part of the plain bounded ratio test rather than a separate technique.

**The primal long step exists only in phase 1**, where the objective is a sum of
infeasibilities and therefore piecewise linear in `t`. See section 4.

### Degeneracy, per choice

| rule | step at a degenerate vertex | termination |
|---|---|---|
| plain min-ratio | `t = 0` often | cycling possible |
| Harris two-pass | `t > 0` usually, from `delta` | no finiteness proof |
| EXPAND | `t > 0` always, from `t_min` | authors claim no stalling in exact arithmetic; Hall & McKinnon show cycling still possible |
| Bland | `t = 0` allowed | finite, slow |

### Citations

- Harris, P.M.J. "Pivot selection methods of the Devex LP code." *Math. Prog.*
  5(1), 1–28 (1973). DOI 10.1007/BF01580108. **Crossref-verified. Text not
  reached.** The often-quoted reprint in *Math. Prog. Study* 4, 30–57 (1975) is
  **UNVERIFIED**, from a search result only.
- Gill, P.E., Murray, W., Saunders, M.A., Wright, M.H. "A practical anti-cycling
  procedure for linearly constrained optimization." *Math. Prog.* 45, 437–474
  (1989). DOI 10.1007/BF01589114. **Verified on the Springer article page;
  abstract read, full text not reached.**
- Hall, J.A.J., McKinnon, K.I.M. "The simplest examples where the simplex method
  cycles and conditions where EXPAND fails to prevent cycling." *Math. Prog.*
  100(1) (2004). DOI 10.1007/s10107-003-0488-1. Preprint arXiv:math/0012242.
  **Crossref-verified except the page range, which Crossref did not return.
  Preprint full text READ as HTML.**
- Fourer, R. "Notes on the Dual Simplex Method." Draft, Northwestern University,
  1994. **Record verified on Optimization Online. An unpublished working note,
  never refereed — mark it as a draft wherever it is cited.** Text not reached.

---

## 3. Primal pricing

**Use Devex first.** Exact primal steepest edge costs an extra BTRAN *and* an
extra full PRICE per iteration. Devex costs neither. That asymmetry is the most
important fact in this section.

### What the primal maintains whatever the pricing rule

`d_N = c_N - N'y`, updated by `d_j := d_j - (d_q/alpha_rq)*alpha_rj`. That needs
`rho_r = B^-T e_r` (BTRAN) and `alpha_r = rho_r'N` (a row PRICE). **JAOS's dual
already computes both, so the pivot row is free to the primal.**

### Primal steepest edge (Goldfarb & Reid 1977; Forrest & Goldfarb 1992)

Score: choose `q` maximising `|d_j| / sqrt(gamma_j)`. Weight:
`gamma_j = 1 + ||B^-1 a_j||^2` in the "space of all variables" variant.

**READ AT SOURCE, with the derivation, in arXiv:1803.05167 §3.1.** Goldfarb &
Reid and Forrest & Goldfarb are both paywalled and still unread, but the score
does not need them — it falls out in three lines. When nonbasic `x_k` increases
by `theta_k`, the objective falls by `-d_k*theta_k` and each basic `x_i` moves
from `bbar_i` to `bbar_i - abar_ik*theta_k`. So the difference vector between
the two solutions has length

```
    theta_k * sqrt( 1 + sum_i abar_ik^2 )
```

and the objective decrease **per unit length** is `-d_k / sqrt(1 + sum_i
abar_ik^2)`. Steepest edge maximises that. **The `1` is the entering variable's
own movement, `theta_k^2`, and it is not a floor bolted on for safety** — which
is worth knowing before anyone "simplifies" it away.

**One consequence for JAOS, now exact rather than structural.** At the cold
basis `B = -I`, so `B^-1 a_j = -a_j` and `gamma_j = 1 + ||a_j||^2`: the model's
own column norm, one pass over the matrix, no triangular solve. That is the
same free-exact-start the dual gets from `dse[i] = 1`.

**CHECK AT SOURCE — the recurrences below were not verified in either primary
paper.** They are written in the form the technique is standardly stated in.

```
extra vector:  w = B^-T alpha_q                        <- ONE EXTRA BTRAN
nonbasic j != q:
    gamma_j := gamma_j - 2*(alpha_rj/alpha_rq)*(a_j'w)
                       + (alpha_rj/alpha_rq)^2 * gamma_q
leaving p = B(r):
    gamma_p := max( gamma_q/alpha_rq^2 , 1 )
```

`a_j'w` over all nonbasic `j` is `N'w` — **a second full PRICE**.

**What it costs beyond a dual steepest-edge implementation**, which is the
question §0 asked: one extra BTRAN, one extra full PRICE, and an `n`-length
weight array **indexed by nonbasic variable**. Dual steepest edge needs one
extra FTRAN, no extra PRICE, and weights **indexed by basic position**. The two
weight arrays are different objects of different lengths over different index
sets, so JAOS's `dse` gives nothing reusable but the arithmetic patterns. That
gap is a large part of why the dual is the faster algorithm on netlib-shaped
problems, and it lines up with D81.

Forrest & Goldfarb 1992 give a **family** of variants differing by the space the
edge norm is measured in, including variants where that space changes during the
solve — the reference-framework idea in general form. **The paper was not read,
so no variant can be named and no measured result carried. Get this paper.**

### Devex (Harris 1973)

Reference framework: fix a set `R` at a reset; the weights approximate the edge
norm in the subspace `R` spans. At a reset, `R` becomes the current nonbasic set
and every weight becomes 1.

**CHECK AT SOURCE.** Let `q` enter with weight `w_q` and row `r` leave:

```
nonbasic j with alpha_rj != 0:  w_j := max( w_j, (alpha_rj/alpha_rq)^2 * w_q )
leaving p:                      w_p := max( w_q/alpha_rq^2, 1 )
```

**Only the pivot row is needed. No extra BTRAN, no extra PRICE.** That is the
whole point of it.

Extra state beyond a dual steepest-edge implementation: one `n`-length weight
array indexed by nonbasic variable, plus reference-set membership flags.

Reset rule: reset when the largest weight passes a threshold. **A value around
1e6 is commonly quoted and Harris's own threshold was NOT verified — treat that
number as folklore until the paper is read.**

### Partial and multiple pricing

Partial pricing prices one rotating slice of `N` per iteration; multiple pricing
picks a small candidate set and runs several minor iterations on it. **Both are
refused in JAOS on the dual side, on wrong answers rather than on a trade — see
D82 and D84, and `TODO.md` §0's constraint table.**

**Maros's pricing report was READ AT SOURCE, and it removes the apparent tension
with those refusals.** In its taxonomy of pricing rules it says of steepest
edge: *"It is a full pricing and does not adapt to the multiple pricing
scheme."* And of Devex: *"It is a full pricing and is not suitable for multiple
pricing."* So the two normalized rules a primal simplex would actually want are
**incompatible with multiple pricing anyway**. D84's refusal costs the primal
nothing it could have had.

The same page confirms the asymmetry §3 opens with, in Maros's own words:
steepest edge *"is used in the dual simplex more frequently because it requires
less extra computations there"*, while Devex *"is considered a useful tool for
the primal SSX but it is also easily adaptable to the dual"*. **That is the
recommendation of this section, stated by a source that was read rather than
inferred.**

### Citations

- Goldfarb, D., Reid, J.K. "A practicable steepest-edge simplex algorithm."
  *Math. Prog.* 12(1), 361–371 (1977). DOI 10.1007/BF01593804.
  **Crossref-verified. Text not reached.**
- Forrest, J.J., Goldfarb, D. "Steepest-edge simplex algorithms for linear
  programming." *Math. Prog.* 57(1–3), 341–374 (1992). DOI 10.1007/BF01581089.
  **Crossref-verified. Text not reached.**
- Maros, I. "A General Pricing Scheme for the Simplex Method." Technical Report
  DTR01-3, Dept. of Computing, Imperial College London, 2001. **FULL TEXT READ**
  from `doc.ic.ac.uk/research/technicalreports/2001/DTR01-3.pdf`. Later published
  as *Annals of Operations Research* 124, 193–203 (2003) — **that journal record
  is UNVERIFIED at the publisher.**
- Anon. "A generalization of the steepest-edge rule and its number of simplex
  iterations for a nondegenerate LP." arXiv:1803.05167. **FULL TEXT READ.** Used
  only for §3.1's derivation of the steepest-edge score, which it gives from
  first principles. **It is not a source for the weight-update recurrence** and
  nothing else here comes from it.
- Swietanowski, A. "A New Steepest Edge Approximation for the Simplex Method for
  Linear Programming." *COAP* 10(3), 271–281 (1998).
  DOI 10.1023/A:1018317206484. **Crossref-verified. Text not reached.** A named
  alternative if Devex underperforms.

---

## 4. Phase 1

**Build the composite / piecewise-linear primal phase 1 (Maros 1986).** It is
what modern revised codes do, it needs no artificial variables, and it has an
implementation-level write-up in Maros's Chapter 9.

### The options

**(a) Textbook artificial variables.** One artificial per row, minimise their
sum, drop them. `n` grows by `m`; a second objective and a phase switch; simple
to write. The phase-1 optimum is often a bad phase-2 start. Huang et al.'s
survey — **one of the few sources actually read** — reports that phase 1 is
often *more* expensive than phase 2, citing Stojkovic et al. (2012); **that
secondary citation was not chased and is UNVERIFIED.**

**(b) Big-M.** One objective, `c` plus `M` times the artificial costs. No phase
switch, but `M` has to be chosen and a bad one wrecks the conditioning. Not
standard in modern revised codes.

**(c) Composite objective (Wolfe 1965).** Minimise a weighted mix of the true
objective and the infeasibility sum, changing the weight as the solve proceeds.
One extra parameter, needing a sweep.

**(d) Composite / piecewise-linear primal phase 1 (Maros 1986). RECOMMENDED.**

- Starts from **whatever basis it is given**. No artificials.
- The phase-1 objective is the sum of bound violations of the *basic* variables,
  which is piecewise linear in `t`.
- Phase-1 reduced costs come from a phase-1 cost vector with entries only for
  currently-infeasible basics: `-1` below a lower bound, `+1` above an upper
  bound, `0` otherwise.
- **Long-step ratio test.** Every basic that reaches a violated bound is a
  breakpoint. Sort the breakpoints by `t` and walk them in order with a running
  slope; at each breakpoint the slope rises by `|alpha_iq|`. Keep stepping while
  the slope is favourable and stop at the first breakpoint where it turns.
  **Several basics can become feasible in one iteration.**

Cost in code: a sorted breakpoint list per iteration (`O(k log k)` for `k`
candidates), a running slope accumulator, and a phase-1 cost vector updated
whenever a basic crosses a bound. **Phase-1 reduced costs are maintained
separately from phase 2.** No benefit number is carried — the paper was not
read.

**(e) The general theory.** Fourer's three-part piecewise-linear simplex series.
Part II is where the finiteness, feasibility and degeneracy arguments for the
long-step ratio test live; if the long step needs a termination proof, that is
the paper.

### What JAOS specifically needs

**Crossover does not need a general phase 1.** It hands the primal a basis that
is already primal feasible or nearly so, so the crossover path should go
straight to phase 2. Build (d) anyway, for warm starts and for the D19 case.

**The requirement (d) imposes and (a) does not: phase 1 must start FROM A GIVEN
BASIS.** A phase 1 that builds its own basis out of artificials is useless for
crossover. Design for that from the start.

### Citations

- Maros, I. "A general Phase-I method in linear programming." *EJOR* 23(1),
  64–77 (1986). DOI 10.1016/0377-2217(86)90215-8. **Crossref-verified. Text not
  reached.**
- Wolfe, P. "The Composite Simplex Algorithm." *SIAM Review* 7(1), 42–54 (1965).
  DOI 10.1137/1007004. **Crossref-verified. Text not reached.**
- Fourer, R. "A simplex algorithm for piecewise-linear programming I: Derivation
  and proof." *Math. Prog.* 33(2), 204–233 (1985). DOI 10.1007/BF01582246.
  **Crossref-verified. Text not reached.**
- Fourer, R. "… II: Finiteness, feasibility and degeneracy." *Math. Prog.*
  41(1–3), 281–315 (1988). DOI 10.1007/BF01580769. **Crossref-verified. Text not
  reached.**
- Fourer, R. "… III: Computational analysis and applications." *Math. Prog.*
  53(1–3), 213–235 (1992). DOI 10.1007/BF01585703. **Crossref-verified. Text not
  reached.**
- Maros, I. "A Piecewise Linear Dual Phase-1 Algorithm for the Simplex Method."
  *COAP* 26(1), 63–81 (2003). DOI 10.1023/A:1025102305440. **Crossref-verified.
  Text not reached.** The dual twin, so the shape will be familiar. Also exists
  as Imperial DoC TR DTR00-13.
- Koberstein, A., Suhl, U.H. "Progress in the dual simplex method for large
  scale LP problems: practical dual phase 1 algorithms." *COAP* 37(1), 49–65
  (2007). DOI 10.1007/s10589-007-9022-3. **Crossref-verified. Text not reached.**
- Huang, M. et al. "Simplex Initialization: A Survey of Techniques and Trends."
  arXiv:2111.03376 (2021). **READ.** Already cited in `crash-basis.md`.

---

## 5. Sharing machinery, and crossover

### What is genuinely common

`B` and its LU factors, the Forrest–Tomlin update and the refactorization
schedule; FTRAN and BTRAN; the pivot row `alpha_r`; the entering column
`alpha_q`; `bbar` and `d`, which **both algorithms maintain**; nonbasic status;
bound-flip arithmetic on `bbar`; scaling, presolve, postsolve, basis recovery.

### What is genuinely different

- **Pricing weights.** Dual weights index basic positions; primal weights index
  nonbasic variables. Different arrays, different formulas, different extra
  solves.
- **The extra solve.** Dual SE: one extra FTRAN. Primal SE: one extra BTRAN plus
  one extra full PRICE. Devex: neither.
- **Ratio test direction.** The dual scans a *row* over nonbasics, limited by
  dual feasibility. The primal scans a *column* over basics, limited by primal
  feasibility.
- **The invariant.** The dual keeps `d` feasible and drives `bbar` feasible; the
  primal keeps `bbar` feasible and drives `d` feasible. **Every tolerance in
  shared code has to know which invariant it is guarding.** This is exactly
  where D184's `can_move` units question lands.
- **Shifting.** The primal shifts *bounds* to repair primal infeasibility; the
  dual shifts *costs*. JAOS has the cost side. The bound side is new.
- **Bound flipping.** The dual flips many nonbasics at once; the primal flips at
  most one.

### There is no paper about carrying both in one implementation

**No published work was found whose subject is one implementation carrying both
algorithms over one basis representation. It is folklore; it lives in
implementations.** The closest citable description is Maros's book, where Part I
is the design of an LP system (Ch. 4 "Design Principles of LP Systems" pp.
59–67, Ch. 5 "Data Structures and Basic Operations" pp. 69–85, Ch. 8 "Basis
Inverse, Factorization" pp. 121–159) and Ch. 9 (primal) and Ch. 10 (dual) then
sit on the same machinery.

**Cite Maros's chapters for the design. Do not cite anything as if it settled
the one-struct-or-two question; nothing does.**

### Crossover

1. **Megiddo (1991). READ AT SOURCE, and it says something sharper than the
   restatement it arrived as.** The agent carried Ge et al.'s paraphrase — "at
   most `n` pivots, each pushing one variable to a bound". The paper's own two
   theorems are a complexity statement, and the pair of them is what matters
   here:

   > **Theorem 0.1.** If there exists a strongly polynomial time algorithm that
   > finds an optimal basis, given an optimal solution for *either* the primal
   > or the dual, then there exists a strongly polynomial algorithm for the
   > general linear programming problem.
   >
   > **Theorem 0.2.** There exists a strongly polynomial time algorithm that
   > finds an optimal basis, given optimal solutions for *both* the primal and
   > the dual.

   **The design consequence, and it is a hard requirement rather than advice.**
   Crossover needs **both** a primal-optimal and a dual-optimal solution.
   Producing an optimal basis from only one of them is not a harder engineering
   problem; by Theorem 0.1 it would settle an open question in complexity
   theory. Whatever supplies JAOS's starting point has to supply the pair.

   The paper's introduction says the same thing in implementation terms, and it
   is the sentence to keep: *"Given any primal-optimal solution (not necessarily
   basic), it is easy to find a primal-optimal basis. Analogously, given any
   dual-optimal solution, it is easy to find a dual-optimal basis. However, none
   of the two bases found in this way is guaranteed to be an optimal basis."*
   The dual solution attached to a primal-optimal basis can be infeasible. **So
   crossing over the primal alone does not produce an optimal basis**, however
   carefully it is done.

   Three pages, and not an implementation.
2. **Bixby & Saltzman (1994). This is the paper to implement from.** Rank
   variables by distance from their bounds, build a candidate basis from those
   furthest from one, complete it with logicals, then run primal and dual to
   clean up. **Abstract read; full text not reached.**
3. **Andersen & Ye (1996).** Build a perturbed LP from the current interior
   iterate whose strictly complementary solution is known; if the iterate is
   close enough to the optimal face, any optimal basis of the perturbed problem
   is optimal for the original. **That is a conditional guarantee, not an
   identity.** Abstract read.
4. **Andersen (1999), Andersen & Andersen (2000).** The practical follow-ups.
5. **Ge, Wang, Xiong, Ye, "Smart Crossover".** The modern treatment; **HTML full
   text read.** Three phases: a starting method, basis identification (the
   "push" phase), reoptimization (the "cleanup" phase). It states plainly that
   **crossover's running time has no reliable theoretical estimate**, because the
   distance from the candidate basis to a feasible basis is unknown. Carry that
   caveat.

### What crossover requires of the primal simplex — the list that shapes §0

1. It must accept an **arbitrary starting basis**, given as a basis list plus a
   nonbasic status list. Not a slack basis.
2. It must accept a basis that is primal feasible but **not** dual feasible and
   optimise from there. That is plain phase-2 primal.
3. It must tolerate a **nearly** feasible basis — the push phase leaves small
   violations — so phase 1 must start from a given basis, which is option (d).
4. It must exit with **every nonbasic exactly at a bound**.
   `postsolve-basis-recovery.md` already cares about this.
5. **Pricing weights must be initialisable for a non-identity basis.** For Devex
   that is free: every weight is 1 at a reset. For exact primal steepest edge it
   is `m` extra solves. This is the same problem Koberstein documented for dual
   steepest edge with a crash basis (see `crash-basis.md`), and it is a second
   practical argument for starting with Devex.

### One gap in the chain, and it should be written down before crossover is designed

`TODO.md` §0 states the chain as primal simplex → crossover → D97. **JAOS has no
interior-point method.** Crossover as published starts from an interior point.
The Megiddo / Bixby–Saltzman machinery still applies if the starting point comes
from somewhere else — it needs only a feasible non-basic point and a ranking —
but where that point is meant to come from is not written anywhere.

### Citations

- Megiddo, N. "On Finding Primal- and Dual-Optimal Bases." *ORSA J. Computing*
  3(1), 63–65 (1991). DOI 10.1287/ijoc.3.1.63. **Crossref-verified and FULL TEXT
  READ** from the author's copy at `theory.stanford.edu/~megiddo/pdf/bases.pdf`.
  The page range 63–65 is confirmed by the article's own header.
- Bixby, R.E., Saltzman, M.J. "Recovering an optimal LP basis from an interior
  point solution." *ORL* 15(4), 169–178 (1994).
  DOI 10.1016/0167-6377(94)90074-4. **Crossref-verified; abstract read, full
  text not reached.**
- Andersen, E.D., Ye, Y. "Combining Interior-Point and Pivoting Algorithms for
  Linear Programming." *Management Science* 42(12), 1719–1731 (1996).
  DOI 10.1287/mnsc.42.12.1719. **Crossref-verified; abstract read.**
- Andersen, E.D. "On Exploiting Problem Structure in a Basis Identification
  Procedure for Linear Programming." *INFORMS J. Computing* 11(1), 95–103
  (1999). DOI 10.1287/ijoc.11.1.95. **Crossref-verified. Text not reached.**
- Andersen, E.D., Andersen, K.D. "The MOSEK Interior Point Optimizer for Linear
  Programming." In *High Performance Optimization*, Applied Optimization 33,
  Springer US, 2000, pp. 197–232. DOI 10.1007/978-1-4757-3216-0_8.
  **Crossref-verified. Text not reached.**
- Ge, D., Wang, C., Xiong, Z., Ye, Y. "From an Interior Point to a Corner Point:
  Smart Crossover." arXiv:2102.09420. **arXiv record verified; HTML full text
  READ.** The journal reference it lists (*INFORMS J. Computing* 37(6),
  1670–1688, 2025) is **UNVERIFIED at the publisher.**
- Maros, I., Khaliq, M.H. "Advances in design and implementation of optimization
  software." *EJOR* 140(2), 322–337 (2002). DOI 10.1016/S0377-2217(02)00072-3.
  **Crossref-verified. Text not reached.**
- Bixby, R.E. "Solving Real-World Linear Programs: A Decade and More of
  Progress." *Operations Research* 50(1), 3–15 (2002).
  DOI 10.1287/opre.50.1.3.17780. **Crossref-verified. Text not reached.**
- Koberstein, A. "The Dual Simplex Method, Techniques for a Fast and Stable
  Implementation." PhD thesis, University of Paderborn, 2005. **Record verified
  at the university's own repository and at the German National Library. Text
  not reached.**

---

## 6. Every constant this work introduces

Each needs its own sweep here. **A number from a paper is a starting point,
never a justification** — and none of the numbers below were read at source.

| constant | where | what the report carries |
|---|---|---|
| `delta`, the relaxed primal feasibility tolerance | Harris pass 1 | nothing; paper not read |
| `tau_pivot`, minimum acceptable `\|alpha_iq\|` | Harris pass 2 | nothing; paper not read |
| `delta_initial`, `delta_final` | EXPAND | nothing; GMSW not read |
| `K`, the EXPAND reset interval | EXPAND | about 10000, from Hall & McKinnon's restatement and NOT from GMSW |
| `tau = (delta_final - delta_initial)/K` | EXPAND | derived |
| `t_min = tau/\|alpha_rq\|` | EXPAND | derived |
| Devex reset threshold on the largest weight | Devex | about 1e6 is **folklore here**, and searching nine free sources did not find it — Harris (1973) is paywalled and is where it lives |
| Devex initial weight | Devex | 1 |
| primal steepest-edge weight floor | primal SE | 1 |
| partial pricing slice count | pricing | nothing; and D82 refuses the technique |
| multiple pricing candidate-set size | pricing | nothing; and D84 refuses the technique |
| composite phase-1 weight | Wolfe | nothing; paper not read |
| phase-1 infeasibility tolerance | phase 1 | nothing; paper not read |
| crossover distance-from-bound ranking cutoff | crossover | nothing; Bixby–Saltzman not read |
| cycle-detection threshold that switches Bland on | anti-cycling | **JAOS already owns this on the dual side (D26)** |

---

## 7. What conflicts with D8, and what changes the answer

### Reproducibility (D8)

- **EXPAND: safe.** The tolerance depends on the iteration counter alone.
- **Harris pass 2 tie-break: needs care.** Ties on "largest `|alpha_iq|`" must
  break on the lowest basis position, never on iteration order over a hash or an
  address.
- **Devex reference-framework reset: safe** if the trigger is a weight threshold
  and the reset walks an index-ordered set.
- **Partial pricing: conflicts as commonly implemented.** The standard trick
  starts each scan at a random or clock-derived offset. A deterministic variant
  starting at `iteration_count mod n_slices` is a straightforward substitution.
  (Moot here: D82 refuses the technique anyway.)
- **Randomised perturbation: conflicts.** Any perturbation must be a
  deterministic function of the column index, as JAOS's cost shifts already are.
- **Breakpoint sorting in the phase-1 long step: needs care.** Equal breakpoints
  are common at a degenerate vertex, so the sort must be stable and keyed on
  `(value, index)`.
- **Summation in PRICE and in the infeasibility sum: needs care.** No
  reassociation; `-ffp-contract=off` covers FMA; any SIMD reduction must keep a
  fixed order.
- **Crossover ranking by distance to bound: needs care.** Ties break on index.

### Two things that change the ANSWER rather than the path

1. **Harris and EXPAND both accept a point slightly outside its bounds.** The
   published step can leave a basic up to `delta` past a bound. JAOS's checker
   has an absolute bar on primal feasibility, and **D24 keeps that test
   absolute deliberately** — it measured the relative alternative and found it
   wrong in both directions at once. **So the primal ratio test must
   snap the leaving variable exactly onto its bound, and the final answer must
   be re-verified against the TRUE bounds and not the expanded ones.** Otherwise
   the gate rejects solves that are correct.
2. **Andersen & Ye's crossover solves a perturbed problem.** Its optimal basis is
   optimal for the original only when the iterate is close enough to the optimal
   face — a conditional guarantee. **Bixby–Saltzman does not have this property
   and is the safer starting point.**

### D2 and D12

Nothing in this report needs a third-party library, and no solver source was
read.

---

## 8. What is still unread, and what it blocks

Nine free sources were read. **Three paywalled items remain, and one of them
blocks code.**

1. **Harris (1973). BLOCKS CODE.** It is the only place named anywhere for the
   Devex weight-update recurrence and its reset threshold, and neither appears
   in any of the nine sources read — they were searched for in all of them. The
   recurrence in §3 is written in its standard form and is **not verified**. A
   sign or a `max()` in the wrong place there gives a solver that works and is
   slow, which is the hardest kind of defect to find. Harris also holds the
   primal two-pass ratio test in its original statement.
2. **Maros (2003), Chapter 9, pp. 161–260.** Answers questions 1, 2, 3 and 4 in
   one place at implementation level. Nothing depends on it that is not covered
   elsewhere, but it would replace several second-hand statements with one
   source.
3. **Forrest & Goldfarb (1992).** The steepest-edge variant family and the
   reference framework in general form. Only needed if Devex underperforms.

**What no longer blocks anything.** §3's steepest-edge *score* is derived at
source (arXiv:1803.05167) and needs neither Goldfarb & Reid nor Forrest &
Goldfarb. Maros's pricing report is read. The formulas in sections 1 and 4 are
standard and were not in doubt.
