# 02-146 — is `s->verified` ever stale when a reader looks at it?

D233. The lead D232 left open, measured rather than argued.

## The question

`bench/measurements/02-121/simplex.c.md` proposed
`assert(!s->verified)` at `pivot()`'s entry, for the sentence "every caller
clears it before `pivot()`". Reading the code says that sentence is false:
`reenter_after_settling` calls `primal_cleanup`, which pivots, and clears the
flag only afterwards. So either the prose is wrong or a reader somewhere
trusts a verification a pivot has spent.

## What is here

| file | what it does |
|---|---|
| `run-verified-census.sh` | four counters in a release build, over all 139 gate instances |
| `run-verified-controls.sh` | the assert that replaced the proposed one, and the edits that do and do not fire it |
| `census.txt` | the census, as run |
| `controls.txt` | the arms, as run |

Both derive the repository root and run from anywhere (D217). Neither is a
gate tool. `run-verified-controls.sh` links `02-145/probe.c`.

**The census is `# PINNED: 4d1430e`**, the tree before this change. It asks
what the code did with the flag written at fifteen scattered sites, and that
question is answered once; at HEAD there is one writer and the patch has
nothing to attach to. `record-check` is what noticed — it refused the script
the moment `pivot()`'s first line moved.

## The census

| set | pivots | with the flag already set | stale reads | longest stale run |
|---|---|---|---|---|
| 94 standard | 527020 | 6 | 0 | 1 |
| 29 infeasible | 71882 | 0 | 0 | 0 |
| 16 Kennington | 434624 | 0 | 0 | 0 |

The six are two each on `etamacro`, `wood1p` and `pilot87`. **`stale_read` is
0**: no reader ever sees a verification a pivot has spent. The prose is
wrong and the code is right.

The counters go into a RELEASE build, so this runs at gate speed rather than
the fifty minutes an assert-enabled Kennington costs (02-145). `bench/run`'s
workers leave through `_exit(0)`, which runs no destructor (D229), so the
report is called by hand at that site — one worker is one instance, so the
report is per instance, in a single `write()` because twelve workers share one
stderr (02-99).

## What the arms say

Only the **canary** fires the assert: `set_verified` stops resetting the
counter, so it grows from the first pivot and the next reader trips. That is
what says the assert is live.

**No realistic single-site edit fires it**, and the four quiet arms are why
that is a measurement rather than a gap:

- Dropping the dual loop's pre-pivot clear changes nothing, and so does
  dropping the primal phase 1's. The census says why — the flag is already
  false wherever those two loops pivot, so both clears are defensive rather
  than load-bearing.
- Dropping one of `reenter_after_settling`'s two clears changes nothing,
  because the other still covers the property.
- Dropping **both** changes nothing either. After a `primal_cleanup` pivot,
  `reenter_after_settling` either returns or ends its round; it never reaches
  one of `run()`'s seven readers. The sequence the assert catches — verify,
  then pivot, then read — is unreachable on this population however many
  clears are removed.

The assert stays for the reason D232 kept the FORCING one: it is free in a
shipping build, it states the property the prose got wrong, and the canary
shows it would catch the sequence if a refactor created it. The record does
not claim a realistic defect proved it.

## Reproducing

```
bash bench/measurements/02-146/run-verified-census.sh    # ~12 min
bash bench/measurements/02-146/run-verified-controls.sh  # ~15 min
```
