# `jaos_basis` publishes something that is not a basis on 70% of solves

Taken 2026-08-18, confirming on the gate what `02-40/` found in the warm
campaign. Closed as D131.

## The question

`02-40/` measured 61 of 88 warm-campaign solves starting from a stored basis
whose basic count is not `num_row`. That was read at the top of a second
solve, so it is what a previous solve had published — but it was read through
the warm driver, after a bound change, on a model presolve had already
reduced once.

This asks the gate directly: at the end of `jm_dual_simplex`, on `p.orig` —
the caller's own model, after postsolve has expanded into it. That array is
the one `jaos_basis` hands back, verbatim:

```c
jaos_status jaos_basis(const jaos_model *m, jaos_basis_status *col_status,
    jaos_basis_status *row_status)
{
    ...
    if (m->solve_status != JAOS_SOLVE_OPTIMAL || m->sol_col_status == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    memcpy(col_status, m->sol_col_status, ...);
```

## The answer

| set | optimal solves | count exact | **wrong** | over | under | worst over | worst under |
|---|---|---|---|---|---|---|---|
| netlib | 188 | 56 | **132 (70%)** | 72 | 60 | 596 | 169 |
| Kennington | 32 | 8 | **24 (75%)** | 8 | 16 | **12104** | 406 |
| infeasible | 0 | — | — | — | — | — | — |

The infeasible set publishes no basis at all, which is correct: `publish`
memsets the arrays on every path but OPTIMAL, and `jaos_basis` refuses to
return anything unless the status is OPTIMAL.

**Seventy percent of netlib's optimal solves and seventy-five percent of
Kennington's publish a basic count that is not `num_row`**, in both
directions. `12104` too many on a Kennington model is not a rounding artefact
or an off-by-one; it is a different object from a basis.

## Why this is a defect and not a convention

`src/model.c` says what a basis is, in its own words, twice:

> a model with n rows needs n basic variables. That is exactly what
> `jaos_set_basis` enforces on a basis handed in, and it is **structural** —
> no later event makes a wrong count right.

> Checked here: … that exactly `num_row` of them are basic. Those are
> **structural** — they say whether the thing handed over is a basis.

`jaos_set_basis` refuses one whose count is wrong. `basis_survives_or_goes`
clears one that becomes wrong. **`jm_model_remember_basis` checks nothing**,
and `publish` writes `sol_col_status`/`sol_row_status` from the reduced
model's `s->status` before postsolve expands them into the caller's indices.

So the solver publishes, and stores for its own next solve, something it would
refuse from a caller.

## What it does not affect

**No answer is wrong and the gate is green.** Values, objective, duals,
reduced costs, the independent checker and every solution digest are
unaffected — the basis is published beside the answer and nothing in the
solve reads it back except `build_warm_basis`, which refuses it. That is
exactly why 21 `src/` commits passed with nobody noticing (D129), and why no
predicate on any of the three sets moves.

**And it is the cause of D129's lost warm starts**, not a separate defect.
`build_warm_basis` rejecting the count is the symptom; this is the source.

## What is left open

The repair, which is now two questions rather than one:

- **`publish` and postsolve should produce a basis.** That is where the count
  is decided, and `02-40/` shows presolve's own mapping is exact, so nothing
  is to be fixed there.
- **`jm_model_remember_basis` should check.** A one-line guard makes the
  invariant honest, and on its own it changes nothing measurable: a stored
  basis that fails the count is already rejected by `build_warm_basis`, so
  clearing it earlier reaches the same cold start. It belongs with the repair
  above rather than instead of it.

`TODO.md`'s standing debt names a single postsolve family and a minimum case
of one status. **The measurement is 132 solves and a worst error of 12104**,
so the minimum case is a corner of this rather than a description of it.

## Reproducing it

`run-published-basis.sh`, beside this file. The enum is compared symbolically
and not by its integer value: guessing that value printed "0 optimal solves"
on a set where 94 instances reach the optimum, which reads exactly like a
clean result. `src/` is read and never written.
