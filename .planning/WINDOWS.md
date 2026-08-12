---
schema_version: 1
open_count: 1
waived_count: 0
fixed_count: 0
total_count: 1
last_updated: 2026-08-12T16:09:15.812Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 01 | deviation | .planning/phases/01-candidate-admission-in-the-ratio-test/01-04-PLAN.md |  | 01-04 Task 3 requires spawning the jaos-measurer subagent; the executor context had no agent-spawning tool, so its checklist was run inline by the same context that produced the numbers. Independent read still owed. | open |  | 2026-08-12T16:09:15.812Z |  |

````json
[
  {
    "id": 1,
    "kind": "deviation",
    "phase": "01",
    "file": ".planning/phases/01-candidate-admission-in-the-ratio-test/01-04-PLAN.md",
    "line": null,
    "description": "01-04 Task 3 requires spawning the jaos-measurer subagent; the executor context had no agent-spawning tool, so its checklist was run inline by the same context that produced the numbers. Independent read still owed.",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-08-12T16:09:15.812Z",
    "resolved_at": null
  }
]
````
