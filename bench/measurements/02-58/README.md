# The retried warm repair is correct now and refused on cost: dfl001 pays 172x for a doomed attempt the guard then throws away

Taken 2026-08-19, the D145 retry that D148 reopened. Closed as D149.

## The judgement

The candidate is 02-53's diff re-applied on the guarded HEAD (src hunks
verbatim; the SHORT pinned test re-added; the LONG one had already landed),
plus two composition-review dispositions: the repair announces itself in
the DETAIL log, and the SHORT test's comment names the fixed-order loop it
actually exercises. `numerics-reviewer`'s composition pass (fifth delivery
this run) confirmed all four questions clean — the retry cannot re-enter
the repair, the promotion leaves nothing the certificate misreads, the
guard's cold branch stays unreachable from a repaired start, and the work
bill composes once. Suite green in all three variants; the gate
bit-identical on 94 + 29 + 16.

**Correctness, the bar D145 set: met.** `disagreed=0, rejected=0` on both
warm campaigns, against 02-53's 8 and 2. The certificate guard catches
every doomed warm trajectory and the cold restart answers correctly.

**Cost, and it refuses the candidate:**

| | committed (no repair) | 02-53 (repair, no guard) | **retry (repair + guard)** |
|---|---|---|---|
| netlib work geomean | 0.2553 | 0.1636 + 8 wrong answers | **0.2605** |
| netlib iterations geomean | 0.1381 | 0.1535 | **0.0752** |
| netlib worst work ratio | 1.0000 | 8.34 (`pilot-ja`) | **172.03 (`dfl001`)** |
| Kennington work geomean | 0.0572 | 0.0070 | **0.0070**, clean |

`dfl001`'s line says the mechanism exactly: warm = 21985 iterations — the
cold count, because the restart's solve IS the cold solve — carrying
4.72e11 work units against cold's 2.74e9. The repaired basis (shortfall
596) launches a ~2e6-iteration trajectory to an uncertifiable vertex, the
guard fires, the honest accumulator keeps the bill, and the caller who
changed one bound pays 172x. `bnl2` shows the other cost shape: certified
but slow, 7.8x work through a worse vertex path, no guard involved.

## What separates gold from garbage, and why nothing lands tonight

Kennington's five recoveries (shortfall 1 each) and netlib's cheap wins
(`adlittle` 80→2, `blend` 97→12, `boeing1` 391→24) against `dfl001`'s 596
and the 13 warm-worse instances: **the shortfall size is the visible
separator, and any threshold on it is a new constant that D8 says needs a
sweep on both sides.** The sweep material exists and is named: 02-52's
per-instance shortfalls joined against this record's per-instance
outcomes. Refused as a blanket repair until that sweep is run; the
candidate is kept whole at `warm-retry-candidate.diff`.

## Reproducing it

Apply `warm-retry-candidate.diff` in a worktree; `retry-warm.txt` and
`retry-warm-kennington.txt` are the campaign records beside this file.
