# 02-398 — the far circulations: HiGHS is no judge, and no barrier switch finishes them

Taken on 2026-10-21 on the tree of 4b9aacb. `read.sh` takes the readings.
No code changed.

## HiGHS's answers

TODO row J24's seven 12-node circulations from 02-365's `netqp.py` that end
`numerical_error` (seeds 1104, 1156, 1163, 1282, 1308, 1340 and 1357).
HiGHS 1.15.1 calls each one optimal. None is: from each of HiGHS's points,
`cycle.py` finds a cycle of two or three arcs bounded only above along
which the objective still falls (`highs.txt`). On seed 1156, arcs x13 and
x5 have a slope of -7761 and a curvature of 1.56e-10 between them, so the
objective falls 1.93e17 below HiGHS's -2.34e15. JAOS finishes two of the
seven under some switch below, and there its objective is HiGHS's plus
that fall: -8.1064e12 on seed 1340 and -6.3469e14 on seed 1357. On seed
1357 the optimum puts x0 at -7.84e11, which is the cycle's own optimum,
1618.67 / 2.064e-9. These models have an answer, with flows of 1e11 to
1e13, and HiGHS cannot judge it.

## The walk on seed 1156

`walk1156.txt` is the walk with the growth stop lifted. By iteration 40 it
is at objective -3.1184e17, with primal 9.7e-6, dual 8e-7 and gap 4e-4.
For the next 120 iterations the primal residual stays at 9.741e-6 while mu
falls to 1e-27. `dump1156.txt` is the state at iteration 60. The far arcs
carry 1.1e13 to 5.3e13. Rows n0, n3, n9, n10 and n11 keep residuals of 0.2
to 5.5, and every arc that leaves that set of rows sits at a bound with a
slack near 1e-10. To move flow across the set's edge the walk would have
to lift one of those arcs; it drives the set's duals and those arcs' bound
duals to 1e13 instead. Those duals then make the gap: arc x22's bound dual,
2.7e13, times its bound, 3.83, is 1e14 on its own.

## The switches

`knobs.py` adds seven switches to the barrier, each described in its
header; `knobs.txt` reads them on the seven seeds. With no switch all seven
end `numerical_error`.

| switch | finishes |
|---|---|
| the primal step cut where a slack times its dual would pass 1000 mu | 1357 |
| the growth stop at 1e16 or 1e30 instead of 1e10 | none |
| the duals' free shift dropped in the walk past 1e3 of the data, stop at 1e30 | 1340, 1357 |
| the same past 1e6 | 1340 |
| a row residual under 1e-12 of its row's traffic counted as zero, stop at 1e30 | 1340 |
| one refinement step on the normal equations, stop at 1e30 | 1357 |
| a push when the walk stalls within 1e-3, stop at 1e30 | 1340, 1357 |

Each answer they give passes the checker. The other five seeds fail under
every switch. `near.txt` logs the last switch: the push from the stall
does not settle on 1104 or 1156, and on 1282 and 1308 the walk reaches
1e-10 and all three pushes still fail. So
two parts fail: the walk, once a set of rows is closed off by arcs at
bounds, and the push, at these scales. No switch lands. TODO row J24 holds
the next step.
