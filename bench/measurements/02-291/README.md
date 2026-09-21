# 02-291 — a crash basis for the primal simplex, refused

Taken on 2026-09-22 on the tree of e4a9f82 for TODO row C3, by
`bench/primal` on the standard 94 at the cap of 10x the dual's work.

`primal-crash.diff` is the candidate: the triangular crash of
`../02-282/` in Bixby's order (free columns first, then one finite bound,
then boxed, sparser first in each group), installed as the primal's start
when no basis is on the model. The dual keeps the slack basis. 02-282
measured the crash for the dual only.

| arm | primal / dual work | iterations | overruns |
|---|---|---|---|
| the slack basis (`primal-control.txt`) | 2.5796 | 1.1744 | 6 |
| the crash, pivot 0.9 (`primal-crash.txt`) | 2.1893 | 0.9743 | 9 |
| the crash, pivot 0.5 (`primal-crash-pivot05.txt`) | 2.1160 | 0.9442 | 10 |

The control's six are d6cube, dfl001, fit1d, fit2d, pilot and seba.
pilot is the one that is not in row C3's list of five: it joined when
the aggregator (2e04b47) made the dual cheaper.

The crash lowers the mean by 15% and 18%, and it does not bring any of
row C3's five inside the cap. It brings pilot inside and sends grow15,
sctap2, sctap3 and sierra out at pivot 0.9, and bnl2 too at pivot 0.5:
sctap2's primal goes from 329 iterations to 673. A start that is not
feasible costs the composite phase 1 more on those models than the slack
basis does.

`primal-control.txt` is also the new `bench/results/primal.txt`: the file
had not been retaken since the aggregator.
