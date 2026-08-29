# 02-135 — the asserts D221 adds to `src/check.c`, and a breaker that was a statement about the models

`bench/measurements/02-121/check.c.md` listed the contracts the comment purge
kept as prose. This is the assert half: four of them, each with the arm that
proves it live.

## The arms

Built on 02-134's shape, with its three lessons in from the start: the canary
comes first, every breaker is read against the code it breaks, and every arm
names the set it runs.

| arm | what it breaks | set | fired | which assert |
|---|---|---|---|---|
| `canary` | an assert false by construction | 94 | **94** | the canary — `-UNDEBUG` reaches the compiler |
| `live` | nothing | 94 | 0 | — |
| `live-infeas` | nothing | 29 | 0 | — |
| `magnitude` | `split_term` stops negating the negative half | 94 | **91** | `a.pos >= 0.0L && a.neg >= 0.0L` |
| `step` | `certified_step` stops clamping room at zero | 94 | **86** | `t >= 0.0L` |
| `loosen` | one declared lower bound written looser | 94 | **94** | `cl[j] >= m->col_lower[j]` |

## The breaker that fired nothing, and why that was not about the assert

The first `loosen` breaker dropped the `lim < cu[j]` test at one of the two
upper-bound write sites. **It fired 0 times, and the assert was fine.** It
could only bite on a column whose lower bound is infinite and whose upper
bound is finite — the outer loop skips a column with both finite — and then
only where the implied limit happened to exceed the declared one. No column of
the 94 meets both conditions.

0 of 94 was a statement about the models. That is 02-134's lesson arriving a
second time, in a different disguise: there the gate could not reach the
function at all, here it could not reach the *shape* the breaker needed.

The breaker writes the violation directly now, on the first column with a
finite lower bound. Whether real code could produce it is a different
question; an assert is a tripwire and what has to be shown is that the
tripwire is live and correctly placed. It pays for itself twice: firing on all
94 also proves `implied_bounds` runs on these instances, which no `live` arm
can say.

## What the four asserts are for

- **`a.pos`/`a.neg` and their `_model` pair are magnitudes.** `split_term`
  negates the negative half on the way in, so a sign fault there would read as
  a plausible smaller gap and nothing else. The gap is their difference, which
  is only a gap because both are magnitudes.
- **`certified_step` is only called where the column's opposite bound is
  infinite.** The caller's `drops` test is the same condition; the assert
  states it at the callee, where the function's own comment claims it.
- **`certified_step` never returns a negative distance.** A negative would
  turn a certified suboptimality into a claim that the point is better than
  optimal.
- **`implied_bounds` only ever tightens.** A loosened bound enlarges the
  region the gap is measured over, and the verdict would certify a point the
  model does not contain.
