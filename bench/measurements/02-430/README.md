# 02-430 — the conic tree in rounds of four nodes, against brute force

Taken on 2026-10-22 on the tree of ee5b4c7. `read.sh` takes the reading
(`mip.txt`, `dis.txt`). No code changed.

02-425 to 02-427 changed how the conic tree reads a node, and each node
of a round runs that logic. The gates read the tree one node at a time.
`batch.py` makes 02-255's `misocp.c` and 02-326's `discrete.c` solve
each tree with `jaos_set_mip_tree_batch(m, 4)` on 4 threads, with
02-376's and 02-390's `unsure.patch`, and both judge each answer against
brute force. Seeds 1 to 4 and 30 to 33, 1000 models a seed.

| generator | models | optimal | infeasible | unbounded | failed | brute force unsure | tree numerical errors |
|---|---|---|---|---|---|---|---|
| 02-255 `misocp.c` | 8,000 | 4,684 | 1,600 | 1,715 | 0 | 1 | 0 |
| 02-326 `discrete.c` | 8,000 | 5,047 | 983 | 1,966 | 0 | 4 | 0 |

The unsure models are those whose brute force has a piece that ends
`numerical_error` on its own: `misocp.c` seed 4's model 533 (TODO's
J21), `discrete.c` seed 2's models 240 and 620 (J22 and J21), seed 4's
model 403, whose two failed pieces stall next to infeasibility
certificates the checker does not confirm, and seed 30's model 288
(02-428's).
