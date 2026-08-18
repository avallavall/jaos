# Diagrams — how JAOS works, drawn

Seven files, one subject each. Every diagram was traced from the source at
commit `33bb85d` (2026-08-18). Diagrams name functions and files, not line
numbers, because line numbers drift.

GitHub renders the mermaid blocks directly. `docs/architecture.html` is the
same set compiled into one page, with a short explanation beside each
diagram. Open it in a browser; it loads the mermaid renderer from a CDN, so
it needs network access once.

| file | subject |
|---|---|
| `01-repo-map.md` | what lives where, and which document owns which statement |
| `02-modules.md` | the source files, grouped, and who calls whom |
| `03-solve-pipeline.md` | from an MPS file to an answer, end to end |
| `04-simplex-iteration.md` | the dual simplex loop, one iteration, and the LU |
| `05-presolve.md` | the reduction families, the record arena, and postsolve |
| `06-outcomes.md` | every terminal status, who sets it, and the checker |
| `07-gate.md` | how a change is judged: campaigns, baselines, verdicts |

These files describe the code. They own no decision and no measured number.
The reasoning lives in `DECISIONS.md`, the constants and their sweeps in
`docs/tolerances.md` and the source; the diagrams point at both. A commit
that changes a structure drawn here should update the affected file, or
delete it.
