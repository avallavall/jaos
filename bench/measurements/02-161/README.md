# 02-161 — the seven round-exhausted re-entries: stuck or slow, and what a bigger budget buys

D251. After D250, seven forced-primal solves exhaust all 128 settle
rounds. D245's question one level up: are they cut off while converging,
or oscillating?

## What is here

| file | what it does |
|---|---|
| `round-trace.sh` | throwaway build printing one line per re-entry round (violation, objective); runs the seven |
| `round-trace.txt` | the trace, as taken at `SETTLE_ROUNDS_PRIMAL = 128` |
| `rounds-sweep.sh` | in-place sweep of the constant over 256 and 512, `make clean` between settings, restoring source and record |
| `rounds-sweep.txt` | the sweep, as run |

Both derive the repository root and run from anywhere (D217).

## The trace: three stuck, four descending

| instance | shape at round 128 |
|---|---|
| `cycle` | objective frozen at -4.23387 for the last ~50 rounds, violation bouncing 0.5–16: stuck |
| `modszk1` | objective never moves once in 128 rounds (699998.447 throughout): stuck |
| `scsd8` | violation pinned at ~181 the whole run while the objective grinds: stuck on feasibility |
| `d6cube` | still descending, ~0.06 objective/round, ~50 away from the dual's answer: glacial |
| `stocfor3` | violation and objective both still improving at 128 |
| `truss` | still descending, far from target |
| `woodw` | still descending, close: 1.3446 against the dual's 1.30448 |

## The sweep: 256 buys woodw, 512 buys nothing at all

At 256: `woodw` converts to ok (77 measured), `stocfor3` moves to the
honest overrun column, the rest stay. At 512 the record is byte-identical
to 256 — the same 77/11/6 and the same per-instance labels. The plateau is
what the trace predicted: no round budget reaches a frozen trajectory.
`SETTLE_ROUNDS_PRIMAL` moves to 256; 512 is refused on the identical
record.
