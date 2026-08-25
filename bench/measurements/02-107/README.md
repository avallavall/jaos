# 02-107 — the bound flip's 1e10 delta is real and fires on nothing, and one canary of two could be built

2026-08-26. `TODO.md` §0, the third of D191's answer-changing findings.

## The question

`primal_bound_flip`'s destination comes from `real_upper`/`real_lower` and its
origin from `nonbasic_value`, which reads the raw `lo`/`up` and so may read a
bound dual phase 1 invented. `delta` can then be the size of an artificial
bound. The ratio test skips any row with `|col[i]| < PIVOT_MIN = 1e-9`, and
those rows still move by `delta * col[i]`. D191 put the worst case at ~9,
which is 1e8 times `primal_tol`.

## The measurement

Both flip sites instrumented in a worktree, over all 94 with
`cfg.force_primal` (`flip-census.txt`):

| | phase 2 | phase 1 |
|---|---|---|
| flips | **8** | **10604** |
| origin was an invented bound | 0 | **3974** |
| the phase's own measure grew | **0** | **0** |
| largest `\|delta\|` | 200 | **1e+10** |
| phase-2 feasibility broken | **0** | n/a |

**`delta` really does reach 1e10 from an invented origin, 3974 times. Nothing
downstream of it moves either phase's own measure.**

## The predicate was wrong the first time, and the wrong one fired 113 times

The first version asked whether `primal_worst_violation` grew, for both
phases. It reported **113 firings**, every one inside phase 1, and every one
innocent: **phase 1 minimises the SUM of violations and deliberately lets a
basic go further out** — `primal_phase1_ratio` skips a row that is already
under its bound and moving further under, in as many words. The worst growing
inside phase 1 says nothing at all.

So the predicate is per phase. Phase 2's invariant is primal feasibility, so
its measure is the worst violation. Phase 1's is the total. Both read 0.

## One canary of two

**Phase 1's predicate is validated.** With every flip followed by a forced 1e6
push on every basic, 260750 of 393041 flips report the total growing, largest
growth 1e11. It fires.

**Phase 2's predicate is NOT validated, and two attempts failed to reach it.**

- Raising `PIVOT_MIN` from 1e-9 to 1e-3, meaning to make the hazard a million
  times more reachable, reported nothing — **and took the flip count from
  10604 to 221**. Refusing that many pivots changes the trajectory instead of
  holding it still, so it proved nothing either way (`canary-pivotmin.txt`).
- Forcing the damage in both phases produced **zero phase-2 flips over all
  94**: the damage keeps every solve inside phase 1, which never hands over.
- Forcing it in phase 2 only, on the 8 instances D194 named as pure phase-2
  runs, also produced zero — because **D194's naming of those 8 was wrong**,
  which is D195 and `bench/measurements/02-108/`.

So the phase-2 row above is 8 flips with no firing and no instrument test
behind it. It is written down as unproven rather than as evidence.

## The verdict

**No repair, and the reason is measured rather than assumed.** The hazard is
real by inspection and reaches nothing on this set. Fitting a guard to zero
observations is what this project's own rules forbid.

**Reopen conditions.** A flip in phase 2 whose worst violation grows past
`primal_tol`; a change to `PIVOT_MIN`; or a starting basis that is not the
slack basis, since every one of the 3974 invented origins comes from dual
phase 1's loans and a crossover would supply different ones.

## Re-running

Every script writes beside itself and replaces this directory's evidence. All
worktrees go under `mktemp -d`, outside the repository, because `make clean` is
`rm -rf build`.
