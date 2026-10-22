# 02-425 — models with cones edited between solves

Taken on 2026-10-22 on the tree of 765c52b. `read.sh` takes the first
reading (`warmcone-head.txt`). `gate.sh` takes the gate of the change in
`src/conictree.c` and the misocp reading after it (`warmcone.txt`).

## The harness

`warmcone.c` is 02-404's `warmqp.c` on models with cones: 02-235's LPs,
minimising, with one or two cones over columns drawn from the model, a
quadratic cone over 2 to 4 columns or a rotated one over 3 to 4. The
curvature edit gives way to two cone edits, a new cone and a deleted cone,
and a deleted column is drawn from the columns outside every cone, since
a cone member cannot be deleted before its cone. After each edit the
model is solved again and a copy of it (`jaos_model_copy`) is solved from
nothing. Both must end with the same status (P1). An optimum must match
the copy's objective within 1e-6 of its size and pass
`jaos_check_conic_solution` with its row and cone duals at 1e-6 (P2). An
infeasible answer must carry a certificate that
`jaos_check_conic_certificate` takes at 1e-7 (P3). In the `misocp` mode
each column is integer with probability one half; an optimum must then be
integral and pass `jaos_check_solution`'s primal half, cones included,
and P3 is not read. The edited solves of the `misocp` mode run under a
limit of 2e8 work units, and a pair that stops on it is counted as capped
and not read. The first solve of each model has no limit.

## The first reading

Seeds 1 to 3, 300 models each, eight edits a model, on 765c52b.

| mode, seed | optimal | infeasible | unbounded | capped | P1, P2, P3 |
|---|---|---|---|---|---|
| socp 1 | 905 | 1437 | 58 | 0 | 0, 0, 0 |
| socp 2 | 1005 | 1320 | 75 | 0 | 0, 0, 0 |
| socp 3 | 888 | 1473 | 35 | 0 | 0, 0, 0 |
| misocp 1 | 824 | 1499 | 60 | 12 | 0, 0, 0 |
| misocp 2 | 852 | 1447 | 75 | 20 | 0, 0, 0 |
| misocp 3 | did not end | | | | |

The misocp seed 3 run was stopped after 1 h 42 min. It was in the first
solve of a model, so no limit applied.

## The endless tree

`gdb` wrote the model from the running process. It has 24 columns, 10
rows, 14 integer columns and one rotated cone over C10, C18, C23 and C14.
The node being branched was 757,645 levels deep. The first 40 of its
fixings were C18 = 0, C19 in [3, 4], and then C20 in [0, 1] 38 times.

The node's relaxation, solved as a node solve, stops rough: the walk does
not reach its tolerances, and the point it returns has C19 at 2.948 and
C20 at 1.280, both outside the node's bounds. Solved as an ordinary
model, the same relaxation ends `numerical_error`. The tree branched on
C20 at 1.280. The down child took C20 in [0, floor(1.280)] = [0, 1],
which is the parent's box, and the up child took [2, 1], which is empty.
So the same node came back one level deeper each time, with its parent's
bound 46.35, which no incumbent closed. Each node bills its relaxation's
work, so under a work limit the solve stops: with `--work-limit 50000000`
the old code ends `work_limit` after 35 nodes. With no limit it never
ends.

Each node also copies and applies again every fixing on its path, and
the work counter does not see that. Of 12 stack samples near depth
757,000, 11 were in this work or in freeing nodes, and 1 was in a
relaxation. The change leaves this cost unbilled.

## The change

`jm_conic_branch_and_bound` moves the node's point into the node's
bounds before it branches. A rough point outside the box then gives
children with smaller boxes. The model ends optimal at 48.5 after 290
nodes and 289,631,880 work units. The most-fractional rule (402 nodes),
4 threads and no heuristics give 48.5 too.
`tests/data/cone_rough_branch.mps` is the model.

## The gate

`gate.sh` runs 02-390's integer sets on HEAD and on the change. The
change's records are `mip.txt`, `dis.txt`, `cblib-mip.txt` and
`miqcqp.txt`; HEAD's are the same files byte for byte.

| set | HEAD | the change |
|---|---|---|
| 02-255, 8000 models | 2 unsure, 1 tree numerical error | the same lines |
| 02-326, 6000 models | 4 unsure | the same lines |
| CBLIB's 80 mixed-integer instances at 1e10 | 30 optimal | the same lines |
| QPLIB's 14 mixed-integer QCQPs at 1e10 | 14 at the limit | the same lines |
| `make cblib` | | 29 of 29 checked, 0 regressed against the baseline |

## The reading after the change

| mode, seed | optimal | infeasible | unbounded | capped | P1, P2, P3 |
|---|---|---|---|---|---|
| misocp 1 | 824 | 1499 | 60 | 10 | 0, 0, 0 |
| misocp 2 | 852 | 1447 | 75 | 20 | 0, 0, 0 |
| misocp 3 | 791 | 1558 | 34 | 13 | 0, 0, 0 |

On seed 1, model 257's first two edits were capped on HEAD. With the
change both end `numerical_error`, warm and cold, as model 138's edits 3
to 6 and model 269's edit 2 do on both trees. In model 257's tree the
incumbent is -53.6013 and the bound -53.6279. One leaf, with every
integer column fixed, is set aside: its fixed model, a continuous one,
ends `numerical_error` with duals off by 0.399. Its rotated cone over C1,
C7, C17 and C14 has C1 = 1 and C7 = 2, so it holds C17² + C14² <= 4. The
smallest C17² + C14² over the leaf's rows and bounds is 4, at C17 = 2 and
C14 = 0. So the cone meets the other constraints at one point only, no
feasible point lies strictly inside the cone, and the walk ends with
`tau` and `kappa` both near 0. This was TODO's J26, which
`bench/measurements/02-426/` closes.
