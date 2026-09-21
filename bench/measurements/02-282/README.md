# 02-282 — a crash basis for the dual, started on Devex weights, refused

Taken on 2026-09-21 on 64c27d6 (TODO B13). The `SPECS-crash-basis` refusal
held because the dual starts from exact steepest-edge weights, which only
the slack basis gives for free. The dual Devex of `../02-278/` starts from
weights that are exact for any basis, so the question could be asked again.

`crash-candidate.diff` is the candidate, behind `JAOS_B13_CRASH`: a
triangular crash in Bixby's preference order. Free columns first, then
columns with one finite bound, then boxed ones, sparser columns first
inside each group, fixed and empty columns never. A column enters when its
largest entry in a row no earlier column has touched is within 0.9 of its
largest entry overall; it then touches every row it has an entry in, so
the basis stays triangular. Rows left over keep their logicals. The basis
is installed as a warm start, and the dual starts in Devex with it as the
framework. Node solves of the MIP tree and the primal keep the slack basis.

Through the gate runner the netlib set did not finish in 11 minutes, where
it takes under one. `run-capped.sh` then solved each instance through the
tool with a work limit of 4 times its baseline (`netlib-capped.txt`:
name, baseline work, status, work, iterations, objective):

- 29 of the 94 stop at the limit: 25fv47, bandm, cycle, czprob, d2q06c,
  d6cube, degen2, degen3, dfl001, greenbea, greenbeb, maros, modszk1,
  pilot, pilot-ja, pilotnov, scrs8, scsd6, scsd8, share1b, ship08l,
  ship08s, ship12l, sierra, standata, standmps, stocfor2, stocfor3, woodw.
- The 65 that finish read 1.5843x their baseline work, 24 of them past 2x.

The literature said as much (`docs/research/crash-basis.md`): a crash
basis is not dual feasible, so the dual pays shifts and a longer walk
before it reaches the slack basis's first vertex, and the basis is denser
to factor.
