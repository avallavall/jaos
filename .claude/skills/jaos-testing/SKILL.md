---
name: jaos-testing
description: Designing tests for numerical code where the dangerous failures are wrong answers rather than crashes. Load before adding or changing tests, and before trusting a green gate about anything the checker never reads — a basis, a published status, a verdict on a small model. Covers testing the thing rather than the wrapper, the -DJAOS_NO_PRESOLVE reference build as the only oracle for output no predicate judges, validating an instrument before believing it is green, building the case a predicate must reject, pinned change-detector tests, what to do when a defect cannot be constructed small, and why a passing suite has repeatedly failed to catch real defects here.
---

# Testing a solver

A crash is the easy failure. The dangerous one is a confident wrong answer,
and it passes every test that only checks the code ran.

## Check the thing, not the wrapper

The question a test must answer is whether the *answer* is right, not whether
the function returned a status and filled a buffer. For a solver that means
verifying the solution against the original problem, independently of the
solver's own bookkeeping: activities within row bounds, values within column
bounds, dual signs, complementary slackness, primal and dual objectives
agreeing.

Independence is what gives that value. A verifier that shares the solver's
data structures, its scaling, or its arithmetic will agree with it about a
wrong answer. Verify against the model **as loaded**, in the original space,
with its own accumulation.

The one thing such a check cannot catch is a reader that built the wrong
model — then verifier and solver agree about the wrong problem. That gap is
closed only by an externally published reference value for a named instance,
which is why those references are load-bearing rather than convenient.

## Half the published answer has no test at all, and one build is its oracle

`jaos_check_solution` never reads a basis status. It recomputes activities
from `col_value` and reduced costs from `row_dual` (`src/check.c`), and the
gate's digest is taken over `x` and `y` only (`bench/run.c`). So **no
predicate any of the three sets reports can see a wrong basis.** A change
that moves only the statuses is invisible to a green gate, to a digest
comparison and to the determinism check, which re-solves cold.

The instrument that does see it is a second build of the same model with
`EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`. Presolve compiles out, postsolve never
runs, and what the solver publishes is the reference the reduced path has to
agree with. Use it whenever a question is about published output the checker
does not judge — statuses, a basis count, a verdict on a small model — and
whenever a number looks wrong and you have nothing to compare it against.

Three findings came from it on 2026-08-14 (D99), and none of them was
reachable from the suite or the gate:

- A frozen surviving row was published `BASIC`, with a paragraph of correct-
  sounding reasoning beside it in the source. The reference published
  `AT_LOWER` and a basis of the size `jaos.h` promises. Defined is not
  correct.
- A model with no feasible point came back OPTIMAL with a column violation of
  93. The reference said INFEASIBLE, which turned "strange number" into "a
  defect, and here is the model that shows it".
- It is what separated "the remaining rejections are dual and unrelated to
  this change" from a guess: all five came back with `dual=0` compiled
  without presolve.

Two things make it an instrument rather than a flag:

- **`make clean` between the two settings.** `make` does not see a `CFLAGS`
  change (D82), and without the clean you measure one binary twice — which
  reads as a perfect agreement between the two builds.
- **A canary that must move.** With the flag on, the `presolve=` field must
  come back UNREDUCED on both sides of its arrow. If it reduced, the flag
  never took and everything under it is worthless.

Build outside the repo tree and pass an absolute path to `-o` and to `-c`.
Two stray binaries landed in the repo root during D99's work, from a relative
`-o` after a failed `cd`, and nearly went into a commit.

## A green result is not a proof

This has a receipt. A checker rule here once passed the entire unit suite and
every instance in the reference set **while certifying the whole feasible
region as optimal**. It was green, it was a regression-free diff, and it was
completely wrong.

So, whenever you change a predicate, a tolerance or a check:

> **Build the case it must reject, and confirm that it does.**

A test that the good input passes is half a test. The half that matters is
the input that must fail, especially the one that would pass under the
mistake you were most likely to make. Write that one first.

## Validate the instrument before believing it

A fuzzer that finds nothing is worth nothing until it has been shown able to
find something. The same is true of a determinism harness, a checker, a
residual monitor — anything whose output is "all clear".

The technique: **inject a real fault and confirm the instrument catches it.**
Change an off-by-one in a bounds check, perturb a value, break an invariant
— then run the instrument and require it to fail. Revert. Now its green
result means something.

Record which injected fault was used and where it was caught. A future reader
needs to know the instrument was calibrated, not just written.

## Pinned change detectors

Some properties have exactly one correct value for a fixed input, and pinning
it turns any unintended change into a test failure. The deterministic work
count for a small fixed model is the example here: it is one exact number, so
any change to what is charged moves it.

Two disciplines make this useful rather than annoying:

- **Say in the test what the number means and what last moved it.** A pinned
  constant with no note is re-pinned thoughtlessly the first time it fails.
  A pinned constant whose comment reads "last moved by X: 4411 → 8517, which
  is the extra factorization it costs" makes the diff of that constant a
  record of what each change did.
- **Re-pin deliberately, never reflexively.** If it fails and you did not
  intend to change the accounting, that is the bug it exists to catch.

The weak form to avoid: asserting `work > some_floor`. A single factorization
satisfies that, so deleting every other charge leaves it green.

## When the defect cannot be built small

Some states only arise after thousands of accumulated operations — a basis
that goes singular from drift at a scale no hand-built matrix reaches, a
cycle that begins at iteration three thousand. No unit test constructs them.

Do not pretend otherwise, and do not skip the test. **Test the family
instead**: the class of input that leads to the state, and the handling once
the state exists. If the danger from a rank-deficient basis is a wrong
verdict rather than a crash, build rank-deficient inputs and assert the
verdict. Note in the test why the real trigger is not reproduced, so the next
reader does not assume it is covered.

## Tolerances in tests

- Assert against a tolerance that means something, not against `1e-9`
  because it happened to pass. Say where the number comes from.
- Comparing floating-point results for exact equality is right when the
  claim is determinism, and wrong when the claim is accuracy. Know which one
  the test is making.
- A test asserting an exact objective to sixteen digits is asserting
  bit-reproducibility of the whole path. That is a valuable thing to assert
  and a painful thing to debug; keep it, and keep it separate from the test
  that asserts correctness to a tolerance.

## Sanitizers are part of the suite, not a special occasion

Memory and undefined-behaviour errors in numerical code very often present as
slightly wrong numbers rather than as crashes. Run the whole suite under
address and undefined-behaviour sanitizers, and treat a sanitizer failure as
a failing test.

A fuzzing corpus is only meaningful under a sanitizer. Without one, a fuzzer
proves the code did not segfault, which is not the property in question.

## What a good test here looks like

- It names the property it is protecting, in a comment, in one sentence.
- It fails for exactly one reason.
- It would have caught a real defect, and if it was written after one, it
  says which.
- If it enforces an invariant that some other code depends on, it is an
  assert or a test — **not a comment**. A comment stating an invariant has a
  failure rate of 100% given enough time, and this codebase has a documented
  case of a correct, prominent warning comment being violated by new code and
  costing weeks.
