# The basic-ness migrates from restored rows to restored columns, and `SINGLETON_ROW` is where

Taken 2026-08-18, following D131. It establishes two things and **fails to
establish a third**, which is written down here rather than glossed.

## The arithmetic a correct postsolve satisfies

The reduced solve leaves exactly `rrow` basic variables and
`jm_postsolve_expand` copies those onto the surviving rows and columns. So the
replay must add exactly one basic variable per row it restores — no more and
no less — for the published basis to have `num_row` of them.

## What is established

**Every removed row and column is claimed by a record.** `ORPHANED` reads
`0/0` on all 204 solves. There is no coverage gap: the defect is not an
entity nobody restores.

**The row-restoring families balance, and `SINGLETON_ROW` does not.**

| family | basics | rows restored | drift | solves off |
|---|---|---|---|---|
| `REDUNDANT_ROW` | 1138 | 1138 | 0 | 0 |
| `FORCING_ROW` | 3672 | 3672 | 0 | 0 |
| `EMPTY_ROW` | 1758 | 1758 | 0 | 0 |
| `IMPLIED_FREE_COL` | 2088 | 2088 | 0 | 0 |
| **`SINGLETON_ROW`** | **1898** | **8622** | **−6724** | **130 of 172** |

netlib. Kennington is the same shape and larger: `SINGLETON_ROW` restores
81646 rows and marks 29174 of them basic, **−52472**, on 24 of 32 solves.

`SINGLETON_ROW` restores half of netlib's removed rows and leaves 78% of them
nonbasic. It is the family at the centre of the defect.

**And the basic-ness is not lost, it moves to a column.** The same run counts
9770 restored *columns* coming out BASIC on netlib where the rows are short by
6724 — `FIXED_COL` 3620 of the 32000 columns it restores, `SINGLETON_COL` 6132
of 17234, `EMPTY_COL` 18 of 2148. `JM_PS_SINGLETON_ROW`'s own replay writes
`orig->sol_col_status[j] = JAOS_BASIS_BASIC` for the column its row folded
into (`src/presolve.c:2037`), which is exactly that migration.

## What is NOT established, and the probe is why

**Which family wrote each of those column basics.** The probe attributes an
entity to the record that *restored* it, read off the arena. That is not the
same as the record that *wrote its status last*: `JM_PS_SINGLETON_ROW` assigns
a status to a column that a different record restored, so a column counted
under `FIXED_COL` here may carry a status `SINGLETON_ROW` wrote.

The per-family column numbers above are therefore **counts of what each
family's entities ended up as**, not an attribution of who set them. The row
numbers do not have this problem — no family writes a row another family
restored — which is why the `SINGLETON_ROW` deficit stands and the column
split does not.

Settling it needs a last-writer probe: a global set before each
`ps_replay_one` call and recorded at every `sol_*_status` write inside it.
That is a bigger patch than this one and it is the obvious next step.

## Two probe errors, both caught, both worth naming

- **Ownership was read forwards.** The replay is strictly LIFO (D-07), so the
  *lowest* arena index touching an entity writes last. Walking the arena
  forwards records the first writer instead. Fixed — and the numbers came back
  identical, which is itself the finding that no entity is claimed by two
  records.
- **A helper placed before the includes does not compile**, because the tag
  enum is declared in `jaos_internal.h`. It now sits after it.

## Reproducing it

`run-basis-attribution.sh`, beside this file. `src/` is read and never
written; the patch is applied in a throwaway worktree.
