# 02-136 — `jaos_internal.h`'s contracts as asserts, and a breaker that hung instead of firing

`bench/measurements/02-121/jaos_internal.h.md`'s assert list. The contracts are
declared in the header; the asserts live in the files that implement them, so
this touches `src/simplex.c`, `src/presolve.c` and `src/scale.c`.

## The six

| contract | where | what a violation does |
|---|---|---|
| `jm_harris_pick`'s numerators are non-negative, denominators strictly positive | `simplex.c` | a negative numerator widens the window the wrong way; a non-positive denominator divides by zero or flips the quotient's sign. Either yields a wrong pivot, not a crash |
| its return is a valid index whenever `n > 0` | `simplex.c` | callers index with it without testing |
| `jm_pattern_order`'s scratch bitmap is clean on entry and left clean | `simplex.c` | a stale word puts a position in the output that the input never named, and the pattern is what the next FTRAN trusts |
| its output ascends | `simplex.c` | states what the loop shape gives, and catches a rewrite that stops giving it |
| `nbmark` equals `{v : status[v] != JM_BASIC}` | `simplex.c` | a bit out of step drops a candidate from the ratio test or offers a basic one. Maintained by hand at eight sites, read at one |
| a presolve record's `index` is non-negative | `presolve.c` | a garbage index replays into the wrong row of the caller's model and publishes a wrong answer. **The sign only** — see below |
| `jm_postsolve_expand` entered only when presolve reduced, and `reduced` aliases nothing of the caller's | `presolve.c` | replays an arena twice, or writes the answer over the data it is reading |
| every scale factor is an exact power of two | `scale.c` | every scaled quantity carries a rounding no record can see |

## The control

Third in the series after 02-134 and 02-135, carrying both of their lessons
from the start: canary first, and an unmodified arm per instance set, because
presolve's asserts are reached only on the infeasible models.

| arm | fired | which |
|---|---|---|
| `canary`, false by construction | **94** | the canary |
| `live` 94, `live-inf` 29 | 0, 0 | — |
| `harris` | **94** | `num[k] >= 0.0` |
| `pattern` | **82** | `mark[w] == 0` |
| `nonbasic` | **84** | `in_map == (s->status[v] != JM_BASIC)` |
| `scale` | **94** | `frexp(m->row_scale[i], &e) == 0.5` |
| `psindex` | **22** | `rec.index >= 0` |

## The assert that segfaulted, and what the header already said

The first version of the presolve index assert bounded the index against the
original dimensions, through `p->orig`. `jaos_internal.h`'s own comment on
that field reads: *"`jm_dual_simplex` sets it directly; nothing inside
presolve.c needs to."* It is null during the presolve pass, so every test that
drives `jm_presolve_run` directly segfaulted at the assert.

**ASan in `make configs` found it**, which is the whole reason that
configuration exists — the release build compiles the assert out and the plain
`make test` would have passed.

The bound was redundant as well as wrong. The replay already checks the index
per tag, against the right dimension, at the six sites that legitimately hold
`orig`. So the assert reached for a field the design forbids in order to
duplicate a check that was already correct, and the sign test is what is left.

## The breaker that hung

**The first `harris` breaker wrote its negative numerator AFTER the assert
block.** The assert passed on clean data, the corrupted value went into the
ratio test, and the solver took wrong steps for 24 minutes on a set that
takes 85 seconds. It never fired and it never finished.

The give-away was the runtime, not the output. **A breaker that corrupts state
instead of tripping a check does not announce itself** — it produces no
failure, no firing, and no result, which is a fourth way for an arm to be
worthless on top of the three 02-134 and 02-135 found. Every arm has a 600
second timeout now that reports the failure rather than holding the control
open.

Counting the series: three controls, and each was wrong before it was right.
Zero firings on dead arms (02-134), zero firings because the gate could not
reach the assert (02-134 again), zero firings because no instance had the
shape the breaker needed (02-135), and now a hang instead of a firing. **The
asserts were correct in every case.** What keeps being wrong is the thing that
tests them, which is the argument for testing it.
