# The two singleton families are the whole of it, and the sum closes exactly

Taken 2026-08-18. **It corrects D132** at the point D132 named as unestablished,
and the correction is large. Closed as D133.

## What D132 could not do, and why

D132 attributed every restored entity to the record that *restored* it, read
off the arena. It said plainly that this is not the record that *wrote its
status last*, because `JM_PS_SINGLETON_ROW` assigns a status to a column a
different record restored. Its row numbers stood; its column split did not.

This records the writer **at the moment of the write**. Every access to
`sol_col_status` and `sol_row_status` inside `ps_replay_one` — six and eight
sites, all with a simple `[i]` or `[j]` index — goes through a helper that
stamps the arena index currently replaying and returns the slot, so the call
site stays an assignment. A branch not taken records nothing.

## The attribution, and it is complete

| last writer | netlib basics | rows | drift | Kennington basics | rows | drift |
|---|---|---|---|---|---|---|
| `REDUNDANT_ROW` | 1138 | 1138 | 0 | 296 | 296 | 0 |
| `FORCING_ROW` | 3672 | 3672 | 0 | 10272 | 10272 | 0 |
| `EMPTY_ROW` | 1758 | 1758 | 0 | 11150 | 11150 | 0 |
| `IMPLIED_FREE_COL` | 2088 | 2088 | 0 | — | — | — |
| **`FIXED_COL`** | **0** | **0** | **0** | **0** | **0** | **0** |
| **`EMPTY_COL`** | **0** | **0** | **0** | — | — | — |
| **`SINGLETON_COL`** | 5902 | 0 | **+5902** | 482 | 0 | **+482** |
| **`SINGLETON_ROW`** | 6624 | 8622 | **−1998** | 106818 | 81646 | **+25172** |
| survivors (unwritten) | 148294 | 148294 | 0 | 411412 | 411412 | 0 |

**The sum closes.** netlib: `5902 − 1998 = +3904`, and the published basics
are 169476 against 165572 rows — off by **3904**. Kennington:
`482 + 25172 = +25654`, published 540430 against 514776 — off by **25654**.
Nothing is unaccounted for, which is what the canary at the foot of the script
checks.

**The survivors balance exactly**, 148294 and 411412 both ways. The reduced
solve's basis is a basis; nothing is wrong upstream of the replay.

## Three corrections to D132

- **`FIXED_COL` and `EMPTY_COL` contribute exactly nothing.** D132 read 3620
  and 18. Both were misattribution, and the case writes `AT_LOWER` or
  `AT_UPPER` and never `BASIC` — which is what reading
  `src/presolve.c:1846` says, and now what the measurement says too.
- **`SINGLETON_ROW` on netlib is −1998, not −6724.** It writes 6624 basics for
  the 8622 rows it restores, not 1898.
- **`SINGLETON_COL` is the larger contributor on netlib**, +5902 on 96 of 172
  solves, and D132 did not name it at all.

## The mechanism, which is now readable

`JM_PS_SINGLETON_COL` (`src/presolve.c:2131`):

```c
orig->sol_col_status[j] =
    (xv == rec->hi) ? JAOS_BASIS_AT_UPPER : JAOS_BASIS_BASIC;
```

**Its row survives** — the record's `index` names a row that stays, relaxed —
so it restores a column, marks it `BASIC` whenever the value is not at `hi`,
and no row comes back to pay for the basis position. That is +1 per firing,
and it is the whole of netlib's `SINGLETON_COL` drift.

`JM_PS_SINGLETON_ROW` restores a row and writes both that row's status and the
status of the column its row folded into. The net depends on which branch each
firing takes, and the sign differs between the sets: **−1998 on netlib and
+25172 on Kennington**. A repair has to handle both directions.

## What is left open

The repair. It now has two families, an exact per-family price, and a closing
sum to check any candidate against. Nothing here is costed, and no source file
was touched.

`SINGLETON_COL`'s shape is the simpler of the two and is stated above in one
line of code. `SINGLETON_ROW` changes sign between the sets and needs its
branches counted before anything is proposed.

## Reproducing it

`run-last-writer.sh`, beside this file. The rewrite is a regex over
`ps_replay_one`'s body and asserts it found exactly 6 and 8 sites, so a future
write site that the regex misses fails the run instead of being dropped.
`src/` is read and never written.
