# 02-494 — a time limit that every step before the tree reads

TODO row J33. On `neos-3402454-bohle`, `jaos solve --time-limit 30` ended
`time_limit` after 149 s with no node (`02-491/sizing/bohle-30s.txt`).
The 240-instance run of 02-491 stops each solve at its limit, so an
overrun there reads as JAOS's time. Taken on 2026-11-18 under WSL on the
Ryzen 5 9600X, two runs at a time while other agents loaded the machine
(load 2 to 10 on its 12 threads), so the seconds are indicative. The
readings before the change are on 6cda6d89, where one thread was the
default. The change was written there and rebased onto 64fa237b, which
adds sub-MIPs, ZI rounding and threaded strong-branching probes and makes
every core the default; the readings after it are on the rebased tree
with `--threads 1`, except `threads-30.txt`. The steps that overran
(`src/symmetry.c`, the presolves, the separators) are the same at both
commits.

- `run.sh BIN LIMIT OUT LIST [JOBS] [ARGS...]` runs `jaos solve
  --time-limit LIMIT --log progress ARGS` on each instance of LIST from
  `bench/instances-miplib2017-bench` (or `DIR`), stamps every log line
  with the seconds since the run started (CLOCK_MONOTONIC) and writes one
  line per run: status, JAOS's `time`, the wall seconds, the stamp of the
  first log line, the work units.
- `top20.txt`: the 20 largest of `bench/miplib2017-bench.manifest` by
  nonzeros, `bohle` among them (02-491's `sizing/jaos-top20.txt`).
- `before-30.txt`, `after-30.txt`: the 20 at 30 s, before and after.
- `six-300.txt`, `after-300.txt`: six of them at 300 s, after.
- `threads.txt`, `threads-30.txt`: six at 30 s on every core, one at a
  time, after.
- `trace.patch`: the temporary trace, on 6cda6d89, that logs the seconds
  and work after each step of the tree's start and of the root, the MIP
  presolve's seconds, and every LP presolve or set-up past 0.3 s.
  `trace-30.txt`: its readings on 11 of the 20 at 30 s, `bohle` under the
  plain tool at `--log detail`, and `co-100`.
- `after-30-early.txt`, `after-300-every64.txt`: readings of earlier
  states of the change, on 6cda6d89, that set its last three steps.
- `work-limit.txt`: `bohle` at `--work-limit 100000000`, the change on 6cda6d89.

## Where the time went

`bohle` at 30 s, before: the read took 15 s and the solve 141 s. The log
prints the tree's settings at 15.0 s and nothing more until the tree
stops at 141.9 s, with 0 nodes and 3,040,692,694 work units. The symmetry
search, which runs at the root because orbital branching is on by
default, has a cap of 250 times the model's size, 250 × 11,854,085 =
2,963,521,250 units, and it spent all of it: 2.96e9 of the 3.04e9 units
and about 127 s, reading no clock. The tree's first check then found the
limit gone.

The search overran on more than `bohle`. Its seconds at 30 s
(`trace-30.txt`, and the plain logs of `before-30.txt` for the three the
trace did not run): `neos-3402294-bobin` 48 (its whole cap again),
`square47` 34.5, `neos-5114902-kasavu` 29.8, `neos-2075418-temuka` 24.2,
`neos-5052403-cygnet` 18.9, `square41` 14.8, `neos-4647030-tutaki` 13.6,
`ns1760995` 12.4, `s100` 10.9, `neos-5104907-jarama` 10.4, `bab2` 5.4.
The other steps that ignored the limit:

- **The separators inside a root cut round.** On `tutaki` one round of
  MIR aggregation took 12.9 s and 2.3e10 work units, from 22.0 s to
  35.0 s of the tree's clock. The round loop read nothing between its
  separators, and the re-solve after them stopped at once, too late. On
  `co-100` the clique separator takes 3 to 4.5 s a round for 1.7e6 work
  units, a count far below its cost, and once the symmetry search
  stopped in time it still ended a 30 s run 2.4 s late
  (`after-30-early.txt`; the `co-100` part of `trace-30.txt`).
- **The read and the MIP presolve.** `--time-limit` covered the solve,
  not the read: 15 s on `bohle`, about 18 s on `square47`. The MIP
  presolve ran before the tree's clock started: 0.9 s on `tutaki`, 1.2 s
  on `square41`, and on a model it leaves unchanged its seconds were left
  out of `time` as well.
- **LP presolve and the aggregator** ran before the LP's own clock
  started, so a relaxation's limit did not count them; at 30 s no LP
  presolve or set-up passed 0.5 s. The simplex then read the clock every
  64 iterations, which on `bohle`'s root LP, at about 0.12 s an
  iteration, is every 7.6 s: at 300 s its tree stopped 2.1 s late and the
  run ended at 302.8 s (`after-300-every64.txt`).

By reading the code, more steps read no clock: the second symmetry
search before the first branch (1000 times the size), probing's loop,
strong branching's candidates and its last re-solve, the sub-MIPs of RINS
and local branching, which were given no time limit at all, and the loops
of the combinatorial heuristics (feasibility jump, the exchanges,
threshold accepting, the big-M moves, the schedule search, the link
heuristic), which read only their work budgets. At 300 s `tutaki`'s root
spends 78 s in its heuristics, up to the feasibility jump's incumbent, and
116 s in the second symmetry search. The root LP itself read its limit:
on `square41`, `supportcase19`, `s100` and `bab2` it ran to 30.2 to
30.5 s and stopped.

## What reads the limit now

A deadline, the start plus the limit, read by `jm_past` (`src/util.c`),
which reads the clock only when there is a limit: with none it returns
false at once, so a run without a time limit takes the same steps and
counts the same work as before.

- The tool reads the clock before the read (`jaos_clock`, new in the C
  API, `jaos.clock()` in Python) and gives the solve what the read left;
  a read that spends it all leaves the solve `DBL_MIN`, and the solve
  stops at its first look.
- The MIP's clock starts where `jaos_solve` enters the branch and bound:
  the MIP presolve reads it between its passes, and the tree, the
  presolved model's tree and the reruns after it get what is left.
- The symmetry search reads it before each refinement pass, every
  `SYM_CLOCK_STRIDE` (65536) adjacency entries a pass keys, and before
  each merge level of its sort, and does not start once the limit is
  gone.
- Every root cut round reads it before its separators and before its
  re-solves, and the separators read it row by row, Gomory cuts row of
  the tableau by row and the clique separator seed by seed.
- Probing, strong branching's candidates, threaded or not, and its last
  re-solve, every heuristic's LP solves (through `heur_budget`) and the
  main loops of the combinatorial heuristics read it, and every sub-MIP
  gets the time that is left instead of none.
- An LP's clock starts on entry, so presolve and the aggregator count;
  presolve reads it between rounds, the aggregator between passes, and
  the simplex at every iteration, so `TIME_CHECK_EVERY` is gone. The
  barrier and PDLP count from the same entry.

A tree stopped in its root cut rounds, or after probing, now reports the
root relaxation's bound instead of none: `tutaki` ended at 30 s with bound
26911.64 where it reported `-inf`.

## The 20 largest at 30 s

Wall seconds of the whole process, the read included, and the overrun
past 30 s (`before-30.txt`, `after-30.txt`). JAOS's `time` is the
solve's alone, and after the change it is the limit less the read.

| instance | before: JAOS s | wall s | overrun | after: JAOS s | wall s | overrun |
|---|---|---|---|---|---|---|
| `square47` | 36.1 | 54.3 | +24.3 | 18.8 | 30.5 | +0.5 |
| `neos-3402454-bohle` | 141.4 | 156.8 | +126.8 | 24.3 | 30.3 | +0.3 |
| `square41` | 30.6 | 39.1 | +9.1 | 23.3 | 30.5 | +0.5 |
| `neos-2075418-temuka` | 30.5 | 33.6 | +3.6 | 27.7 | 30.4 | +0.4 |
| `neos-5114902-kasavu` | 32.7 | 35.5 | +5.5 | 23.6 | 30.2 | +0.2 |
| `neos-4647030-tutaki` | 42.5 | 44.1 | +14.1 | 26.5 | 30.2 | +0.2 |
| `neos-5052403-cygnet` | 30.5 | 32.5 | +2.5 | 28.7 | 30.2 | +0.2 |
| `supportcase19` | 30.4 | 34.1 | +4.1 | 27.4 | 30.2 | +0.2 |
| `supportcase7` | 30.1 | 31.2 | +1.2 | 29.1 | 30.1 | +0.1 |
| `roi5alpha10n8` | 30.2 | 31.7 | +1.7 | 28.8 | 30.1 | +0.1 |
| `supportcase12` | 30.6 | 32.2 | +2.2 | 28.6 | 30.2 | +0.2 |
| `supportcase22` | 30.6 | 32.7 | +2.7 | 29.2 | 30.1 | +0.1 |
| `ns1644855` | 30.6 | 31.7 | +1.7 | 29.4 | 30.1 | +0.1 |
| `neos-5104907-jarama` | 31.2 | 33.2 | +3.2 | 29.3 | 30.3 | +0.3 |
| `neos-3402294-bobin` | 48.2 | 50.5 | +20.5 | 28.8 | 30.2 | +0.2 |
| `bab2` | 30.3 | 32.4 | +2.4 | 28.6 | 30.1 | +0.1 |
| `co-100` | 30.5 | 31.9 | +1.9 | 29.2 | 30.1 | +0.1 |
| `ns1760995` | 30.8 | 32.5 | +2.5 | 29.1 | 30.2 | +0.2 |
| `s100` | 30.2 | 32.8 | +2.8 | 28.1 | 30.1 | +0.1 |
| `savsched1` | 30.2 | 31.8 | +1.8 | 28.9 | 30.2 | +0.2 |

After, every run ends between 30.1 and 30.5 s; before, between 31.2 and
156.8. The last tenths are the tree's end and the process's exit, which
frees up to 3 GB. `after-30-early.txt`, before the separators read the
clock and before `SYM_CLOCK_STRIDE`, ended `square41` at 34.1 s, `co-100`
at 32.5 and `temuka` at 31.9 on a machine at load 8 to 10.

## At 300 s

Six of them, the four heaviest and two whose root LP runs long
(`after-300.txt`), and the same before the simplex read the clock at
every iteration (`after-300-every64.txt`, on 6cda6d89):

| instance | status | JAOS s | wall s | overrun | every 64: wall s | overrun |
|---|---|---|---|---|---|---|
| `neos-3402454-bohle` | time_limit | 295.1 | 301.4 | +1.4 | 302.8 | +2.8 |
| `neos-4647030-tutaki` | time_limit | 298.9 | 300.3 | +0.3 | 301.1 | +1.1 |
| `square47` | time_limit | 286.2 | 300.5 | +0.5 | 300.6 | +0.6 |
| `neos-5114902-kasavu` | time_limit | 298.0 | 300.5 | +0.5 | 301.1 | +1.1 |
| `neos-2075418-temuka` | time_limit | 297.9 | 300.3 | +0.3 | 300.9 | +0.9 |
| `supportcase19` | time_limit | 297.4 | 300.2 | +0.2 | 301.0 | +1.0 |

`bohle`'s tree stops at 300.2 s; the 1.2 s after it frees its 2.9
million-row relaxation and the process. Five of the six end their 300 s
at the root, one node and one relaxation: the root LP of `bohle`,
`kasavu`, `temuka` and `supportcase19` is still running, and `square47`'s
is solved and stopped before its first cut round, with bound 9 where the
stop before the change would have reported `-inf`. `tutaki` reaches 15
nodes.

## On every core

The six of `threads.txt` at 30 s with no `--threads`, every core being
the default at 64fa237b, one at a time (`threads-30.txt`):

| instance | status | JAOS s | wall s | overrun |
|---|---|---|---|---|
| `neos-3402454-bohle` | time_limit | 24.8 | 30.3 | +0.3 |
| `square47` | time_limit | 19.1 | 30.4 | +0.4 |
| `neos-4647030-tutaki` | time_limit | 28.9 | 30.1 | +0.1 |
| `co-100` | time_limit | 29.4 | 30.2 | +0.2 |
| `supportcase19` | time_limit | 27.9 | 30.2 | +0.2 |
| `bab2` | time_limit | 29.2 | 30.1 | +0.1 |

Strong branching's threaded probes read the limit through each probe's
own simplex, and the loop over their results stops once it is gone.

## The work limit

Unchanged. The steps above run under work caps that are not the
caller's: every sub-MIP and every strong-branching probe sets
`work_limit` on its own model, and the symmetry search, presolve and the
separators run inside them in a default run, often with a cap smaller
than their own. A work-limit check there would stop them in the default
gates too, and move them. So a work limit still overruns where a step's
own cap exceeds what is left: `bohle` at `--work-limit 100000000` ends
`work_limit` at 3,040,692,694 units after 167 s (`work-limit.txt`).

## The default readings

Without a time limit nothing moves. On the rebased tree, against the
records at 64fa237b, these rewrote their `bench/results/` file byte for
byte: `make netlib netlib-infeas J=4`, `make netlib-kennington J=2`,
`make miplib J=2`, `make miplib2017 J=2` (its 30 lines, the same 9
solved), and `make primal barrier pdlp concurrent warm J=2`. The same
held on 6cda6d89 before the rebase against its own records. `make test`,
`make sanitize`, `make tsan` and `make python-test` pass.
