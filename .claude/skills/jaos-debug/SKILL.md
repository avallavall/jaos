---
name: jaos-debug
description: How to find out why one instance out of many gives a wrong answer, in a solver where the failing run takes tens of thousands of iterations and no unit test can reproduce it. Load before instrumenting anything. Covers the throwaway-diagnostic-build pattern, what to dump and in what order, the traps that make instrumented numbers lie, and the sequence of questions that has actually located defects here rather than the one that feels natural.
---

# Debugging one bad instance

The situation: a hundred models solve correctly and one does not. There is no
unit test that reproduces it — the failing solve takes fifty thousand
iterations on a matrix with thousands of rows, and the state that matters
appears at iteration 49962. Ordinary debugging does not apply.

## Ask the questions in this order

This sequence is owned here; `fp-numerics` points at it rather than repeating
it. Each step is cheaper than the next, and skipping to the interesting one is
how weeks get spent on the wrong thing.

1. **Is the reference right?** A model disagreeing with a published optimum
   is not evidence about which of them is wrong. Check the source, and check
   for a convention it may omit — an objective constant, a sign, a scaling.
2. **Does the reported number mean what you think?** A Lagrange multiplier's
   magnitude is not a distance. A large multiplier on a perfectly satisfied
   condition is normal. Read the definition of the quantity that is being
   printed before theorising about its size.
3. **Does it move with the tolerance?** Judge the *same* saved answer at
   several thresholds. A violation that shrinks in proportion to the
   threshold is a measurement artefact; one that does not move at all is
   real. This single test has settled several cases here in minutes.
4. **Do two computations of the same quantity agree?** If the solver's
   carried value and an independent recomputation differ, the defect is in a
   computation, not in a bound test — and you have just localised it to a
   solve.
5. **Compare against the terms.** Relative to the sum of magnitudes that
   produced it, is this residue one ulp or ten million? A sum is known no
   more finely than its terms.
6. **Only then** consider precision, and see `fp-numerics` for what would
   count as evidence. It is almost never the answer.

## The throwaway diagnostic build

Never leave instrumentation in the tree, and never measure with a build that
carries it by default.

```
gcc-14 -std=c23 -Wall -Wextra -ffp-contract=off -O2 -g -DNDEBUG \
       -DJAOS_DIAG -Iinclude src/*.c bench/run.c -o build/diag/run -lm
```

- Guard every hook with `#ifdef JAOS_DIAG` so the normal build cannot
  possibly change.
- Build to a separate output directory so the release objects the gate uses
  are untouched.
- Write the patch as a **script** that applies and reverts, kept outside the
  repository. Hand-editing instrumentation in and out of a large file is how
  a debug `fprintf` reaches a commit.
- Revert before running any campaign. A gate result is only valid for the
  tree that produced it.

## What to dump, and in what order

Start with the shape of the failure, not the values:

- **A trajectory, not a snapshot.** One line per outer round or per phase:
  the objective, the iteration count, how many violations stand, the worst
  one. A single dump of the final state tells you where it ended; the
  trajectory tells you whether it converged, oscillated, or stopped because a
  loop counter ran out. On a real case here the trajectory showed the error
  was already present in round 0 and the following twenty-five rounds bought
  almost nothing — which relocated the entire investigation.
- **Then the offenders, with every number the code's own tests read.** For
  each violating entity, dump not just the violation but each input to each
  predicate that decides what happens to it — the bounds, the scale, the
  magnitudes, the computed threshold. The defect is very often that a
  predicate is reading a quantity that is not what it thinks.
- **Then the outcome of each decision.** Count how many candidates entered a
  loop and how many were acted on, and log *why* each was skipped. "Twelve
  candidates on entry, one action, zero candidates on exit" is a finding that
  no amount of staring at values would have produced.

Prefer counts and per-round summaries you can `grep` over per-iteration
dumps you cannot read.

## Traps that make instrumented numbers lie

- **Applying a change and undoing it is not bit-exact.** `x += d` then
  `x -= d` does not restore `x` in floating point. A build that does this to
  "measure without changing" has changed: its iteration counts are not
  comparable to a baseline. Use it to read a residual; never to compare a
  trajectory.
- **Instrumentation that consumes shared scratch** will corrupt the very
  state you are observing. Allocate your own buffers in the diagnostic code.
- **Printing costs work units if you bill it.** Do not add anything inside a
  billed kernel that changes the count, or the run stops being comparable.
- **A diagnostic build's output is not a result.** Confirm every finding with
  a clean build before writing it down.

## Localising in a long solve

- Bisect by iteration: dump a summary every N iterations first, find the
  interval where the quantity goes wrong, then dump densely only there.
- Look for **exact repetition**. A solve that repeats bit for bit from some
  iteration is cycling, not stalling, and the two have completely different
  cures. Comparing a full state hash across iterations finds it immediately;
  reasoning about progress does not.
- When a defect cannot be reproduced small, **test the family rather than the
  case**. Some states — a basis that goes singular only after thousands of
  accumulated updates — cannot be constructed in a unit test at all. What can
  be tested is the class of input that leads there and the handling once it
  happens.

## Before writing the finding down

- Reproduce it on a clean build.
- Say what the defect *is*, not where it appears. "The row is 1.7e-6 outside
  its bound" is a symptom; "the carried activity and its recomputation
  disagree, so it is a residual of the basis solve" is the defect.
- State what you ruled out and how, because the next person will otherwise
  re-run it. Wrong hypotheses that were measured are worth recording — each
  one is what pointed at the next.
