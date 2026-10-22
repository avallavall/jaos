# 02-428 — the conic tree against brute force on new seeds

Taken on 2026-10-22 on the tree of 200662d. `read.sh` takes the reading
(`mip.txt`, `dis.txt`). No code changed.

02-425 to 02-427 changed how the conic tree reads a node: a rough point
is held in its box, a rough node takes a bound from its own duals, a
leaf closes only on a proof, and a failed fixed solve's feasible point
can be an incumbent. Their gates ran 02-255's and 02-326's generators on
the seeds 02-390 uses. Both generators judge each answer against brute
force over every integer value and every piece, so this reading runs
them on 16 new seeds each, 30 to 45, 1000 models a seed, with 02-376's
and 02-390's `unsure.patch`.

| generator | models | optimal | infeasible | unbounded | failed | brute force unsure | tree numerical errors |
|---|---|---|---|---|---|---|---|
| 02-255 `misocp.c` | 16,000 | 9,287 | 3,200 | 3,513 | 0 | 0 | 0 |
| 02-326 `discrete.c` | 16,000 | 9,926 | 1,985 | 4,088 | 0 | 1 | 0 |

The unsure model is `discrete.c`'s seed 30, model 288: one piece of the
brute force, a continuous model, ends `numerical_error` next to an
infeasibility certificate the checker does not confirm, so brute force
has no verdict to compare with. `dis-seed30-288.mps` is that piece.
