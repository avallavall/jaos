# 02-324 — tau's FTRAN with a density history of its own, refused

Taken on 2026-09-25 on the tree of f3e8c9b, for TODO row J8.

The FTRAN picks its hyper-sparse path from a running mean of its results'
density, one mean for calls with a pattern and one for calls without.
The dual steepest-edge `tau` shares the second with every dense right-hand
side of `compute_primal`, so the diff gives it a third mean of its own
(`jm_lu_ftran_alone`, `tau-class.diff`).

`make netlib J=12`: every iteration, objective, digest and basis the same,
work 1.0006x (sctap2 0.999x, bore3d 1.007x). `tau` averages 17% dense
(TODO J8), above the 10% at which the hyper path takes over, so its own
mean keeps it on the dense path nearly always, and the mean cannot follow
a density that splits between under 1% and over 10% from one solve to the
next. Refused as `tau-own-density-class`.
