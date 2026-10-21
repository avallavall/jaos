# 02-395 — the format round trips on new seeds

Taken on 2026-10-21 on the tree of 2e8ba6e. `read.sh` takes the sweep. No
code changed.

| generator | seeds | models | result |
|---|---|---|---|
| 02-226's `formats.c` | 7 to 10 | 12000 | 38089 round trips, none bad (`formats.txt`) |
| 02-252's `writers.sh` | 3 and 4 | 2000 | 0 files differ (`writers.txt`) |

In `writers.txt`, the files that do not plainly agree are the kinds
02-252 named: 25 where a third reading sides with JAOS, and 14 where HiGHS
ran out of its 20 s.
