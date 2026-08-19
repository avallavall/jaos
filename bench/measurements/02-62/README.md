# The row-activity check becomes an invariant, and it took four wrong versions that each reported a defect that was not there

Taken 2026-08-19, the standing debt `numerics-reviewer` proposed in its own
words. Closed as D153.

**This record was rewritten.** Its first version reported `pilotnov` as a
genuine defect. That was the third of four false alarms, and the correction is
the useful part of the entry.

## What was open

Two producers in the replay assign `sol_row[i]` outright where every other
producer accumulates: `JM_PS_EMPTY_ROW` writes `0.0` and `JM_PS_SINGLETON_ROW`
writes `rec->coef * xv`. Both are correct today by an argument about arena
order that nothing checks, and **the class has already cost one campaign
(D106)**.

The proposed enforcement: recompute every row's activity from `sol_col` at the
end of postsolve and assert it matches `sol_row`. **D152 is what made this
runnable** — before it, eleven of the 94 aborted an assert build before
reaching any new assert.

## The answer

**The two assignments are fine.** With the predicate stated correctly, all 139
instances pass. The debt is answered and the check is now an invariant of
every debug build.

## Four wrong versions, each of which reported a defect

Every one of these is the obvious thing to write, so each is recorded with the
measurement that killed it.

| # | version | what it reported | why it was wrong |
|---|---|---|---|
| 1 | no OPTIMAL gate | all **29** netlib-infeas instances | both call sites run the replay whatever the verdict, because the index mapping is owed even for a stopping point; there `sol_col` and `sol_row` are not required to agree |
| 2 | fixed multiple of eps × traffic | `osa-30`, `osa-60` | their rows carry **72554 and 173365 nonzeros**; a naive sum of n terms is bounded by `(n-1)·eps·Σ\|t\|`, not by a constant times eps |
| 3 | comparing every row | `pilotnov`, 18 rows, worst 131x | a row whose logical rests on a bound publishes the tight value, not the column sum — see below |
| 4 | *(the harness, not the check)* | 29 of 29 infeasible | the script omitted `-e infeasible`, so every instance looked like a gate failure rather than an assert |

### Version 3 is the one worth reading

The first version of this record called `pilotnov` row 931 a genuine defect:
an equality at zero, three nonzeros, published activity exactly 0.0 while the
published columns make -1.93e-07 — 100 times what a three-term sum can
accumulate. The arithmetic in that sentence is right. The conclusion was not.

**The split by basis status is total** (`disagreeing-rows.txt`):

| instance | rows out | logical basic | logical on a bound |
|---|---|---|---|
| `pilotnov` | 18 | **0** | **18** |
| `osa-30` | 1 | 1 | 0 |
| `osa-60` | 1 | 1 | 0 |

A nonbasic logical means the basis is asserting the constraint is tight. The
activity published for it is the tight value; the column sum is a different
quantity, carrying the **basis solve's primal residual**. That residual is
bounded by the basis conditioning, and nothing at that site bounds it — 4.6e-14
relative on `pilotnov` is conditioning, not rounding in a three-term sum.

The row trace settled which producer wrote it (`../02-63/row-trace.txt`): only
one touches row 931, the copy from the reduced solve. No `ps_row_add`, no
`EMPTY_ROW`, no `SINGLETON_ROW` — so the two suspected assignments are
directly exonerated on the very row that looked worst.

**A fifth version was tried and is also wrong**: asserting that a nonbasic
row's activity equals its ORIGINAL bound exactly fires on **44 of the 94**.
The replay adds restored columns on top of a reduced activity, so the original
bound is not what is left there.

## What ships

```c
if (orig->sol_row_status[i] != JAOS_BASIS_BASIC)
    continue;
const double w = ps_round_tol(traffic[i]);
const double window = nnz[i] > 1 ? w * (double)(nnz[i] - 1) : w;
assert(fabs(orig->sol_row[i] - act[i]) <= window);
```

Under `#ifndef NDEBUG`, on both postsolve paths, gated on an OPTIMAL solve.
No new constant: `ps_round_tol` carries the already-swept
`PRESOLVE_ROUND_ULPS`, and the term count comes from the matrix.

## What it cost

**Validated before believed** (`activity-check.txt`), which is the whole
reason the false alarms were caught rather than committed as findings:

| build | check fires on | with asserts OFF |
|---|---|---|
| `JM_PS_EMPTY_ROW` overwrites a row whose share already arrived | **45 of 94** | 0 of 94 |
| every basic row's activity moved by 1.0 | **81 of 94** | 0 of 94 |

The "asserts OFF" column is the control that matters: the injected fault is
silent on its own, so the aborts are the check firing.

**Clean tree: 0 of 139.** netlib 0/94, netlib-infeas 0/29, Kennington 0/16.

**D152's property survives**: all 94 standard instances still run under
`-UNDEBUG`, 0 aborts.

**Inert in the release build, measured rather than argued**: the compiled
objects have the same md5 as the parent's, so the gate campaign carries over
untouched. `make test`, the `-DJAOS_NO_PRESOLVE` reference build and
`make sanitize` all exit 0.

## Reproducing it

```
bash bench/measurements/02-62/run-activity-check.sh     # validate, then run
bash bench/measurements/02-62/run-disagreeing-rows.sh   # the status split
bash bench/measurements/02-63/run-row-trace.sh 931      # who writes the row
bash bench/measurements/02-61/run-assert-build.sh       # D152's property
```
