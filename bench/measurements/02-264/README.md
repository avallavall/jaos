# 02-264 — the conic tree takes its nodes in rounds

SPECS row 82, deterministic parallel tree search, was missing. The conic
branch and bound of `src/conictree.c` solves a cold conic walk at every
node, with no warm start to carry from parent to child, so its nodes are
independent work of about the same size. That is what a round can spread
over threads.

## The change

`--tree-batch N` (the option `mip_tree_batch`, `jaos_set_mip_tree_batch`,
Python at both layers) sets how many open nodes the tree takes at a time,
1 by default and at most `CT_BATCH_MAX` (64). The round's first node is
the one the tree would have taken next (the plunge child, then the
depth-first stack, then the best bound in the heap), and the rest come
from behind it in the same order. Each node is solved on its own copy of
the relaxation, up to `--threads` of them at once, and the answers are
taken in the round's own order: the same pseudocosts are learned, the
same children are pushed, the same incumbents are offered. A round's
remaining work limit is divided between its nodes, so the limit holds as
it did with one node.

Nothing in a round depends on the thread count, so the tree, its bound,
its node count and its work are the same at any count, as `--algorithm
concurrent` already promised for an LP. `tests/test_conic.c` solves the
weighted rounding model at 1 and at 4 threads and compares the nodes, the
work, the objective and the point, bit for bit.

Writing it turned up one defect of its own, caught by the readings: a
round that a work limit stops leaves its remaining nodes open, and the
first version dropped them from the final bound, which then claimed more
than the tree had proved (shortfall_200_3's bound read -1.1373 where
-1.1421 was proved). They are folded into the bound now, and a round of
one reproduces the one-node tree instance for instance.

## CBLIB's 80 mixed-integer instances at 1e10 work units

`bench/measurements/02-255/cblib.sh 10000000000 --tree-batch N --threads
N`, N = 1, 2, 4 and 8 (`cblib-1e10-round1.txt` and the rest). A round of
one is the committed tree, to the bit, on all 80.

| round | `OPTIMAL` | work limit | work on those both solve | bound better at the limit | incumbent worse by 1% at the limit |
|---|---|---|---|---|---|
| 1 | 30 | 50 | 1.000x | | |
| 2 | 30 | 50 | 0.868x | 35 | 13 |
| 4 | 30 | 50 | 0.851x | 39 | 15 |
| 8 | 29 | 51 | 0.825x | 39 | 15 |

- A round of two trades sssd-weak-25-4, which stops reaching its optimum,
  for turbine07_lowb, which starts. A round of four changes no verdict. A
  round of eight loses sssd-strong-25-4.
- The work over the 80 is 5.660e11 units at 1, 5.582e11 at 2, 5.570e11 at
  4 and 5.592e11 at 8.
- Where a round helps most it helps a lot: robust_50_3 ends in 33 nodes
  against 171, robust_50_1 in 29 against 70, sssd-strong-20-4 in 678
  against 947.

## At 1e11 work units

`cblib-1e11-round4.txt` against the committed tree's reading
(`bench/measurements/02-263/cblib-1e11-final.txt`):

- 43 end `OPTIMAL` either way, but not the same 43: classical_50_3 and
  shortfall_50_3 start reaching their optimum, pp-n100-d10000 stops, and
  turbine07_lowb ends `NUMERICAL_ERROR` because the round's search leaves
  it a failed leaf whose bound its incumbent cannot close.
- The work over the 41 both solve is 0.890x, the geometric mean 0.856,
  and over the 80 it is 4.479e12 against 4.367e12 units.
- At the work limit the bound is better on 27 and the incumbent worse by
  more than 1% on 14, the sssd-weak files the worst of them.

**That is why the default is one node.** A round reaches an optimum with
less work, and a run a limit stops keeps a better bound and a worse
incumbent, which is the wrong trade for a caller who takes the incumbent
and goes.

## The thread count

A round of four, on the same machine, with the answer lines compared:

| instance | 1 thread | 2 | 4 | 8 |
|---|---|---|---|---|
| sssd-strong-20-4 (678 nodes) | 1.54 s | 0.83 s | 0.59 s | 0.57 s |
| estein5_nr21 (971 nodes) | 1.61 s | 1.02 s | 0.57 s | 0.55 s |
| robust_50_3 (33 nodes) | 0.53 s | 0.34 s | 0.33 s | 0.33 s |

Every line but `time` is the same at every count. A count above the round
buys nothing, since a round holds four nodes.

## The generated models

02-255's 3000 mixed-integer models at the default round: the same nodes
(2410, 2488, 2486), the same work and the same digests as the committed
tree.
