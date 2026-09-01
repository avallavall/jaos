# 02-149 — two more `presolve.c` contracts become asserts

D236. Two contracts from `bench/measurements/02-121/presolve.c.md`.

## What is here

| file | what it does |
|---|---|
| `run-box-controls.sh` | six arms, each over all 139 gate instances |
| `controls.txt` | the arms, as run |

Derives the repository root and runs from anywhere (D217). Not a gate tool.

**The population run and the controls are one script here.** Every solve stops
as soon as presolve returns, so a full 139-instance pass costs under a minute
(02-148) and there is no reason to separate the quiet arm from the loud ones.

## The two

| the contract | the check |
|---|---|
| boxes only narrow | a live column never leaves the box the caller gave it |
| an entry survives only when its column AND its row are alive | the reduced matrix never carries a row index of -1 |

An **inverted** box is legal input (`jaos.h`), and the fold's clamp is skipped
there, so the first assert excludes those columns rather than claiming
something about them.

## Two of the arms are inverted asserts, not defects

An assert that is never evaluated is never violated, so a quiet intact arm
means nothing until something proves each check is reached. The `canary-*`
arms flip each assert and require it to fire.

| arm | firings | what it says |
|---|---|---|
| `intact` | 0 | both hold on all 139 |
| `canary-box` | 131 | the box loop is reached on 131 instances |
| `canary-compaction` | 115 | the fill loop is reached on 115 |
| `break-box` | 42 | the fold taking the implied bound without intersecting the box |
| `break-compaction` | 75 | the fill pass keeping a dead row's entry |
| `restored` | 0 | — |

Both asserts are therefore reached, quiet where they should be, and fired by a
realistic one-line defect. That is the full set of questions; D233's assert
could only answer two of the three.

## Reproducing

```
bash bench/measurements/02-149/run-box-controls.sh   # ~8 min
```
