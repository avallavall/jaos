# 02-160 — how each of the fourteen re-entry failures actually ends

D250. After D249 took `agg` out of the "settled point is not dual
feasible" family, fourteen instances remained, assumed one family. A
census of `reenter_after_settling`'s exit paths says they are two, and
neither is numerical:

| exit | instances |
|---|---|
| the re-entry's dual run stops on the WORK LIMIT (`again=4`) | `80bau3b` (round 27), `bnl2` (0), `d2q06c` (23), `fit1p` (32), `greenbea` (4), `pilot-ja` (11), `pilotnov` (7) |
| all 128 rounds used (`SETTLE_ROUNDS_PRIMAL`) | `cycle`, `d6cube`, `modszk1`, `scsd8`, `stocfor3`, `truss`, `woodw` |

The `cleanup-no-pivots round=0` line every instance also shows is the
plain dual solve of the pair, which re-enters, finds nothing to do, and
leaves cleanly at round 0.

## What is here

| file | what it does |
|---|---|
| `reentry-census.sh` | throwaway diagnostic build tagging every exit of `reenter_after_settling`; runs the fourteen |
| `reentry-census.txt` | the census, as taken on the tree before D250's fix |

Derives the repository root and runs from anywhere (D217). The repository
tree is never touched; the patch asserts each anchor matched exactly once.

## What each half means

The first seven are the forced-primal harness's own 10x work budget
expiring inside the re-entry. The driver then found the settled point not
dual feasible — of course: the repair was cut off mid-flight — and
published NUMERICAL_ERROR, so a budget stop wore a numerical error's
label. That mislabel reaches the public API: a caller's
`jaos_set_work_limit` expiring inside any re-entry got the same wrong
verdict, and the resume contract the header states was unreadable from
it. D250's fix propagates the stop.

The second seven are D245's shape one level up: the round backstop binds
at 128. Whether their trajectories are still descending (a budget cut,
D245's answer) or oscillating (D89's) is the next question, and
`02-157/run-settle-rounds.sh` is the instrument that answers it.
