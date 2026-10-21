# 02-396 — every LP algorithm on generated models; the primal hands off

Taken on 2026-10-21 on the tree of ec0cc55. `read.sh` takes the reading.

## The generator

No generator ran the barrier, PDLP or the concurrent solve on small
models; the gates run them on Netlib only. `algos.c` takes 02-235's LPs (8
to 24 columns, 6 to 16 rows, most of them infeasible or unbounded) and
solves each with the five algorithms. Each answer must be the dual
simplex's status, an optimum within 1e-6 of its objective that the checker
passes, an infeasibility certificate that `jaos_check_certificate` takes,
or a ray that `jaos_check_ray` takes.

## What it found

On HEAD the barrier, PDLP and the concurrent solve pass every model on
seeds 1 to 3. The primal simplex fails on every infeasible model where its
phase 1 decides: `jaos_solve` returns `JAOS_ERR_NUMERICAL` with "the primal
phase 1 cannot reduce a total bound violation of ... any further", since
phase 1 publishes no proof. Two rows, `x + y <= 1` and `x + y >= 2`, are
enough. `--algorithm primal` therefore gave an error on an infeasible LP,
though `docs/cli.md` says the two simplex methods give the same answer.

## The change

When the primal simplex, run because the caller asked for it, ends with no
answer, `src/simplex.c` starts the dual simplex from the slack basis on
the same model, and the dual decides it, with a certificate when it is
infeasible. The iterations carry over. An arm of the concurrent solve keeps
giving up (a new `cfg.concurrent_arm`), since the dual runs there as its
own arm; `tests/test_concurrent.c` now sets that flag to state its premise.

## The readings

| set | HEAD | the change |
|---|---|---|
| `algos.c`, seeds 1 to 3 (1577 optimal, 1358 infeasible, 65 unbounded) | the primal fails 196, 218 and 232; the other three none (`head.txt`) | none fail (`new.txt`) |
| `make netlib netlib-infeas J=12`, `make netlib-kennington J=2` | | 0 regressed, the same results files |
| `make primal barrier pdlp concurrent warm J=12` | | the same results files |

A MIP was not affected: its tree handles a node's failed solve itself. 300
of 02-394's models outside the two general-integer families, solved with
`--algorithm primal`, end as the default does on 297, and reach the work
limit on 3 larger knapsacks, on HEAD as with the change.
