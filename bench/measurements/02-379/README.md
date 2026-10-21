# 02-379 — a bound point at the conic tree's root, refused

Taken on 2026-10-21 on the tree of c5fc71f, for TODO row J13.

## The idea

`bound-point.patch` on `src/conictree.c`. At the root, when the rounding
of the relaxation and the dive leave no incumbent, solve the model twice
with every integer column fixed: once at its lower bound, once at its
upper bound (0 when that bound is infinite).

## The reading

`read.sh`: HEAD against the tree with the patch.

| | HEAD | patch |
|---|---|---|
| CBLIB's 80 mixed-integer instances at 1e10 (`cblib-mip-*.txt`) | 30 optimal, 78 with an incumbent | the same |
| QPLIB's 14 mixed-integer QCQPs at 1e10 (`miqcqp-inc-1e10.txt`, `miqcqp-inc-patch-1e10.txt`) | 2 with an incumbent | the same 2 |
| 02-255, seed 1 work units (`mip-*.txt`) | 381220029 | 428961205 |
| 02-255, seed 2 work units | 393187439 | 442170630 |
| 02-255, seed 3 work units | 395618929 | 441631094 |

No model gains an incumbent. On CBLIB only the work of a few
`sssd-weak-*` lines and `pp-n100000-d10` moves. 02-255's answers and
digests do not change, and its trees cost 11.6% to 12.5% more work. The
patch is refused (`bench/refusals.txt`: `conic-tree-bound-point`).

QPLIB_10010, the smallest of the 12 with no incumbent, shows why: its
first row asks that 50 of its binaries be 1, so neither bound point is
feasible.

## A correction to 02-376

02-376 said that all 14 of QPLIB's mixed-integer QCQPs stop at 1e11 with
no incumbent. Its `miqcqp.sh` prints no incumbent for a work-limit status,
so that line could not see one. `miqcqp-inc-1e10.txt` prints it: on HEAD
QPLIB_3678 and QPLIB_7579 have one from node 1 (-0.10019 against QPLIB's
-0.11009, and -0.09495 against -0.10263). The other 12 have none at 1e10.
At 1e11 (`miqcqp-inc-1e11.txt`) 5 of the 14 have one:

| model | incumbent | QPLIB's optimum | first found at node |
|---|---|---|---|
| QPLIB_10001 | -25488.99 | -25766.82 | 172 |
| QPLIB_10004 | -2491.96 | -2588.21 | 463 |
| QPLIB_10010 | -22906 | -23003.31 | 281 |
| QPLIB_3678 | -0.10019 | -0.11009 | 1 |
| QPLIB_7579 | -0.10004 | -0.10263 | 1 |

The other 9 have none.
