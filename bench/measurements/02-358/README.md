# 02-358 — exact solving of LPs

Taken on 2026-10-08 on the tree of 5c51216, for the SPECS row "Exact solving
with no tolerances" (TODO J19). The standard 94 Netlib models, one process,
120 to 300 seconds a model.

## The width test

`jaos_verify` refused a basis before any arithmetic when `bound_bits`, a
Hadamard-style estimate of the largest number the proof could need, passed
the limb budget. The real numbers are usually much smaller. `verify-netlib.sh`
reads the tree as it was (`verify-128-bound.txt`), and
`verify-netlib-nobound.sh` reads a build whose `src/verify.c` skips the test
under `JAOS_NOBOUND` (`build-limbs.sh` builds it with a chosen
`JM_EXACT_LIMBS`):

| build | proved | refused | broken | timed out | seconds |
|---|---|---|---|---|---|
| 128 limbs, width test | 28 | 60 | 6 | 0 | |
| 128 limbs, no width test | 50 | 33 | 11 | 0 | 112 |
| 512 limbs, no width test | 59 | 9 | 16 | 10 | 1557 |

Every operation on a `jm_nat` checks its limbs, so a number that outgrows
them ends the proof as refused; the test only saved time. With it removed,
22 more bases prove at 128 limbs, and the slowest refusal takes 13 s
(`stair`). 512 limbs reach further at ten times the time, so the default
stays at 128 (D275 in `docs/tolerances.md` made the same choice).

## The repair

A broken basis fails by a tiny amount: 8 of the 11 at 128 limbs on a reduced
cost (as small as 5.6e-17 on `degen3`), the other 3 (`ganges`, `scorpion`,
`sierra`) on a basic value outside its bound. `jaos_set_exact` checks the
floating-point basis and, where the check breaks, pivots in exact arithmetic:
dual simplex steps from a dual feasible basis, primal steps from a primal
feasible one, and for a basis that is neither, the costs of its wrong-signed
columns are moved until it is dual feasible, that problem is solved, and the
costs are restored. Ties go to the smallest index (Bland), so no step cycles.

`exact-netlib.sh` runs `jaos solve --exact` on the 94 (`exact-128.txt`):

- 59 optima proved: 50 at once, 9 after exact steps (`ship04l`, `ship04s`
  and `ship12s` 1 pivot, `ship12l`, `boeing2` and `czprob` 2, `degen3` 4,
  `boeing1` 7, `ganges` 82 pivots, 1 bound flip and 1 cost shift).
- 2 infeasible over the rationals: `sierra` after 18 dual steps and
  `scorpion` after 6. A dual step that finds no entering column shows that
  the leaving value cannot reach its bound; the model's double data, where
  0.04 is the double nearest 0.04, are inconsistent by about 1e-17, inside
  the floating-point tolerance. `scorpion`'s exact certificate writes a proof
  file that `jaos check --proof` accepts; `sierra`'s proof file outgrows the
  limbs.
- 33 end `numerical_error` because a number outgrew the limbs.

381 seconds in all; `degen3` takes 204 of them, since each exact solve of
its 684-row block takes about 15 s.
