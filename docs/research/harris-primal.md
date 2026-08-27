# The primal two-pass ratio test, as published

The design note for the primal simplex (`primal-simplex.md`) reconstructed
Harris's two-pass test without reading the paper. This page records what a
`literature-scout` pass on 2026-08-27 could verify, what it could not, and
which values belong to whom. It exists because D211 measured the defect this
test repairs: `pilot87`'s primal phase 1 pivots on elements down to 1e-9 with
no preference for a larger one, and the basis goes nearly singular.

## What is confirmed, and by what

**Harris's primal form.** Hall and McKinnon (2004), read in full, state it in
section 1 and restate the mechanism in section 5.1 as the `tau = 0` case of
EXPAND:

> "Harris introduced the Devex row selection method, which allowed small
> violations of the constraints and used the resulting flexibility to choose
> the largest pivot. [...] The variable leaving the basis does not normally do
> so at one of its bounds, but is shifted to that value, resulting in
> inconsistent values for the basic variables. The method attempts to correct
> this inconsistency at regular intervals (usually after each reinversion) by
> doing a reset, in which the basic variable values are recalculated from the
> values of the nonbasic variables."

So, for entering column `alpha_q = B^-1 a_q`, direction `sigma` and basic
values `bbar`:

- **Pass 1.** Relax every basic bound outward by `delta`. For
  `sigma * alpha_iq > 0`, `t_i = (bbar_i - l_i + delta) / (sigma * alpha_iq)`;
  for `sigma * alpha_iq < 0`, `t_i = (bbar_i - u_i - delta) / (sigma * alpha_iq)`.
  `t1 = min t_i`, capped by the entering variable's own range.
- **Pass 2.** Among the rows whose **unrelaxed** ratio is at most `t1`, take
  the one with the largest `|alpha_iq|`. Its unrelaxed ratio is the step.
- **The overshoot.** The step can leave the leaving variable up to `delta`
  past its true bound, and can be negative. The leaving variable is **placed
  exactly on its bound**; the other basics are then inconsistent by that
  shift, and are recomputed from the nonbasics at the next refactorization.
  JAOS's `refresh` every `REFACTOR_EVERY` updates is that reset.

The two-sided-bound form above is a transposition of the one-sided text that
was read. Harris's own abstract (Springer page, read) says the same in one
sentence: relaxing the constraint makes "room for a further selection
criterion based on pivot size."

**EXPAND** (Gill, Murray, Saunders, Wright 1989), from Hall and McKinnon
section 5.1, verbatim: the tolerance grows by `tau` each iteration; pass 1
finds the largest step against the expanded bound; pass 2 takes the largest
pivot among rows whose true ratio fits; a minimum step `alpha_min = tau /
p_r` is enforced and the step is `max(alpha_min, alpha_full)`. Every `K`
iterations the tolerance resets and nonbasics off their bounds are put back.
Constants from the authors' own MINOS and SNOPT documentation (read):
`delta_i = delta_f / 2`, `tau = 0.5 * delta_f / K`, `K = 10000`,
`delta_f = 1e-6` at their default feasibility tolerance.

**Why a tiny pivot ruins the basis**, a derivation and not a citation:
`B_new^-1 = E B^-1` with `1/alpha_rq` on the diagonal of the eta column, so
every entry grows by factors of `|alpha_iq / alpha_rq|`; and
`det(B_new) = alpha_rq * det(B)`, so the Forrest-Tomlin update's new diagonal
is `|alpha_rq|` times the old one, which is why `LU_UPDATE_TOL` refuses the
pivot almost by identity. That is the three-line pattern in
`bench/measurements/02-126/`.

## What is not confirmed

- **Harris's own `delta`.** Text not reached. No value is carried from
  Harris (1973). The phase-1 argument below bounds it above by `PRIMAL_TOL`.
  What JAOS ships is the **ratio** `delta_f / 2`, and only the ratio. In the
  MINOS and SNOPT documentation above that is where EXPAND *starts* a
  tolerance which then grows by `tau` toward `delta_f` and resets every `K`.
  JAOS holds its width fixed at `PRIMAL_HARRIS_DELTA = 0.5` and carries no
  `tau`, no `K` and no reset, so this is one number borrowed from a method
  that is not implemented here — not GMSW section 3.2, which is where the
  two-pass test itself comes from. The seven-setting sweep behind the value
  is `bench/measurements/02-127/` and D213, and it does not choose the value:
  0.01 to 0.5 measures the same on the campaign and the gate does not move
  anywhere in 0 to 10.
- **A minimum pivot in pass 1.** Not in Hall and McKinnon's restatement. The
  MINOS/SNOPT line has an **absolute** pivot tolerance, 3.67e-11 (SNOPT
  documentation, read). Whether Harris 1973 has one is unverified.
- ~~Clamping a negative step to zero.~~ **Confirmed at source after the
  scout ran.** GMSW 1989 section 3.3, read from the Stanford SOL scan: "If
  α2 < 0, the Harris procedure sets α = 0 but retains the same blocking
  variable x_r, which then becomes nonbasic." The same section prices the
  snap: moving x_r onto its bound "is equivalent to performing an extra step
  [...] of order δ" in satisfying Ax = b, "eliminated each time the basis is
  refactorized", with "provision [...] to return to Phase 1 if the recomputed
  variables lie outside their bounds by more than δ." GMSW's own alternative,
  freezing the nonbasic at its infeasible value, is not open to JAOS: D24 and
  D189 judge the published point against the true bounds.
- **No pivot tolerance inside the ratio test in GMSW either.** Section 3
  says the textbook test "offers no mechanism for avoiding small pivots";
  the preference for a larger pivot *is* pass 2. Section 4.3's reset moves
  nonbasics within `δ_f` of a bound onto it and recomputes the basics.
- **Pass 2 inside a composite phase 1.** No source reached. What follows is
  a derivation.
- **A pivot floor relative to the column's largest entry**, D207's rule: no
  literature match found. The cited practice is absolute.

## Phase 1, derived

JAOS's phase 1 is Maros's composite short-step form. Three kinds of row meet
the ratio test:

1. A **feasible** basic reaching a bound: as in phase 2. Relaxed by `delta`,
   it ends at most `delta` outside. That still counts as feasible in phase 1
   only if `delta <= PRIMAL_TOL`, so **`delta` is bounded above by
   `PRIMAL_TOL`**. GMSW keep their tolerance under the feasibility tolerance
   for the same reason.
2. An **infeasible** basic travelling **back** to its violated bound: it
   blocks because the phase-1 objective's slope changes there, not because
   feasibility is threatened. Relaxing lets it travel `delta` into the
   feasible region, which is harmless; the predicted decrease is wrong by the
   slope change times the overshoot, bounded by `delta`.
3. An **infeasible** basic travelling **away**: never blocks. Nothing to
   relax.

Pass 2 asks about the pivot, not about feasibility, so "largest `|alpha|`
among rows whose exact breakpoint is at most `t1`" applies unchanged over
the phase-1 candidate set.

## What the implementation has to keep from the record

- D207: an emptied candidate set must not read as "no blocker". Pass 0's
  winner stands.
- D210: `PIVOT_MIN` never decides an `UNBOUNDED`.
- D189, D24: the published point is judged against the true bounds by an
  absolute test. The leaving variable is snapped onto its bound at every
  pivot, and the final refactorization recomputes the basics.
- Determinism: Harris is stateless; EXPAND needs one per-solve counter and
  a derived tolerance, reset at solve start.

## Citations, with what was reached

| source | verified | reached |
|---|---|---|
| Harris, "Pivot selection methods of the Devex LP code", Math. Prog. 5 (1973), DOI 10.1007/BF01580108 | Crossref, Springer page | abstract and reference list only |
| Gill, Murray, Saunders, Wright, "A practical anti-cycling procedure for linearly constrained optimization", Math. Prog. 45 (1989), DOI 10.1007/BF01589114 | Crossref | not by the scout; Stanford SOL scan saved to disk |
| Hall, McKinnon, "The simplest examples where the simplex method cycles and conditions where EXPAND fails to prevent cycling", Math. Prog. 100 (2004), DOI 10.1007/s10107-003-0488-1, arXiv:math/0012242 | Crossref, arXiv | **full text, via ar5iv** |
| Maros, *Computational Techniques of the Simplex Method*, Kluwer 2003, ch. 9, DOI 10.1007/978-1-4615-0257-9_9 | Crossref | chapter abstract only |
| Maros, "A general Phase-I method in linear programming", EJOR 23 (1986), DOI 10.1016/0377-2217(86)90215-8 | Crossref | not reached |
| GAMS documentation for MINOS and SNOPT | | read; the EXPAND authors' own user documentation |
| Nannicini, arXiv:1910.10649, section 5.7 on the two-pass test and conditioning | arXiv | read |

Not consulted, and not to be: any solver's source. The search surfaced
SoPlex documentation pages and a copy of Maros's book on a file-sharing site;
neither was opened.
