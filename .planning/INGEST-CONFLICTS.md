## Conflict Detection Report

Ingest of 10 documents, MODE new, precedence ADR(0) > SPEC(1) > PRD(2) > DOC(3-5)
per `.planning/ingest-manifest.yaml`. All 92 entries of `DECISIONS.md` are
treated as LOCKED.

### BLOCKERS (0)

None. The three checks that could have produced one — LOCKED-vs-LOCKED
contradiction, UNKNOWN classification at low confidence, and cross-reference
cycles — were all run; their results are recorded in the INFO section below.
The cycle result is a judgment call and is written up as such.

### WARNINGS (3)

[WARNING] Phase ordering between presolve and the per-iteration work is unresolved
  Found: PLAN.md's phase table numbers presolve as phase 3 and speed as phase 6, so presolve precedes it in the stated order. D81 measured presolve at 1.417x for HiGHS and 1.136x for SoPlex against a per-iteration gap of 2.53x that no rung moves, and states in as many words: "That reorders the plan ... The cheaper iteration is the larger lever and it is also the one already in progress." PLAN.md's phase 3 text agrees — "so phase 6 item 3 now has a measured claim on going first" — but the phase numbers were never changed, and the sentence that would resolve it is itself stale: phase 6 item 3 is closed, refused in both halves by D82 and D84, so the live head of phase 6 is item 3a (the ratio test's candidate admission), which PLAN.md says "needs its own decision before any code".
  Impact: the two readings produce different roadmaps. Synthesis cannot pick between them — D81 states a magnitude comparison, not an ordering, and PLAN.md states an ordering its own body argues against. Precedence does not resolve it, because the locked source never says what order to build in.
  → Decide whether the next build item is presolve (REQ-presolve) or the ratio test's candidate admission (REQ-ratio-test-candidate-admission), noting that the latter is gated on a decision that does not exist yet.

[WARNING] PLAN.md's carried-defect register contradicts itself about whether the re-entry oscillation is open
  Found: PLAN.md heads the section "Known defects, carried — **all four are now closed**" and repeats "All four are now closed" in the first line under it. Four lines later the same section reads "**Two of the four are closed, and neither by the cure it was filed with**", and eight lines after that "Reproducible, diagnosed, not yet fixed." The four entries below are struck through for defects 1 (D91), 3 (D86) and 4 (D85) and not for defect 2, whose text ends "What remains open is the oscillation itself, and no cure is named for it." D89 agrees — its own "What is left open" section reads "The oscillation itself" — and D74 closed the only cure that had been proposed for it. PLAN.md then carries a second section headed "Known defects, carried — none".
  Impact: whether REQ-reentry-oscillation is scheduled work or an accepted limitation cannot be read off the source. Two "carried defects" headers plus the "Open, and it is what closing that defect exposed" section make three registers with different verdicts, and a reader taking the headers at face value would drop a live item or resurrect three closed ones.
  → Decide whether the oscillation is roadmap work. The measured cost is 278 iterations of 116,071 on `pilot87` at interval 24 — 0.24%, D89 — and no cure is named, so "accepted limitation" is defensible, but it should be an explicit decision rather than an artefact of which header is read.

[WARNING] The M2 close criterion carries an unstated constant
  Found: bench/compare/README.md defines what closes M2 as "JAOS strictly faster than the best competitor at T0, on the geometric mean of per-instance time ratios over the standard set, **with a guard that no single instance is more than a stated factor slower**". No factor is stated there or anywhere else in the ingest set — SPECS.md §8 records the measured gaps (3.72x, 1.34x, 3.77x, per D83) without a target, PLAN.md states no M2 close criterion, and no D-numbered decision sets one.
  Impact: the project's headline milestone has a half-specified acceptance criterion, in a repository whose first rule is that every number carries a measurement on both sides (D17, SPECS.md premises). Any downstream document that filled the number in would be inventing a constant, which this project names repeatedly as how it loses weeks.
  → Set the per-instance guard factor with a measurement behind it, or record explicitly that M2 closes on the geometric mean alone. Left absent in `requirements.md` under REQ-m2-competitive-gate rather than guessed.

### INFO (13)

[INFO] No LOCKED-vs-LOCKED contradiction across the 92 decisions
  Note: DECISIONS.md carries 92 locked decisions with heavily overlapping scopes, and eleven of them supersede or reframe an earlier one — D28 retires one of D24's four arguments, D45 overrides D44's own work-unit sweep for `SPARSE_COL_DEN`, D71 and D73 and D87 and D91 each rework D47, D74 closes the direction D49-D51 opened, D86 refutes the cure D72 proposed, D90 lifts the refusal D68 installed, D92 re-measures the route D27 refused. Every one of those states the supersession in its own text and gives the reason the earlier premise expired — D90 "the premise of the refusal was therefore gone", D92 "D27's own refusal of the published reading had expired", D86 "that cure is refuted here". No pair contradicts on the same scope without the later one accounting for the earlier, so nothing was auto-resolved: there was nothing to resolve.

[INFO] No UNKNOWN or low-confidence classification
  Note: all 10 classifications in `.planning/intel/classifications/` read `"confidence": "high"` with `"manifest_override": true`. The type of every document came from `.planning/ingest-manifest.yaml`, which is authoritative, so no document had to be typed by heuristic and none needs re-tagging.

[INFO] Cross-reference cycles exist and are deliberately not treated as blocking
  Note: this is a judgment call, recorded so it can be overruled. The `cross_refs` graph over the 10 ingested documents is densely cyclic — DECISIONS.md <-> PLAN.md, DECISIONS.md <-> SPECS.md, PLAN.md <-> SPECS.md, and every DOC points back at PLAN.md, which points at every DOC (PLAN.md -> docs/tolerances.md -> PLAN.md, PLAN.md -> bench/README.md -> PLAN.md, and the same for docs/work-units.md, docs/scaling.md, docs/format-support.md and bench/compare/README.md). Graph depth is 3, far inside the traversal cap of 50. A literal application of the cycle rule would mark all 10 documents as cyclic and block the entire ingest. These edges are navigational — "see also" pointers and D-number citations — not derivation edges: no document's content has to be resolved through another to be read, and this project's stated convention is that its four root documents cross-reference each other by design. Synthesis was performed by reading each source directly rather than resolving references transitively, so the failure the rule guards against — a synthesis loop — did not arise. If the strict reading is wanted, re-run with a manifest that cuts the cross-references; nothing will be synthesized.

[INFO] Auto-resolved: ADR > PRD on carried defect 1 needing a factorization
  Note: PLAN.md states "defect 1 needs a factorization inside the checker and an answer to what 'independent' then means, which is the larger open design question of the two". D87 refutes exactly that: "The one thing this does remove is the argument that route B needs the solver's basis. It does not. Whatever closes the rest will not need one either." D91 then closes the defect outright, and PLAN.md's own struck-through entry agrees ("Closed (D91)"). DECISIONS.md (precedence 0, locked) wins; the sentence is a stale paragraph from an earlier session and is not carried into `requirements.md`.

[INFO] Auto-resolved: ADR > PRD on "defect 2 has a cure proposed and never tried"
  Note: PLAN.md states "Defect 2 has a cure proposed and never tried (D50), with a prerequisite D49 stated and nobody has answered." Both halves are contradicted by DECISIONS.md. D89 tried D50's proposal and landed it, and D89 also attributes D49's factor of 280 to the per-column scaling ("that answers the half D49 left open"). DECISIONS.md wins. What genuinely remains is the oscillation, which is WARNING 2.

[INFO] Auto-resolved: ADR > PRD on "reproducible, diagnosed, not yet fixed"
  Note: PLAN.md carries "Reproducible, diagnosed, not yet fixed. Both came out of varying `REFACTOR_EVERY` over 16..256" inside the carried-defects section, alongside "Two of the four are closed". D85 (defect 4), D86 (defect 3) and D91 (defect 1) each close one of the four with all 139 answers unmoved. DECISIONS.md wins; the paragraph is stale, from the same stratum as the material in WARNING 2.

[INFO] Auto-resolved: ADR and SPEC > PRD on the warm re-solve geometric means
  Note: PLAN.md's phase 2 text reports "0.0055 of the iterations and 0.0166 of the work" over 92 of the standard 94, citing D69. SPECS.md §8 reports "0.0052 of the iterations, 0.0162 of the work", citing D69 as improved by D90. D90 states the figures directly: "the geometric means improve from 0.0055 to 0.0052 in iterations and 0.0166 to 0.0162 in work". PLAN.md's own D90 paragraph carries the corrected numbers further down the same file. D90 and SPECS.md win; `constraints.md` carries 0.0052 and 0.0162.

[INFO] Auto-resolved: ADR and SPEC > DOC on the T0 headline figures
  Note: bench/compare/README.md opens with "against HiGHS 1.15.1 JAOS is 3.71x slower per solve and against SoPlex 8.0.3 1.35x, on 1.47x and 0.70x their iteration counts (D52, D53, D60)". Those figures are D81's re-measurement, not D52/D53/D60's — D60's own table reads 3.70x and 1.31x. The current reading is D83's, taken with three competitors in one session: 3.72x, 1.34x, 3.77x, which is what SPECS.md §8 carries. The DOC (precedence 4) is one session behind and mis-attributes its own numbers; SPECS.md and DECISIONS.md win. The next paragraph of the same DOC does carry D83's Clp reading, so only the opening figures are stale.

[INFO] Not a conflict: 3.71x and 3.72x both appear in PLAN.md and both are correct
  Note: PLAN.md §1.3 reports 3.72x vs HiGHS citing D83, and PLAN.md phase 3 reports "JAOS is 3.71x behind HiGHS with neither side presolving" citing D81. Those are two measurement sessions, each attributed to the decision that took it, and D81 and D83 both report their own figure. Recorded so it is not raised later as a discrepancy. The same applies to README.md's rounded "3.8x / 1.4x".

[INFO] Auto-resolved: ADR > DOC on the work-unit weights still being called drafts
  Note: docs/work-units.md heads its weight table "Drafts until calibrated (PLAN.md 2.7); the definition becomes public contract at 1.0". D16 already makes the work unit a public contract, and D32 closed the calibration question it points at — the per-iteration weight is zero and the row leaves the table, while "Both fixed weights that remain — `JM_WORK_UPDATE` at 64 and `JM_WORK_FACTOR` at 4096 — stay exactly as they are." The citation is also circular: PLAN.md's redirect table maps "PLAN 2.7 — work units" to `docs/work-units.md`, so the document cites a section that resolves to itself. DECISIONS.md wins — no "calibrate the work-unit weights" requirement is extracted. What is genuinely outstanding is only the 1.0 freeze.

[INFO] An open item lives in a DOC and in no higher-precedence source
  Note: docs/scaling.md ends with "What is not settled yet — Which mode is the better default across Netlib is a question for the campaign, decided by measurement rather than by preference." Curtis-Reid is the current default. No entry in DECISIONS.md closes the question and PLAN.md does not carry it among Q2, Q5, Q8 and Q11. This is not a contradiction, so nothing is auto-resolved against it; it is recorded in `context.md` so the item is not lost. D64 removed the caller-facing half — SPECS.md moves "turn scaling off or pick the mode" to **out of scope** — leaving only the internal-default question open.

[INFO] PLAN.md's phase 6 item numbering is not unique
  Note: phase 6 lists items 1, 2, 3, 3a, 4, 5, 5, 6, 7 — two different items carry the number 5 (BTRAN's `L'` pass, and the eta passes), and 3a was inserted after 3 closed. Requirement IDs in `requirements.md` are derived from descriptive slugs rather than from the item numbers. Items 2, 3 and the first 5 are recorded as closed or explicitly not-to-be-recosted rather than as requirements: item 2 closed by D55 and D56, item 3 closed by D82 and D84, and BTRAN's `L'` pass carries "Recorded so it is not costed again."

[INFO] Every SPECS.md feature status agrees with PLAN.md
  Note: the check this ingest was most likely to trip on came back clean. Solve time, callbacks, presolve, primal simplex, crash basis, partial and multiple pricing, hyper-sparsity, the certified suboptimality bound, sensitivity and ranging, the writers and the Python bindings all carry the same standing in SPECS.md §1-§7 as in PLAN.md's phases and its settled table. The only numeric disagreement between the two documents is the warm re-solve means, recorded above.
