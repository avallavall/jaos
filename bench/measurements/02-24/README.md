# Directed rounding on the activity-range readings: two designs, both refuted

Taken 2026-08-17 in a git worktree (`directed-rounding`), while the `plato-pds`
campaign held the main tree. `candidate.diff` beside this file is the change
that produced the numbers below. **Nothing landed.**

The motivation is `docs/research/dual-postsolve-imposed-bound.md` §11f and §13:
Fourer & Gay 1994 fixed AMPL's presolve declaring solvable models infeasible —
on `maros`, one of D97's own four — by computing the activity bounds with IEEE
directed rounding rather than by widening a tolerance.

## Design 1: widen both ends outward, drop every window

`min_act` rounded down, `max_act` rounded up, all four comparisons bare. Sound
by construction. **Refuted by `make test` in under a minute.**

    test_presolve.c:1937:test_forcing_row_round_trip:FAIL: Expected 0 Was 2
    test_presolve.c:2280:test_activity_range_counters_are_exact:FAIL: Expected 1 Was 0

`make_forcing_row_model` is `x0 + x1 <= 0` with both columns in `[0, 10]`. Its
minimum activity is exactly 0 and its upper bound is exactly 0, so the row
forces both columns to 0. **One ulp of outward widening declines it.**

That is the whole objection and it generalises: **the FORCING test detects an
equality, not an inequality.** Outward rounding makes an inequality proof
survive rounding; it destroys an equality detection. The interesting case for
forcing is attained exactly, and any widening in the safe direction loses it.
`x0 + x1 <= 0` over non-negative columns is not a corner case — it is the
canonical shape the family exists for.

## Design 2: outward only where the reading proves an inequality

The three readings do not ask the same kind of question:

| reading | asks | wants |
|---|---|---|
| INFEASIBLE | no point satisfies the row | outward |
| FORCING | the range's end **equals** the row bound | inward |
| REDUNDANT | every point satisfies the row | outward |

So design 2 leaves FORCING alone, keeps INFEASIBLE (which is bit-identical
either way, since `min_act - err > ru` is `min_act > ru + err`), and makes only
REDUNDANT sound: `min_act - err >= rl && max_act + err <= ru` in place of
`min_act >= rl - err && max_act <= ru + err`.

That is a real defect being fixed. The old form drops a row whose minimum
activity is within `err` **below** `rl` — a row that can still bind. It is the
failure Fourer & Gay describe as AMPL "discard[ing] constraints that kept the
problem from being unbounded".

`make test` and `make sanitize`: **0 failures**, both.

### The gate says no

94 instances, `J=6`, against the committed baseline:

    94 instances: 94 solved, 94 shape ok, 94 objective ok,
                  94 checker ok, 94 deterministic, 0 failed
    gate: PASS

    -- against baseline --
    pilotnov  REGRESSED  work: 86587427 -> 2378158900 (27.5x),
                         iters 2374 -> 63240
    1 regressed, 0 improved, 0 new

**27.5x on one instance**, against a bar of 2.0x. Every other instance is
unmoved.

### The mechanism, named rather than inferred

The same instance through both binaries:

| | presolve | rows removed | iterations | work |
|---|---|---|---|---|
| HEAD | `975/2172/13057 -> 874/1811/11768` | **101** | 2374 | 8.659e7 |
| candidate | `975/2172/13057 -> 906/1811/11876` | **69** | 63240 | 2.378e9 |

**32 rows.** Columns are identical at 1811 on both sides, so nothing else in
presolve moved. Thirty-two rows that survive instead of being dropped cost
60866 iterations and 2.29e9 work units. This is the D108 and D112 class: a
reduction whose effect is on the trajectory, not on the reduction site.

**The answer is unchanged and the residuals are better.** Both builds return
`-4497.2761882188706` to the last bit, and the candidate's row residual is
`3.09e-10` against HEAD's `1.93e-07`, its relative row residual `7.9e-14`
against `9.32e-13`. So the 32 rows buy a numerically cleaner answer at 27.5x
the cost.

## The verdict, and what would reopen it

**Refused.** The unsoundness design 2 removes is real but bounded by `err`
(8 ulps of the row's traffic), and no instance in this repository has ever been
shown to give a wrong answer because of it. What removing it costs is measured
and it is 27.5x on `pilotnov`.

Reopens when either:

- an instance is found where a wrongly-dropped redundant row produces a wrong
  answer, a wrong verdict, or a checker rejection — the loud version of the
  defect, which is what the theoretical argument predicts and nothing has yet
  exhibited; or
- `pilotnov`'s 27.5x is shown to be trajectory rather than the reduction, and a
  rule separates the 32 rows from the 69 that still fire. That is D108's
  reopen condition in a new place, and D108 refused exactly that rule once.

## What this does not refute

Fourer & Gay's own result stands: their `maros` failure was real and directed
rounding fixed it. What this measures is that **JAOS is not in that position**,
because D103 already replaced the judgement constant with the error bound, and
the residual unsoundness is 8 ulps rather than AMPL's pre-fix tolerance. The
`docs/research` §13 claim that this change would be "a no-op or a small gain"
was wrong in both designs, and is corrected there.
