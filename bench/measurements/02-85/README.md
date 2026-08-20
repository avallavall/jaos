# 02-85 — the sum that decides which point is published

D175. One source change in `src/simplex.c`, with the two arithmetic steps
moved out of `src/model.c` so both callers share them.

## What it is

`settled_objective` summed `cost0[v] * x[v]` naively. That number does not
describe a trajectory: `better_point` ranks two rounds by it and
`take_best_if_better` restores and publishes the winner, so **the sum decides
which point the caller receives**. It is now the compensated sum with
Dekker's split that `jm_model_publish_objective` has used since D169 and D172.

`obj_add` and `two_product_residue` were `static` in `src/model.c`. They are
`jm_obj_add` and `jm_two_product_residue` now, declared in
`src/jaos_internal.h`. A second copy of that arithmetic in one tree is how
two answers to one question appear.

## The failure is a tie, not a small error — `two-points.txt`

258 columns and no solve. A column of cost `1e16` at 1, then 256 columns of
cost 1, then a column of cost `-1e16` at 1:

| | naive | compensated |
|---|---|---|
| point A, unit columns at 1 | **0** | 256 |
| point B, unit columns at 0 | **0** | 0 |

One ulp at `1e16` is 2, so every unit term is lost as it arrives and the
`-1e16` brings the total to exactly 0.0 for both. `better_point` reads
`0 < 0`, answers no, and the loop keeps whichever round it stopped on —
publishing A and leaving 256 on the table with no number anywhere recording
it.

**The order is the mechanism.** The first version of this file put the
`-1e16` column second. The cancellation then happened first, the unit terms
landed on zero and survived, and the naive sum was right. `settled_objective`
walks the variables in index order, so the large term has to arrive before
the small ones.

## Can a solve reach two such points? — `objcmp.txt`, `objcmp-raw.txt`

A throwaway diagnostic build (`patch-objcmp.py`, applied to a copy of the
tree, every hook inside `#ifdef JAOS_DIAG`) records both objectives both ways
at every comparison the settling loop makes, on all three sets.

**Measured on the parent, `efe5884`, because the question is what the NAIVE
sum did.** 94 + 29 + 16 instances.

| | |
|---|---|
| comparisons recorded | **304** — netlib 272, Kennington 32, infeasible **0** |
| verdict flips under the compensated sum | **0** |
| settled by dual feasibility, the objective playing no part | 80 |
| exact ties | 220, and **every one is a point compared with itself** |
| **two distinct points — the informative population** | **4**, all netlib |

The infeasible set contributes nothing because an infeasible model never
reaches the settling loop's optimum path. Kennington's 32 are all at
`take_best`, all a point against itself. **`take_best_if_better`, the site
that publishes, was never exercised on two distinct points in 220 tries.**

Two margins on those 4, and they are different questions:

| | worst |
|---|---|
| **spread** — each side's error against the gap being decided | **0.571** |
| **flip margin** — `\|errc - errb\|` against it, which a changed verdict needs | **1.53e-06** |

An error common to both sides cancels inside `a < b`, which is why the flip
margin is the one that governs. On the two records where the spread is 0.571
the two errors are bit-identical and cancel exactly — a property of those
instances, not of the method. **Both numbers are reported because the first
version of this record printed them under one name and they disagree by five
orders** (`numerics-reviewer`).

## The cost

`gate: PASS` on all three sets, `0 regressed, 0 improved, 0 new`, and
`bench/results/*.txt` are **byte-identical to the committed records** — no
digest, work unit, iteration count, basis hash or objective figure moved
anywhere. That is what 0 verdict flips predicts, and it is confirmed a second
way: 02-83's exact-objective records are bit-identical on all 110 published
objectives, from a different program on a different code path.

`make configs` exits 0 — all five configurations.

## There is no test that fails at the parent, and that is the honest limit

The only state that separates the two versions is a settling loop holding two
distinct points that tie under a naive sum. No model built here steers a solve
there, and `tests/` reaches the library through its public interface only, so
a static function cannot be called directly. The evidence is the constructed
two-point case plus the 304 comparisons — not a test. Writing one that passes
either way would be worse than having none.

## Three defects in this measurement, all found after it looked finished

Kept because each looked like a clean result.

1. **`run-objcmp.sh` never passed `-d "$dir"`.** `bench/run` defaults the
   instance directory to `bench/instances` and takes the manifest separately,
   so all three passes read the standard set: **Kennington recorded zero
   comparisons while the output printed its name** (`numerics-reviewer`). The
   script prints the instance count per set now.
2. **The probe measured the repaired tree.** It copied `src/` from the working
   directory, where the compensated sum already was, so it compared the
   compensated sum against itself and every error column read exactly 0. It
   takes the tree as an argument now and defaults to the parent commit.
3. **One name, two numbers.** `objcmp.txt` printed "worst margin 0.5714" while
   the source comment said "1.53e-06", both computed here from the same
   records. They are `spread` and `flip` above and are named apart now.

## Reproducing

```
bench/measurements/02-85/run-objcmp.sh            # the parent, three sets
bench/measurements/02-85/run-objcmp.sh working    # the tree as it stands
gcc-14 -std=c23 -ffp-contract=off -O2 two-points.c -o tp && ./tp
```
