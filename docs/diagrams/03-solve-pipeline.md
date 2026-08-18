# 03 — The solve pipeline, end to end

From a file on disk to an answer a caller can read. Traced at commit
`33bb85d`.

## The minimal caller

```mermaid
sequenceDiagram
    actor C as caller
    participant M as model.c
    participant R as mps.c / lpfmt.c
    participant S as simplex.c
    participant K as check.c

    C->>M: jaos_model_new(&m)
    C->>R: jaos_read_mps(m, path)
    R->>M: jaos_load_lp(...) — validate, sort, commit
    opt configuration (survives a load)
        C->>M: jaos_set_*_tolerance / _limit / _log_callback / _set_basis
    end
    C->>M: jaos_solve(m)
    M->>S: jm_dual_simplex(m)
    S-->>M: solve_status + sol_* arrays
    C->>M: jaos_status_of(m) — must be JAOS_SOLVE_OPTIMAL
    C->>M: jaos_objective / jaos_solution / jaos_basis
    opt independent verification
        C->>K: jaos_check_solution(m, x, y, tol, &report)
    end
    C->>M: jaos_model_free(m)
```

Rules a sequence diagram cannot show:

- `jaos_objective`, `jaos_solution` and `jaos_basis` refuse unless the
  status is `JAOS_SOLVE_OPTIMAL`. A stale answer is an error, never a
  number.
- Any model mutation (`jaos_set_*`, add/delete) discards the answer and
  resets the status to `NOT_RUN`. The stored basis survives mutation; a
  load drops it. That is what makes warm re-solve automatic (D68).
- Settings (`m->cfg`) survive a load, so `set → read → solve` works.

## Inside `jaos_solve` — the orchestrator `jm_dual_simplex`

```mermaid
flowchart TD
    A["jaos_solve — model.c<br/>(null-check, clear err, delegate)"] --> B["jm_presolve_run<br/>(skipped under -DJAOS_NO_PRESOLVE)"]
    B --> C{presolve outcome}
    C -- "SOLVED<br/>every column consumed" --> S1["jm_postsolve_solved<br/>publishes OPTIMAL, 0 iterations,<br/>no solver state ever built"]
    C -- "INFEASIBLE / UNBOUNDED<br/>proved by a reduction" --> S2["jm_postsolve_infeasible_or_unbounded<br/>publishes the verdict, zeroes arrays"]
    C -- "REDUCED" --> T1["target = the reduced model"]
    C -- "NONE<br/>nothing fired" --> T2["target = the caller's model"]
    T1 --> D["sx_init<br/>resolve tolerances · scale if not valid ·<br/>build CSR mirror · allocate solver state ·<br/>build scaled copy of the matrix"]
    T2 --> D
    D --> E{"stored basis present<br/>and basic count == rows?"}
    E -- yes --> W["build_warm_basis<br/>DSE weights ← 1.0 · shift_pending"]
    E -- no --> F["build_initial_basis<br/>slack basis B = −I ·<br/>lend artificial bounds to free columns"]
    W --> R1["run — the dual simplex loop<br/>(04-simplex-iteration.md)"]
    F --> R1
    R1 --> G{outcome}
    G -- OPTIMAL --> H["settle_shifts — repay every cost loan"]
    H --> I["reenter_after_settling — bounded rounds,<br/>keeps the best point"]
    I --> J["classify_optimum —<br/>OPTIMAL / UNBOUNDED / NUMERICAL_ERROR<br/>(06-outcomes.md)"]
    G -- "limit / interrupted /<br/>infeasible / numerical" --> P
    J --> P["publish — unscale, duals by BTRAN,<br/>remember basis, normalise −0.0"]
    P --> Q{presolve REDUCED?}
    Q -- yes --> PS["jm_postsolve_expand<br/>replay the record arena LIFO<br/>back into original indices"]
    Q -- no --> Z["free solver state, return"]
    PS --> Z
```

## Why each stage exists

- **Presolve before anything** removes what no iteration should pay for,
  and can prove infeasibility or unboundedness without building the solver
  at all. It reads the model through a `const` pointer; the caller's model
  is never rewritten.
- **Scaling** (Curtis–Reid, exact powers of two) exists so one badly scaled
  row cannot poison the pivoting; powers of two keep every application
  exact, which bit-identical results require (D8). The stored matrix is
  never modified; the solver works on a scaled copy.
- **The slack basis** is the cold start because its steepest-edge weights
  are exactly 1.0 — a crash basis was measured and refused for destroying
  them (SPECS §3).
- **Publish is the only unscale point**, and it normalises `−0.0` to `0.0`
  so byte-for-byte digest comparison stays a valid no-op instrument (D21).
- **Postsolve replays the reduction record strictly LIFO**, so each undo
  sees exactly the model state its reduction saw.

## The sub-cases

| condition | where it forks | effect |
|---|---|---|
| `-DJAOS_NO_PRESOLVE` (reference build) | compile-time, in `simplex.c` only | presolve and postsolve compiled out; solver runs on the caller's model; the only oracle for output no predicate judges (D96, `jaos-testing`) |
| presolve fires nothing | outcome `NONE` | no reduced model is allocated; publish writes straight into the caller's model |
| presolve consumes every column | outcome `SOLVED` | answer published with zero simplex iterations |
| MAXIMIZE model | `sigma = −1` folded into costs at `sx_init`; flipped back at publish; presolve and checker read the sense themselves (D103) | internally everything is minimize |
| warm basis stored | `build_warm_basis` succeeds | no artificial bounds lent; dual feasibility bought by one cost-shift sweep at the first refresh |
| LP vs MPS input | the caller picks the function | no sniffing; both converge on `jaos_load_lp` |
| work/time limit, callback stop | inside `run` | basis is written and remembered, published statuses then zeroed: resumable, but nothing readable in between (D70, D79) |

## Memory, in three waves

```mermaid
flowchart LR
    A["load wave<br/>model arrays<br/>(until next load or free)"] --> B["derived caches<br/>CSR mirror + scale factors<br/>(rebuilt only when the matrix changes)"] --> C["per-solve wave<br/>presolve workspace + solver state + LU<br/>(freed at the bottom of jm_dual_simplex)"]
```

The six `sol_*` answer arrays and the two `start_*` basis arrays are the
only per-solve products that outlive the solve. `alloc.c` is two functions
with C23 overflow-checked multiplication; there is no arena and no pool.
