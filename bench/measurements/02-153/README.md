# 02-153 — the primal's unboundedness verdict, and the ray it still cannot see

D241. TODO.md section 0 stage 7.

## What is here

| file | what it does |
|---|---|
| `run-branch-census.sh` | counts how often the forced primal meets a column nothing blocks, over the 94 standard instances |
| `branch-census.txt` | the run, as recorded |
| `run-verdict-arms.sh` | removes the verdict and its one condition in turn, in both builds |
| `verdict-arms.txt` | the arms, as run |

Both derive the repository root and run from anywhere (D217). Neither is a
gate tool. `run-branch-census.sh` needs `bench/fetch.sh` to have run.

## The census, and the census before it that proved nothing

The verdict sits in a branch of `run_primal`'s phase 2, so the first question
was whether that branch is reached at all.

**It is not, on the standard set: 0 firings in 102 phase-2 ratio tests.** The
reason is in the campaign's own split. Phase 2 runs **one** iteration on 83
of the 94 instances and at most five on any of them, because the dual
re-entry takes the model as soon as the shifted problem looks optimal
(D194, D197). There is almost no phase 2 in which to meet a ray.

**The first attempt at this census counted zero and was worthless**, because
its canary counted zero too. It used four synthetic unbounded models as the
canary, and every one of them is answered by `classify_optimum` before phase
2 ever prices a column — so a zero there said nothing about the instrument.
The census here instruments the phase-2 ratio test itself as the control, and
that reads 102.

## Where the branch IS reached, and why those models are unusual

`classify_optimum` already answers UNBOUNDED whenever the ray is one column
leaving the bound phase 1 lent it, and that covers most unbounded models: if
`c'd < 0` then some `j` has `c_j d_j < 0`, that `j` is unbounded in direction
`d_j`, so its cost points at a bound it does not have and the cold start
lends it one.

The models that escape that argument are the ones where the loaded column is
pulled into the **basis** on the way, leaving nothing sitting on a loan to
read the verdict off. Two are in `tests/test_simplex.c`, and the forced
primal refused both before this change:

| model | why the loan disappears |
|---|---|
| `min -y` over `y - w = 0`, `x + w >= 3`, `x` in [0,1] | the equality pulls `y` into the basis |
| `min z` over `z + w = 0`, `z` free | a free column is never lent a bound at all |

## The arms

| arm | build | what goes red |
|---|---|---|
| the verdict never fires | default | the phase-2 ray test |
| the verdict never fires | reference | both ray tests |
| `shifts_outstanding` always false | default | nothing |

The free-column model needs the reference build to be evidence: with presolve
on, it is answered before the simplex sees it. Arm 2 moves nothing, and that
is reported rather than dressed up — no model here reaches the branch with a
cost still borrowed, so that condition is unexercised by the suite. It is
kept because it can only refuse.

## The check that was written, armed, and then deleted

The change first carried a `primal_ray_confirmed` that asked the ray question
a second time with the absolute pivot floor. The stated reason was D210's:
the ratio test's floor is relative to the column's own largest entry, so a
row it drops might still block, and a ray read off a blocker nobody looked at
is what D19 refuses.

**The arm for it moved no test, and reading the code says why.**
`primal_apply_floor` ends `return m > 0 ? m : n;` — when its floor would
empty the candidate list it hands back the unfiltered one. It can never
remove the last candidate. So `r < 0` already means what the second question
was asking, and the second question restated the first. It was deleted.

What remains true, and is written at the verdict rather than hidden: both
this verdict and the dual's `improves_without_limit` skip a row moving slower
than `PIVOT_MIN`. Neither is better than the other on that point.

## The ray neither verdict can see

`improves_without_limit` moves one column. A model can be unbounded along a
direction that moves several, and this one is:

```
min -p - q   s.t.  p - q = 0,  p + q >= 2,  p, q >= 0, neither capped
```

The equality ties the pair together, the inequality pushes them out, and the
objective is `-2p`. Moving either column alone runs into the equality, so
nothing single-column sees the ray and the solve refuses.

**It is unbounded, and JAOS cannot be its own witness.** Capping `p` and
solving gives an optimum of exactly `-2` times the cap at 10, 1e3, 1e6 and
1e9 — the ladder is in `tests/test_simplex.c` so it runs every build.

The refusal was claiming "the optimum is finite but lies beyond the reach of
this phase 1", which is false on this model. The message now says only what
was established: moving that column alone runs into a constraint, and
whether the model is bounded or unbounded is not decided here. The verdict
itself was always safe — a missing answer, not a wrong one.
