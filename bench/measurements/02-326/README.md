# 02-326 — SOS sets, semi-continuous columns and indicator rows in both trees

Taken on 2026-09-25 for TODO row J13, on the tree of 3ba4192 with the
batch that lets the conic tree take SOS sets, semi-continuous columns and
indicator rows, and with the fix below.

## The change

`src/conictree.c` refused a model with cones or quadratic rows and any of
the three. It now treats them as `src/mip.c` does. The relaxation holds a
semi-continuous column in `[0, upper]`, SOS members in their boxes, and an
indicator row only while its column is fixed at the switching value. A
semi-continuous column strictly between 0 and its lower bound is branched
on beside the integer columns, into 0 and its box. Once those hold, an SOS
set with too many nonzero members branches at its weighted middle, and an
indicator row the relaxation breaks while its column sits at the
switching value branches on that column. The solve that closes an
integral node fixes the SOS members within `CT_INT_TOL` of 0 and the
semi-continuous columns under half their lower bound at 0, and keeps the
indicator rows its fixed columns switch on. An answer whose SOS sets do
not hold is not taken.

## The fix

An unbounded relaxation made both trees look for an integer point with
the objective cleared and answer `UNBOUNDED` when they found one. That
rests on rational data: an unbounded relaxation of a model with an
integer point makes the model unbounded. An SOS set or an indicator row
joins several pieces into one relaxation, and a ray of the joined
relaxation need not lie in any piece. `min -x1` with `x1 - x2 <= 1` and
SOS1 `{x1, x2}` has the optimum -1 (`x2 > 0` forces `x1 = 0`), and
`min -x` with `x <= y` and `z = 1 -> y <= 5` over `z >= 1` has the
optimum -5; both trees answered `UNBOUNDED` on both.

Now a node whose relaxation is unbounded, in a model with SOS sets or
indicator rows, is split without a bound: on the first SOS set whose
members the node's box leaves too many of, at the middle of those
members, or on the first indicator row whose column the box leaves free to
take the switching value or another (`jm_open_piece` in `src/mip.c`, used
by both trees). The children keep the node's bound. Once the box settles
every set and row, the search with the objective cleared runs in the
node's box (`jm_piece_bounds`); an integer point there ends the tree
`UNBOUNDED`, and none closes the node. Models without SOS sets or
indicator rows take the old path.

## The reading

`discrete.c` extends 02-255's `misocp.c`: the same models, with 0 to 2
integer columns, and in half of them each a semi-continuous column
planted inside a box with a positive lower bound, an SOS set of type 1 or
2 over 2 or 3 continuous columns with the members its type leaves out
planted at 0, and (with an integer column) an indicator row, satisfied at
the planted point when its column sits at the switching value and broken
there when it does not. Brute force solves every piece: each integer
value, both sides of the semi-continuous column, each SOS pattern, and the
indicator row dropped where its column does not switch it on.
`discrete.sh RUNS SEED OUT` builds it against `build/release/libjaos.a`.

| seed | models | with a cone or a quadratic row | semi | SOS | indicator | optimal | infeasible | unbounded | failed | digest |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | 1000 | 841 | 431 | 344 | 279 | 629 | 115 | 256 | 0 | 48f691eaa23771fb |
| 2 | 1000 | 841 | 433 | 343 | 260 | 641 | 120 | 236 | 0 | e74fbe466943b5dc |
| 3 | 1000 | 828 | 421 | 333 | 282 | 659 | 115 | 226 | 0 | 65282102dfb69f85 |

Every verdict agrees with brute force; the worst objective gap is 3.5e-11
and the worst violation the checker reads 2.5e-13. Brute force was unsure
of 3 models (a piece it could not settle), which count as neither.

On 3ba4192 the first 300 models of each seed read 557 of 900 refused
(the conic tree's refusal) and two wrong verdicts from the linear tree,
`UNBOUNDED` where brute force found an optimum.

02-255's `misocp.sh` writes the same answers and digests at seeds 1 to 3
(1000 models each) as on 3ba4192. Its work moves by 0.01% to 0.03%, from
the MIP defaults of 02-325 on its models without cones.
