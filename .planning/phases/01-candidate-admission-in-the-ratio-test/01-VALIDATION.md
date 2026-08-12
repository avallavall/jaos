---
phase: 1
slug: candidate-admission-in-the-ratio-test
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-12
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Unity v2.7.0, vendored at `tests/vendor/unity/` |
| **Config file** | none — Makefile-discovered (`$(wildcard tests/test_*.c)`, `Makefile:120`) |
| **Quick run command** | `wsl -d Ubuntu-24.04 -- bash /mnt/c/.../run-test.sh` (`make test`) |
| **Full suite command** | `make test && make sanitize` (ASan+UBSan) |
| **Estimated runtime** | `make test` seconds; the gate campaigns are minutes and take `J=N` |

**Trap that invalidates a run:** `$?` does not survive Git Bash → WSL. Write the
commands to a script file and run that. `/tmp` does not persist between `wsl`
invocations.

---

## Sampling Rate

- **After every task commit:** `make test` — dev build, no `NDEBUG`, so it
  exercises the D-08 assertion once that is wired
- **After every plan wave:** `make test && make sanitize`
- **Before the phase gate:** `make netlib`, `make netlib-infeas`,
  `make netlib-kennington` — all at `J=12`, all green, per-instance baseline
  diff clean
- **Max feedback latency:** `make test` is seconds; the full gate is ~10 min at
  `J=12` for the three sets

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 1-01-01 | 01 | 0 | REQ-ratio-test-candidate-admission | — | N/A | unit (plain-array) | `./build/dev/test_simplex` | ❌ W0 | ⬜ pending |
| 1-01-02 | 01 | 0 | REQ-ratio-test-candidate-admission | — | N/A | unit, negative case | `./build/dev/test_simplex` | ❌ W0 | ⬜ pending |
| 1-01-03 | 01 | 1 | REQ-ratio-test-candidate-admission | — | N/A | runtime assertion (D-08) | `make test`, `make sanitize` | ❌ W0 | ⬜ pending |
| 1-01-04 | 01 | 2 | REQ-ratio-test-candidate-admission | — | N/A | integration / gate | `make netlib && make netlib-kennington && make netlib-infeas` | ✅ | ⬜ pending |
| 1-01-05 | 01 | 2 | REQ-ratio-test-candidate-admission | — | N/A | gate + baseline diff | `make netlib` diffed against the committed baseline | ✅ | ⬜ pending |
| 1-01-06 | 01 | 3 | REQ-ratio-test-candidate-admission | — | N/A | measurement, not pass/fail | `make netlib J=1` before and after | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

**On the last row:** a time ratio is a measurement, not a test. It reports
4.2% or better (D-13) or the phase closes INCONCLUSIVE. It never turns red.

---

## Wave 0 Requirements

- [ ] A **plain-array unit test** for the list/bitmap maintenance primitives —
  the membership invariant is `status[v] != JM_BASIC`, **never** "has a finite
  bound", or `JM_FREE` variables are silently dropped. Buildable today without
  touching `sx` visibility.
- [ ] The **negative test that must fail first**: construct the failure mode the
  chosen representation is actually vulnerable to and confirm the maintenance
  test rejects it, before its passing is treated as evidence (D-07,
  CONTEXT.md `<specifics>`).
- [ ] A `#ifndef NDEBUG` block in `simplex.c` wiring the D-08 cross-check.
  **This is the first use of that idiom in `src/` — there is no `assert(` and no
  `NDEBUG` anywhere in the tree today.** Needs a clean `-Werror` build under both
  `DEV_CFLAGS` and `RELEASE_CFLAGS` before it counts as done.
- [ ] A `docs/work-units.md` update describing the new charge for the dense
  branch, landing with the D-09 code change — the doc states the pre-change
  formula explicitly and reads as stale otherwise.

**Test-reachability blocker, found by reading the code:** `sx` is not declared in
`jaos_internal.h` at all, and `admit_candidate` and `dual_ratio_test` are both
`static` in `simplex.c` (lines 1527 and 1557). Test binaries link against
compiled objects, so `tests/test_simplex.c` cannot reach any of the three today.
The codebase's own precedent for exactly this problem is `jm_bland_pick`
(`:1614`), `jm_pattern_order` (`:1637`) and `jm_harris_pick` (`:1678`) —
non-static, `jm_`-prefixed, taking plain arrays instead of `sx *`, and unit
tested directly. The new maintenance primitives should take that shape.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| The `J=1` time ratio and its geometric mean | REQ-ratio-test-candidate-admission | A measurement, not an assertion — and D17 says a WSL number is a development number | Run the standard set before and after on the same tree state, report a geometric mean of per-instance ratios (D46), name instances that move against the mean |
| Deciding whether 4.2% was cleared | REQ-ratio-test-candidate-admission | The verdict is a judgement against a threshold derived from the harness's 1.4% repeatability | `jaos-measure` governs how the result is read; `jaos-measurer` can return ACCEPT / REJECT / INCONCLUSIVE with per-instance evidence |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency: `make test` per commit
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
