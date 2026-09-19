# 02-259 — the conic tree's root heuristics on the MIP tree's MIQP

02-258 read the conic branch and bound against the MIP tree on QPLIB's 17
convex mixed-integer QPs. The conic tree found incumbents the MIP tree
does not, through its two root heuristics: a solve with every integer
column fixed at the relaxation's rounded value, and a dive that fixes
the half of the fractional integer columns nearest an integer at each
solve. The MIP tree's own rounding keeps the continuous columns where the
relaxation left them, which breaks the equality rows of a mixed model,
and its dive fixes one column per solve up to `MIP_DIVE_HEURISTIC` (50)
solves, each of them a barrier solve on a MIQP.

## The change

At the root of a model with a quadratic objective, before the MIP
tree's own dive and while no incumbent is known, `src/mip.c` runs the
rounded solve (`fixed_for_point`) and, when it gives no feasible point,
the halving dive (`halve_for_point`), capped at `MIP_DIVE_HEURISTIC`
solves and ended by the rounded solve on its last point. The point goes
through `rounded_point` and the incumbent path like every other
heuristic point. `--no-heuristics` and `--dive-heuristic 0` turn both
off. A model with no quadratic objective never reaches them.

## QPLIB's 17 convex mixed-integer QPs at 1e11 work units

`bench/measurements/02-258/miqp.sh`, before (02-258's "after") and after.
A cell is the verdict, or the incumbent's distance above `qplib.solu`'s
value when the work limit came first.

| instance | before | after | nodes before | nodes after |
|---|---|---|---|---|
| 10050 | optimal | optimal | 6387 | 6387 |
| 10056 | optimal | optimal | 1401 | 1401 |
| 10069 | optimal | optimal | 1 | 1 |
| 3980 | no incumbent | no incumbent | 3204 | 3203 |
| 3913 | no incumbent | +32.6% (56.925) | 2747 | 2769 |
| 4270 | no incumbent | +6.4% (105.882) | 1 | 1 |
| 3871 | +27.1% | +27.1% | 2215 | 2214 |
| 3547 | +100.0% (0) | +64.2% (-0.2007) | 3454 | 3459 |
| 3698 | +65.7% | +65.7% | 506 | 504 |
| 3792 | +59.1% | +59.1% | 758 | 755 |
| 3694 | +70.7% | +70.7% | 341 | 340 |
| 3861 | +41.4% | +41.4% | 316 | 314 |
| 3708 | optimal | optimal | 77 | 77 |
| 5577 | no incumbent | no incumbent | 1 | 1 |
| 5924 | no incumbent | no incumbent | 1 | 1 |
| 5527 | no incumbent | no incumbent | 1 | 1 |
| 5543 | no incumbent | no incumbent | 1 | 1 |

Three instances gain; no verdict and no incumbent gets worse. Where the
heuristics find nothing better than the MIP tree's own dive, their
solves cost 1 to 3 nodes of the 1e11 budget. QPLIB_5577, 5924, 5527 and
5543 spend the whole budget at the root node, the last three inside its
relaxation, and none of them gets an incumbent. On the five DML files that had an incumbent the new
points are no better than the MIP tree's own dive finds, so the
incumbents stay where they were, 27% to 71% above the reference; the
conic tree's end 1.5% to 62% above it (02-258).

`make miplib`: the 24 instances give the same lines to the bit, since
none has a quadratic objective.
