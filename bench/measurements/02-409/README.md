# 02-409 — the IIS and the feasibility relaxation; a presolve verdict its certificate refuses

Taken on 2026-10-21 on the tree of 42d2597. `read.sh` takes the reading.

## The harness

`iis.c` puts each of 02-235's LPs that the dual simplex finds infeasible
to `jaos_iis` and to `jaos_feasrelax` under its three scopes. The IIS's
model must be infeasible, and feasible without any one of its member
sides. Adding every move the relaxation returns to its bound must give a
feasible model, and the total must match an elastic LP built in the
harness, with a slack of cost 1 on every finite side that may move.

## What it found

On HEAD, seeds 1 to 3, 3000 models each (`iis-head.txt`): 4020 infeasible
models, 43637 member sides each dropped once, 11768 relaxations. One
property breaks: on seed 2's model 1156 the rows' relaxation moves six
row sides by a total of 100.333, the elastic LP's optimum, and the model
with the moves applied is still infeasible to every algorithm. The
relaxation itself finds that moved model feasible with no further move.

Its sides carry thirds: 40 - 40.666666666666664 leaves -0.6666666666666643
on row R13, so the stored model is infeasible by about 3e-15. Presolve
fixes the columns of R13, finds it empty with a lower side of 2.66e-15
against its rounding tolerance of 1.78e-15, and calls the model
infeasible. The certificate it publishes has a gap of 2.6e-15, 7e-16 of
its terms, and the checker refuses it. Without presolve, the dual simplex
finds a point that breaks no row by more than 6.2e-15, which its
tolerance accepts.

## The change

`jm_dual_simplex` in `src/simplex.c` holds an infeasible or unbounded
verdict that presolve reached alone to its certificate or ray: when the
checker does not certify it at `PRESOLVE_CERT_TOL` (1e-9), the model is
solved again without presolve (a new internal `cfg.no_presolve`). Every
LP algorithm and the MIP tree's node solves pass through it.
`tests/data/lp_presolve_edge.mps` is the moved model; the dual, the
primal, the barrier and PDLP now end optimal on it, and the checker
passes their points.

At `PROBE_CERT_TOL` (1e-6), the first value tried, Netlib's infeasible
`gran` went the same way: its presolve certificate has a gap of 8e-6,
4e-8 of its terms, which certifies at 1e-9 and not at 1e-6, and the
simplex took 1586 iterations and 72220x the work to reach the same
verdict. 1e-9 is the tolerance the generated-model readings hold every
certificate to.

## The readings

| set | HEAD | the change |
|---|---|---|
| `iis.c`, seeds 1 to 3, 3000 models each | 1 broken (`iis-head.txt`) | none (`iis-new.txt`) |
| 02-396's `algos.c`, seeds 1 to 3 | | byte-identical |
| `make netlib netlib-infeas J=12`, `make netlib-kennington J=2` | | the same results files |
| `make primal barrier pdlp concurrent warm J=12` | | the same results files |
| `make miplib J=2` | | the same results file |
