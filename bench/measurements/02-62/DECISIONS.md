
## D153 — The row-activity check finds a real one: pilotnov's published point does not satisfy its own equality row

**The question, as asked.** `numerics-reviewer`'s own proposal, standing in
`TODO.md`: two replay producers assign `sol_row[i]` outright where every
other producer accumulates, both correct today by an argument about arena
order that nothing checks, and the class has already cost one campaign
(D106). The enforcement it proposed is one pass at the end of postsolve
recomputing every row's activity from `sol_col`. Expected: a clean pass,
turning an unchecked argument into a check. **D152 is what made it
runnable** — before it, eleven of the 94 aborted before reaching any new
assert.

**The measurement** (`bench/measurements/02-62/`). The instrument was
validated against two injected faults before it was believed on anything:
an empty row overwriting a share that already arrived fires on 45 of 94,
a published activity moved by 1.0 fires on 86 of 94, and both are silent
with asserts off — so the aborts are the check and not the fault breaking
something else first.

**It does not pass clean. 138 of 139 do; `pilotnov` does not.** Its row
931 is an equality at zero with **three** nonzeros whose published
activity reads exactly 0.0 while the published columns make -1.93e-07.
Against the row's traffic of 4.15e6 that is 4.6e-14 relative, about 100
times what a three-term sum can accumulate (the bound is 2·eps ≈
4.4e-16). Eighteen of its 36 disagreeing rows survive the correct error
bound, worst 131x on a row of five nonzeros. No answer is wrong today —
the checker judges relative to scale and the gate reads `checker=ok` —
but the published point does not satisfy a constraint the published row
activity claims it satisfies.

**What was refuted.** Two windows, both the obvious thing to write:

- A fixed multiple of eps times the traffic. It also fired on `osa-30`
  and `osa-60`, whose rows carry **72554 and 173365 nonzeros**. A naive
  sum of n terms is bounded by `(n-1)·eps·Σ|t|`, not by a constant times
  eps; taking the n out leaves **0 rows disagreeing on both**. The
  window's shape was wrong and the solver was not.
- The check with no OPTIMAL gate. It fired on all 29 netlib-infeas
  instances. Both call sites run the replay whatever the verdict, because
  the index mapping is owed even for a stopping point, and on that path
  `sol_col` and `sol_row` are not required to agree. Asking them to finds
  the convention, not a defect.

A third false alarm was the harness: the first run reported 29 of 29
infeasible instances failing because the script omitted `-e infeasible`.
It now separates an assert abort from a gate failure.

**Why it ships opt-in.** Behind `-DJAOS_VERIFY_ACTIVITY`, deliberately
not in a plain assert build. D152 had just bought the property that all
94 standard instances run under `-UNDEBUG`, and trading that away for a
check reporting one already-open defect is a net loss until the defect is
repaired. It is inert by default and that is measured: the compiled
objects without the flag have the same md5 as HEAD's.

**What is left open, in `TODO.md`.** `pilotnov`'s disagreement, with the
row named and the numbers recorded. Whether it shares a mechanism with
D118's 29%-wrong publish and D119's refactorization-interval finding is
not established and is not claimed. The check becomes an invariant, under
`#ifndef NDEBUG`, the day it is repaired.
