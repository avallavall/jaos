# stocfor3's presolve gap is the aggregator

The count `TODO.md` §5's presolve item asked for, taken 2026-08-17: what
HiGHS removes on `stocfor3` and which families do it. D113 is the decision
it closed. Method: HiGHS 1.15.1 at the P0 options with one presolve rule
suppressed per run, via the documented `presolve_rule_off` option
(`ablate.sh`, results in `ablation.txt`). Reading a competitor's log and
using its documented options is what the comparison harness already does;
no competitor source is read.

## The instrument's limit, found by its own calibration

The suppressible rules are 6 through 19. The base rules (0 through 5)
cannot be turned off, and the `maros-r7` calibration proves the limit:
its 984-row reduction — known from 02-10 to be the implied free column
singleton — is untouched by every suppression, so HiGHS runs that
elimination in its base rules where this instrument cannot see it.
Ablation attributes only what the suppressible rules do, marginally and
with interactions; it is not an additive decomposition.

## The reading

On `stocfor3`, eleven of the twelve single-rule ablations change nothing
at all. Two move:

| rule off | rows removed | iterations | against baseline |
|---|---|---|---|
| (none) | 8416 | 6404 | |
| Free col substitution | 8200 | 6727 | −216 rows, +5% iterations |
| Doubleton equation | 8407 | 6387 | −9 rows, noise |
| **Aggregator** | **2859** | **14788** | **−5557 rows, 2.31x iterations** |

The doubleton rule looks idle only because the aggregator subsumes it:
with the aggregator on, doubletons are aggregated before the doubleton
rule sees them. What carries `stocfor3` is equality substitution at any
degree — the aggregator — and it alone buys HiGHS 2.31x iterations there.

With the aggregator off, HiGHS needs 14788 iterations against JAOS's
18431: **1.25x, near parity**. So of `stocfor3`'s 30.0x at the P0 re-take,
the iteration half is almost entirely aggregation JAOS does not have, and
the remainder is the per-iteration cost the M2 split already tracks.

## Where this lands

Equality substitution beyond the free/implied-free cases needs bound
transfer onto the survivor, which is the machinery D97 refused six designs
of. §3 already said the doubleton population puts a second prize behind
D97 (28% of Kennington's rows); this count puts a third and larger one
there: the whole of the worst instance in the comparison. The reopen
condition D97 carries is unchanged — derive the over-tightening on its
four instances, and build a dual postsolve for an imposed bound — and what
it unlocks keeps growing.
