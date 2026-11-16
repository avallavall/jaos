# 02-462 — why `misc03` grows under probing, and why no probe cap passes MIPLIB 3

Taken on 2026-11-16 on 0e16a62, for TODO row J7. 02-461 left strong
branching behind `--reliability` because `misc03` took 2.9x the work under
`--reliability 4 --probe-cap 1`. `read.sh` takes every reading here with
HEAD's tool and `bench/run`, one solve at a time:

- `misc03.txt`: `misc03` across reliability, cap and depth;
- `caps.txt`: the cap on the six instances that move;
- `enigma.txt`: `enigma`'s first incumbent;
- `miplib3-nocap.txt` and `miplib3-cap4.txt`: MIPLIB 3 under reliability 4,
  with no cap and with cap 4.

## `misc03` grows from the capped probes

The default solves `misc03` in 248 nodes. It finds the optimum 3360 at
node 101, by a sub-MIP. Under cap 1 the tree has 622 nodes, and the
optimum comes at node 116, so the larger tree is not a late incumbent.
Across reliability and cap:

| reliability | no cap | cap 1 | cap 4 |
|---|---|---|---|
| 1 | 257 nodes, 1.34x | 447, 2.41x | 305, 1.62x |
| 2 | 257, 1.35x | 455, 2.37x | 305, 1.62x |
| 4 | 257, 1.35x | 622, 2.90x | 303, 1.61x |
| 8 | 257, 1.35x | 640, 2.96x | 303, 1.61x |

With no cap, the 32 probes are exact, and the tree is the default's plus
9 nodes. Under cap 1, 44 of 115 probes stop, and each gives its pseudocost
the Lagrangian bound of a basis a few iterations from the parent's. That
bound is weak, so the gain it records is small. A capped probe is also
cheaper, so `MIP_PROBE_SHARE` lets more of them run, 115 against 32, and
more of the pseudocosts start from such small gains.

Probing at the root only (16 probes, none capped) still takes 411 nodes.
The tree also moves with which columns the probes inform, not only with
how well.

## `enigma`'s tree is the search for its first point

`enigma`'s root bound is 0, which is its optimum. The tree ends the moment
a feasible point appears, and no heuristic finds one (0 points by
rounding, dive, RINS or pump):

| setting | nodes | first incumbent at |
|---|---|---|
| default | 895 | 895 |
| reliability 4, cap 1 | 730 | 730 |
| reliability 4, no cap | 8,670 | 8,670 |

Its work against the baseline measures where a feasible leaf happens to
lie in the order the branching visits. That order changes with any
branching change, for better or worse.

## No cap passes MIPLIB 3

Under reliability 4, against `bench/miplib.baseline`:

| cap | 0.25 | 0.5 | 1 | 1.5 | 2 | 3 |
|---|---|---|---|---|---|---|
| `misc03` | 2.44x | 1.22x | 2.90x | 1.29x | 1.72x | 1.46x |
| `enigma` | 3.00x | 5.20x | 0.86x | 2.28x | 2.80x | 4.74x |
| `mod008` | 1.28x | 1.36x | 1.53x | 1.64x | 1.35x | 1.34x |
| `p0201` | 0.90x | 0.46x | 1.54x | 0.57x | 1.54x | 1.27x |
| `rgn` | 1.18x | 1.07x | 1.21x | 1.26x | 1.15x | 1.92x |
| `misc07` | 0.68x | 0.61x | 0.65x | 0.73x | 1.61x | 0.95x |

`enigma` is under 2x only at cap 1, which is where `misc03` is past it.
On the whole set:

| cap | total work | past 2x |
|---|---|---|
| none | 1.259x | `enigma` 3.9x |
| 1 (02-461) | 0.796x | `misc03` 2.9x |
| 4 | 1.006x | `enigma` 4.35x |

## What would find `enigma`'s point

`enigma` is a cryptarithm. Each of its 100 binaries puts a letter on a
digit. Ten rows give each letter one digit, and ten give each digit one
letter. `BILANCIO` is the sum, with coefficients up to 9,090, and it must
equal 0. A point is a permutation that satisfies `BILANCIO`.

`heuristics.sh` reads HEAD's heuristics at larger settings, and the
feasibility jump with `MIP_FJ_WORK` at 100 and 1000 times its value and no
cap from the root's work (`heuristics.txt`):

| setting | first incumbent at node | work |
|---|---|---|
| default | 895 | 18,684,934 |
| `--feaspump 200`, with and without `--pump-obj 0` | 895 | 25.1M, 31.4M |
| `--dive-heuristic 500` | 895 | 18,684,934 |
| `--dive-heuristic-depth 100` | 832 | 51,892,202 |
| `--dive` | 1,291 | 19,653,402 |
| `--dive --dive-backtrack 1000` | 22,733 | 153,358,379 |
| `--local-branching 10` | 895 | 18,684,934 |
| feasibility jump, 100x its work | 895 | 1.66e9 |
| feasibility jump, 1000x its work | 895 | 1.64e10 |

None finds it before the tree does. Even at 1.64e10 work units, more than
the benchmarks' whole budget, the jump finds none: it moves one binary at
a time. From a permutation, any single flip breaks two assignment rows,
and the next permutation is four flips away, two letters trading digits.
A move made of such exchanges would be a search for permutations under
one equation, which is `enigma`'s own shape.

## The verdict

Nothing changes in the code. Strong branching stays behind
`--reliability`.

Under the gate, no setting of the cap can make it the default while
`enigma`'s tree is a search for a point. No heuristic JAOS has finds that
point at any setting read here. A heuristic that did would take
`enigma` out of the reading, and the cap could then be chosen on `misc03`
and the rest: there, cap 0.5 or 1.5 and no cap keep `misc03` under 1.4x.
The only move that reaches it is a search for permutations, made for
`enigma`'s shape, and that is not built.
