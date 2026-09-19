# 02-258 — the conic interior point after a failed barrier

`TODO.md` row 6 said QPLIB_3708 ends `NUMERICAL_ERROR` at a node after an
incumbent 33% above the reference, and row 4 listed three Maros-Meszaros
QPs the barrier cannot settle: dtoc3, ksip and ubh1. The node of
QPLIB_3708, dumped as MPS from a scratch build (12930 columns, 12918
rows), ends `NUMERICAL_ERROR` on its own after 401 barrier iterations,
and the conic interior point of `src/conic.c` solves the same file to
-9544.4566538 in 143 iterations.

## The change

`jaos_solve` hands a model with a quadratic objective whose solve ends
`NUMERICAL_ERROR` (the barrier, and the dual simplex's probe after it) to
the conic interior point, `jm_conic_after_barrier` in `src/conic.c`. The
conic walk's work count starts at the barrier's, so the work limit holds
for the two together; its iterations add to the barrier's; the time
limit gets what the barrier left. The tree's node flag is cleared for the
call, so a walk that stops near an optimum the checker refuses ends
`NUMERICAL_ERROR` instead of passing as a rough node. A model the barrier
answers never reaches it.

## Maros-Meszaros (`make maros-meszaros`)

| instance | before | after | work before | work after |
|---|---|---|---|---|
| dtoc3 | numerical error, 5199 iterations | optimal, 235.26248103522531, checker ok | 622681432 | 639210809 |
| ksip | numerical error, 215 iterations | optimal, 0.57579794124010686, checker ok | 71821633519 | 71842978259 |
| ubh1 | numerical error, 7111 iterations | optimal, 1.1160008165409683, checker ok | 685586308 | 1785347146 |

BPMPD's values are 235.26248, 0.57579794 and 1.1160008. The other 135
lines are the same to the bit, status, work and digest. The set reads 137
`OPTIMAL` (134 before), 136 taken by the checker (133), 133 at the
reference (130) and 132 clean (129); `values` stays refused as not
convex. `bench/maros-meszaros.baseline` gains the three.

`make cblib`: the 29 continuous CBLIB instances give the same lines to
the bit, so the conic walk's new starting count changes nothing when it
starts at zero.

ksip on 101 points instead of 1001 keeps the failure: 20 free columns,
`Σ_j t^j x_j >= sin(t)` at `t = 0, 0.01, ..., 1`, the cost and the
diagonal of `Q` both `1/(j+1)`. Written by Python with `t ** j` and
solved by the tool, the barrier ends it `NUMERICAL_ERROR` after 207
iterations and 1.19e8 work units; the conic walk takes it to 0.575773803
at 1.21e8, and the checker takes both sides. On 51 points and 10 columns
the barrier solves it. `tests/test_quadratic.c` builds the 101-point
model with repeated products and asserts the hand-over and the answer.

## QPLIB's 17 convex mixed-integer QPs at 1e11 work units

`miqp.sh`. "02-256" is the reading before, "after" the one with the
change. The three columns between are routings measured first, each a
two-line change to `jaos_solve` in a scratch copy: the MIQP handed to the
conic branch and bound of `src/conictree.c` with its nodes solved by the
barrier or by the conic walk, and the LP tree kept with every node solved
by the conic walk. A cell is the verdict, or the incumbent's distance
above `qplib.solu`'s value when the work limit came first.

| instance | 02-256 | conic tree, barrier nodes | conic tree, conic nodes | LP tree, conic nodes | after |
|---|---|---|---|---|---|
| 10050 | optimal, 6387 nodes | limit, at the reference | limit, at the reference | limit, at the reference | optimal, 6387 nodes |
| 10056 | optimal, 1401 nodes | limit, at the reference | limit, at the reference | optimal, 1401 nodes | optimal, 1401 nodes |
| 10069 | optimal, 1 node | optimal, 1 node | optimal, 1 node | optimal, 1 node | optimal, 1 node |
| 3980 | no incumbent | +72.7% | +72.7% | no incumbent | no incumbent |
| 3913 | no incumbent | +10.5% | +11.1% | no incumbent | no incumbent |
| 4270 | no incumbent | +6.4% | +6.4% | no incumbent | no incumbent |
| 3871 | +27.1% | +6.1% | +0.0% | +27.1% | +27.1% |
| 3547 | +100.0% | +1.5% | +1.8% | +7.5% | +100.0% |
| 3698 | +65.7% | +48.7% | +23.3% | +65.7% | +65.7% |
| 3792 | +59.1% | +14.1% | +5.4% | +59.1% | +59.1% |
| 3694 | +70.7% | +118.1% | +62.0% | +70.7% | +70.7% |
| 3861 | +41.4% | +80.8% | +20.4% | +41.4% | +41.4% |
| 3708 | numerical error | numerical error | optimal, 45 nodes | optimal, 57 nodes | optimal, 77 nodes |
| 5577 | no incumbent | no incumbent | no incumbent | no incumbent | no incumbent |
| 5924 | no incumbent | no incumbent | no incumbent | no incumbent | no incumbent |
| 5527 | no incumbent | no incumbent | no incumbent | no incumbent | no incumbent |
| 5543 | no incumbent | no incumbent | no incumbent | no incumbent | no incumbent |

After the change QPLIB_3708 ends `OPTIMAL` at -9411.9989082312 against
-9411.998908, taken by the checker; every other line is the same as
02-256's, nodes and bounds included. 4 end `OPTIMAL` and 13 reach the
work limit.

The conic tree finds incumbents the LP tree does not, through its root
rounding and dive, and with conic nodes it reaches QPLIB_3871's
reference; it also loses the proofs of QPLIB_10050 and 10056. With
conic nodes their bounds stop at -26.66 and -34.25 against -25.70 and
-33.86, the optimal incumbent in hand since node 1, because the conic
tree branches on the most fractional column with no pseudocosts or cuts. The LP tree with conic
nodes solves QPLIB_3708 and gives QPLIB_5924, 5527 and 5543 a finite root
bound where the barrier's root spends the budget, but loses QPLIB_10050's
proof: 3504 nodes in 1e11 work units against 6387 in 7.5e10, a conic node
costing 2.4 times a barrier node there. Both are refused in
`bench/refusals.txt` (`miqp-conic-tree`, `miqp-conic-nodes`).

## Files

- `miqp.sh`: the 17-instance reading, against the tree, with the files
  02-256's `qpfetch.sh` fetches into `bench/instances-qplib`.
