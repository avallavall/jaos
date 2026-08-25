# 02-101 — the primal method's first version, and the two things that made it a no-op

2026-08-25. `TODO.md` §0 stage 1. Two defects were found by instruments rather
than by reading, and both produced a passing test suite while the primal
simplex did nothing at all.

## The first: the tests were satisfied by the dual

`run-negative-control.sh` doctors `run_primal` to declare optimality
immediately, without a single pivot, and runs the suite.

**Before the fix, all four primal tests still passed.** The settling re-entry
calls `run()`, the dual repaired the point, and the answer, the independent
checker and `jaos_iterations() > 0` were all satisfied by the wrong algorithm.
A test asserting a correct answer cannot tell which method produced it.

The repair is a count only `run_primal` can raise — `n_primal_iters`, reported
on the closing summary line — and assertions on that instead. Re-run of the
same control afterwards:

```
test_the_primal_reaches_the_optimum_from_a_feasible_basis:FAIL:the primal
    method did not take a single pivot
test_the_primal_and_the_dual_agree_on_the_same_model:FAIL:the primal solve
    took no primal iterations
```

Both fail doctored and both pass clean. That is the instrument validated in
the direction that matters.

## The second: the warm start shifted away the primal's work

`basis-census.c` builds a two-row model presolve leaves alone, enumerates
**every one of the 24 bases it admits**, forces the primal on each, and reads
the primal iteration count off the summary line.

**Before the fix, every accepted basis gave `0 primal iterations`** and the
correct answer — 10 of them from points that were primal feasible and plainly
suboptimal, including `(0, 5)` at an objective of 15 against a true 2.

The cause is one line. `build_warm_basis` arms `shift_pending`, and the next
`refresh` pushes every breached reduced cost onto the feasible side by shifting
the cost behind it. **The dual needs that** — it requires dual feasibility to
start, and a warm basis carries no such guarantee. For the primal it removes
the work: dual infeasibility is exactly what the method consumes, so a sweep
that zeroes every breach hands the loop an optimal point on arrival.

`run_primal` clears `shift_pending` before its first refresh. After:

```
cols  rows   status   obj   primal-iters
BU    BL     optimal  2     1   <== USABLE
BU    BU     optimal  2     1   <== USABLE
LB    BL     optimal  2     1   <== USABLE
LB    BU     optimal  2     1   <== USABLE
LB    LB     optimal  2     1   <== USABLE
LB    UB     optimal  2     1   <== USABLE
LU    BB     optimal  2     1   <== USABLE
UB    BL     optimal  2     1   <== USABLE
UB    BU     optimal  2     1   <== USABLE
UL    BB     optimal  2     1   <== USABLE
```

Ten of the 24 now do real primal work and every one reaches the optimum. The
rest are refused by `jaos_set_basis` as not bases, or are primal infeasible and
refused by the method itself.

## A third thing, found on the way and not part of stage 1

**The simplex's error messages never reached the caller on any model presolve
had reduced.** `jm_set_err(s->m, ...)` writes to the model the simplex ran on,
which is `p.reduced`; the caller holds `m`. Measured on the standard set:
exactly **8 of 94** instances carried a message through, and those eight are
precisely the ones whose `presolve=` column is unchanged — `degen2`, `degen3`,
`fit1d`, `fit2d`, `scsd1`, `scsd6`, `scsd8`, `truss`.

The messages it lost are the ones a caller most needs: the iteration guard's
"this is a JAOS defect", and `classify_optimum`'s refusal to answer past a
bound phase 1 lent. The driver now copies the message out on the error path.
After the fix all 94 carry it.

## What the primal's reach actually is

`build/bench/primal -j 12` over the standard set:

```
measured 0, skipped 0, unreached 94, disagreed 0, rejected 0, errors 0
  94 of 94 could not be started: no primal phase 1 yet, and a cold basis is
  dual feasible rather than primal feasible.
```

**Zero, from a cold start, exactly as §0 predicted.** That is the number stage
4 exists to change, and it is worth having written down before it moves.

## Gate

`make configs` — **all 5 configurations build and pass**, which is the rule
because `tests/` changed. All three sets `0 regressed, 0 improved, 0 new`, and
`git diff --stat bench/results/` **empty**: every record byte-identical. An
unread switch and a method nothing reaches must cost nothing, and it does.
