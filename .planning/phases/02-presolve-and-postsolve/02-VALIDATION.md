---
phase: 2
slug: presolve-and-postsolve
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-08-12
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.
> Derived from `02-RESEARCH.md` § Validation Architecture and the locked
> decisions D-09 through D-16 in `02-CONTEXT.md`.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Unity — vendored, one of the two closed exceptions to the no-dependencies rule |
| **Config file** | `Makefile` (`test` target). No separate framework config exists. |
| **Quick run command** | `wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && make -j12 test"` |
| **Full suite command** | Same as quick run. The unit suite is the fast layer; the **campaigns are a separate, slower layer** and are not "the test suite". |
| **Estimated runtime** | Unit suite: seconds. Campaigns at `J=12`: `netlib` ~85 s · `netlib-infeas` ~10 s · `netlib-kennington` ~8 min. |

**Two traps that have cost this project time before, and apply to every command in this file:**
`$?` does not survive Git Bash → WSL — write the commands to a script file and run that. And
**all campaigns take `J=N`**; without it they run sequentially and cost minutes instead of seconds (D57).

---

## Sampling Rate

- **After every task commit:** `make -j12 test` (unit suite, seconds)
- **After every wave / after each reduction family lands:** all **three** campaign sets, not just
  the standard one — `make netlib J=12`, `make netlib-infeas J=12`, `make netlib-kennington J=12`.
  A change that looked clean on the standard and infeasible sets has previously been caught costing
  3.2x work only on Kennington-scale instances.
- **Before `numerics-reviewer` runs:** all three sets green with presolve on, **and** all three
  bit-identical to the current baselines with presolve off (D-09).
- **Before `/gsd-verify-work`:** the above, plus the `jaos-measurer` verdict step.
- **Max feedback latency:** 60 s at the unit layer; ~9 min for a full three-set sweep at `J=12`.

**Ordering rule (D-06 of phase 1, carried):** finish every source edit before launching a campaign.
A run takes tens of minutes and is only valid for the tree that produced it — even a comment
invalidates it.

---

## Per-Task Verification Map

Task IDs do not exist yet — this file is written before the planner runs. Rows are keyed by
success criterion; the planner assigns task IDs and the executor fills Status.

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| TBD | TBD | — | REQ-presolve — criterion 1: same verdict and an objective within the gate's tolerance of Koch's reference, reductions on or off | — | N/A | integration / campaign | `make netlib J=12 && make netlib-infeas J=12 && make netlib-kennington J=12` | ✅ `bench/run.c` + manifests exist; needs a presolve-off build variant to compare against, not a new file | ⬜ pending |
| TBD | TBD | — | REQ-presolve — criterion 2: the unmodified checker accepts all 139 postsolved answers | — | N/A | integration / campaign | Same three campaign runs; the `checker=ok` column in the record | ✅ exists | ⬜ pending |
| TBD | TBD | 0 | REQ-presolve — criterion 2 at unit level: a correctly-shaped but wrong index map is **rejected** (D-10) | — | N/A | unit, negative case | `make -j12 test` | ❌ **Wave 0** — one round-trip test per family plus its must-fail-first companion | ⬜ pending |
| TBD | TBD | 0 | REQ-presolve — criterion 3: each reduction reports what it removed (D-13) | — | N/A | unit, white-box | `make -j12 test` | ❌ **Wave 0** — depends on the per-family counter struct in `src/jaos_internal.h` | ⬜ pending |
| TBD | TBD | — | REQ-presolve — criterion 4: determinism across two solves with the basis cleared between them | — | N/A | integration / campaign | Same three campaign runs; the `det=ok` column | ✅ **already fully enforced** — `bench/run.c:437-438` (infeasible path) and `:574-577` (optimal path) compare status, iterations, work units, the objective's bits and the digest after `jaos_clear_basis`. Extend **only** for a new path presolve introduces, e.g. a presolve-only infeasibility short-circuit. See corrected D-12. | ⬜ pending |
| TBD | TBD | 0 | D-09 negative control: presolve compiled off leaves all 139 digests bit-identical to today's committed baselines | — | N/A | integration / campaign | Three campaign runs built with the presolve guard off, diffed against `bench/*.baseline` | ❌ **Wave 0** — the guard macro is new; the baseline-diff mechanism already exists | ⬜ pending |
| TBD | TBD | — | D-15 deliverable: geometric mean of per-instance ratios, presolve-on vs presolve-off, at `J=1`, with the negative-control instances reading 1.00x | — | N/A | measurement — **not** a pass/fail test | `.claude/skills/jaos-measure/scripts/geomean.py` (verified present, alongside `preflight.sh` and `record_diff.py`) | ✅ script exists; the per-plan raw-reading directory is a new artifact | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] **Round-trip tests, one per reduction family**, each shown to fail on a deliberately broken
      postsolve index map before its passing is treated as evidence. The in-tree template is
      `tests/test_check.c`'s `test_t1_flags_wrong_dual_sign` /
      `test_t1_flags_complementarity_break` / `test_t1_flags_primal_violation` — that trio is
      already what "build the case it must reject" looks like here. File location is Claude's
      discretion per D-10.
- [ ] **Per-family counter struct in `src/jaos_internal.h`** (D-13) — nothing white-box can read
      the counts until it exists.
- [ ] **A build guard that compiles presolve out**, wired through the existing `EXTRA_CFLAGS` hook
      (`Makefile:94-101`). Name at Claude's discretion. D-09's negative control cannot be built
      without it, and D-03 makes this the only switch presolve gets.
- [ ] **`.planning/phases/02-presolve-and-postsolve/<plan>-MEASUREMENT/`** for the plan whose
      deliverable is the D-15 figure — the timing logs, any callgrind annotations and the analysis
      script, committed. Not a test; a process gap. Phase 1 is the receipt: 12 timing logs and 2
      callgrind annotations were never committed and survived only because a scratchpad happened
      to still hold them. By the time anyone looked again they were gone.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| The dual and reduced-cost recovery rule per reduction family | REQ-presolve criterion 2 | The research reports the general recipe from the literature but explicitly does **not** claim it verified against JAOS's own sign convention in `src/check.c`. A formula that is right in the paper and wrong in this codebase's convention produces a plausible answer in the right shape. | Routed to two places by design: the D-10 round-trip tests, which must be shown to reject a wrong map first, and the mandatory `numerics-reviewer` task on the postsolve diff — **before** the campaigns, because a finding after a campaign costs the campaign. |
| The verdict on what presolve is worth | REQ-presolve criterion 3 | A measurement is not a test. It has no pass/fail line, and this phase deliberately has no numeric target — `REQ-presolve`'s acceptance is recorded as **absent** on purpose, because D81's 1.417x and 1.136x are readings of two competitors. | The mandatory `jaos-measurer` task, judging figures produced by a context that did not produce them. Its raw readings are committed per the MEASUREMENT rule above. |
| Whether the round cap for D-02's fixed-point loop is set where the propagation actually converges | REQ-presolve | A cap can only be justified by a sweep, and the sweep is the deliverable. | Follow the `IMPLIED_ROUNDS` precedent (`src/check.c:264`, written up in `docs/tolerances.md` with the sweep that set it). `make clean` between settings, and a canary that must move — otherwise the sweep measures one binary N times. |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or a Wave 0 dependency
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all ❌ references above
- [ ] No watch-mode flags
- [ ] Feedback latency < 60 s at the unit layer
- [ ] The D-09 negative control is green **before** any measurement is believed
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
