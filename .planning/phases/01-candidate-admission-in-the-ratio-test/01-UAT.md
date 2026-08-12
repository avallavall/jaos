---
status: testing
phase: 01-candidate-admission-in-the-ratio-test
source: [01-VERIFICATION.md]
started: 2026-08-12T17:15:00Z
updated: 2026-08-12T17:15:00Z
---

## Current Test

number: 1
name: Read the per-instance timing table and the geometric mean in 01-04-SUMMARY.md (harvested deferred human-check from 01-04-PLAN.md)
expected: |
  The figure quoted as the result is the geometric mean of per-instance ratios,
  not the ratio of totals; iteration counts agreed between both binaries on
  every instance; and the verdict follows the 4.2% rule rather than being
  argued around it.
awaiting: user response

## Tests

### 1. The verdict was read off the right figure, from the right binaries

expected: The quoted result is the geometric mean of per-instance ratios (0.9709x), with the ratio of totals (0.9847x) shown beside it and labelled as not the answer; iteration counts agreed between candidate and parent on every instance across all 12 passes; the verdict follows the 4.2% rule.
why_human: The verdict decides whether D93 is written as a gain or a refusal, and no automated check can confirm that the right binary was timed.
note: The independent `jaos-measurer` audit recomputed all six readings, the ratio of totals, `truss`, control 4 and every callgrind headline from the raw logs while they still existed, and reproduced them exactly. It also confirmed control 2 — 0 iteration counts moved across all 12 passes on all 94 instances — and that `analyse.py` genuinely `sys.exit(1)`s on a control failure before any minimum is computed. The verifier could not re-derive this because the logs were never committed.
result: [pending]

### 2. Decide whether raw measurement artifacts must be committed in future

expected: A rule either way. Today the only measurement in this phase that cannot be re-derived from the repository is the one that produced the verdict — the 12 timing logs and both callgrind annotations were never committed and no longer exist. `build/bench/run-parent` (464096 B) and `run-candidate` (472232 B) survive as distinct binaries, which closes the "same binary timed twice" failure mode but not the iteration-identity control.
why_human: A process decision about what the record must carry, not a fact about the code.
result: [pending]

## Summary

total: 2
passed: 0
issues: 0
pending: 2
skipped: 0
blocked: 0

## Gaps

None. The verifier scored 27/29 with **no gaps and nothing failed** — the two
unverified items are present and substantive, and are unverifiable only because
their raw artifacts were not committed. Neither is a defect in the code.
