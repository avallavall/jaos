# TODO — what is open, in the order it should happen

The whole of the planning record. `SPECS.md` says what JAOS is, `DECISIONS.md`
says why closed questions closed, `CHANGELOG.md` says what landed, `bench/`
says what it costs. This file says what is next. When something lands, its
line leaves this file in the same commit.

## Where the last session stopped — 2026-08-18

### The state of the tree, first, because everything below assumes it

**Nothing is in flight. The tree is clean, `main` is pushed, no worktree is
registered, and the three gate baselines were rewritten deliberately after
D122 and confirmed by a following run that reads `0 regressed, 0 improved, 0
new` and exits 0 on all three.** `make test`, `make sanitize` and
`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` all exit 0.

Two things about this working session that a later one needs and cannot infer:

- **Another Claude session was committing and pushing to this repository at
  the same time.** Its commit was documentation only (`docs/diagrams/`,
  `docs/architecture.html`) and could not affect a solve, so no measurement was
  invalidated. If two sessions are live again, `CHANGELOG.md`, `DECISIONS.md`
  and `TODO.md` are the files that will collide.
- **A campaign cannot be run in the main tree while it is dirty**, so a
  candidate is measured in a git worktree with `bench/instances*` symlinked in.
  `preflight.sh` follows symlinks now. See the memory note
  `measure-a-candidate-in-a-worktree`.

### What closed today, in one line each

`fome`'s candidates all go to D95 before D106 sees them (**D117**) → giving
D106 first refusal is **refused**, `pilotnov` publishes 29% wrong (**D118**) →
that was not presolve, the same reduced model is right at a shorter
refactorization interval (**D119**) → five explanations closed, one
contradiction left (**D120**) → the contradiction is a cost that never comes
back (**D121**) → **repaired and landed** (**D122**).

The repair: a repayment restores from a write-once `cost0` instead of
subtracting the recorded loan, because `x += d; x -= d` does not restore `x`.
Costs 1.0001x on netlib, 0.9975x on Kennington, 29 of 29 infeasible instances
bit-identical, iterations moving on one instance of 139. `jaos-measurer`
ACCEPT.

## → START HERE: §5a, three things left in the shift machinery

None of the three is reached by any of the 139 instances, which is why the gate
is green. They are open because each is a defect that a harder model would
reach, and one of them touches published output.

**Do them in this order. The numbering is the order.**

### 1. Is a loan still outstanding when the answer is published?

One assert answers it, and it either finds a live defect in published duals or
removes the suspicion. **This is the one to start with.**

`refresh` re-runs `shift_to_feasible` over every variable when
`repair_singular_basis` fired, and it is called from `take_best_if_better` and
from `restore_settled` **after** each has called `repay_shifts`. On that path
`reenter_after_settling` returns with loans still in the costs, and nothing
settles them before `classify_optimum` and `publish`. The published objective
is safe — `publish` builds it from `m->col_cost` — but `sol_dual` comes from
`s->cost` and `sol_redcost` from `s->d`, and both would carry the loan.

**What to do:** assert every `s->shift[v]` is zero on entry to `publish`, build
with `EXTRA_CFLAGS=-UNDEBUG`, and run the three sets. Expect 128 instances to
answer; the other 11 abort earlier on a pre-existing assert (see the standing
debts). If it never fires, the suspicion is removed and the assert can stay. If
it fires, that is a defect in published duals and it needs its own decision.

Found by `numerics-reviewer` while reviewing D122, and deliberately kept out of
D122 so one change did one thing.

### 2. 186 loans go missing, and nothing explains it

`pilotnov` ends with **186** variables whose lent and repaid totals differ, the
worst by **256** (`bench/measurements/02-29/loan-balance.txt`). D122 made this
harmless wherever a settle runs, because the cost is restored whatever the
record says — so it is now a question about the record rather than about the
answer. It still means **`shift[v]` cannot be trusted to say how much a cost
moved**, and `src/simplex.c` says so where it matters.

**What to do:** it is a diagnostic before it is a repair. Instrument the one
lend site and both repayment sites to name the variable and the round where a
loan goes missing. `bench/measurements/02-29/run-loan-balance.sh` already
tallies the totals and only needs the site attribution added.

### 3. Nothing bounds a loan relative to the cost it lands on

A `need` of 1e32 on a cost of one is not a repair of a sign condition, it is
the sign condition being overwritten. `shift_to_feasible` also sets
`s->d[v] = 0.0` unconditionally, which asserts the cost moved by exactly
`need` — false whenever `need` is below the ulp of the cost.

**What to do:** this is a new constant, so it needs a sweep on both sides and a
row in `docs/tolerances.md` beside the others. Load `fp-numerics` first; it
gives the shape of the bound, which is that a sum is known no more finely than
its terms. **Do not pick a number and measure one side of it.**

**`REFACTOR_EVERY` is not a proposal in any of this.** It is what D119 swept to
prove the failure was numerical, on one instance, and one instance is not a
population — `bench/measurements/02-28/` says so in as many words.

### What else is open, if §5a is dropped or finished

§5a is first because it is correctness. It is not the only open thing, and the
rest of this file is not all background:

| where | what |
|---|---|
| §4, end | how often `plato` should run — `pds` alone is 6.4 hours of wall clock. And `nug20`/`nug30` are unmeasured rather than unsolvable |
| §4, end | **`nug` has no row removed by any family**, all three instances. Nobody has asked why |
| §3 | doubleton equalities — 8.55% of netlib's live rows and 29.36% of Kennington's, and **99.7% of it is behind D97** |
| §5 | the rest of M2: factorization fill, Devex pricing, and closing the competitive gate |
| §6 | feature expansion, decided but not started |
| standing debts | about a dozen at the end of this file, each small and real. Two are worth more than they look: the `assert(want_lo <= want_hi)` clamp, because **no assert-enabled build can run 11 of the 94 instances** until it lands; and the `warm` records, 21 `src/` commits behind |

D97 is the largest prize in the file and it is behind one precondition:
a dual postsolve for an imposed bound. Its first precondition was met by D114.
`docs/research/dual-postsolve-imposed-bound.md` is the design and nothing is
built.

Everything below this line is that detail, in order.

---

**The fourth set exists and has been run (§4, `bench/measurements/02-23/`).**
`fome` 4/4 and `pds` 8/8, both `gate: PASS`, every instance `shape=ok
checker=ok det=ok`. `nug08-3rd` solved; `nug20` and `nug30` are unmeasured, not
unsolvable. Nothing is in flight and `baseline-header` is merged.

**The number §4 was built to get.** The pds ladder is now twelve points over a
52.9x range in rows, four of which were already in the tree. Split it:

| | iteration exponent | work exponent |
|---|---|---|
| `pds-02` … `pds-20`, the range netlib and Kennington live in | **1.27** | **2.61** |
| `pds-20` … `pds-90`, above it | **2.08** | — |
| whole 52.9x range | 1.69 | **2.77** |

**Small models understate iteration growth by a factor of 1.6, and understate
work-unit growth by 6%.** So this repository's chosen unit of cost holds its
shape across a 53x change in model size and the iteration count does not —
CLAUDE.md's "work units are the unit of cost", measured rather than assumed.

**And the set is expensive.** `pds` alone is 23016 s of solve time, 6.4 hours of
wall clock at `J=4`, and `pds-100` costs 6.425e11 work units — twenty times the
whole netlib standard set. The three `netlib*` targets stay the gate. How often
`plato` should run is open and is not decided in 02-23.

The tree is clean and **§1 is finished**: the P0 comparison was re-taken
after D106 (3.15x HiGHS, **0.95x SoPlex — faster per solve for the first
time** — 2.57x Clp; worst instance now `stocfor3` at 30.0x, which is
presolve's, not the factorization's — its fill is 1.036), and all five
questions D106 opened closed in one day, four by measurement alone and one
with a source change: D107 (inequality rows are a tenth, refused), D109
(the window's floor declines nothing, refused), D108 (two overcost
mechanisms, no refuse rule), D110 (`maros-r7`'s factor collapsed 7.9x),
D111 (the postsolve recovery is compensated; `jaos-measurer` ACCEPT; 9
netlib digests moved where the rounding lived; netlib baseline rewritten
deliberately and confirmed). The reference build debt is repaired in
spirit by D111's test discipline but item four below still stands as
written.

**§2 closed too (D112), and `stocfor3` is counted (D113): the aggregator
alone owns its iteration half, so D97's reopen now guards three prizes.**

**D97's derivation is DONE (D114, `bench/measurements/02-21/`)**: the
over-tightening was never the implied bounds' values — a forcing window
scaled by the activity's magnitude (941.58 on a row whose bounds are 0)
certified 5.86 of real slack as binding and pinned the vertex `pilot`
cannot have. The shipped forcing family already windows by the row
bounds, which is why it is green.

**D97's second precondition is designed, 2026-08-17:
`docs/research/dual-postsolve-imposed-bound.md`.** Thirteen sections, the
scout run folded in as §11. Nothing built, nothing measured, no source file
touched, and no `DECISIONS.md` entry — a design is not a decision. What it
establishes:

- The transfer `y_i += d_j / a_ij` is legal in exact arithmetic in all four
  sign cases, because `x_j` at the imposed bound forces the implication's
  premises tight by feasibility alone.
- The cascade is acyclic and strictly decreasing in derivation time, so the
  arena's existing LIFO replay is the whole of the ordering it needs.
- **The arithmetic and the ownership test already ship**, in
  `JM_PS_SINGLETON_ROW` (`src/presolve.c:2035`, `1977-2021`). What is new is
  only the implying row's *other* columns, which a singleton row does not
  have. Smaller work than D97 implies.
- The basis, not the reduced cost, is where the risk is. §8c proves the
  postsolved point is still a vertex for one active imposed bound, with
  exactly one local basic/nonbasic swap. §8d finds where that breaks: **an
  equality row imposing bounds on two of its own columns**, which needs a
  refusal at the firing site. D112 measured 94% of the widening family's
  firings on equality rows, so the refusal is not free.
- `sol_redcost` must be recomputed from the duals, not patched — the copy at
  `src/presolve.c:2529` goes stale for every surviving column of the implying
  row. The literature recomputes (Cederberg & Boyd 2026, §2.1).
- **Two halves of it are in print after all** (§11a, §11b — `poppler-utils`
  installed in WSL 2026-08-17, which is what made the PDFs readable). Galabova
  2023 states the status rule and the record design, and names §8a's
  multiple-optima hazard. But the published state of the art *attempts* the
  basis assignment and falls back; **§8c's rank argument is stronger than
  anything found in print**, which is a claim that has to survive review before
  it is relied on. HiGHS also checks the KKT conditions after **each individual
  postsolve rule**, which JAOS does not do and should, under a diagnostic build.
- **The rule itself is published, and the scout's "folklore" headline is wrong
  (§11c).** Gould & Toint 2004, Math. Prog. 100, 95-132, section **6.2**, titled
  "Tightening a bound on the variables", is D97's second precondition with its
  own two numbered equations. (6.1) is `y_i += z_k/a_ik`, this design's §2
  unchanged; (6.2) is the implying row's other columns, which §9 had identified
  as the only genuinely new part. The scout missed it because it could not read
  a PDF. **One discrepancy to settle before copying it**: (6.2) is written as an
  assignment where §3's derivation gives an increment, and the two agree only
  where the other columns' reduced costs are already zero.
- **What is still nobody's**: the basis. Gould & Toint's solver is
  interior-point, so §8 cannot arise in their treatment either. §8c and §8d
  remain this repository's own.
- **A third design nobody here had considered** (§11b): publish the imposed
  bound deliberately loose so it can never be tight, and §2 through §9 have
  nothing to do. PaPILO ships it for previously-unbounded variables, crediting
  Fourer & Gay 1994. **Gould & Toint measured it over 160 problems (§11d) and
  the loosest mode wins**: 12% average gain against tightest's 11%, and 14%
  against 10% on linear problems specifically, with the fewest failures. Their
  own caveat is the whole caveat — an interior-point solver cannot see §8.
- **Directed rounding, a design D97 never considered (§11f).** Fourer & Gay
  1994 hit exactly D97's failure class — presolve declaring a solvable model
  infeasible — on **`maros`** (one of D97's four) and on `greenbea`,
  **`greenbeb`** (one of D108's three), `perold` and `woodw`. Their fix was not
  a tolerance. They computed the activity bounds with **IEEE directed
  rounding**, so the deduced bound is valid by construction and the judgement
  window is not needed for validity at all. D114 derived why JAOS's window
  failed; this says the window may not have to exist. Their measured cost is "a
  few percent" of presolve time and under 1% of the combined total. `fesetround`
  is C99 and deterministic, so D8 is not at risk — but that gets measured here,
  not assumed. Their same section reports a fused-multiply-add reviving one
  false infeasibility, and their fix is a compiler option forbidding it: JAOS's
  `-ffp-contract=off`, arrived at independently thirty years apart.
- **The slack question has a simplex answer too, and it is 1994's default
  (§11f).** AMPL keeps two bound sets and **passes the looser one by default**,
  because "if AMPL passes the strongest variable bounds it can deduce to a
  simplex-based solver, the solver often takes more iterations". And
  "degeneracy is much less of an issue for interior-point than for simplex
  algorithms", so the §11d effect is **larger** for JAOS, not smaller. Their own
  qualification is in the record too: simplex sometimes runs better with the
  tighter bounds, because it picks a different pivot order.
- No constants to inherit. Both windows get measured here from zero.

**Directed rounding was tried first and is REFUSED
(`bench/measurements/02-24/`, 2026-08-17).** Built in a worktree while the
`plato-pds` campaign held the main tree; nothing landed. Two designs, both
refuted, and the second one is the useful entry:

- **Widening both ends outward and dropping every window** dies on `make test`
  in under a minute. The FORCING reading detects an **equality**, not an
  inequality, and outward widening destroys an equality detection instead of
  making it safer. `x0 + x1 <= 0` with both columns in `[0, 10]` — the test
  suite's own `make_forcing_row_model` — has minimum activity exactly 0 against
  an upper bound of exactly 0, and one ulp declines it. That shape is why the
  family exists.
- **Outward only for INFEASIBLE and REDUNDANT**, which do prove inequalities,
  passes `make test` and `make sanitize` clean and then fails the gate:
  `pilotnov` goes 86587427 → 2378158900 work units, **27.5x** against a 2.0x
  bar. Mechanism named, not inferred: 32 rows survive instead of being dropped
  (101 → 69 removed, columns identical at 1811 both sides) and cost 60866
  iterations. The answer is bit-identical and the residuals are *better*, so it
  is a cost question, not a correctness one.

So the residual unsoundness stays and is now written down: the redundant test
can drop a row whose minimum activity is within 8 ulps of traffic below `rl`.
Bounded, never observed to produce a wrong answer here, and 27.5x to remove.
The reopen conditions are in `02-24`. **Fourer & Gay's `maros` fix was real and
JAOS is simply not in that position**, because D103 already replaced the
judgement constant with the error bound.

Next, therefore, is unchanged from before the detour: the deliberate-slack
design against §8d's refusal, measured here because both published directions
came from solvers that could not see the basis. Then the deliberate-slack design against §8d's refusal, measured here
because both published directions came from solvers that could not see the
basis. Alternatives if this is dropped: §4's fourth set, §5's Devex.

Three things this session left deliberately unmeasured, so nobody re-derives
them by accident: `greenbeb`'s 1.5126x, `maros-r7`'s 15.7x per-iteration drop,
and what a zero margin admits. Each has its own subsection with its numbers.

## 1. Implied free column singletons — what its own measurements left open

**The equality-row half landed 2026-08-15 (D106).** It removes 1041 rows,
2040 columns and 47043 nonzeros over 17 of the 94 standard instances, and
**980 rows from `maros-r7`**, the figure stated before the code existed.
Kennington is bit-identical. Work over the standard set is a geometric mean
of 0.9527x. `maros-r7` alone goes from 21010708013 work units to 328053926
and from 10479 iterations to 2576.

**Every question this section opened is closed**: §1a (D107), §1b (D109),
§1c (D111), §1d (D108), §1e (D110). The subsections below are the closed
record; nothing here is open work.

### 1a. Inequality rows — closed 2026-08-17 by D107: a tenth of the count, refused

The sign-respecting count is **341 rows of the 3315**, 10% and not two
thirds: 2980 of the as-loaded hits are equality rows, so the gap between
3315 and the shipped 1041 is margin (§1b owns 1353 of it) and presolve-time
interaction, not row sense. 304 of the 341 sit on the six `ship*` instances,
all below the comparison's 0.05 s floor; `stocfor3` carries zero; Kennington
carries zero. The sign condition declines nothing a feasible bounded model
can carry, and that is derived in the entry. Refused at this population; the
reopen condition is in the refusals table. Readings in
`bench/measurements/02-13/`.

### 1b. The margin's floor — closed 2026-08-17 by D109: it declines nothing, and the window ships as it is

Two measurements closed it in one day. `d2q06c`'s 2.2163x at margin zero is
the D108 trajectory class, with the extra iterations on degraded pricing —
no correctness or relaxation defect behind the number
(`bench/measurements/02-15/`). And a floor-less window at the shipping 8 is
a bit-identical no-op over all 94 standard instances, digests included: the
1353 rows between margin 8 and margin 0 are declined by any nonzero window,
because their bounds are zero or too small to absorb a margin of any scale
(`bench/measurements/02-16/`, with the instrument's self-proof and the
prediction stated before the run). The constant stays `ULPS = 8` with both
floors; the reopen condition is in the refusals table.

### 1c. The recovery's error — closed 2026-08-17 by D111: the accumulation is compensated

The 02-18 case (11.4x the margin's promise, predicted bit for bit) settled
the choice: compensate rather than degree-scale, because the arithmetic can
simply not make the error. Every replay accumulation goes through
`ps_row_add`, readers read sum plus carry, and the walkers fold once.
`numerics-reviewer` read the diff first and its findings carry dispositions
in D111; `jaos-measurer` returned ACCEPT from its own campaign: verdicts,
iterations and work units identical everywhere, 9 netlib digests moved
where the replay's rounding lived, infeas and Kennington 45 of 45
bit-identical. The pinned test fails on the unrepaired tree and is green
now. The netlib baseline was rewritten deliberately after the verdict and
confirmed by a following run.

### 1d. `greenbeb`, `scfxm3` and `forplan` — closed 2026-08-17 by D108: two mechanisms, no refuse rule

The record split and a calibrated callgrind attribution say the three do not
share a mechanism: `greenbeb` pays in iterations (1.3779x, per-iteration
1.03x in instructions — trajectory), `scfxm3` pays per iteration in the
ratio-test path (`update_dual` 1.71x, `admit_candidate` 1.57x, LU side
1.05–1.11x, iterations 1.054x), `forplan` is small and diffuse. Both are
downstream of an exact substitution, nothing at the reduction site separates
them from the 14 instances the family made cheaper, and a refuse rule on
predicted trajectory is refused — the reopen condition is in the refusals
table. §2's relaxing family and its candidate rule are untouched. Readings
in `bench/measurements/02-14/`.

### 1e. `maros-r7`'s cheaper iteration — closed 2026-08-17 by D110: the fill collapsed

Measured with a per-refactorization fill print on both trees, calibrated
until the pre side reproduced D46's committed 4.801x on its own: L falls
28.5x (90523 to 3172 nonzeros), the whole factor 7.9x, the fill ratio 4.801
to 1.457, on a model that shrank 31%. The refactorization cadence is
unchanged. That is where the 15.7x per-iteration drop lives, and the fact
now sits in §5's factorization item. Readings in
`bench/measurements/02-17/`.

## 2. Presolve makes `grow22` and `grow7` far worse — closed by D112 (opened by D103)

Not a regression from D103 — the records either side of it are bit-identical —
so this arrived with presolve and has never been asked about.

```
grow22   4.4e7 -> 4.9e8 work   11.16x    2179 -> 16381 iterations   7.51x
grow7    6.4e6 -> 5.5e7 work    8.56x     544 ->  4804 iterations   8.82x
```

They are the only two instances of 94 past 2x, and they set the standard set's
worst case while its geometric mean is 0.810x.

**Measured, in `bench/measurements/02-11/`.** One family fires on them and
nothing else: `JM_PS_SINGLETON_COL`, the cost-0 bounded singleton column
(D95), exactly 20 times on each of `grow7`, `grow15` and `grow22`. So it is
the whole of the difference, and `-DJAOS_NO_PRESOLVE` already measured the
other side.

What the 20 firings do is not visible in the `presolve=` field, which reports
20 columns and 20 nonzeros of 8252 and no rows at all. Every one of the 20
rows is an **equality `== 0`** and every one becomes a **range of up to
5e5**. Twenty exact pins become twenty things that constrain nothing in
practice, and a dual simplex steers by those pins. None of the 60 records
leaves a row unconstrained on both sides, so a check for "did this row become
free" would miss it: the damage is the width.

**Closed 2026-08-17 by D112, on the candidate rule's own counter
(`bench/measurements/02-19/`).** The widening distribution over the
standard set: 8617 firings on 60 instances, 94% on equality rows, 98.6%
past the row's own scale — the "unbounded relative widening" the rule would
refuse is the family's normal act, so the rule is the family's off switch.
And the discriminator does not discriminate: `grow7`, `grow15` and
`grow22` carry the same maximum widening, 5.524e5, and one is helped while
two are hurt. Refused; the reopen condition (a mechanism that predicts
trajectory direction from the firing site) is shared with D108 and is in
the refusals table. `grow22` and `grow7` stand as the set's worst cases
against the reference build, below the gate's own bar.

## 3. Doubleton equalities: a family nobody has counted (opened by D104)

An `==` row with exactly two entries. One variable is substituted out, the row
goes, and every nonzero the substituted column had goes with it. It is not one
of the five live families and not one of the three D101 deferred, so no
measurement in this repository has ever counted it. On the model as loaded:

| set | doubleton equality rows | of all rows | instances carrying one |
|---|---|---|---|
| netlib | 6504 | **7.53%** | 67 of 94 |
| Kennington | 72459 | **28.15%** | 12 of 16 |

`ken-18` alone carries 48276, and it is the slowest instance in that set.
Against D101's 0.15% for the three deferred families this is fifty to a
hundred and ninety times larger, which makes it a different proposition rather
than a fourth deferral.

**Measured at presolve's exit, and that is where it stops being simple.**
The five live families barely touch these rows — 6504 to 6153 on netlib,
72459 to 60382 on Kennington — so the population survives to where a sixth
family would run. But a doubleton is substituted by eliminating one endpoint,
and unless that endpoint is free its bounds must be transferred onto the
survivor, which is bound tightening.

| set | surviving | share of live rows | with a free endpoint |
|---|---|---|---|
| netlib | 6153 | 8.55% | **19** |
| Kennington | 60382 | 29.36% | **0** |

**99.7% of the family is behind D97**, which refused bound tightening in six
designs, every one returning INFEASIBLE on a model that has an optimum. What
is buildable today is 19 rows across six netlib instances.

So this is not "build doubletons next". It is that **the prize behind D97 is
much larger than D97 knew**: it weighed bound tightening on its own, and this
is a second family that cannot exist without it. D97's reopen condition
already says what would settle it — derive the over-tightening on
`pilot`/`pilot87`/`agg`/`maros`, and have a dual postsolve for an imposed
bound. That work now buys two families instead of one.

**And a third, larger still (D113, 2026-08-17):** `stocfor3`'s rule
ablation says HiGHS's aggregator — equality substitution at any degree,
which needs the same bound transfer — alone owns the iteration half of the
worst instance in the comparison (6404 vs 14788 iterations with it off,
against JAOS's 18431). D97's condition is unchanged; its prize now
includes `stocfor3` whole.

The remaining cost question is unchanged and still unmeasured: a substitution
adds the eliminated column's terms into the survivor's rows, so it creates
fill. `subnz` says a non-interacting pass would remove 2.65% of netlib's
nonzeros and 5.13% of Kennington's, against row shares three times larger,
and neither figure accounts for fill.

## 4. A fourth instance set — the conclusions are population-dependent and say so

Every verdict in this repository is taken on 139 models: 94 netlib standard,
16 Kennington, 29 netlib infeasible. Netlib is a 1980s collection and it is
small. Three entries already in the record say the population is doing more
of the deciding than it should:

- **D46**: two instances are **74%** of the standard set's total work. A set
  total is a statement about `maros-r7` and `dfl001`.
- **D101** deferred three families because they remove 0.15% *on these 139
  models*, and its reopen condition is written as a model population, not as
  an opinion. A fourth set is the executable form of that condition.
- **§1's own counter** reads 3315 rows on netlib and **0 on Kennington**. The
  two sets already disagree about what is worth building.
- **HiGHS says it in print, with its own numbers** (Galabova 2023, §3.7, read
  2026-08-17): "Most problems in the classic Netlib test set are too small to
  be of interest", and presolve's geometric-mean speed-up is **1.10 on netlib
  against 1.67** on a 74-problem set built from Mittelmann's benchmarks plus
  four industrial models. Same code, same measure, the population alone moves
  the verdict by 52%. That is the strongest argument in this section and it
  did not come from here.

**What blocks it, and how Kennington already solved it.** netlib has
published exact rational optima (Koch), which is what lets the gate say
`objective=ok`. A modern set has no such reference. Kennington answers that
by entering at a lower tier — `bench/README.md` calls it "the same, for
correctness only" — and a fourth set enters the same way: checker verdict,
solution digest, determinism and work units, no reference objective.

**The blocker expired and nobody noticed.** This section said "Not before §1
closes". §1 closed 2026-08-17 (D107, D108, D109, D110, D111) and §2 closed
with D112. Four refusals in the table below name a model population as their
reopen condition, and **three of them already have their script written**:
D101 (`bench/measurements/02-07/`), D107
(`02-13/run-sign-count.sh`), D109 (`02-16/run-floorless.sh`). D107's entry even
names this section as the standing candidate. That is D24's pattern for the
fourth time.

### Sizes read 2026-08-17, before fetching anything

| where JAOS is now | rows | cols |
|---|---|---|
| `stocfor3`, biggest of netlib standard | 16675 | 15695 |
| `ken-18`, biggest of Kennington | 105127 | 154699 |
| `pds-20`, largest pds JAOS carries | 33874 | 105728 |

**Mittelmann's LPopt is the right set to aim at and the wrong one to adopt
today** (`plato.asu.edu/ftp/lpopt.html`, 1 Jul 2026, read at HEAD). Its
smallest instance, `qap15`, is 6331 × 22275 with 110700 nonzeros — already
3x `dfl001`'s nonzero count. The median is around 1.5M nonzeros and `dlr2` is
7.1M rows × 38.9M cols × 78M nonzeros. On that set HiGHS solves 54 with a
shifted geometric-mean runtime of 494 s, and **SoPlex solves 31**. JAOS reads
0.95x SoPlex and 3.15x HiGHS per solve at P0, so it would time out on most of
it. Sixteen of the instances are undisclosed in any case.

**The step that is actually available**, from the same host, all
`emps`-packed the way Kennington already is:

| family | instances | compressed | note |
|---|---|---|---|
| `pds/` | pds-30 … pds-100 (9) | 848K–4.4M | **the same family JAOS already carries** at pds-02…pds-20; `pds-100` is 156244 × 505360 |
| `fome/` | fome11, 12, 13, 21 | 310K–1.6M | `fome13` is 48569 × 97840 |
| `nug/` | nug08-3rd, nug20, nug30 | 325K–3.3M | QAP lower bounds |
| `rail/` | rail507, 516, 582, 2586, 4284 | 285K–8.0M | set covering; `rail4284` is 4284 × 1092610 |
| `fctp/` | 30 instances | 2.2K–111K | **too small — netlib's own problem again** |
| `network/` | 10 instances | 3.4M–164M | too big for now |

So the first fourth set is `pds-30…pds-100` plus `fome` plus `nug`, which
walks the size up by a factor along a family already in the tree instead of
leaping. Twenty-odd instances.

**Licence, settled.** No licence statement exists on
`plato.asu.edu/ftp/lptestset/`; its `00README` gives origins and citations
only. The position is the one this repository already takes for netlib and
Kennington, unchanged: fetch at build time, pin by sha256, **never
redistribute**. The three instance directories are in `.gitignore` beside the
other three.

**The fetch path exists and is proved, 2026-08-17.** `bench/fetch.sh` gained a
`bz2-emps` mode (plato serves netlib's own emps packing, bzip2'd instead of
gzip'd — three lines, not a second pipeline). Run against
`bench/plato-fome.manifest`: `verified 4, already present 0, failed 0`, exit 0,
and the cached re-run reads `already present 4`. The three manifests are
pinned and their dimensions cross-check against Mittelmann's published size
table, agreeing on every column count with the same one-row objective offset
the Kennington manifest already documents — except the three `nug` instances,
which show no offset and are recorded unexplained.

| manifest | instances | largest |
|---|---|---|
| `bench/plato-pds.manifest` | 8 | `pds-100`, 156243 × 505360, 54.5 MB expanded |
| `bench/plato-fome.manifest` | 4 | `fome21`, 67748 × 211456 |
| `bench/plato-nug.manifest` | 3 | `nug30`, 52260 × 379350, 58.6 MB expanded |

`fome11 → fome12 → fome13` doubles exactly in both dimensions, which is the
one family here that can say whether a cost grows linearly or worse with
nothing else about the model changing. `nug` is the shape the tree does not
have: every model JAOS reads today is economic, transport or stochastic, and a
QAP relaxation is none of those.

**The runner can read the set, 2026-08-17.** `bench/run.c` gained a third
expectation, `EXPECT_OPTIMAL_NOREF` (`-e noref`), and six `make plato-*`
targets exist. Shape, checker verdict, determinism, digest and work units are
asked exactly as they are for the other sets; only the comparison against a
published optimum is gone, and it prints `objective=none` rather than `ok` so
the two cannot be read as the same thing.

Built with the case it has to reject, in **both** directions
(`bench/measurements/02-22/reject-case.sh`, all four as expected): a `none`
manifest under `-e optimal` exits 2, and — the direction that matters, because
it is the silent one — `netlib.manifest` under `-e noref` also exits 2 rather
than quietly ceasing to check Koch's optima. `make test` and `make sanitize`
both exit 0.

**`plato` is not part of the gate**, and the three `netlib*` targets still are.

**The set is measured (D115).** Every figure is in
`bench/measurements/02-23/`, and `ladder.py` there derives them from the
manifests and the baselines rather than restating them.

### 4a. The first thing it found — closed 2026-08-18 by D117: another family takes them first

`bench/measurements/02-25/` asked why D106 fires on none of `fome`'s 166 / 332
/ 664 candidates and ruled out the margin with a canary that moves.
`bench/measurements/02-26/` answered it with a decline reader compiled into a
copy of `src/presolve.c`: **`JM_PS_SINGLETON_COL` takes 100% of them in round
0**, from a branch above D106's in the same column pass. The frozen row, this
file's leading suspect, declines 2 candidates over the whole standard set.

Three calibrations passed before any new number was read — `maros-r7` at 984
candidates and 980 firings, netlib at 3321 and 02-12's 8639 rows removed, and
zero candidates left disagreeing with the code. The 1353 the margin declines
reproduces D109's own figure, which nobody asked it to.

That closes §4a and opens §4b, which is the header of this file.

### 4b. Should D106 be preferred over D95 — closed 2026-08-18 by D118: refused

Built and measured, `bench/measurements/02-27/`. One branch moved, nothing
else. The footprint was exactly what D117's read-only counter predicted, nine
instances for nine: `ganges` 12, `czprob` 11, `dfl001` 9, `pilotnov` 7,
`pilot-ja` 7, `perold` 6, `seba` 1, `scrs8` 1, `d2q06c` 1.

It buys `ganges` 0.8429x, `dfl001` 0.8951x and `czprob` 0.9227x, and it makes
`pilotnov` publish an objective 29% wrong as `optimal`. Refused on that alone.
Geometric mean 1.0358x over 94; `netlib-infeas` and `netlib-kennington` both
`0 regressed, 0 improved, 0 new`.

The reopen condition is a fifth restriction on D106 that declines `pilotnov`'s
seven, which is §4c above.

### 4c. Why D106 answers `pilotnov` wrong — closed 2026-08-18 by D119: it does not

Opened by D118 and answered the same day, `bench/measurements/02-28/`. The
wrong answer is **numerical, not structural**. The same reduced model reaches
Koch's published optimum to the last bit at `REFACTOR_EVERY = 16`, and costs
1.032x HEAD at 8 against 30.2x at the shipping 64. D106's substitution on those
seven columns is sound and presolve cut off nothing.

What the candidate found instead is in §5a, at the top of this file: the
termination test never re-reads dual feasibility, so a numerically damaged
solve publishes `optimal`. That is HEAD's, not the candidate's.

**Also open:** how often `plato` should run — `pds` alone is 6.4 hours of wall
clock — and `nug20`/`nug30`, which are unmeasured rather than unsolvable.
`nug` also turns out to have **no row removed by any family** on all three
instances (`bench/measurements/02-26/counts/nug.txt`), which is its own
question and is not asked anywhere yet.

## 5. After presolve — the rest of M2, in order

### 5a. 186 loans go missing, and nothing bounds one — OPEN

Opened by D119, narrowed by D120, located by D121 and half-repaired by D122,
all on 2026-08-18 (`bench/measurements/02-29/` and `02-30/`). The repaired half
and the two that are left are in the header of this file. It sits first in this
section because it is a correctness question and everything below it is a speed
one.

Five explanations are closed by measurement and should not be re-derived: the
solver not re-reading dual feasibility (it does, six re-entry rounds, its own
violation zero), the basics' assumed-zero reduced costs (recomputed, worst
3.82e-14), a column resting on a bound phase 1 lent (72 lent, 0 resting), the
reduced model differing between refactorization intervals (identical, family by
family), and the carried `x_B` (drifts 7.22e-10, four orders too small).


- **Factorization** (REQ-lu-fill-and-markowitz, REQ-hyper-sparse-downstream):
  the stale live counts Markowitz chooses on, and the fill — factors carry
  2.673x the basis nonzeros set-wide (D46, which predates D106; `maros-r7`
  read 4.801x before D106 and 1.457x after it, D110). Measured at HEAD with
  D110's instrument: `stocfor3` reads **1.036** — no fill at all, so its
  30.0x against HiGHS belongs entirely to the presolve item below — and the
  live fill cases are `pilot87` at 3.610 and `pilot` at 3.261, the same two
  instances the fewer-iterations half of the split names
  (`bench/measurements/02-17/`); keep sparse triangular
  results sparse downstream (`stocfor3`: 6.79x per iteration, solves 43%,
  memset/memcpy/malloc 18.8% against 11.3% on `dfl001`). Left-looking
  elimination is a rewrite and needs its own decision first. Struck off by
  measurement, do not re-cost: `compact_pivot_row`'s row-to-position lookup
  (<0.5% on `maros-r7`); per-column arrays vs one arena (0.73% + 0.30%; the
  locality argument needs a cache simulation before it is costed or dropped).
- **Search path** (REQ-devex-pricing — acceptance stated: full gate with
  iteration count and per-iteration cost reported separately;
  REQ-reentry-oscillation — investigative first: 0.24% on `pilot87` at
  interval 24, D51 names the mechanism, D74 closed the only proposed cure,
  D89 removed the consequence).
- **Close M2** (REQ-m2-competitive-gate): needs a controlled host — D17 says
  a WSL number cannot close a gate, and this machine is Windows/WSL with a
  measured repeatability of 6.27% (D93). The per-instance guard factor is
  unset and is measured, not guessed. The ladder is recalibrated and the
  question is closed: **P0** is the rung, presolve on both sides, and T0 keeps
  its definition and its record as a historical rung
  (`bench/compare/README.md`). Standing at P0, re-taken 2026-08-17 after
  D106: **3.15x HiGHS, 0.95x SoPlex, 2.57x Clp**, on 2.04x / 1.51x / 1.95x
  the cost of an iteration. The per-iteration figure is what M2 is aimed at:
  three independently written dual simplexes put JAOS's iteration between
  1.5x and 2.0x theirs.

- **`stocfor3`'s presolve gap is counted, and it is the aggregator (D113,
  `bench/measurements/02-20/`).** Rule ablation on HiGHS at the P0 options:
  turning off its Aggregator collapses the 8416-row reduction to 2859 and
  lifts it from 6404 to 14788 iterations (2.31x) — against JAOS's 18431,
  near parity. Eleven of twelve other rules change nothing; the doubleton
  rule is subsumed by the aggregator. So the iteration half of `stocfor3`'s
  30.0x is equality substitution at any degree, and that machinery is
  behind D97 (§3), which now carries three prizes. The `maros-r7` half of
  this item closed with D106 (1.33x HiGHS).

## 6. After M2 — feature expansion (decided 2026-08-13)

Two decisions are locked: the two premises are absolute (no external code,
bit-identical everywhere; a feature that cannot be built under them is not
built), and the goal is the best open solver that is deterministic across
machines and ships its own checker — not matching Gurobi.
`docs/feature-matrix.md` is the scoreboard; read it at every close. Whether
M2 finishes as scoped is answered when presolve closes.

Proposed order: cheap breadth first (write MPS, write LP, write a solution
file, Python bindings, sensitivity and ranging, infeasibility certificates),
then primal simplex, then barrier with crossover, then MILP, then
QP/conic/NLP/MINLP. VIPR-format certificates are a cheap differentiator —
only SCIP 10.0 emits them and JAOS already ships a checker. For exact
rational verification, GMP is excluded (D11); the methods to weigh are
iterative refinement, interval arithmetic in double, or hand-rolled
rationals for the final basis only.

## 7. Presolve is closed — what that means

**REQ-presolve is done.** Six families live, three deferred on a count with
an executable reopen condition (D101), the postsolve defect closed in both
halves (D99, D100), an infeasible model no longer published OPTIMAL (D102),
the sense and window defects repaired (D103), and a removed column now paying
every row it touches (D106). `jaos-measurer` returned **ACCEPT** on D103 from
its own binaries; D106 was judged on its own campaign and its own sweep, with
`numerics-reviewer` on the diff. The three baselines have been rewritten
deliberately after each and confirmed by a following gate run: all three read
`0 regressed, 0 improved, 0 new` and exit 0.

Nothing in the sections above is presolve being unfinished. Every one is a
question presolve's own measurements raised, and every one has its number
already.

## Refusals and deferrals — what would reopen each

A refusal is a measurement, and a measurement is valid while its premises
hold. D24's reason expired when presolve landed, and it was caught by an
accident rather than a checklist (D94). This table is the checklist: when a
change satisfies a condition in the right column, re-ask that question. Until
then, do not — a refusal whose premise has not changed just fails again.

| decision | what was refused or deferred | reopens when |
|---|---|---|
| D101 | duplicate rows, duplicate columns, dominated columns — 0.15% left to remove on these 139 models | a model population where `bench/measurements/02-07/`'s counter reports a non-trivial share. The condition is executable, not a matter of opinion. Three pieces of the work have no published source and would have to be derived with their own tests |
| D97 | bound tightening — INFEASIBLE on models with an optimum, six designs | **first precondition met 2026-08-17 (D114)**: the over-tightening is derived — a forcing window scaled by the activity certified 5.86 of slack as zero, and the design requirements for a retry are in `bench/measurements/02-21/`. What remains: a dual postsolve for an imposed bound; then only under a campaign. **The condition is unchanged and the prize is not**: doubleton substitution needs the same machinery, and it is 8.55% of netlib's live rows and 29.36% of Kennington's, of which 19 rows in total can be built without it (§3). D97 weighed this feature alone; it now unlocks two |
| SPECS §3 | crash basis — destroys the exact slack-basis steepest-edge weights | pricing stops starting from exact steepest-edge weights; REQ-devex-pricing landing is the trigger |
| D74 | removing the re-entry loan — 2.372x `pilot87` iterations for 0.980x `pilot` | the oscillation mechanism itself changes (phase 4's investigation) |
| D63 | restarting weights to exact instead of 1.0 | the pricing rule changes; Devex would replace the question |
| D107 | the inequality implied free column singleton — 341 sign-ok rows, 10% of the count, 304 of them on `ship*` instances below the harness floor, zero on `stocfor3` and Kennington | a model population where `bench/measurements/02-13/run-sign-count.sh` reports a non-trivial sign-ok share. **Asked of §4's fourth set 2026-08-18 and NOT satisfied** (`bench/measurements/02-25/`): zero inequality candidates across all fifteen instances, on 02-13's own instrument with both its calibrations reproduced. The refusal now stands on 154 models across four sets and a 53x range in rows, so the population is no longer the objection to it |
| D108 | a refuse rule for the implied free column singleton on trajectory grounds — `greenbeb` and `scfxm3` pay through different machinery, both downstream of an exact reduction, and no site-local predictor exists | an instance crosses the gate's 2.0x work bar from this family's firings, or a measured mechanism predicts trajectory direction from the reduction site |
| D109 | removing the implied-free window's `max(1, scale)` floor — a bit-identical no-op over all 94 standard instances, digests included | a model population where `bench/measurements/02-16/run-floorless.sh` reports a moved instance line; or the D106 sweep's own reopen |
| D112 | the unbounded-relative-widening refusal for the cost-0 singleton column — 98.6% of firings would be refused, and the helped and hurt `grow*` instances carry the same widening | D108's condition: a measured mechanism that predicts trajectory direction from the firing site; or an instance crossing the gate's 2.0x work bar from this family |
| D95 | eliminating nonzero-cost singleton columns | a dual-informed elimination design exists (the lift condition is in the entry). **Checked against D106 and NOT reopened, deliberately.** D106 eliminates nonzero-cost singleton columns, so the question was re-asked. It does not satisfy D95's condition and does not need to: D95 refused *choosing which bound is optimal*, and an implied free column has no bound to choose — it is interior, so `d_j = 0` is forced and the dual falls out of one division. The columns D95 still refuses are the ones whose own bounds can bind, and D106 declines exactly those |
| D118 | giving the implied free column singleton first refusal over D95's bounded cost-0 singleton column — `pilotnov` publishes an objective 29% wrong as `optimal`, `checker=REJECTED`, `dual=0.89`, 30.2x work | **the condition was rewritten the same day by D119, because the first one looked in the wrong place.** It is not a fifth restriction on D106: the substitution is sound, and the same reduced model reaches Koch's optimum to the last bit at `REFACTOR_EVERY = 16`. It reopens when the solve stops publishing `optimal` without re-reading dual feasibility, or when the refactorization interval stops collapsing on `pilotnov` — §5a, both. The prize is real and stated: `ganges` 0.8429x, `dfl001` 0.8951x, `czprob` 0.9227x |
| D93 | the 4.2% time bar — unmeasurable on this host | a controlled host that satisfies D17 |
| D92/backlog | `pilot87`'s suboptimality bound, not understood | it blocks a gate (trigger already recorded) |
| D82, D84 | partial and multiple pricing | nothing scheduled — refused on wrong answers, not on a trade; a new scheme is a new decision, not a retry |
| D34, D11, D2 | `long double`, GMP, any external code | never, while the two absolute premises stand (locked 2026-08-13) |

## Standing debts — small, real, none blocks the sections above

- ~~`preflight.sh` does not check committed records for `baseline: NOT
  COMPARED`.~~ **Done 2026-08-18.** It reads the line D93 said nobody read:
  STOP when a committed record carries it, WARN when only the working-tree
  copy does. Validated against the case it must reject, in a worktree — clean
  tree clear, working-tree copy WARN and exit 0, committed copy STOP and exit
  1.
- The `REFACTOR_EVERY` 16..256 trajectory sweep is manual; three of M1's four
  defect closures came from it and no target automates it. **D119 is the
  fourth** — `pilotnov` under D118's candidate is right at 16 and 29% wrong at
  the shipping 64, on the same reduced model
  (`bench/measurements/02-28/sweep-refactor.txt`).
- Test ceilings drift silently — the `<62000` one drifted 2800 units with
  nothing watching. Re-measure a ceiling's both sides when touching its
  subject.
- `pilot87`'s suboptimality bound is not understood (`gap_positive` moves
  0.0068–26.7 across D92's variants while every answer is inside tolerance).
  Deferred with a trigger: it re-enters the plan if it blocks a gate, and it
  already refused two of D92's three candidate repairs.
- Restricting the candidate set ahead of `bfrt_walk`/`jm_harris_pick` is open
  and not refused (D93); it puts Harris's guarantees at stake and needs its
  own decision before any code.
- `galenet` makes two `dual_ratio_test` calls in a one-iteration solve —
  calls are not iterations in any work-saved arithmetic (D93).
- **The `warm` record predates presolve.** `bench/results/warm.txt` and its
  Kennington sibling were last written at `44c0ef6`, an 01-03 commit, so a
  diff against them reports the whole of presolve and cannot isolate a later
  change. It read 92 of 98 instances moved and flagged `scrs8` as a
  regression, where the movement against HEAD was five lines and no
  regression. Rewriting it is the same deliberate act as rewriting a gate
  baseline, with the same precondition.
  **It is watched now, and it was not.** `preflight.sh` asks every record in
  `bench/results/` how many `src/` commits it was written before, and reads
  **21** for both warm records. It only ever asked the three netlib ones, which
  is why nobody noticed — and asking all of them turned up two more straight
  away: `netlib-infeas.txt` at 3 and `netlib-kennington.txt` at 7, both gate
  records. Found by `jaos-measurer` hitting the `scrs8` line again while
  judging D122, 2026-08-18. The count is not a verdict: a record written before
  N commits is still valid if those commits were no-ops on that set, which is
  why the baselines being behind is correct rather than stale.
- **A collapsed fold leaves a bound no record owns.** When a singleton row's
  intersection collapses inside the fold's rounding window, `src/presolve.c` puts
  the midpoint of the two ends into both folded bounds, and that midpoint is
  no row's implied bound. The record that collapsed carries it and can still
  be paid; the record that produced the other side keeps its own value and
  compares unequal for ever. When the reduced cost's sign points at that
  other side, no record pays and the cost is left on a column strictly inside
  its own box. `min x0 + x1 + x2 s.t. x0 >= 5, x0 <= 5 - 1e-13, x1 + x2 >= 3,
  x0 in [0, 10]` publishes `x0 = 4.9999999999999503` with
  `max_dual_violation = 1`. **Not a regression**: the pre-fix code refuses it
  by the same magnitude on row 1 rather than on the column, measured on both
  trees. The repair is a decision about what a collapsed record should record,
  not a patch — the midpoint is deliberate and symmetric in the two ends, and
  whatever replaces it has to keep that. Found by `numerics-reviewer` and
  re-run independently, 2026-08-14.
  **D103 gave this a stated size, and it is not bounded.** The midpoint is
  unclamped, so the published value can sit up to half the window outside a
  bound the caller stated: `4 * DBL_EPSILON * row_traffic[i] / |a|`. The
  traffic term is new with D103 and nothing caps it. A row whose fixed column
  left at `a*v = 1e9` and whose surviving singleton has `a = 1e-6` reads 0.89.
  `tests/test_presolve.c`'s `test_a_fold_onto_the_box_at_scale_still_collapses`
  documents the shape with the measured 2.4e-7 rather than asserting
  containment, and says in as many words that its own bound holds only because
  that model's traffic is zero.
- **The other half of `assert(want_lo <= want_hi)`: an empty intersection of
  an ulp.** D102 closed the half where the model really is infeasible. The
  half that remains is rounding: 11 of the 94 standard instances reach
  `ps_replay_one` with `want_lo` above `want_hi` by 2.2e-16 to 1.3e-15, and
  the replay publishes `want_lo`, which is that far outside the column's own
  box. `bnl1` row 581 wants 2.1850000000000005 from a column whose upper bound
  is 2.1850000000000001; the others are `finnis`, `80bau3b`, `bandm`, `cycle`,
  `dfl001`, `nesm`, `perold`, `pilot-ja`, `pilot`, `pilotnov`. The checker's
  tolerance absorbs it, so no answer is wrong today, but a declared bound is
  being published outside and the assert cannot be enabled while this stands.
  The repair is to clamp the published value into `[rec->lo, rec->hi]`, and it
  had to come after D102: clamping first would have masked the gap of 93 as
  though it were rounding. Measured 2026-08-14, readings in
  `bench/measurements/02-08/`.
  **The consequence, written down 2026-08-18 and never before: no
  assert-enabled build can run those eleven instances at all.** They abort at
  `src/presolve.c:2127`, so every assert in the solve is untested on them, and
  a campaign built with `EXTRA_CFLAGS=-UNDEBUG` covers 128 of the 139 rather
  than all of them. Found by `jaos-measurer` checking whether a newly added
  precondition ever fires; the parent aborts on the identical list, so this is
  the debt above and not a new defect. It raises the value of the clamp: it is
  not only a bound published outside its box, it is the reason a whole build
  configuration cannot be run.
- ~~**`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` is RED.**~~ **Repaired
  2026-08-18 and green for the first time.** The two tests were
  `test_singleton_col_between_two_removals_solved_path` (expected 3 basic, got
  2) and `test_a_fold_onto_the_box_at_scale_still_collapses` (expected
  OPTIMAL, got INFEASIBLE). Both now assert the reference build's **own**
  answer instead of being guarded out, because in both cases that answer is
  the right one and skipping would have thrown it away: 2 basic is what the
  model has, and `x0 >= 1e9 + 5e-7` against `x0 <= 1e9` really has no common
  point. Both tests run and pass in both builds; `make test`, the reference
  build and `make sanitize` all exit 0.
  **A second pin was added beside it the same day (D118).** The basis-count
  pin above only holds while D95 wins a race against the implied free family:
  under D118's refused candidate that model read the correct 2 and stopped
  detecting anything at all. `test_the_basis_count_promise_breaks_on_a_declined_column`
  pins the same defect on a column the implied free family declines by
  **margin** rather than by order, so no change of order can retire it
  quietly. Found by `numerics-reviewer`.

- **Two positive tests had no fault-build guard.** `make test
  EXTRA_CFLAGS=-DJAOS_PRESOLVE_FAULT_OFFBYONE` did not compile at all until
  2026-08-15 (`make_frozen_row_infeasible_model`, fixed), and once it did,
  `test_a_maximised_singleton_row_is_owed_its_multiplier` and
  `test_a_maximised_empty_column_takes_its_upper_bound` failed because a
  fault build is meant to break exactly what they assert. Both now carry the
  guard every other positive test in the file has. **All seven off-by-one
  negative tests pass**, which is the first time that has been true since the
  compile broke.

- **A row's own width can be destroyed by the shift that removes a column,
  and no family is involved.** `cur_rl[i]` and `cur_ru[i]` are running
  differences, so a row the caller wrote as `[1, 2]` reads as a single number
  once a fixed column takes 1e17 off both: `ulp(1e17)` is 16, and `1 - 1e17`
  and `2 - 1e17` are the same double. The row handed to the simplex has lost
  its own width, and the answer is wrong by up to that width whatever
  presolve does next. `row_traffic[i]` is exactly the quantity that would say
  when this has happened and no site compares against it for this purpose.
  The implied free column singleton now declines such a row
  (`test_a_range_row_that_shifted_into_an_equality_is_declined`), which keeps
  that family inside its measured scope and does not repair this. Found by
  `numerics-reviewer`, 2026-08-15.

- **Two assignments in the replay are correct by an argument no assert
  states.** `JM_PS_EMPTY_ROW` writes `sol_row[i] = 0.0` and
  `JM_PS_SINGLETON_ROW` writes `sol_row[i] = rec->coef * xv`, where every
  other producer accumulates. Both hold today: an empty row had every column
  dead before it fired, and a singleton row had exactly one live column, so
  neither can be overwriting a share that already arrived. Both depend on
  arena order rather than on anything checked, and this class has now cost one
  campaign (D106). The cheap enforcement `numerics-reviewer` proposes is one
  debug-build pass at the end of `jm_postsolve_expand`: recompute every row's
  activity from `sol_col` and assert it matches `sol_row`. That is the
  predicate the checker already applies, extended from the instances that
  reach it to all of them.

- **`row_traffic[i]` saturates to `+inf` and never recovers.** The relaxation
  at `src/presolve.c` adds `max(|cmax|, |cmin|)` to it, and a column with a
  half-infinite box makes that infinite. Measured: all 117 standard-set rows
  that reach the frozen-row test at exactly zero margin carry
  `row_traffic == inf`. What should accumulate is the finite part actually
  subtracted from `cur_rl`/`cur_ru`. Fixing it changes the frozen-row test's
  behaviour on existing reductions, so it needs its own measurement rather
  than riding along. Found by `numerics-reviewer`, refused as out of scope for
  D102 with that reason.
  **D103 did not change its severity and the source now says why.** The two
  sites that read the traffic guard against an infinite value, and both guards
  are unreachable: the only site that can saturate it sets `row_frozen[i]`
  four lines later, `row_frozen` is never cleared, and the round loop's row
  pass skips a frozen row. They are guards against a sixth family that relaxes
  a row without freezing it, and they are labelled as that in the file.
- **The basis the singleton-column family publishes breaks the count
  promise.** `jaos.h` promises exactly `num_row` of the `num_col + num_row`
  statuses are basic. It does not hold when the replay recovers the column
  strictly inside its own box: the column is published basic, and so is the
  row it was relaxed out of, which is one basic too many. Minimum case, on
  the `jm_postsolve_expand` path: `min x0 s.t. x0 + x1 = 7, x0 in [0,20],
  x1 in [0,100] cost 0` publishes 2 basic against `num_row = 1`, where
  `-DJAOS_NO_PRESOLVE` publishes row0 `AT_LOWER` and 1. **That model is
  `test_the_basis_count_promise_breaks_on_a_declined_column`, added 2026-08-18,
  and the repair announces itself there** — expect its 2 to become 1. The
  two-row `jm_postsolve_solved` model in
  `test_singleton_col_between_two_removals_solved_path` publishes 3 against 2
  and pins that too, but it holds only while D95 wins a race against the
  implied free family: D118's refused candidate gave it to the implied free
  family and the test stopped detecting anything. The minimum case cannot lose
  that race, because the implied free family declines it on the **margin** (its
  implied box is `[-13, 7]`, below x1's own lower bound of 0). When the
  column lands on its own bound instead, the count is right —
  `make_singleton_col_model` is that case. So the discriminator is which
  bound determined the value, which `ps_replay_one` has already computed
  when it picks `want_lo`; `JM_PS_FREE_COL_SINGLETON` derives its row's
  status as the mirror of its column's, with the row-count argument written
  beside it (`src/presolve.c` 1619-1634), and that is the pattern. Cost is a
  lost warm start, not a wrong answer — `build_warm_basis` falls back to cold
  when the count does not hold, and no checker or digest reads a status — so
  the repair is measured on `make warm` and `make warm-kennington`, which is
  what it changes.
