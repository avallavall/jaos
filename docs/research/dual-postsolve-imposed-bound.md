# Dual postsolve for a presolve-imposed bound — the design on paper

D97's second precondition, worked out on paper 2026-08-17. **Nothing here is
built and nothing is measured.** No source file changed to produce it.

Written against `src/check.c`'s own rule and against the two families already
shipping that do the same kind of derivation: `JM_PS_FORCING_ROW`
(`src/presolve.c:2303`) and `JM_PS_SINGLETON_ROW` (`src/presolve.c:1894`).
§11 is a `literature-scout` run of the same date. **Its headline — that the
transfer rule is folklore — is wrong, and §11c is the correction.** The rule
is published, in Mathematical Programming, with its own numbered subsection
and two numbered equations. The scout could not find it because it could not
read a PDF; the tooling was fixed later the same day.

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

**And the other published treatment does the opposite** (§11c). Gould & Toint's
(6.2) patches exactly these columns, `z_j = -(a_ij/a_ik) z_k+` for every `j` in
the row, and does not recompute. So both designs are in print, and the choice
between them is a measurement here rather than a question of which one has a
citation.

The recompute is the safer first version: `d = c - A'y` over the whole model
has a cost that can be measured and a correctness that needs no argument, where
the patch inherits D111's compensated-accumulation discipline on every term
plus the assignment-versus-increment discrepancy §11c flags in (6.2). Take the
recompute, measure it, and only then ask whether the patch buys anything.

## 10. The refusal the first version should keep

`JM_PS_FORCING_ROW` refuses to fire when any of its columns touches a row
removed earlier in forward time, because that row's multiplier still reads
zero when `d0` is computed. The same hazard exists here and the same refusal
is the safe first version. Whether it can be lifted is a second question and
should not be answered in the same change.

## 11. What the literature says — `literature-scout`, 2026-08-17

**Read §11c first.** This section was written before Gould & Toint 2004 could
be opened, and its headline — that the transfer rule is folklore — is wrong.
What follows is still accurate about every *other* source, and is kept because
the negative results in it are worth as much as the positive one.

**The transfer rule of §2 was not found in any source the scout could read.**
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

### 11c. Gould & Toint 2004 §6.2 — the rule is published, and this corrects §11

Read 2026-08-17 from the authors' own copy at
`numerical.rl.ac.uk/media/people/nick-gould/GoulToin04_mp.pdf`, with
`pdftotext -layout`. N. I. M. Gould and Ph. L. Toint, "Preprocessing for
quadratic programming", Mathematical Programming 100, 95–132 (2004),
`10.1007/s10107-003-0487-2`. **Section 6.2 is titled "Tightening a bound on the
variables" and it is exactly D97's second precondition.**

The problem, in their words:

> "If a bound on `x_k`, say, is tightened during the analysis, it may happen
> that the solution of the reduced problem has a nonzero dual variable `z_k+`
> associated with this constraint. Since it is purely artificial, `z_k` must be
> set to zero in the solution of the initial problem, while maintaining both
> dual feasibility and complementarity. This typically requires modifying the
> multipliers associated to the constraints involving `x_k` and, as a
> consequence, the duals `z_j` of the other variables appearing in these
> constraints."

Their equation (6.1), for the doubleton case, and (6.2) for a row with more
than two variables:

    y_i = y_i+ + (1/a_ik) z_k+          (6.1)
    z_j = -(a_ij / a_ik) z_k+           (6.2), for all j with a_ij != 0

**That is §2's rule and §9's obligation, both, published.** Relabel their `k`
as this document's `j` and (6.1) is `y_i += d_j / a_ij` unchanged. (6.2) is the
other columns of the implying row, which §9 identified as the only genuinely
new part of the family. So §11's headline was wrong and §5b's claim that the
arithmetic already ships in `JM_PS_SINGLETON_ROW` is the doubleton case of
(6.1), which they derive from the same premises this document does.

**One discrepancy, to be checked and not adopted quietly.** (6.2) is written as
an *assignment*, `z_j = -(a_ij/a_ik) z_k+`, where §3's derivation gives an
*increment*, `w_j -= a_ij * delta`. The two agree only where `z_j+` is already
zero, which their text asserts for the doubleton case ("the fact `z_j+ = 0` by
design") but not obviously for (6.2)'s general row. Either their setting
guarantees it or the paper is compressing. **Do not copy (6.2) as an
assignment without settling that**, because a column of the implying row that
carried a nonzero reduced cost in the reduced solve would have it silently
discarded.

**What they do not do: the basis.** Section 6.2 says nothing about basic and
nonbasic status, or about a count. Their solver is QPB, an interior-point
method, which has no basis to keep. So §8 is genuinely unaddressed here as
well, and the scout's finding stands: nobody publishes the basis question.

**The hardest case they name is not §8d's.** Theirs is "when a bound on `x_k`
is imposed as the result of the analysis of dual constraints", where "the
multipliers of all constraints `i` such that `a_ik != 0` must be considered".
That is a different generalisation from two imposed bounds on one equality
row. §8d remains this document's own.

### 11d. The deliberate-slack question, measured — §12's item 3

Gould & Toint implement §11b's third design as a **user-selectable mode**, and
credit the idea to Fourer & Gay 1994. Three modes: `tightest` publishes the
tightest bounds the reduction found; `medium` publishes the best bounds known
not to be redundant; `loosest` publishes "the loosest bounds that are known to
guarantee the equivalence of the reduced problem and the original one".

Their measured result, over 160 problems:

| mode | average gain | on linear problems | failures |
|---|---|---|---|
| none | — | — | 16 |
| tightest | 11% | 10% | 6 |
| medium | 2% | 9% | 7 |
| loosest | **12%** | **14%** | **5** |

Verbatim: "The loosest mode therefore appears to be of real interest from the
efficiency point of view, especially for linear problems. Note also that the
choice between the different presolving modes could well depend on the solver
used."

**So the direction favours slack, and most on linear problems.** That is JAOS's
case, and it argues for §11b's design over §8d's refusal.

**The caveat is theirs and it is the whole caveat.** QPB is an interior-point
algorithm. It has no basis, so the entire §8 obstacle — the one thing that
makes this hard for JAOS — cannot appear in their measurement, and their own
last sentence says the choice depends on the solver. A 12%-versus-11% gap
measured on a solver that cannot see the problem is a reason to run the
experiment here, not a reason to skip it.

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

### 11e. Fourer & Gay 1994, read 2026-08-17 — the oldest statement, and two things nobody here had

Read from the authors' own copy at `ampl.com/REFS/pripre.pdf`. R. Fourer and
D. M. Gay, "Experience with a Primal Presolve Algorithm", in Hager, Hearn &
Pardalos (eds), *Large Scale Optimization*, Springer 1994, 135–154,
`10.1007/978-1-4613-3632-7_8`.

**Its "Recovering Dual Variables" section is the earliest statement of §7 and
§4.** Verbatim:

> "To compute dual variables for constraints eliminated by presolving, it is
> necessary to record which eliminated constraints were responsible for the
> bounds conveyed to the solver. We then examine the eliminated constraints in
> the reverse order of their elimination."

That is the record design and the LIFO ordering, in 1994, nine years before
Galabova and ten before Gould & Toint. It then covers the two cases JAOS
already ships: a constraint with "one remaining nonzero coefficient"
(`JM_PS_SINGLETON_ROW`) and a constraint that "together with several
then-current variable bounds, fixed several variables", whose equations (8) and
(9) are the forcing condition and whose rule — "AMPL chooses `y_i` to make one
of these conditions hold with equality" — is `JM_PS_FORCING_ROW`'s
`min`/`max` over `d0/a_ij`.

**What it does not cover** is this document's case. Their scheme keys on
*eliminated* constraints; a tightening that leaves the implying row alive is
not in it. Their non-interference claim — "this has no effect on the other
components of (7) for variables and constraints not yet fixed or removed when
constraint `i` was eliminated" — is §4's argument asserted rather than proved,
and scoped to what was still live at elimination time. Nothing about the basis,
as usual.

### 11f. Two things this changes

**1. The slack question has a simplex answer, and it is 1994's default.**
§11d's result came from an interior-point solver. Fourer & Gay's "Degeneracy"
section answers it for simplex directly:

> "if AMPL passes the strongest variable bounds it can deduce to a
> simplex-based solver, the solver often takes more iterations than it takes
> with variable bounds relaxed to those implied by eliminated constraints.
> AMPL therefore maintains two sets of variable bounds [...] **By default it
> passes the latter set**"

And the caveat §11d states from Gould & Toint's own text is confirmed from the
other side: "Degeneracy is much less of an issue for interior-point than for
simplex algorithms." So the effect is **larger** for JAOS than the 12%-vs-11%
of §11d, not smaller, and AMPL has shipped the loose default for thirty years.

Their own honest qualification, which goes in the record too: "despite
increased degeneracy, simplex algorithms sometimes run better with the tighter
bounds because they choose a different pivot order."

**2. Directed rounding, which is a design D97 never considered.** This is the
finding, and it lands on D97's own instances:

> "A preliminary version of the computational experience reported below
> revealed a case (netlib's `lp/data/maros`) where AMPL's default presolve
> settings made it discard constraints that kept the problem from being
> unbounded. Of course, roundoff error was to blame for this difficulty. When
> we modified the presolve algorithm to use the directed roundings that are
> available with IEEE arithmetic, this difficulty went away. On four other
> problems from netlib's lp/data (`greenbea`, `greenbeb`, `perold`, and
> `woodw`), AMPL's presolve reported inconsistent constraints before we
> introduced directed roundings."

`maros` is one of D97's four failing instances. `greenbeb` is one of D108's
three. AMPL hit the same failure class — presolve declaring a solvable model
infeasible — and the fix was **not** a tolerance. It was computing the activity
bounds with IEEE directed rounding, so the deduced bound is valid by
construction and no judgement window is needed for validity at all. D114
derived *why* JAOS's window failed; this says the window may not have to exist.

Cost, from their measurement: "usually only increases by a few percent the time
AMPL spends to process a problem", and "seldom add more than 1%" of the
combined AMPL-plus-solver time on larger problems.

And one line that reads like this repository's own Makefile:

> "on an IBM Risc System 6000, which by default computes `α × β + γ` with just
> one rounding error (a 'fused multiply-add'), one of these infeasibility
> diagnostics returned. Our current policy is to use a compiler option that
> forbids fused multiply-adds in the presolve algorithm."

JAOS ships `-ffp-contract=off` and CLAUDE.md calls it load-bearing. Same defect,
same fix, arrived at independently thirty years apart.

**Does it break D8?** No. `fesetround` is C99, the mode is set explicitly, and
the same mode gives the same bits on every machine. It is a portability and
performance question, not a reproducibility one — but that has to be measured
here, not assumed.

## 12. What is still open

1. ~~Read Galabova 2023.~~ **Done, §11a.** It does not settle §8d — it does not
   discuss two imposed bounds on one row at all. What it gives instead is a
   fallback for when the slot assignment fails, which is what a solver does
   when it has no proof. §8c and §8d are still this document's own.
2. ~~Confirm the PaPILO sentence.~~ **Done, §11b**, and it carried three things
   the summary did not.
3. ~~Cost the deliberate-slack option against §8d's refusal.~~ **The direction
   is read, §11d: loosest wins, 12% against 11%, and 14% against 10% on linear
   problems specifically.** It is still not settled *here*, because their
   solver is an interior-point method with no basis, so §8 — the one thing that
   makes this hard for JAOS — could not appear in their measurement, and their
   own text says the choice depends on the solver. The experiment to run here
   is now well defined and that is the whole of what changed.
4. **§4's empty hazard**: an imposed bound whose implying row is later removed.
   No literature. Must be reasoned out here or refused at the firing site, the
   way `JM_PS_FORCING_ROW` refuses.
5. The measurement §6 names, which no amount of reading replaces.

## 13. Directed rounding, worked against this tree

§11f says AMPL's fix for D97's failure class was IEEE directed rounding. This
section is what that means against `src/presolve.c` as it stands, because
"the literature says do X" is not a design.

**Where the arithmetic is.** `ps_row_range` (`src/presolve.c:370`) builds a
`ps_range` holding `lo_sum`, `hi_sum` and `traffic`, each through `ps_acc_add`,
a compensated accumulator. The two ends are already kept apart, and infinite
terms are counted rather than summed. So the structure directed rounding needs
is the structure that is already there: `lo_sum` wants rounding toward `-inf`
and `hi_sum` toward `+inf`, nothing else changes.

**JAOS already took half of Fourer & Gay's step.** Their fallback for machines
without directed rounding was a tolerance `τ`. JAOS's equivalent was
`PRESOLVE_TIGHTEN_EPS = 1e-9`, and it is **gone** — D103 and `02-09` replaced
all three of its sites with `DBL_EPSILON × max(1, traffic)`, which is the
error bound rather than a judgement. That is the same move, made for the same
reason, and the file says so at `src/presolve.c:164`.

**What directed rounding adds, and it is not a smaller window.** It removes the
need for a window in the forcing test *at all*. Today the test asks
`max_act <= rl + w` and something has to supply `w`; D114 is the record of that
`w` taking its scale from a materialized magnitude of 9.42e11 and certifying
5.86 of real slack as binding. If `hi_sum` is a **proven** upper bound on the
row's maximum activity, the test is `hi_sum <= rl` and there is no `w` to get
wrong. The failure mode is not narrowed — the thing that failed stops existing.

**Do not reach for `fesetround`.** Three reasons, and the third is the one that
decides it:

1. It needs `#pragma STDC FENV_ACCESS ON`, which GCC does not fully implement;
   with it off, the compiler may hoist arithmetic across the mode change.
2. A global mode interacts with vectorisation and with any library call made
   while it is set.
3. **D8.** Bit-identical results on every machine is absolute here. A global
   rounding mode is exactly the kind of ambient state whose effect can differ
   between two builds of the same source, and `-ffp-contract=off` is in the
   Makefile because this project has already been bitten by ambient FP
   behaviour.

**The variant that fits.** Keep round-to-nearest and the compensated
accumulation, then widen the finished sum outward by the accumulator's own
error bound — `nextafter` a bounded number of ulps, or one subtraction of
`DBL_EPSILON × traffic` from `lo_sum` and one addition to `hi_sum`. The result
is a proven bound, computed with no fenv, no pragma, no ambient state, and the
same bits everywhere. It is deterministic by construction rather than by
inspection.

**And it is a tightening, not a loosening — which is the opposite of what this
section first said.** `docs/tolerances.md:300` is explicit that the three
activity-range readings are deliberately not in the tolerance table: "the only
thing that can separate two numbers that should be equal is the rounding in the
sum, so the window is a small multiple of `DBL_EPSILON` times the traffic
through it." `ps_row_tol` is that window and it is **8** ulps — a small multiple
chosen to cover the compensated accumulation *and* the comparison on top of it.

A proven outward-rounded bound needs no multiple at all. The comparison becomes
exact, because the value already errs on the safe side. So the conservatism
goes from *8 ulps of slack in the comparison* to *1 ulp of widening in the
value*, and **more** reductions fire, not fewer. The gain is not the arithmetic;
it is that the guarantee is proven where today it is a multiple somebody chose.

**How much headroom that multiple actually has, measured.** `02-09` briefly
routed `ps_row_tol` through the shared constant and put the three activity
readings on the `EXTRA_CFLAGS` hook. Review caught it with a run:

    make netlib EXTRA_CFLAGS=-DJAOS_PRESOLVE_ROUND_ULPS_VALUE=64

reproduced `02-04`'s failure — `pilot` INFEASIBLE with column 3554 pinned.
**So the margin at these exact sites is between 8 and 64 ulps of traffic**, and
that is one factor of eight, not the eight decades the `PRESOLVE_ROUND_ULPS`
sites enjoy. Those are different sites with a different measurement: 12 residues
above zero out of 32240 probes, all on `netlib-infeas`, none below 3.69e8 ulps.
**Do not carry that margin across.** The activity-range readings are the tight
ones, and they are the ones this section proposes to change.

### 13a. Measured, and refused — `bench/measurements/02-24/`

Everything above this line predicted a no-op or a small gain. **Both designs
were built and both are refuted**, in a worktree, the same day. The record is
`bench/measurements/02-24/` with the candidate diff beside it.

**The framing above is wrong about FORCING.** That reading detects an
*equality* — the range's end has reached the row's own bound — and outward
widening does not make an equality detection safer, it destroys it.
`make_forcing_row_model` in the test suite is `x0 + x1 <= 0` with both columns
in `[0, 10]`: minimum activity exactly 0, upper bound exactly 0. One ulp of
outward widening declines it, and `make test` said so in under a minute. That
shape is the reason the family exists, not a corner case.

**The narrowed design — outward only for INFEASIBLE and REDUNDANT, which do
prove inequalities — passes every test and fails the gate.** `pilotnov` goes
from 86587427 work units to 2378158900, **27.5x**, against a 2.0x bar. The
mechanism is named rather than inferred: 32 rows survive instead of being
dropped (`101 -> 69` removed, columns identical at 1811 on both sides), and
those 32 rows cost 60866 iterations. The answer is bit-identical and the
residuals are *better*, which makes it a cost question and not a correctness
one.

So the residual unsoundness stays: JAOS's redundant test can drop a row whose
minimum activity is within 8 ulps of traffic below `rl`. It is bounded, no
instance has ever been shown to give a wrong answer because of it, and removing
it costs 27.5x on one instance. The reopen conditions are in `02-24`.

**What survives from §11f.** Fourer & Gay's `maros` failure was real and
directed rounding fixed it. JAOS is not in that position, because D103 already
replaced the judgement constant with the error bound — their pre-fix tolerance
was orders wider than 8 ulps. Reading a published fix is not the same as
needing it.

## 14. Superseded questions

Kept so nobody re-asks them. Answered above: whether A&A covers imposed bounds
(unknown, paywalled); whether §4's acyclicity is published (no); what tolerance
others use (none state one); whether anyone declines instead (yes, PaPILO);
whether two columns can break the count (yes, §8d, equality rows only); whether
the other columns' reduced costs need patching (no, recompute them, §9).
