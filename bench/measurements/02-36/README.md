# The wrong-signed dual step is holding pilot87 up: clamping it costs 3.228x

Taken 2026-08-18, following `TODO.md` §5a item 2. One candidate, one gate run,
one isolation run. **Refused by the gate.** Closed as D127.

## What was tried, and it was not a guess

02-35 established that both exits of `dual_ratio_test` compute

```c
*theta_out = s->d[best] / s->alpha[best];
```

from the raw `d`, while `admit_candidate` had already clamped the numerator
the pick was made on — `rnum[k] = dist > 0.0 ? dist : 0.0`. At HEAD that
reaches **248 of netlib's 477562 picks and 170 of Kennington's 435418**, worst
step 8.37e-09.

The code contradicts its own stated design. The comment above
`dual_ratio_test` says an already-infeasible cost *"blocks at once, and the
step that follows repairs it exactly"*. A step of `d/alpha` with `d` past zero
does not repair it; it carries the breach into every other reduced cost
through `update_dual`, amplified by an `alpha` required only to exceed
`PIVOT_MIN = 1e-9`.

The candidate is `candidate.diff` beside this file: a `blocking_cost` helper
returning the same number the pick used, read at both exits. A free nonbasic
is left alone, because `admit_candidate` calls its distance zero for a
different reason.

## The gate says no

`netlib-diff.txt` beside this file carries all 94 instances. The headline:

```
94 instances compared: 72 bit-identical, 22 moved, 14 digest change(s)

-- REGRESSIONS --
  WORK   pilot87   18818789905 -> 60754965471   (3.228x)
  RSUB   pilot87   suboptimality bound 2.58e-06 -> 3.59e-05  (13.9x, D91 threshold)
```

`pilot87` runs **108973 iterations against 40246**. Two other instances move
the same way and stay inside the bar: `25fv47` 1.109x, `d2q06c` 1.069x. Every
answer is still `optimal` with `checker=ok`, so this is a cost and not a wrong
answer.

## It is entirely the Harris exit

Reverting the Bland exit and keeping the Harris one reproduces the regression
**to the digit**: work 60754965471, iters 108973, digest
`93805090cdc362f8`, and `25fv47` and `d2q06c` unchanged from the full
candidate. Bland's rule is a rare fallback and contributes nothing here.

## Why it costs so much, which is the part worth keeping

Today's tiny wrong-signed step is not doing nothing. `update_dual` applies
`d[v] -= theta_dual * alpha[v]` to every other reduced cost, so a `theta` of
8e-09 with the wrong sign perturbs the whole dual vector slightly. Clamped to
exactly zero, the iteration becomes fully degenerate: no dual movement at all.

`pilot87` is the instance this project already knows oscillates — D25 and D89
were written for it, and D74 measured 2.372x on its iterations from a related
change. **The accidental perturbation is what keeps it moving.** Removing it
triples the work.

That is the second candidate in a row where the apparently-sloppy thing turns
out to be load-bearing; D126 was the first, on the neighbouring line of the
same machinery.

## What stands

**The inconsistency is real and stays unrepaired**, with its size on the
record: 248 netlib picks and 170 Kennington picks take a dual step of at most
8.37e-09 with the wrong sign. Every answer at HEAD is `optimal`,
`checker=ok`, and no instance regresses — so it costs nothing today.

**A zero is the wrong replacement, and that is what was measured.** It does
not follow that no replacement works. A step small enough to keep the
perturbation and signed correctly would be a new constant, and a new constant
needs a sweep on both sides and a row in `docs/tolerances.md`. Nothing here
proposes one.

## Reproducing it

Apply `candidate.diff`, `make clean`, then `make netlib J=12`. The isolation
run is `./build/bench/run -j 4 -b bench/netlib.baseline pilot87 25fv47 d2q06c`
with the Bland exit reverted to `s->d[bv] / s->alpha[bv]`.
