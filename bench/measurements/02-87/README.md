# 02-87 — what D97's §8d refusal would cost, and why it is drafted too wide

No decision and no source change. `docs/research/dual-postsolve-imposed-bound.md`
§8d proposes the refusal a first version of D97 should carry, and calls it
"one flag per row in the forward pass". This counts what that flag throws
away.

## The question

§8c's rank argument holds when a row imposes a bound on **one** of its
columns and breaks when it imposes on **two**. §8d's refusal is therefore:

> do not impose a bound from a row that already imposed one on another of its
> columns

Nobody had counted the collision rate. If it is a few percent the refusal is
free; if it is most of the family the reduction is not worth building behind
it.

## The tree, and the caveat that comes with it

**`7c7375c`**, where the activity tightening exists — the minimal failing
design D114 later refused for taking a verdict window from an invented
magnitude. `bench/measurements/02-21/` uses the same commit for the same
reason. **So these counts describe THAT tightening, not a corrected one**, and
what they are good for is the order of the collision rate rather than an exact
figure.

Two checks that the tree is the one intended: `pilot` and `pilot87` read
`infeasible` here, which is 02-21's reproduction to the word; and the
unpatched control emits **0** `IMPOSE` records, so a zero anywhere below would
have meant the family not firing rather than the hook not compiling.

Records are attributed per instance by a driver that announces each one,
because a row index means nothing across models — pooling them would overstate
every collision.

## The count — `impose-count.txt`

| | netlib | Kennington |
|---|---|---|
| instances where the tightening fires | 83 of 94 | 16 of 16 |
| imposed bounds | 183665 | 852699 |
| rows imposing at all | 44980 | 139081 |
| rows imposing on 2 or more columns | 19775 | 78371 |
| of those, the row is an equality | 15238 | 51866 |
| of those, it is not | 4537 | 26505 |

| the refusal | netlib | Kennington |
|---|---|---|
| **as §8d writes it** | **50.2%** | **82.3%** |
| **restricted to equality rows** | **35.5%** | **20.3%** |

## What this says

**The refusal as drafted is not a flag, it is the reduction.** It declines
half the imposed bounds on netlib and four fifths of them on Kennington —
the set where D97's prize is largest, at 29.36% of live rows.

**And the design contains its own argument for a narrower one.** §8d proves
the breaking configuration *forces the implying row to be an equality*: `j1`'s
tightening derives from `rl_i` and `j2`'s from `ru_i`, so `rl_i = ru_i`. A
refusal restricted to equality rows is therefore just as safe by §8d's own
reasoning, and on Kennington it costs **20.3% instead of 82.3%** — a factor of
four on the set that matters most.

**Both figures are still over-approximations**, and the narrower one by more.
A row imposing on two columns is not yet the breaking configuration: §8d also
needs both columns *resting at those imposed bounds* in the final solution.
Counting that needs the solve, not the presolve pass, and it is the next
measurement rather than this one.

## What it does not say

It does not say D97 is worth building, and it does not close anything. It
replaces "one flag per row in the forward pass" with a number, and it moves
the drafted refusal from the design's recommendation to its fallback.

## Reproducing

```
bench/measurements/02-87/run-impose-count.sh            # 7c7375c, ~4 min
bench/measurements/02-87/run-impose-count.sh <ref>      # any tree with the pass
```
