# The remember-basis guard is refused, and the warm re-measure finds D138/D139 cost netlib's warm ratio 3.7x through the mapping

Taken 2026-08-19. One review finding opened both questions; two probes and
four warm campaigns closed them. Closed as D142 (the guard) and D143 (the
warm regression and its mechanism).

## The guard candidate, and why it is refused (D142)

TODO item 2 asked for a count check in `jm_model_remember_basis`, on the
premise that "a stored basis failing the count is already rejected by
`build_warm_basis`", so refusing to store one changes nothing measurable.
The candidate was built, its reject-case test validated to fail on the
unguarded tree, and `numerics-reviewer` (delivering for the second time in
this run) refuted the premise by reading: `build_warm_basis`
(`src/simplex.c:923`) counts the MAPPED basis on the reduced model, and
`jm_presolve_run`'s mapping (`src/presolve.c:1651`) drops every stored
member whose row or column presolve removes. A basis wrong in orig space
can map exact and warm-start.

Measured, warm campaigns on both trees (`guard-cand-warm.txt` beside this
file against `bench/results/warm.txt`): the guard costs exactly two netlib
instances their warm start — `capri` 1 → 273 iterations (12.3x work) and
`fffff800` 7 → 945 (127x) — and moves the netlib warm work geomean 0.2553 →
0.2766. Kennington is bit-identical. Those two are the probe's two
"orig wrong, mapped exact" solves, the only stored bases the guard clears
that the mapping was still rescuing. **Refused**: the honest invariant costs
two warm starts and buys nothing any consumer reads. The candidate diff is
kept at `remember-guard-candidate.diff`.

## The item-4 re-measure, and what it found (D143)

First re-measure of the `warm` records since D138/D139 landed. The records
were rewritten deliberately from these runs:

| | iterations geomean | work geomean | cold fallbacks |
|---|---|---|---|
| netlib, committed (pre-D138/D139) | 0.0250 | 0.0696 | 23 of 92 (D129) |
| netlib, **now** | 0.1381 | **0.2553** | **54 of 88 mapped** |
| Kennington, committed | 0.0318 | 0.0873 | 6 of 11 (D129) |
| Kennington, **now** | — | **0.0572** | 5 of 11 |

netlib pays 3.7x on the work geomean; a dozen-plus instances that
warm-started in 0–6 iterations (`25fv47`, `adlittle`, `bandm`, `blend`,
`bnl1`, `bnl2`, `brandy`, `cycle`, `czprob`, `d2q06c`, `etamacro`, …) now
run warm equal to cold. Kennington improves.

## The mechanism, measured (`run-warm-mapping.sh`)

One line per warm solve that maps a stored basis, counting the orig-space
and the mapped basis:

| | netlib (88) | Kennington (11) |
|---|---|---|
| orig exact, mapped exact — warm runs | 32 | 6 |
| **orig exact, mapped SHORT — falls back cold** | **35** (worst shortfall **596**) | **5** (shortfall 1) |
| orig wrong, mapped exact — warm runs anyway | 2 | 0 |
| orig wrong, mapped wrong — falls back cold | 19 | 0 |

**The published basis is now right in orig space and wrong after the
mapping.** Pre-D138/D139 the error and the mapping cancelled: the extra
BASIC members sat exactly on rows and columns presolve removes again on the
re-solve, so the drop restored the count. Post-D139 the swap rests the
surviving row's logical on a bound (it maps through as nonbasic) while the
restored basic column is removed again (dropped, −1), so the mapped count
comes out short — by up to 596 members on one instance. 4 of the 92
measured netlib warm solves print no mapping line and are not attributed.

## What this opens, handed to `TODO.md`

The mapping owes the reverse of what postsolve's second pass writes: for a
`JM_PS_SINGLETON_COL` record whose stored column is BASIC and about to be
dropped, the surviving row's logical goes back to BASIC in the reduced
start — the exact mirror of `ps_singleton_col_swap`, with the same forced
pivot `a_ij`. The other families' balance has to be derived the same way
before anything is built. The prize is bounded by this measurement: up to
35 netlib and 5 Kennington warm starts.

## Reproducing it

`run-warm-mapping.sh` and its output beside this file; the warm records in
`bench/results/` are the campaigns' own output; the guard candidate and its
warm reading are beside this file. `src/` is read and never written by the
probe; the guard campaign applied `remember-guard-candidate.diff` in a
throwaway worktree.
