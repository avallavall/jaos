# Synthesis

Entry point for downstream consumers. Ingest of the JAOS planning record,
MODE `new`, manifest `.planning/ingest-manifest.yaml`. All source paths in the
intel files are relative to the repository root,
`C:/Users/vall-/Desktop/projectes/jaos`.

## Documents synthesized — 10 of 10

- **ADR — 1.** `DECISIONS.md`, precedence 0, **locked**.
- **SPEC — 1.** `SPECS.md`, precedence 1.
- **PRD — 1.** `PLAN.md`, precedence 2.
- **DOC — 7.** `docs/tolerances.md`, `docs/work-units.md`, `docs/scaling.md`, `docs/format-support.md` (precedence 3); `bench/README.md`, `bench/compare/README.md` (precedence 4); `README.md` (precedence 5).

All 10 classifications were manifest-overridden at high confidence, so no
document was typed by heuristic. `CHANGELOG.md` is deliberately outside the
ingest set per the manifest — history, not intent.

## Decisions — 92, all locked

`decisions.md`. D1 through D92, one entry each. Every entry is locked: the
source document's header states "Closed decisions only, with the measurement
that closed them", and the manifest sets its precedence to 0 because "a
requirement derived from any other document must not contradict one".

The document's own convention is that each heading *is* the decision rather
than a topic, so each `decision:` field is the heading verbatim plus the
operative content of the entry.

Eleven decisions supersede or reframe an earlier one, and every such
supersession is explicit in the source. **No LOCKED-vs-LOCKED contradiction was
found.**

## Requirements — 25

`requirements.md`. Derived from `PLAN.md`, with two entries whose content sits
elsewhere: `REQ-m2-competitive-gate` (`bench/compare/README.md`) and
`REQ-miplib-subsets` (`SPECS.md`).

Complete per the source, recorded so they are not re-planned:
`REQ-phase1-know-where-we-stand`, `REQ-phase2-make-it-usable`.

Open — 23: `REQ-presolve`, `REQ-write-mps`, `REQ-write-lp`,
`REQ-write-solution-file`, `REQ-sensitivity-and-ranging`,
`REQ-exportable-certificates`, `REQ-exact-rational-verification`,
`REQ-python-bindings`, `REQ-lu-fill-and-markowitz`,
`REQ-ratio-test-candidate-admission`, `REQ-hyper-sparse-downstream-results`,
`REQ-devex-pricing`, `REQ-primal-simplex`, `REQ-barrier-and-crossover`,
`REQ-deterministic-parallel-bnb`, `REQ-milp`, `REQ-qp-conic-nlp-minlp`,
`REQ-nlp-derivative-strategy`, `REQ-lp-mps-dialect-edges`,
`REQ-reentry-oscillation`, `REQ-pilot87-suboptimality-bound`,
`REQ-m2-competitive-gate`, `REQ-miplib-subsets`.

**Eighteen of the twenty-five carry `acceptance: absent`.** That is the source's
own state, not an extraction gap: `PLAN.md` is an ordered list of open work, and
most of its items say what to build without saying what would close it. Three
entries carry a `- note:` line pointing at a conflict; those are the ones not to
take at face value.

## Constraints — 29

`constraints.md`. By type: **nfr** 11, **protocol** 11, **api-contract** 4,
**schema** 3.

By source: `SPECS.md` 13, `bench/README.md` 6, `docs/tolerances.md` 4,
`bench/compare/README.md` 2, `docs/format-support.md` 2, `docs/scaling.md` 1,
`docs/work-units.md` 1.

**Why DOC paths appear in a constraints file.** The manifest types the
acceptance gate and the reference documents as DOC, but their content is not
background — `bench/README.md` defines the gate every future change is judged
by, and this project's rule is that a stated number carries a measurement and is
therefore binding. Each entry keeps its true source path, so precedence can
still be applied downstream. The reasoning behind these constraints, as opposed
to the constraints themselves, is in `context.md`.

## Context topics — 17

`context.md`. Keyed by topic across all seven DOC sources, plus two topics from
`PLAN.md` that are working rules rather than deliverables ("Method worth
keeping", "Settled — do not re-derive"). Each entry carries `- source:`.

## Conflicts — 0 blockers, 3 warnings, 13 info

Full detail in `.planning/INGEST-CONFLICTS.md`.

**Nothing blocks.** The three warnings are the items that need a human decision
before a roadmap is written:

1. **Phase ordering.** `PLAN.md` orders presolve (phase 3) before the
   per-iteration work (phase 6); D81 measured the values and states "that
   reorders the plan", but the numbers were never changed and the sentence that
   would resolve it points at a phase-6 item that has since closed.
2. **The carried-defect register.** `PLAN.md` states both "all four are now
   closed" and "two of the four are closed" in the same section, and defect 2's
   own entry says the oscillation is open — which D89 confirms.
3. **The M2 close criterion** names "a stated factor" for its per-instance
   guard, and no factor is stated anywhere in the ingest set.

The thirteen INFO entries are six auto-resolutions (locked ADR content beating
stale PRD and DOC text), four non-conflicts written down so they are not
re-raised later, and the three gate checks that came back clean. One of those
three is a judgment call worth reading before routing: **cross-reference cycles
exist across all ten documents and were deliberately not treated as blocking**,
because they are navigational rather than derivational, and the strict reading
would block the entire ingest without synthesizing anything.

## Files

- `.planning/intel/decisions.md`
- `.planning/intel/requirements.md`
- `.planning/intel/constraints.md`
- `.planning/intel/context.md`
- `.planning/INGEST-CONFLICTS.md`
- `.planning/intel/classifications/*.json` — the 10 per-document classifications
