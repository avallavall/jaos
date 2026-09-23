# 02-305 — strong branching read again after d6245e0

Taken on 2026-09-23 for TODO row H2, on the tree of 8de9a6c. Since
d6245e0 a node LP starts from its parent's basis whole, and a strong
branching probe is such a solve, so the probes cost less than when D293
was measured (02-192, 02-300).

`reliability-sweep.sh` is 02-300's script with a 4 GB cap on each solve:
every MIPLIB 3 instance at reliability 0, 1, 2, 4 and 8, 1e11 work units,
two at a time; `reliability-sweep.txt` holds the status, work and nodes.
Work against reliability 0, over the instances optimal in both:

| reliability | work | past 2x |
|---|---|---|
| 1 | 1.006x over 23 | misc03 4.35x (207 to 435 nodes), gen 2.45x, rgn 2.23x |
| 2 | 1.095x over 24 | misc03 5.71x, rgn 2.76x, gen 2.45x |
| 4 | 1.242x over 24 | misc03 6.75x, rgn 3.20x, gen 2.45x, mod008 2.11x |
| 8 | 1.458x over 24 | eight, misc03 7.84x the worst |

`bell5` at reliability 1 stopped at the 4 GB cap. The probes still cost
more than the nodes they save, so D293 holds; its line in
`bench/refusals.txt` carries this reading.
