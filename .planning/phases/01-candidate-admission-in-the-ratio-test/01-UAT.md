---
status: complete
phase: 01-candidate-admission-in-the-ratio-test
source: [01-VERIFICATION.md]
started: 2026-08-12T17:15:00Z
updated: 2026-08-12T21:05:00Z
---

## Current Test

[testing complete]

## Tests

### 1. The verdict was read off the right figure, from the right binaries

expected: The quoted result is the geometric mean of per-instance ratios (0.9709x), with the ratio of totals (0.9847x) shown beside it and labelled as not the answer; iteration counts agreed between candidate and parent on every instance across all 12 passes; the verdict follows the 4.2% rule.
why_human: The verdict decides whether D93 is written as a gain or a refusal, and no automated check can confirm that the right binary was timed.
note: The independent `jaos-measurer` audit recomputed all six readings, the ratio of totals, `truss`, control 4 and every callgrind headline from the raw logs while they still existed, and reproduced them exactly. It also confirmed control 2 — 0 iteration counts moved across all 12 passes on all 94 instances — and that `analyse.py` genuinely `sys.exit(1)`s on a control failure before any minimum is computed. The verifier could not re-derive this because the logs were never committed.
result: pass
reviewed_by: orchestrator, at the user's request, against the committed 01-04-SUMMARY.md
finding: |
  All three claims hold.
  (1) geomean.py's own output labels `GEOMETRIC MEAN 0.9709x <-- this is the
      result` against `ratio of totals 0.9847x <-- NOT the result (D46)`, and
      names why it matters here: pilot87 + maros-r7 are 74% of the set's work
      and are where the change is invisible. truss on its own line, 0.9759x.
  (2) Control 2 reads 0 moved across all 12 passes, and the audit confirmed the
      check is structurally enforced — analyse.py exits non-zero on a control
      failure before any minimum is computed. Control 3b is stronger than the
      sha256 the plan asked for: each binary reproduces 94/94 committed work
      integers against its own baseline.
  (3) The verdict follows the rule rather than evading it. The decisive evidence
      is that when the protocol's literal three-round form produced 5.12% —
      over the bar, an ACCEPT — the plan disclosed it as a refutation of its own
      confidence instead of banking it.
  One blemish, not a defect in the measurement: the body's "does not depend on
  which estimator is chosen" was refuted by the audit and was corrected only in
  the appendix. A forward pointer to CORRECTIONS §1 was added at the sentence.

### 2. Decide whether raw measurement artifacts must be committed in future

expected: A rule either way. Today the only measurement in this phase that cannot be re-derived from the repository is the one that produced the verdict — the 12 timing logs and both callgrind annotations were never committed and no longer exist. `build/bench/run-parent` (464096 B) and `run-candidate` (472232 B) survive as distinct binaries, which closes the "same binary timed twice" failure mode but not the iteration-identity control.
why_human: A process decision about what the record must carry, not a fact about the code.
result: pass
decided: |
  RULE ADOPTED by the developer, 2026-08-12. A plan whose deliverable is a
  verdict commits its raw readings, to
  `.planning/phases/<phase>/<plan>-MEASUREMENT/` — the timing logs, any
  callgrind annotations and the analysis script. The existing prohibition is
  unchanged: no wall-clock figure enters `bench/results/*.txt` or a baseline.
  Landed in CLAUDE.md's working habits.
recovery_attempted: |
  The developer asked for 01-04's artifacts to be committed. They no longer
  exist. Every scratchpad on the machine was searched: the three holding timing
  artifacts date from 2026-08-07 to 08-11 and belong to earlier phases, today's
  shared subagent scratchpad holds only GSD tooling scripts, and the agent task
  transcripts do not contain the per-instance timings or the geomean output
  (grepped, not assumed). The rule is therefore prospective only. The two
  binaries `build/bench/run-parent` and `run-candidate` are also gone, being
  build products.

## Summary

total: 2
passed: 2
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

None. The verifier scored 27/29 with **no gaps and nothing failed** — the two
unverified items are present and substantive, and are unverifiable only because
their raw artifacts were not committed. Neither is a defect in the code.
