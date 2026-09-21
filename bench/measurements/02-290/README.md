# 02-290 — rounds of nodes in the linear tree

Taken on 2026-09-22 on the tree of ae25a70 for TODO row B8 (row C7's
design). Under `--tree-batch N` above 1, the tree of `src/mip.c` takes up
to N open nodes in pick order, leaving out a node the incumbent already
closes. Each node of the round gets its own copy of the tree's LP, applies
itself to it (its bounds, its cuts, its basis) and is solved on up to
`--threads` threads. The tree then takes the round's nodes in order as
before. Each one starts from its copy's final basis, so its own solve is a
factorisation and few or no pivots, and everything the node does after its
solve (cuts, conflicts, branching, pseudocosts, incumbents) happens in the
round's order. The tree, its bound and its work are the same at any thread
count (`tests/test_mip.c` on 1 and 3 threads). At the default of 1 nothing
changes.

## Work

Rounds of 4 against `bench/miplib.baseline` (the estimate order):

- the 23 of MIPLIB 3 other than bell5 (`miplib3-rest-batch4.txt`): 1.37x the
  work, iterations 1.10x, four past 2x: misc07 3.28x, enigma 2.69x, p0033
  2.10x and misc06 2.03x. The iterations barely move; the rest of the work
  is the second solve of each node and the copies. l152lav reads 0.61x:
  2123 nodes against 4644.
- bell5 (`miplib3-bell5-batch4.txt`) stops at 1.065e10 work units, twice
  its baseline, with the optimum in hand and no proof.
- the 2017 set at 1e10 work units (`miplib2017-batch4.txt`): mean primal
  plus dual gap 0.9843 against 0.9452, 1.041x, with 16 incumbents against
  17.

## Wall time

`timing.txt`, one run each on a 12-thread machine with nothing else
running:

| instance | one node at a time | rounds of 4, 1 thread | rounds of 4, 4 threads | rounds of 8, 8 threads |
|---|---|---|---|---|
| bell3a | 16.41 s | 31.64 s | 27.72 s | 23.37 s |
| blend2 | 4.91 s | 8.46 s | 6.02 s | 6.36 s |
| stein45 | 16.62 s | 29.50 s | 25.33 s | 23.90 s |
| misc07 | 6.80 s | 26.67 s | 16.98 s | 18.60 s |
| l152lav | 93.91 s | 56.93 s | 35.11 s | 21.28 s |

bell3a, blend2, stein45 and misc07 solve a node in a few pivots from its
parent's basis, so a round's copies and second solves cost more than its
threads save. l152lav takes 750 pivots a node. There rounds of 4 take
56.93 s on one thread, from the smaller tree, and 35.11 s on four; rounds
of 8 on eight threads take 21.28 s against 93.91 s.

Rounds stay off by default: they fail MIPLIB 3's bar on bell5 and four
other instances. They are there for a model whose nodes are expensive.
