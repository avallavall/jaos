# 07 — The gate: how a change is judged

The path every change travels, what one campaign run does per instance,
and the two comparisons that decide a verdict. Traced at commit `33bb85d`.
The gate's own contract is `bench/README.md`; the scripts are
`.claude/skills/jaos-measure/scripts/`.

## The loop a change travels

```mermaid
flowchart TD
    E["finish every source edit<br/>(a campaign is valid only for the tree that produced it)"] --> T["make test && make sanitize<br/>(WSL, ASan+UBSan)"]
    T --> NR["solver internals changed?<br/>numerics-reviewer on the diff,<br/>before any campaign"]
    NR --> PF["preflight.sh —<br/>tree settled · no other runner ·<br/>baselines intact · instances fetched · suite green"]
    PF --> C["the three campaigns, always all three:<br/>make netlib · netlib-infeas · netlib-kennington (J=N)"]
    C --> RD["record_diff.py — per-instance diff of<br/>bench/results/*.txt against the committed record"]
    RD --> GM["geomean.py — any ratio is a geometric mean<br/>of per-instance ratios, never a sum (D46)"]
    GM --> V["verdict — judged by jaos-measurer,<br/>a context that did not produce the numbers"]
    V -- accept --> L["land: commit + CHANGELOG (+ DECISIONS if a<br/>question closed) + raw readings to bench/measurements/&lt;id&gt;/"]
    V -- reject --> RJ["record the refutation in measurements/&lt;id&gt;/;<br/>the candidate never lands"]
    L --> B["baselines rewritten only by *-baseline targets,<br/>deliberately, never while the gate is red"]
```

## What one instance goes through (`bench/run.c`)

```mermaid
sequenceDiagram
    participant D as run_one
    participant J as libjaos
    participant K as checker

    D->>J: jaos_model_new + jaos_read_mps
    D->>D: shape check vs manifest (rows, cols)
    D->>J: jaos_solve (timed — seconds print, never recorded)
    D->>D: objective vs reference within 1e-6 relative
    D->>K: jaos_check_solution at 1e-6
    K-->>D: checker= predicate (primal && dual feasible)
    D->>D: digest — FNV-1a over the bytes of x then y
    D->>J: jaos_clear_basis, then jaos_solve AGAIN
    D->>D: det= — same status, iters, work, objective bits, digest
    D->>D: one record line, 10 judged fields
```

The `jaos_clear_basis` between the two solves is what makes the second a
cold solve; without it the determinism check would measure a warm re-solve
(D68). The infeasible set expects `verdict=ok` (refused, no false optimum)
and carries no digest and no checker verdict.

## The two comparisons — do not conflate them

```mermaid
flowchart LR
    subgraph RUN["during the run — bench/run.c"]
        R1["this run"] -- "per instance:<br/>5 predicates yes→no = REGRESSED ·<br/>work > 2.0x = REGRESSED ·<br/>rsub > 2.0x above its floor = REGRESSED" --> B1["bench/*.baseline<br/>(committed, 10 fields,<br/>no digest, no seconds)"]
    end
    subgraph AFTER["after the run — record_diff.py"]
        R2["bench/results/*.txt<br/>(the record, committed)"] -- "against git HEAD's copy:<br/>status/predicate flips, work > 2.0x,<br/>format drift = regression ·<br/>digest change = a NOTE" --> B2["the committed record"]
    end
```

- **The digest never fails the gate.** It is evidence: every instance
  bit-identical to the committed record is the strongest available proof
  that a change is a no-op.
- **A summary line saying `0 regressed` means only that no predicate
  flipped and no instance crossed 2.0x work.** Read the per-instance diff;
  ten commits once reached main under an unmoved summary line
  (`bench/README.md`).
- **Seconds never enter a record or a baseline.** Work units are the cost;
  a time ratio is taken only where units cannot see the change, at `J=1`,
  minimum over alternating rounds, geometric mean (D45, D93).

## What is judged, in one table

| judgement | rule | consequence |
|---|---|---|
| shape | rows/cols match the manifest | predicate |
| objective | within 1e-6 relative of the reference | predicate |
| checker | primal and dual feasible at 1e-6 | predicate |
| determinism | second cold solve bit-identical | predicate |
| work | ≤ 2.0x the baseline, per instance | regression → exit 1 |
| suboptimality bound | ≤ 2.0x the baseline above its floor | regression → exit 1 |
| digest | vs the committed record | note — no-op evidence |
| record format | columns appear/disappear | regression (not comparable) |

## Everything else in `bench/`

- `make warm` / `warm-kennington` — what warm re-solving buys; reports a
  ratio, not a verdict; no baseline.
- `bench/compare/` — the competitive ladder against HiGHS, SoPlex and Clp;
  its own record, the one place seconds are allowed; rung definitions in
  `bench/compare/README.md`.
- `make pgo` — profile-guided rebuild; deliberately not part of `make`.
- `bench/measurements/<id>/` — raw readings behind each verdict, so every
  verdict is re-derivable; throwaway diagnostic drivers live and die here.
- `plato*` targets — the large-instance set; explicitly not the gate.
