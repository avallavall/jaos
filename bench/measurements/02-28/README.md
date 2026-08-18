# pilotnov's wrong answer is the solve, not the model, and the termination test is why

Taken 2026-08-18, answering `TODO.md` §4c, which D118 opened. Two files here:
`trace.c`, a driver that solves one instance with the summary log on and prints
what the checker says; and `sweep-refactor.txt`, the run.

## The question

D118 refused giving the implied free column singleton first refusal over D95's
bounded cost-0 singleton column. The refusal was on `pilotnov` alone: it
published `obj=-3169.53` against a reference of `-4497.28`, **29% wrong**, and
still reported `optimal`. The point was primal-feasible with a dual violation
of 0.89. `pilot-ja` loses seven rows to the same change and is untouched.

Two possibilities, and they lead in opposite directions. Either presolve cut
off the optimum — D106's substitution is unsound on that population — or the
reduced model is fine and the **solve** failed on it.

## The answer: the solve, and it is provable in one sweep

`sweep-refactor.txt`. Same candidate, same reduced model, four values of
`REFACTOR_EVERY`, each its own binary with its own md5 printed beside it.

| `REFACTOR_EVERY` | iterations | work | objective | dual viol | stability rebuilds |
|---|---|---|---|---|---|
| HEAD, no candidate | 2374 | 86587427 | -4497.2761882188706 | 0 | 0 |
| **64 — what ships** | 87432 | 2616239810 | **-3169.5271937202242** | **0.89** | **156** |
| 16 | 28859 | 1345562616 | **-4497.2761882188715** | 0 | 0 |
| 8 | 2741 | **89348539** | -4497.2761882188752 | 0 | 0 |
| 4 | 2287 | 84697175 | -4497.2761882188743 | 0 | 0 |

**At 16 the candidate reaches Koch's published optimum to the last bit.** At 8
and 4 it is correct to a relative 8.1e-16 and 6.1e-16.

So presolve did not cut off the optimum. D106's substitution on those seven
columns is sound, and the reduced model has the right answer in it.

**And the reduced model is not intrinsically harder.** At `REFACTOR_EVERY = 8`
it costs 89348539 work units against HEAD's 86587427 — **1.032x** — where at 64
it costs 30.2x. The 30x is the refactorization interval, not the model.

## What actually goes wrong

The failure signature at 64 is numerical and says so: **43041 weight restarts
and 156 stability rebuilds**, against HEAD's 1042 and 0 on the same instance
and `pilot-ja`'s 0 and 0 under the same candidate. `pilotnov` was already the
sensitive one; the candidate pushes it over.

Then the solve stops and calls it optimal. **The termination test reads primal
feasibility only.** `src/simplex.c:3455` stops when no basic variable violates
a bound, and D20's second opinion re-reads that same test against a fresh
factorization (`src/simplex.c:3487`). Dual feasibility is an invariant the dual
method maintains and **nothing re-reads it before the verdict is published**.
So a solve whose invariant has been damaged terminates primal-feasible,
dual-infeasible, and reports `optimal`.

The checker caught it — `dual=0`, `dualviol=0.89`, `gap+=369699`. No digest
comparison and no work bar would have: the answer is feasible and
deterministic.

**Postsolve is not the source of the 0.89.** All seven columns have cost
exactly zero, so D106 sets `y_i = c_j / a_ij = 0` for each of their rows and
its cost transfer `c_k -= y_i * a_ik` subtracts nothing from any surviving
column. Those records contribute zero to every dual. The violation is in the
reduced solve.

## This is about the shipped code

The termination test is HEAD's. Nothing here changes it and no instance of the
139 reaches the state today — that is why the gate is green. What the candidate
did was find a model that reaches it, and the candidate is refused for other
reasons.

`TODO.md`'s standing debt already says the `REFACTOR_EVERY` trajectory sweep is
manual and that three of M1's four defect closures came from it. This is the
fourth.

## What this does NOT say

**It does not say `REFACTOR_EVERY` should be lowered.** One instance is not a
population. The interval is a global constant and every instance pays it; the
sweep here is a diagnostic on one model, not a proposal. `docs/tolerances.md`
would need a sweep on all three sets before any value moved.

**It does not un-refuse D118.** At the interval that ships, the candidate still
publishes a wrong answer, and the gate is still red. What changes is the
reopen condition, which was written as "a fifth restriction on D106" and is now
known to be the wrong place to look.

## One reading in this file that is not a defect

`trace.c` judges with an absolute tolerance of 1e-7 and prints
`checker primal=0` for HEAD's own `pilotnov`, whose row violation is 1.93e-7.
The gate's verdict reads the **relative** row test, which is 9.32e-13 there and
`ok`. The driver's tolerance is the difference, not the answer.

Handed to `TODO.md` §4c.
