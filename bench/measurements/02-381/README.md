# 02-381 — a backtracking root dive in the conic tree, refused

Taken on 2026-10-21 on the tree of cd65942, for TODO row J13.

## The idea

`dive-backtrack.patch` on `ct_dive` in `src/conictree.c`. The root dive
fixes half of the fractional integer columns at each step. With the patch,
a step whose solve is not optimal undoes its own fixings, restores the
point, and tries again with a quarter of the share, up to 3 times.

## The reading

QPLIB's 14 mixed-integer QCQPs at 1e10 (`miqcqp-inc-backtrack-1e10.txt`,
against 02-379's `miqcqp-inc-1e10.txt` for HEAD): the same 2 incumbents
and no new one. On QPLIB_10010, the case it was written for, the dive took
14 solves instead of 9, fixed the same 144 columns and still ended
infeasible, so the trouble comes from fixings made before the last step.

The patch did not reach the models it was meant for, so the wider
readings were not taken (`bench/refusals.txt`: `conic-dive-backtrack`).
