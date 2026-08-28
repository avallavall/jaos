# 02-130 — D211's refusal expired at D212, and the script that says so could not have told you

`make refusals` reported `D211 FLIPPED` on 2026-08-28, the first time it was
run since D212 landed. This directory attributes the flip to a commit and
records the instrument defect found on the way.

## What D211 refused

Stage 8d asked whether phase 1 should stop when its own objective rises rather
than grind to the work limit. In exact arithmetic a sum of bound violations
never rises under a correct pivot; in floating point it does, because `xb` is
recomputed from the factorization every 64 updates and the recomputation
differs from the carried values by rounding.

D211 refused the stop rule on a measurement: `pilot-ja` rose **25.0449** above
its running minimum at iteration 2091 and still finished `ok`, so any threshold
worth a constant would kill a solve that was going to finish.

## The attribution

`relrise.sh` at five refs. The verdict is its own exit code: 0 the refusal
holds, 1 it flipped.

| ref | what landed there | largest rise on a solve that ends `ok` | verdict |
|---|---|---|---|
| `e2daf9c` | D211 itself | **25.0449** | HOLDS |
| `da16a20` | **D212, Harris's two-pass ratio test in primal form** | **2.59079e-10** | **REOPEN** |
| `e5bfe3d` | D213, the Harris width | 9.36752e-10 | REOPEN |
| `2ee580f` | a bench record | 9.36752e-10 | REOPEN |
| `3221397` | D214, `can_move`'s units | 9.36752e-10 | REOPEN |

**D212 is the commit.** `pilot-ja`'s rise goes from 25.0449 to 3.3348e-12 there
and does not move again; `wood1p`'s phase 1 goes from 3830 iterations to 251 in
the same step, which is D212's own headline. A two-pass ratio test picks a
larger pivot, and a larger pivot is what keeps the recomputation close to the
carried values.

D213 moves the number from 2.59079e-10 to 9.36752e-10 and changes no verdict,
which is consistent with D213's own finding that the width chooses nothing
inside its plateau.

**`pilot87` still diverges** — 8.07e+11 at `3221397` against 8.43e+13 at
`e2daf9c` — so nothing here says the divergence is fixed. What changed is that
the one instance standing between the rise and a usable threshold no longer
stands there. The window is now roughly nine orders of magnitude wide.

## The instrument defect, which produced a false answer first

`relrise.sh:30` is

    root=/mnt/c/Users/vall-/Desktop/projectes/jaos

an absolute path, and the script `cd`s there before `git rev-parse HEAD`. Run
from inside a worktree it therefore measures the **main tree** and reports that
tree's HEAD. The first attribution pass did exactly that and returned
**9.36752e-10 at all three refs**, including the one where the refusal holds.
Three trees, one binary, one number — D82's failure, and the reading looked
finished.

What caught it was the arithmetic being too clean: three genuinely different
trees cannot agree to six significant figures. `run-attribute.sh` now gives
each ref its own root by rewriting that line into the worktree's copy, and it
**ends with a canary that fails loudly when all readings are equal**, so the
next run of it cannot make the same claim silently. Both passes are in
`run-attribute.txt`, the wrong one first.

The line is fixed in `relrise.sh` itself: `root` is derived from the script's
own location, which is what every other script here does and what makes
`make refusals` behave identically while letting the script be pointed at any
tree.

## What this does not do

It does not build the stop rule. A threshold is a constant and needs a sweep on
both sides; that is `TODO.md`'s item, reopened, not this directory's.
