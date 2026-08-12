---
phase: 01
slug: candidate-admission-in-the-ratio-test
status: verified
# threats_open = count of OPEN threats at or above workflow.security_block_on severity (high)
threats_open: 0
asvs_level: 1
created: 2026-08-12
---

# Phase 01 — Security

> Per-phase security contract: threat register, accepted risks, and audit trail.

The register below was authored at plan time across all five PLAN.md files —
14 numbered threats plus one supply-chain entry repeated in four plans. It was
not reconstructed retroactively, so this audit verifies mitigations rather than
building a register from the implementation.

---

## Trust Boundaries

| Boundary | Description | Data Crossing |
|----------|-------------|---------------|
| network → `bench/instances*/` | The only boundary the phase touches, and it pre-exists it. `bench/fetch.sh` refuses any file whose sha256 does not match the pinned manifest, and instance files never enter the repository. This phase adds nothing here and changes nothing about it. | LP/MPS instance files, public benchmark data, no secrets |
| — (everything else) | **None crossed.** JAOS is a dependency-free static C library (D2, D11) with no network, no IPC and no privilege boundary. Its only untrusted-input surface is the MPS and LP readers, which this phase does not touch. Everything changed is file-private state in one translation unit, reachable only through the unchanged `jaos_load_*`/`jaos_solve` API. | — |

Inventing an external attacker for this register would be dishonest. The real
risk in a phase like this one is internal invariant corruption producing a
*different* published answer rather than an obviously wrong one, and that is
what the register is built around.

---

## Threat Register

| Threat ID | Category | Component | Severity | Disposition | Mitigation | Status |
|-----------|----------|-----------|----------|-------------|------------|--------|
| T-01-01 | Tampering (internal state) | `s->nbmark` maintenance, eight membership sites in `src/simplex.c` | high | mitigate | D-08 cross-check every iteration in non-`NDEBUG`; plain-array unit tests; all published answers unmoved. **Strengthened after audit** — see Audit Trail. | closed |
| T-01-02 | Tampering (internal state) | `jm_nonbasic_insert` / `_remove` bit arithmetic | medium | mitigate | `test_nonbasic_expand_is_ascending_across_words` (inserted back to front across four words, both sides of every boundary) and `test_nonbasic_expand_handles_the_degenerate_counts`; `make sanitize` (ASan+UBSan) exit 0 over the whole suite | closed |
| T-01-03 | Information disclosure | debug scratch arrays `dbg_*` | low | accept | Verified on the object, not assumed: `strings build/release/libjaos.a \| grep -c dbg_` → **0**; the dev object → **8**. Freed in `sx_free`. | closed (accepted) |
| T-01-04 | Repudiation | the work-unit contract (D16) across the commit boundary | medium | mitigate | `b65d9f2` lands `src/simplex.c`, `docs/work-units.md` and the pinned test constant in one commit; `WORK_PINNED` 8545→8536 carries its derivation (3 iterations × (6 nvar − 3 nonbasic) = 9); D93 records the definition change with figures on both sides | closed |
| T-01-05 | Tampering (of the record) | `bench/*.baseline` | high | mitigate | Same structural ordering as T-01-06 | closed |
| T-01-06 | Tampering (of the record) | `bench/*.baseline` rewritten before the digest check | high | mitigate | **Ordering is in the commit history, not in prose.** `44c0ef6` touches only `bench/results/` (no baseline); `e8c2f58` touches only `bench/*.baseline` (no record). Verified independently by the orchestrator via `git show --stat` on both. | closed |
| T-01-07 | Spoofing (of provenance) | committed `bench/results/*.txt` | medium | mitigate | All three committed records carry a real comparison — `baseline: 0 regressed, 0 improved, 0 new` on netlib, netlib-infeas and netlib-kennington. A **pre-existing** instance was found and fixed by this phase; residual tooling gap logged below. | closed |
| T-01-08 | Denial of service (of the measurement) | a second session's runner writing `bench/results/` concurrently | medium | mitigate | `preflight.sh` run as a task precondition in `01-03` | closed |
| T-01-09 | Spoofing (of the measurement) | the two runner binaries | high | mitigate | Control 2: **0 iteration counts moved** across all 12 passes on all 94 instances, and the independent audit confirmed `analyse.py` exits non-zero on a control failure *before* any minimum is computed. Control 3b is stronger than the planned sha256: each binary reproduces 94/94 committed work integers against its **own** baseline. | closed |
| T-01-10 | Spoofing (of the measurement) | a parallel or warm-started timing run | high | mitigate | `J=1` throughout (the Makefile's own comment at line 22 requires it for a time ratio); iteration-count agreement catches a warm re-solve immediately | closed |
| T-01-11 | Repudiation | wall-clock figures leaking into the record | medium | mitigate | Grepped, not assumed: no time/seconds/elapsed field in any `bench/results/*.txt` or `bench/*.baseline`. `01-04`'s `files_modified` is empty by design. | closed |
| T-01-12 | Tampering (of the record) | `DECISIONS.md` headings and index anchors | high | mitigate | D93 lands at **428 insertions, 0 deletions** — nothing renumbered. Index anchor resolves; all nine forward citations (`src/simplex.c` 1, `tests/test_simplex.c` 4, `docs/work-units.md` 4) resolve. The planned `grep -c '^## D9'` == 4 criterion was refuted (it returns 5 — `## D9 —` matches) and the intent was established a sounder way. | closed |
| T-01-13 | Repudiation | a decision entry missing its refuted section | medium | mitigate | D93 carries "What was refuted" as its own section, plus the four required parts and an amendment recording a claim this phase itself falsified | closed |
| T-01-14 | Tampering (of the record) | `SPECS.md` measured figures left stale | medium | mitigate | Warm-against-cold work re-read from `bench/results/warm.txt`: 0.0162 → 0.0164, with the row saying why it moved. `01-05-SUMMARY.md` names the four warm figures that did *not* move and flags the competitive-gap figures as pre-phase rather than repairing them with an invented correction. | closed |
| T-01-SC | Tampering (supply chain) | package-manager installs / `bench/fetch.sh` | low | accept | This phase installs nothing. D2 and D11 forbid external dependencies outright. `bench/fetch.sh` verifies every instance and the `emps` converter against a pinned sha256 before use, and neither is stored in the repository. `valgrind` is a pre-existing development tool, not something this phase added. | closed (accepted) |

*Status: open · closed · open — below high threshold (non-blocking)*
*Severity: critical > high > medium > low — only open threats at or above `workflow.security_block_on` (high) count toward `threats_open`*
*Disposition: mitigate (implementation required) · accept (documented risk) · transfer (third-party)*

---

## Accepted Risks Log

| Risk ID | Threat Ref | Rationale | Accepted By | Date |
|---------|------------|-----------|-------------|------|
| R-01-01 | T-01-03 | The `dbg_*` scratch arrays hold solver-internal doubles already present in `sx`, are freed in `sx_free`, and are absent from every shipped build — confirmed on the release archive (`dbg_` count 0) rather than argued from `NDEBUG`. No gate binary carries them. | Developer, at plan time | 2026-08-12 |
| R-01-02 | T-01-SC | Dependency-free by decision (D2, D11). No install step exists anywhere in the phase. The one fetch path is sha256-pinned and pre-existing. | Developer, at plan time | 2026-08-12 |

*Accepted risks do not resurface in future audit runs.*

---

## Residual items — tracked, not blocking

Neither is a threat at or above the `high` block threshold, and neither blocks
phase advancement. Both are recorded so they are not rediscovered as new.

1. **`preflight.sh` cannot see a T-01-07 record.** `01-03` found that the
   committed standard-set record at `64efcc6:bench/results/netlib.txt` had
   itself been produced by a `-w` run and carried `baseline: NOT COMPARED` — it
   had been checked against nothing. The confirming run replaced it, so the
   committed state is clean. What is *not* fixed is the tooling: nothing in
   `preflight.sh` refuses to commit such a record, and the line that makes the
   condition visible is read by no code. Fixing it mid-plan would have
   invalidated a 27-minute campaign, which is why it was recorded instead.

2. **The `< 62000` work ceiling's headroom has closed from 16% to 6.5%.**
   `01-02` found the ceiling's comment claimed 58141 while HEAD actually
   measured 60941 — roughly 2800 units of drift accumulated unnoticed, because
   a ceiling is read only when it trips. It was re-measured on both sides
   (60701 passing, 64633 with the defect re-injected), so the bound still
   discriminates on current evidence and was deliberately left at 62000 rather
   than moved by eye. Logged as a STATE.md concern.

---

## Audit Trail

## Security Audit 2026-08-12

| Metric | Count |
|--------|-------|
| Threats found | 15 |
| Closed | 15 |
| Open | 0 |

State B — no prior SECURITY.md; register read from five PLAN.md `<threat_model>`
blocks and the `## Threat Flags` sections of all five SUMMARY.md files, every
one of which reported `None`.

`threats_open: 0`, `register_authored_at_plan_time: true`, `asvs_level: 1` —
the L1 short-circuit applies and no auditor subagent was spawned. Mitigation
evidence was verified directly against the repository and the built objects
rather than read off the plans' dispositions.

**One threat's mitigation was found incomplete and was strengthened during this
phase, after the phase's own gates had run.** An independent `numerics-reviewer`
pass on the landed diff found that the D-08 cross-check protecting **T-01-01**
compares candidate *sets* rather than the bitmap invariant. Because
`admit_candidate` rejects `JM_BASIC` on its first line, a *superset* bitmap — a
missed `jm_nonbasic_remove`, or a ninth membership site added without its hook —
produces an identical candidate set and the assertion stays silent. Plan `01-01`
examined exactly that case and correctly judged it benign against the tree it
was written for.

Commit `b65d9f2`, inside this same phase, falsified that judgement. Once the
dense branch bills `visited`, a superset inflates `s->work.units`, which is
compared against `cfg.work_limit` at `src/simplex.c:3354` — so the same model
stops at a different point and **publishes a different answer**. Neither plan's
reading was wrong on its own; the defect lived between them.

Closed by the O(1) invariant `assert(visited == s->nvar - s->nrow)` at
`src/simplex.c:1667` (commit `62ac240`), calibrated rather than assumed: with
`jm_nonbasic_remove` made a no-op — the fault D93's own table records the
cross-check as *silent* on — the new assertion aborts at `src/simplex.c:1686`.
No measured number moved, checked on the binary: the `.text` of
`build/bench/run` is 97,059 bytes with an identical sha256 built from either
source, the whole-file difference being the `-g` line table.
