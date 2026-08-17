# Dual postsolve for a presolve-imposed bound — the design on paper

D97's second precondition, worked out on paper 2026-08-17. **Nothing here is
built and nothing is measured.** No source file changed to produce it.

Written against `src/check.c`'s own rule and against the two families already
shipping that do the same kind of derivation: `JM_PS_FORCING_ROW`
(`src/presolve.c:2303`) and `JM_PS_SINGLETON_ROW` (`src/presolve.c:1894`).
§11 is a `literature-scout` run of the same date; its headline is that the
transfer rule is folklore, with no citable description found.

D114 (`bench/measurements/02-21/`) is the first precondition and is closed.
This document is the second one, and it is a design, not a decision: no entry
belongs in `DECISIONS.md` until something is measured.

## 1. What the checker demands, exactly

`jaos_check_solution` judges a column with `sign_condition` at
`src/check.c:606-623`. The bounds it passes are the **caller's**
`m->col_lower[j]` / `m->col_upper[j]`. The implied bounds `icl`/`icu` are
substituted only where the caller's own bound is infinite, and terms coming
from them are kept out of `pos_model`/`neg_model`, so they never move the
verdict.

Canonical (minimise, `sigma` folded), with `w_j = sigma * d_j`:

- `w_j > 0` requires `x_j <= l_j + tol * max(1, |x_j|)`, else the violation is `w_j`.
- `w_j < 0` requires `x_j >= u_j - tol * max(1, |x_j|)`.
- `|w_j| <= tol` is forgiven outright (`negligible`).

A column resting at a presolve-imposed bound `l'_j > l_j` is strictly interior
to the caller's box. Neither `at_lo` nor `at_hi` is true. So the checker
demands `|w_j| <= tol`, i.e. **`d_j = 0` to tolerance**. The reduced solve has
no reason to deliver that: `l'_j` is a real bound of the reduced problem and
`w_j > 0` there is correct dual feasibility.

That is the whole of D97's second obstacle. It is a postsolve obligation, not
a tolerance.

## 2. The transfer rule

Four ways activity-based tightening can impose a bound on `x_j` from row `i`.
Write `rest_lo_i` / `rest_up_i` for the min/max of `sum_{k != j} a_ik x_k`
over the current boxes.

| from | sign of `a_ij` | imposes | value |
|---|---|---|---|
| A | `rl_i`, `a_ij > 0` | lower | `(rl_i - rest_up_i) / a_ij` |
| B | `rl_i`, `a_ij < 0` | upper | `(rl_i - rest_up_i) / a_ij` |
| C | `ru_i`, `a_ij > 0` | upper | `(ru_i - rest_lo_i) / a_ij` |
| D | `ru_i`, `a_ij < 0` | lower | `(ru_i - rest_lo_i) / a_ij` |

The pair (which end of `x_j` was imposed, sign of `a_ij`) determines which row
bound implied it. A record therefore needs no extra flag for that: `index` =
row `i`, `coef` = `a_ij`, plus one bool for which end of the column.

**The rule is the same in all four cases:**

    delta = w_j / a_ij
    w_i  += delta

`w_j` is the reduced solve's canonical reduced cost for `j`, computed the way
`JM_PS_FORCING_ROW` computes its `d0` — `cr->cost` minus the dot product
against the original duals known so far (`src/presolve.c:2350-2352`).

After the transfer, `w_j` becomes `w_j - a_ij * delta = 0` exactly. That is
the obligation discharged.

## 3. Why it is legal — the premises are forced tight

Take case A. `x_j = l'_j = (rl_i - rest_up_i)/a_ij`, so

    a_i'x = a_ij * l'_j + rest = rl_i - rest_up_i + rest

Feasibility gives `a_i'x >= rl_i`, hence `rest >= rest_up_i`. The tightened
boxes are *implied* bounds, satisfied by every feasible point of the original
model, so also `rest <= rest_up_i`. Therefore

    rest = rest_up_i    and    a_i'x = rl_i

exactly. Two consequences:

1. **Row `i` sits at its lower bound**, so `w_i >= 0` is its own sign
   condition and `delta = w_j / a_ij > 0` moves it further onto the permitted
   side. Never across zero.
2. **Every other column `k` of row `i` sits at the bound that attained
   `rest_up_i`**: `a_ik > 0` puts `x_k` at `u_k`, `a_ik < 0` puts it at `l_k`.
   The transfer moves `w_k` by `-a_ik * delta`, which for `a_ik > 0` makes
   `w_k` more negative (correct at an upper bound) and for `a_ik < 0` makes it
   more positive (correct at a lower bound).

Case D is the mirror: row `i` at its **upper** bound, `w_i <= 0`,
`delta = w_j / a_ij <= 0` since `a_ij < 0`, every other `k` pushed the right
way. B and C are the same two arguments with `w_j <= 0`.

So no sign condition anywhere in the model is broken by the transfer, and the
complementarity terms `w_v (v - bound_v)` stay non-negative, which is what
keeps `gap_positive` a valid bound (`src/check.c:451-453`).

This is the argument D114's entry states. Written out, it holds for all four
cases and does not depend on the tightening being a single pass.

## 4. Cascading — and why reverse order is a single pass

Section 3.2 leaves an obligation: column `k` is now pinned at a bound of its
own, and if that bound is itself imposed, the checker demands `w_k = 0` too,
which the transfer just moved it away from. So transfers chain.

**Claim: the chain is acyclic and strictly decreasing in derivation time, so
one LIFO pass over the arena discharges all of it.**

Let `t_j` be the forward-pass moment at which `l'_j` was derived. `rest_up_i`
in that derivation was taken over the boxes as they stood at `t_j`. Section 3
proved `rest = rest_up_i`, so every `x_k` in row `i` rests at the bound it had
**at time `t_j`**.

- Suppose `k` was tightened later (`t_k > t_j`) to something strictly tighter.
  Then `x_k` at its time-`t_j` bound lies outside `k`'s box **in the reduced
  problem**, because the reduced problem carries every tightening the forward
  pass made, including `t_k`'s. So the reduced solve cannot have produced that
  point at all, and by section 3 it cannot have rested `x_j` at `l'_j` either.
  The case does not arise; it does not need handling.
- Therefore every `k` that needs a transfer of its own has `t_k < t_j`, or its
  time-`t_j` and final bounds coincide.

Note that this step needs no fixed point in the propagation. Section 3's
tightness argument holds for whatever `rest_up_i` the derivation actually
used, because the boxes it used are valid implications; and this step closes
the ordering by reading the reduced problem's own box, not the propagation's
history.

The same argument run at `k` shows `j` cannot be a live column of row `i_k`
with `t_j > t_k`, so `k`'s transfer cannot disturb `w_j` afterwards. The
transfer graph is a DAG ordered by decreasing derivation time.

The arena already replays strictly LIFO (`src/presolve.c:7`), which is
decreasing forward time. **So the ordering this needs is the ordering the
arena already has.** No new mechanism.

The one degenerate case: `l'_j` numerically equal to `j`'s earlier bound. Then
`j` may appear in both rows, but the transfer is a no-op in the direction that
matters because both sign conditions permit the same value. This needs a
constructed case before it is believed.

## 5. Multiple tightenings of the same column — the test already ships

My first draft said the arena should keep one record per `(column, end)`,
overwritten on each further tightening. That is wrong, and
`JM_PS_SINGLETON_ROW` at `src/presolve.c:1977-2021` says why.

More than one row can impose a bound on the same column. The replay reaches
those records in LIFO order, which is **not** the order they tightened in. A
test that reads only the column ("does a zero multiplier work here?") is true
or false for all of them alike, so the first record to arrive takes the whole
reduced cost whether or not its own row produced the bound `x_j` is resting on.
That defect is named and measured: five standard instances (`25fv47`, `bnl1`,
`bnl2`, `e226`, `vtp-base`), each with two records on the offending column.

The shipped fix is to ask the row directly, and this family uses it unchanged:

    this_row_owns = (dc > 0 && row_tightens_lo && v0 == rec->lo) ||
                    (dc < 0 && row_tightens_hi && v0 == rec->hi)

The comparison is **exact**, and that is not a tolerance in disguise: a
nonbasic column rests on its bound bit for bit, and `rec->lo`/`rec->hi` is the
same computation that produced that bound. A record whose bound was later
overwritten by a tighter one compares unequal and declines, which leaves the
reduced cost intact for the record that does own it. Declining is always safe
for the row, because a zero multiplier satisfies every row's sign condition.

`row_tightens_lo`/`hi` alone does not answer it — `bnl1`'s row 638 has
`row_tightens_hi` set and is still not responsible. So the record must carry
the bound value it imposed, not just which end.

**One known hole carries over.** The fold's collapse branch replaces both
bounds with their midpoint, which is no row's implied bound, so the record that
tightened the other side compares unequal for ever. `TODO.md` already carries
that defect. Any tightening family with a collapse branch inherits it.

## 5b. The transfer arithmetic is already in the tree

`src/presolve.c:2035` and `2048-2049` are the rule of section 2, shipping:

    y_i = d0 / rec->coef
    sol_redcost[j] = d0 - rec->coef * y_i     /* = 0 */

So the arithmetic, the ownership test, the canonicalisation and the exactness
argument are all precedent. **What is new in this family is only the implying
row's other columns**, which a singleton row does not have. That is where the
whole of the remaining risk sits, and it is smaller work than D97 implies.

## 6. What is exact and what is not

Everything in sections 3 to 5 is exact arithmetic. In doubles:

- `x_j` rests at `l'_j` only to the reduced solve's own tolerance, so
  `rest = rest_up_i` holds only to that tolerance, so row `i` is tight only to
  that tolerance.
- `delta = w_j / a_ij` is one division. `w_j - a_ij * delta` is not exactly 0
  unless the division is exact.
- `w_k -= a_ik * delta` for every other column in the row is an accumulation,
  and D111's compensated `ps_row_add` discipline applies to it.

The residue is therefore bounded by `eps * traffic` in the D103 form, and D114
already states the requirement that any future tightening design must window
pin verdicts by the row bound's scale and never by a materialized magnitude.

**What has to be measured before any of this is believed:** whether the
residue after the transfer lands under the checker's `tol` on the four
instances D97 named (`pilot`, `pilot87`, `agg`, `maros`), not whether the
algebra is right.

### 6b. The three requirements the tightening half already carries

`bench/measurements/02-21/README.md` states what any future tightening design
must do, and this dual postsolve does not exempt the primal half from any of
it:

1. Forcing and pinning verdicts windowed by the **row bound's** own scale, as
   the shipping forcing family does with `ps_bound_scale(cur_rl, cur_ru)` —
   never by the activity range's magnitude. That is the whole of D114.
2. A pin only where attainment is within the **arithmetic's** error of the sum
   that computed it (`eps * traffic`, the D103 form), never within a judgement
   constant.
3. Materialized implied bounds kept out of scale computations, or the activity
   range recomputed against original boxes for verdict purposes.

Requirement 3 needs one clarification, because it looks like it collides with
section 3 and does not. Section 3's tightness argument *derives* from the
tightened boxes and that is sound: they are valid implications, so
`rest <= rest_up_i` holds at every feasible point. What 02-21 forbids is
letting a materialized magnitude set the **scale of a verdict window**. The
derivation and the window are two different uses of the same numbers.

## 7. What the arena record must carry

Reusing `jm_presolve_rec` (`src/jaos_internal.h:561`), a new tag
`JM_PS_IMPOSED_BOUND`:

| field | meaning |
|---|---|
| `index` | the column `j` |
| `index2` | the implying row `i` |
| `coef` | `a_ij` |
| `cost` | `c_j` as it stood, for the `d0` reconstruction |
| `lo`, `hi` | the bound **this record imposed**, for the ownership test of §5 |
| `row_tightens_lo` / `row_tightens_hi` | which end of the column it imposed |

`lo`/`hi` is the imposed bound, not the pre-tightening bound. Nothing needs
restoring: presolve tightens the bounds of `p->reduced`, and `p->orig` keeps
the caller's own `col_lower`/`col_upper` untouched throughout.

The record is pushed at the tightening site and replays before the records of
anything removed after it, which LIFO gives for free.

## 8. The obstacle nothing in the tree has met yet: the basis count

Every family that pays a multiplier today also **removes** something, and the
removal supplies the slot the status change needs. `JM_PS_FORCING_ROW` says so
outright at `src/presolve.c:2371-2374`: the row it removes takes the single
basic slot the restoration owes. `JM_PS_SINGLETON_ROW` folds its row away.

**Bound tightening removes nothing.** So when the transfer fires:

- column `j` goes from nonbasic-at-a-bound to `JAOS_BASIS_BASIC`, because in
  the original problem that bound does not exist and `x_j` is interior there
  (the reason is spelled out at `src/presolve.c:2029-2034`);
- nothing was removed to pay for it.

The obvious answer is that the balance comes from row `i`: section 3 proved it
sits exactly at `rl_i` and now carries a nonzero multiplier, which is precisely
a **nonbasic** row. Column in, row out, count preserved.

**That answer is wrong, and a two-variable case shows it.** Row `i` can already
be nonbasic in the reduced solve, and then the slot comes from somewhere else
entirely.

### 8a. The worked case

    min  3 x1 + x2
    s.t. x1 + x2 >= 1            (row 1: rl = 1, ru = +inf)
         x1 in [0, +inf)
         x2 in [0, 0.5]

Tightening from row 1: `rest_up` over `x2` is `0.5`, so
`l'_1 = (1 - 0.5)/1 = 0.5` and the reduced problem has `x1 in [0.5, +inf)`.

The optimum is `x1 = 0.5, x2 = 0.5`, objective 2, in both problems —
`3x1 + x2 = 2x1 + (x1+x2) >= 2(0.5) + 1 = 2`, and the original is infeasible
below `x1 = 0.5` anyway, so the tightening changed nothing. Row 1's activity is
exactly 1, which is section 3's tightness, arrived at independently.

Every variable sits on a bound, so the vertex is degenerate and the reduced
problem has **two** optimal bases. One of them is the bad case:

| | basic | nonbasic | `y_1` | `d_1` | `d_2` |
|---|---|---|---|---|---|
| basis A | `x1` | `x2` at upper, logical at lower | 3 | 0 | -2 |
| basis B | `x2` | `x1` at lower, logical at lower | 1 | 2 | 0 |

Basis B has `x1` nonbasic at its **imposed** bound with `d_1 = 2 > 0`, and the
row **already nonbasic** with `y_1 = 1 != 0`. So the configuration §8 asked
about is not only possible, it needs two variables and one row to build.

### 8b. What the transfer does to it, and where the slot comes from

`delta = d_1 / a_11 = 2`, so `y_1` goes from 1 to 3. Then `d_1 = 0` and
`d_2 = 1 - 3 = -2`. Every sign condition holds: `x1` interior with `d_1 = 0`,
`x2` at its upper bound with `d_2 < 0`, row at `rl_1` with `y_1 > 0`. The
duals land exactly on the original problem's unique dual solution. The
arithmetic is right.

The statuses have to move like this:

- `x1`: nonbasic at lower → **basic** (it is interior in the original box)
- `x2`: **basic → nonbasic at upper** (its `d_2` became nonzero)
- row 1's logical: nonbasic before, nonbasic after

So the trade is **column against column**, not column against row. `x2` could
absorb it only because section 3 guarantees it is sitting on a bound — a
degenerate basic variable, which can be made nonbasic without moving its value.

### 8c. The count, settled by a rank argument

This is the question PaPILO's shipped restriction answers with "decline the
reduction" (see §12). It has an answer for one active imposed bound, and the
answer is that the swap always exists and is always exactly one.

Let row `i` have `q` columns, with `x_j` at an imposed bound. Section 3's
tightness gives this active set at the reduced optimum:

    e_j                    x_j at its imposed bound
    a_i                    row i at rl_i
    e_k  (q-1 of them)     every other column of row i, at a bound

That is `q+1` vectors. The `q` unit vectors span row `i`'s whole support, and
`a_i` lies inside that span, so the **rank is `q`** and the active set carries
exactly one dependency. Section 3 does not merely permit the degeneracy — it
manufactures it, every time the family fires.

Now delete `e_j`, which is not a constraint of the original problem. What is
left is `a_i` plus `q-1` unit vectors. `a_i` has a nonzero component on `e_j`
that no `e_k` has, so it is independent of them: **rank still `q`**.

Three consequences:

1. The postsolved point **is still a vertex** of the original problem. It does
   not need a crossover.
2. Exactly one variable moves basic → nonbasic, and at least one is always
   available, because the reduced basis could only hold `q` of the `q+1`
   active constraints as nonbasic.
3. §8a's worked case is not a lucky degenerate example. It is the generic shape.

### 8d. Where it does break: two imposed bounds on one row

The rank argument fails when **two** columns of the same implying row are at
imposed bounds. Then the active set is `e_j1`, `e_j2`, `q-2` unit vectors and
`a_i` — still `q+1` vectors of rank `q` — but deleting two of them leaves rank
`q-1`. One constraint short. The point is not a vertex of the original problem
unless some other row happens to supply the missing one, and then a crossover
is what PaPILO's sentence says it is.

**That configuration forces the implying row to be an equality.** Take `j1`
with an imposed lower bound from `rl_i` and `a_ij1 > 0` (case A). Section 3
then puts every other column at its `rest_up`-attaining bound, so `x_j2` sits
at `u_j2` if `a_ij2 > 0` and at `l_j2` if `a_ij2 < 0`. For that to be `j2`'s own
*imposed* bound, `j2`'s tightening must be case C or case D, and both of those
derive from `ru_i`. Section 3 applied to `j2` then puts row `i` at `ru_i`. Row
`i` is at `rl_i` and at `ru_i`, so `rl_i = ru_i`.

So the hazard is exactly: **an equality row that imposes bounds on two of its
own columns.** That is not an exotic corner — D112 measured 94% of the widening
family's firings on equality rows.

**The refusal a first version should carry:** do not impose a bound from a row
that already imposed one on another of its columns. It is one flag per row in
the forward pass, it is checkable at the firing site, and it keeps §8c's rank
argument the whole story. Lifting it is a separate change and needs the
crossover question answered first.

Two reasons it matters more than it looks:

1. `jaos_check_solution` never reads a status. It recomputes `d_j` from
   `row_dual` (`src/check.c:607-610`) and judges that. So a broken basis count
   passes the whole gate silently. This is exactly the class `jaos-testing`
   warns about: trusting a green gate about something the checker never reads.
2. `jaos_basis` is a documented caller-visible invariant, so getting it wrong
   is a wrong answer, not a cosmetic defect.

## 9. The second thing new in this family: stale published reduced costs

`jm_postsolve_expand` copies surviving columns straight through
(`src/presolve.c:2529`):

    orig->sol_redcost[j] = red->sol_redcost[rj];

The reduced solve's reduced costs know nothing about a transfer onto `y_i`.
After the transfer, **every surviving column of row `i`** has a published
`sol_redcost` that disagrees with the published `sol_dual` by `a_ik * delta`.

Again the checker cannot see it, because it recomputes from the duals. Again a
caller can. So `sol_redcost` for every column of the implying row must be
recomputed after the transfer, or derived from the duals rather than copied.

A singleton row has no other live columns, which is why no shipping family has
had to face this. It is the direct consequence of §4's cascade.

**The literature answers this one, and it says do not patch.** Cederberg & Boyd
2026 (arXiv:2604.23951, section 2.1) publish the dual postsolve transformation
as `y_orig = y_red` and `z_k = c_k - a_k' y_red` — the reduced cost is a
*derived* quantity, recomputed at the end, never carried over. Staleness cannot
arise because nothing is copied. The arrowhead paper's stated aim of "strict
primal-dual complementary slackness" points the same way.

So the design takes the recompute, not the patch. A patch over every column of
row `i` would be a JAOS optimisation with no citation behind it, and D111's
compensated-accumulation discipline would apply to each term of it. A full
`d = c - A'y` recompute has a cost that can be measured and a correctness that
needs no argument.

## 10. The refusal the first version should keep

`JM_PS_FORCING_ROW` refuses to fire when any of its columns touches a row
removed earlier in forward time, because that row's multiplier still reads
zero when `d0` is computed. The same hazard exists here and the same refusal
is the safe first version. Whether it can be lifted is a second question and
should not be answered in the same change.

## 11. What the literature says — `literature-scout`, 2026-08-17

**The transfer rule of §2 is folklore.** No citable description of it was found.
Four things exist around it and none of them is the rule.

| source | status | what it gives |
|---|---|---|
| Tomlin & Welch 1983, Math. Prog. 27, 232-240, `10.1007/BF02591947` | abstract verified, method not read | **names the problem in print**: a reduced model's simplex solution "is not usually 'formally' optimal, in the sense that nonoptimal dual values may be present when the original problem is restored"; the restored problem is "now totally degenerate". They describe a Postsolve procedure for it. |
| Fourer & Gay 1994, in *Large Scale Optimization*, `10.1007/978-1-4613-3632-7_8` | abstract verified, text not read | announces "reconstruction of dual values for eliminated constraints" as a contribution, on the same Brearley et al. reduction family. Best-targeted unread source. |
| Andersen & Andersen 1995, Math. Prog. 71, 221-245, `10.1007/BF01586000` | abstract verified, paywalled | discusses "the restoration procedure in detail". **Whether it covers imposed bounds is unknown. Do not cite it for the rule.** |
| Galabova 2023, Edinburgh PhD, `10.7488/era/2974` | **read** — see §11a | HiGHS's own presolve author. Covers the imposed-bound case directly. |

### 11a. Galabova 2023, read 2026-08-17 — the imposed-bound case is in print

The one source that treats this case. Read with `pdftotext -layout` after
installing `poppler-utils` in WSL; the scout could not read it and neither
could the zlib route, because the body uses 2-byte CID fonts. Section 3.3,
the doubleton equation rule's basis and postsolve, page 21.

**The obligation, stated.** "In addition to satisfying the KKT conditions [...]
a point returned by postsolve must also be a basic feasible solution (BFS) in
order to hot-start the simplex algorithm." So §8 is the right question and the
reason is warm start, which is JAOS's D68.

**The rule, stated.** For a column whose reduced bound was tightened
(`l^r_j > l_j`) and which rests on it:

> "If `x^r_j` is nonbasic it must become basic in the postsolved solution.
> During postsolve an attempt is made to do that. If the dual value of the
> eliminated variable `x_k` is calculated to be infeasible, the dual value is
> transferred to the row by making the row basic."

Two things follow.

1. **§8's status rule is published.** Nonbasic at an imposed bound must become
   basic. That half is no longer folklore.
2. **The slot is not proved to exist.** HiGHS *attempts* the assignment and
   falls back when it fails: "In the cases when `x_k` can be both basic and
   nonbasic [...] an attempt is made to set it to basic. If that assignment of
   values is infeasible, the remaining column `x_j` is selected for the basis."
   So the published state of the art is attempt-and-fallback. **§8c's rank
   argument is stronger than anything in print** — if it survives review, it
   says when the attempt must succeed rather than trying it and seeing.

**§7's record design is published too.** Section 3.4, "Implied bounds":

> "During presolve, it is essential to keep track of which column was used to
> deduce an implied bound on a row and which row was used for an implied bound
> of a column. This is necessary for presolve [...] and also for postsolve, to
> deduce accurate dual values during the postsolve of each rule."

That is exactly the `index2 = implying row` field of §7, with a citation.

**§8a's two-optimal-bases problem is named.** "Due to degeneracy and not only,
at many steps of postsolve there may be multiple correct alternatives for
primal-dual values which satisfy the optimality conditions. A poor selection
may lead to infeasibilities on subsequent steps of postsolve. Such issues are
observed often during the implementation."

**A testing discipline JAOS does not have.** Section 3.4, "KKT conditions
check": HiGHS checks the KKT conditions **after each individual postsolve
rule**, not once at the end, because "a single primal or dual infeasibility at
a particular step of postsolve would get propagated all the way through". JAOS
checks once, at the end, in `jaos_check_solution`. A per-record check under a
diagnostic build is the instrument this family should be built with.

**What HiGHS does about the residue, and it is not a tolerance.** "Additional
simplex iterations after postsolve ensure that the solution returned to the
user is feasible within the desired tolerances." That is Tomlin & Welch 1983's
answer as well, and JAOS already has machinery of that shape (D25, D30). It
also reports that Gurobi "for some problems, return[s] infeasible solutions
after postsolve", so nobody has this exactly right.

**How often the re-iteration is needed, measured.** On netlib, exactly one
problem: **`pilot87`** — one of D97's own four failing instances. On a
74-problem set built from Mittelmann's benchmarks plus four industrial models,
two (`rail02`, `watson_1`) of the 42 solved.

**And an argument for TODO §4, from HiGHS's own numbers.** "Most problems in
the classic Netlib test set are too small to be of interest", and presolve's
geometric-mean speed-up there is **1.10**, against **1.67** on the
Mittelmann-derived set. The population really is doing the deciding.

### 11b. PaPILO declines the reduction — read verbatim 2026-08-17

Confirmed in the SCIP 8.0 report's own words, arXiv:2112.08872 section 6.1,
page 62, read with `pdftotext`:

> "Furthermore, in dual postsolve mode PaPILO only applies variable bound
> tightenings when they fix a variable. Otherwise, the solution to the reduced
> problem may correspond to a non-vertex solution in the original space and
> simple postsolving without an expensive crossover may not be possible."

Reading it whole gives three things the abstract-level summary did not.

**1. It is a user-facing trade, not a law.** "If the basic information is
irrelevant for the user, the variable tightening without fixing can be turned
on by setting the parameter `calculate basis for dual` to false." So PaPILO's
position is that the *duals* are recoverable and the *basis* is not — the same
split §8 arrives at, offered to the user as a switch. JAOS cannot take that
route as written, because a caller reads `jaos_basis` and D68 warm-restarts
from it.

**2. A third design option, which this document did not consider.** For a
previously unbounded variable PaPILO does tighten, and it publishes the bound
deliberately loose:

> "the bound of this variable is set to a finite value, which is slightly worse
> than the best possible bound so that the bound can not be tight in the
> reduced problem."

If the imposed bound can never be active, no column ever rests on it, and §2
through §9 have nothing to do. The whole basis problem is designed away at the
cost of propagation strength. **This is the published form of Gould & Toint
2004's "allowing some slack in these bounds", whose measured effect they call
"numerically significant".** It should be costed against §8d's refusal before
either is built.

**3. PaPILO's own `Substitution` presolver does not support dual postsolve at
all.** Footnote 7 lists the LP presolvers excluded: `DualInfer`,
`SimpleSubstitution`, `Substitution`, `Sparsify`, `ComponentDetection`,
`LinearDependency`. Substitution is the doubleton and aggregation family —
TODO §3, and the machinery D113 says owns `stocfor3`'s iteration half. So the
prize behind D97 is one that PaPILO cannot dual-postsolve either.

Also worth copying: PaPILO checks primal and dual feasibility and the KKT
conditions *after* postsolving and logs the result rather than aborting, since
infeasible solutions can be postsolved too. And "dual postsolving needs to be
informed about every modification found during presolving" — including row
bound changes, because "a row-bound change can lead to changes in the dual
solution due to complementary slackness". JAOS's arena records reductions; a
tightening family makes it record *modifications* as well.

**Where §8 disagrees with them.** §8c's rank argument says that for one active
imposed bound the postsolved point *is* still a vertex and the repair is a
single local swap, so no crossover is needed. PaPILO's worry is real only in
§8d's case. Their restriction to fixing tightenings is far weaker than what D97
needs and would buy none of the three prizes.

**Two adjacent published results.**

- Gould & Toint 2004, Math. Prog. 100, 95-132, `10.1007/s10107-003-0487-2`
  (abstract verified, text not read): "The impact of insisting that bounds of
  the variables in the reduced problem be as tight as possible rather than
  allowing some slack in these bounds is also shown to be **numerically
  significant**." The publish-slack-in-the-imposed-bound question, measured. The
  direction of the result is in the text nobody here has read.
- Kempke et al. 2026, arXiv:2603.03498 (read in HTML): postsolve "reverts all
  reductions applied during presolve in reverse order" off a stack, aiming at
  "strict primal-dual complementary slackness", and states that reverting a
  bound tightening leaves the duals of the tightened variable outdated. That is
  §4's LIFO discipline in print for a solver that does activity-based
  tightening. It is **not** a proof of §4's acyclicity claim.

**Nothing at all on §4's other hazard.** An imposed bound whose implying row
presolve later removed is not named or hinted at in any source reached. Empty
result.

**No constants to inherit.** No paper reached names a tolerance, a residue
window or a threshold for this technique. Both windows in this design must be
measured here from zero, with the flip-canary discipline `docs/tolerances.md`
asks for.

**The exact-versus-tolerance question has nobody on the other side.** No source
states the at-the-bound test in either form, and the reason is now clear: PaPILO
only tightens where the bound *fixes* the variable, so there is no
interior-versus-at-bound question to ask; and the implied-free idiom (Brearley,
Mitra & Williams 1975; already JAOS's D105/D106) never publishes the narrowed
bound at all. §5's exact comparison disagrees with no one.

**Leads recorded so nobody re-hunts them.** Mészáros & Gondzio 2001 addendum
(`10.1287/ijoc.13.2.169.10519`); Mészáros & Suhl 2003
(`10.1007/s00291-003-0130-x`, unverified); Bixby & Saltzman 1994,
`10.1016/0167-6377(94)90074-4`, the crossover PaPILO's sentence avoids. Zhang,
Ploskas & Sahinidis 2026 (`10.1007/s12532-026-00316-3`, open access, read) has a
dual postsolve derivation for constraint aggregation but **not** for bound
tightening — a negative result from a source actually read.

## 12. What is still open

1. ~~Read Galabova 2023.~~ **Done, §11a.** It does not settle §8d — it does not
   discuss two imposed bounds on one row at all. What it gives instead is a
   fallback for when the slot assignment fails, which is what a solver does
   when it has no proof. §8c and §8d are still this document's own.
2. ~~Confirm the PaPILO sentence.~~ **Done, §11b**, and it carried three things
   the summary did not.
3. **Cost the deliberate-slack option against §8d's refusal.** §11b's third
   finding: publish the imposed bound slightly loose so it can never be tight,
   and the whole of §2 through §9 has nothing to do. Two designs now compete,
   and the choice is a measurement, not an argument. Gould & Toint 2004 measured
   the same trade and called it "numerically significant"; their direction is
   still unread (`10.1007/s10107-003-0487-2`, RAL has it open).
4. **§4's empty hazard**: an imposed bound whose implying row is later removed.
   No literature. Must be reasoned out here or refused at the firing site, the
   way `JM_PS_FORCING_ROW` refuses.
5. The measurement §6 names, which no amount of reading replaces.

## 13. Superseded questions

Kept so nobody re-asks them. Answered above: whether A&A covers imposed bounds
(unknown, paywalled); whether §4's acyclicity is published (no); what tolerance
others use (none state one); whether anyone declines instead (yes, PaPILO);
whether two columns can break the count (yes, §8d, equality rows only); whether
the other columns' reduced costs need patching (no, recompute them, §9).
