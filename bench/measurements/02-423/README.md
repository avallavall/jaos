# 02-423 — the steepest-edge FTRAN with a density estimate of its own

Taken on 2026-10-22 on the tree of c03bbdd. `tau-apart.patch` is the
change, refused; `netlib-apart.txt` is `make netlib J=12` with it.

## The idea

TODO row J8: in the dual simplex the FTRAN of the steepest-edge `tau`
takes 14% of `stocfor3`'s instructions (02-353). An FTRAN takes its
hyper-sparse path when a running estimate of its output density is under
1/`FTRAN_HYPER_DEN`, and the estimate is shared by every FTRAN called
without an output pattern. HiGHS keeps one estimate per kind of solve. The
patch gives the `tau` FTRAN an estimate of its own
(`jm_lu_ftran_apart`). Both paths do the same operations on the same
entries in the same order, so the answers cannot change; only the work
does.

## The reading

On the 94 Netlib models every status and iteration count is unchanged, and
the work's geometric mean is 1.0006x; no model moves past 0.9x or 1.02x.
`stocfor3` reads 431000513 work units against 430668173. Its `tau` is
dense, so an estimate of its own picks the dense path as the shared one
did (`bench/refusals.txt`: `ftran-tau-own-density`).
