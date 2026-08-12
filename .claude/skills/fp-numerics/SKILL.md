---
name: fp-numerics
description: Floating-point discipline for a solver that must be both accurate and bit-reproducible — residual versus error, backward error and conditioning, iterative refinement, compensated summation, when extra precision is admissible and when it silently breaks portability, and how to reason about a tolerance and the space it lives in. Load before changing a tolerance, adding a numerical test, or diagnosing a wrong answer that "should be rounding".
---

# Floating point in a solver

## The first question: residual or error?

These are different quantities and confusing them wastes months.

- The **residual** of a computed solution is how far it fails to satisfy the
  equations: `r = b - Ax̂`.
- The **error** is how far it is from the true solution: `x̂ - x`.

They are related by the conditioning: `‖x̂ - x‖ / ‖x‖ ≲ κ(A) · ‖r‖ / ‖b‖`. On a
well-conditioned system a small residual means a small error. On an
ill-conditioned one, a **tiny residual is entirely compatible with a large
error**, and the reverse also happens.

Standard triangular solves are *backward stable*: the computed answer is the
exact answer to a slightly perturbed problem. That guarantees a small
residual, not a small error. If you need the error small on an
ill-conditioned system, backward stability is not enough on its own.

## Iterative refinement

One step, and it is remarkably cheap for what it buys:

```
solve  A x̂ = b
r  = b - A x̂          (computed against the original matrix)
solve  A δ = r         (reusing the same factorization)
x̂ += δ
```

The correction costs one extra solve plus one matrix-vector product. Even
computed entirely in working precision, this reduces the residual to
approximately the level of the rounding in forming `r` — in practice several
orders of magnitude — provided the factorization is not hopelessly bad.

Three practical points:

- **Compute `r` against the original data**, not against the factors. The
  factors are what carries the error; using them to measure it measures
  nothing.
- **Refine both sides of a primal-dual pair, or neither.** Refining a
  primal solve while leaving the corresponding transposed solve unrefined
  produces a *less* consistent point than refining neither: the two halves
  now disagree about the same basis. This is a real failure mode, not a
  theoretical one.
- **Where you refine is a design decision with a measurable cost.** Refining
  numbers that feed a *choice* — which pivot to take next — changes the
  trajectory, and a trajectory is not more correct for being computed from
  more accurate numbers. Refining numbers that *are* the answer is always
  right. If in doubt, refine at the end and measure whether refining
  throughout helps or hurts; it can badly hurt.

## Extra precision: when it is allowed and when it lies

Before reaching for more bits, establish that the problem is precision at
all. The evidence for "we ran out of precision" is that the error *falls* as
precision rises and the method is otherwise doing the right thing. The
evidence against is usually that a published implementation gets the right
answer in the same precision — in which case there is a defect and extra
bits will merely hide part of it, expensively.

| Type | Mantissa | Cost | Portability |
|---|---|---|---|
| `double` | 53 bits | 1x | universal |
| `long double` | **80-bit x87: 64 bits. On aarch64: often 113 bits or just `double`** | ~1x on x86 | **not portable — the type's width is architecture-dependent** |
| `_Float128` / `__float128` | 113 bits | 10–50x, software | portable where provided |
| double-double (two doubles) | ~106 bits | 3–5x | portable, pure C |
| Compensated summation | exact-ish for sums only | ~2x | portable, pure C |

**The portability row is the one that matters for a reproducible library.**
`long double` on x86-64 is the 80-bit x87 type; on aarch64 it is something
else entirely. Using it in a kernel means the same source produces different
answers on different machines — which is exactly the property a deterministic
solver promises not to have. It is safe in a *checker* or a diagnostic that
is not part of the reproducible path, and unsafe in the solve.

**Compensated (Kahan/Neumaier) summation** is the exception worth reaching
for: it is portable, deterministic, needs no exotic type, and removes the
dominant error term in a long accumulation for about a factor of two. If a
dot product over a long sparse column is losing digits to cancellation, this
is the right tool — not a wider type.

## Cancellation, and what a sum is worth

`a - b` with `a ≈ b` is exact in IEEE arithmetic — the subtraction itself
loses nothing. What is lost is that `a` and `b` were *already* inexact, and
the subtraction exposes it. A sum is known no more finely than the terms that
went into it:

> A computed sum `s = Σ tᵢ` carries an uncertainty on the order of
> `eps · Σ|tᵢ|`. Below that, its value is what is left of cancellation, not a
> number.

This has a direct practical use: **before treating a small computed quantity
as meaningful, compare it against `eps` times the traffic through it** — the
sum of the magnitudes of the terms that produced it. A reduced cost, a row
activity, a residual: each is a sum, and each has a floor below which it says
nothing. A test that omits this floor will act confidently on rounding noise,
and the models where it does are the ones with wide bounds or large
coefficients.

Equally: **an absolute tolerance applied to a quantity computed from terms of
wildly differing scale is simultaneously too strict and too lax**, and which
one it is depends on nothing but the row's scale. A row whose terms sum to
1e10 cannot be checked to 1e-7 absolute by any double-precision computation;
a row whose terms sum to 1e-3 passes that same test while being millions of
ulps wrong.

## Tolerances live in a space, and the space is load-bearing

A solver that works on a scaled copy of the model has two spaces, and every
tolerance belongs to exactly one:

- A test applied in scaled space judges the arithmetic the solver actually
  performs.
- A test applied in the original space judges the answer the caller receives.
- **A quantity crossing between them is multiplied by a scale factor**, so a
  breach that is inside tolerance on one side can be outside it on the other,
  with nothing wrong anywhere.

Before adding a numerical test, say which space its threshold is in and why
that is the right one. And prefer, where it exists, **a quantity that has no
space at all** — a product where the scale factors cancel, such as a
multiplier times the distance it acts over. Such a quantity is identical in
both spaces and cannot be got wrong this way.

## Reasoning about a tolerance change

- A tolerance is a claim about what the arithmetic can distinguish, not a
  knob for making a test pass. If a change to a tolerance fixes an instance,
  the burden is to show the old value was wrong on its own terms.
- Every tolerance needs a measurement on **both** sides: what it costs when
  too tight, what it admits when too loose. A number with evidence on one
  side only is a guess.
- **Widening a feasibility tolerance is almost never the fix**, and in a
  primal-dual setting it can be actively unsound: duality-gap identities
  usually assume feasibility as a hypothesis, so relaxing feasibility does
  not extend the bound, it removes what the bound stands on. An infeasible
  point's term can turn negative and cancel a genuine residue elsewhere.
- When a certificate is a difference of two sums, **publish both halves.** A
  small difference of two large halves and a genuinely small quantity are
  indistinguishable from the difference alone.

## Diagnosing a suspicious answer

**The sequence lives in `jaos-debug`** — six questions in cost order, from
"is the reference right?" to "is it precision?". Load that skill rather than
working from memory; it is the same list in both places otherwise, and two
copies of a procedure are two procedures as soon as one is edited.

What belongs here is the last step, because it is the one people jump to.
Precision is almost never the answer, and the evidence that it *is* the answer
is narrow: **the error falls as precision rises, and the method is otherwise
doing the right thing.** The evidence against is usually that a published
implementation gets the right answer in the same precision — in which case
there is a defect, and extra bits will hide part of it, expensively.

Two prior steps have specifically numerical content, and it is above: step 3
is the tolerance-sweep test, and step 5 is the comparison against the sum of
the magnitudes that produced the residue.

## Determinism obligations

- Compile with FMA contraction off, or the same source gives different
  results on machines with and without a fused multiply-add.
- No reassociation: no `-ffast-math`, no vectorised reduction that sums in a
  different order than the scalar loop.
- No iteration order that depends on an address, and no unseeded randomness.
- `log`/`exp` and friends are not correctly rounded and are not pinned across
  C libraries. If a result depends on one, round it to something coarse
  enough that a last-ulp difference cannot change the outcome — and treat
  that as a claim to be tested across machines, not assumed.
