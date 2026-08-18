# Presolve's basis mapping is exact; the basis handed to it is not

Taken 2026-08-18, asking why six instances publish more basic variables than
rows (D130). It moves the defect upstream of presolve entirely. Closed as
D131, together with `02-41/`.

## The hypothesis, and it was wrong

`src/presolve.c:1635` maps a starting basis into reduced indices, and its
comment contemplates one direction only:

> A removed row or column recorded basic has no reduced counterpart to carry
> that status; dropping it **undercounts** the basic total, and
> `build_warm_basis` already falls back to the slack basis whenever the count
> is short of nrow — safe, never wrong, only colder than a fuller mapping
> could be.

Over-counting is contemplated nowhere. The obvious explanation was that
removing a row whose logical is NONBASIC costs a basis position and no basic
variable, so the identity should read

```
over = rows_removed - drop_row - drop_col - adj
```

**It fails on 61 of 88 solves.** `80bau3b` is over by 21 where that formula
predicts −5.

## The identity that does hold

Stated on what the mapping actually reads rather than on what a valid basis
would have contained:

```
nbasic_out = basic_in - drop_row - drop_col - adj
```

**0 failures of 88.** The mapping is arithmetically exact and does precisely
what its comment says.

The first formula assumed `basic_in == nr`, which is what a basis on the
caller's model has by definition. That assumption is the whole difference:

| instance | over by | `basic_in` | `nr` | already off by |
|---|---|---|---|---|
| `80bau3b` | 21 | 2288 | 2262 | **+26** |
| `finnis` | 12 | 512 | 497 | **+15** |
| `standata` | 10 | 344 | 359 | **−15** |
| `standmps` | 11 | 456 | 467 | **−11** |
| `vtp-base` | 2 | 164 | 198 | **−34** |
| `boeing1` | 1 | 351 | 351 | 0 |

**61 of 88 solves start from a stored basis whose count is already wrong on
the caller's own model**, in both directions, by up to 34.

## Two mechanisms, not one

`boeing1` is the control that separates them. Its stored basis counts exactly
right — 351 against 351 — and the mapping still lands it over by one. That is
the original hypothesis, and it is real: a row removed whose logical was
nonbasic. It accounts for one of the six.

The other five inherit a count that was already wrong before presolve saw it.

## Where that comes from

`src/model.c` states the rule twice and enforces it twice:

- `jaos_set_basis` **refuses** a basis handed in whose count is not
  `num_row` — *"structural … no later event makes a wrong count right"*.
- `basis_survives_or_goes` **clears** a stored basis whose count breaks on a
  dimension change — *"a basis that no longer counts is not a worse starting
  point, it is not a basis"*.
- **`jm_model_remember_basis` checks nothing.** It `memcpy`s
  `sol_col_status` and `sol_row_status` straight into `start_*`.

So the solver stores, as its own starting basis, something it would refuse
from a caller. `02-41/` asks the gate whether that is what it publishes too.

## Reproducing it

`run-overcount-ledger.sh`, beside this file. `-j 1` so instance names can be
matched to library output (D130). `src/` is read and never written.
