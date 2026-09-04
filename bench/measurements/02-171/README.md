# 02-171 — an irreducible infeasible subsystem for each reference infeasible

D264. `jaos_iis` (`src/iis.c`) run over the 29 netlib infeasibles under the
default build, with the solver itself as the oracle for the two words in
the name: the sides it returns must be infeasible on their own, and each
one dropped must be feasible.

## What is here

| file | what it does |
|---|---|
| `iis-population.c` / `run-iis-population.sh` | one line per instance: size, where the candidates came from and how many, members split into row and column sides, the filter's re-solves and their work, and the oracle's word. A second call must return the same sides and counts. Runs the instances in parallel, one process each, and prints them in manifest order. Writes `iis-population.txt` |
| `cplex2-replay.c` / `run-cplex2-replay.sh` | the one instance the oracle rejects, replayed: for each row side it does not need, the kept set the deletion filter held when it reached that side, re-solved without it cold and warm, with the checker's report on the point or on the ray. Writes `cplex2-replay.txt` |

## The reading

**28 of 29 pass the oracle, all 29 reproduce, and the certificate's
support was the candidate set on every one** (no instance fell back to
every finite side). `iis-population.txt` has the sizes; `bgindy` is the
largest at 2091 members, 2087 of them column bounds, and `klein3` the
costliest at 138 re-solves for 5.8e8 work units.

**`cplex2` keeps three sides a cold re-solve does not need**, and they are
column sides: column 26 lower, column 30 upper and column 35 lower, out of
232 members. (This paragraph said "rows 34, 41 and 203" and "338 members"
until 2026-09-04; both came from a run this directory superseded eleven
minutes later. D269.)

`cplex2-replay.txt` shows why, in three arms per side. Dropping the side
from the final subsystem re-solves INFEASIBLE, cold (arm B) and warm
(arm A), with a ray the checker certifies. Replaying **the filter's own
walk** with the side relaxed at the re-solve where the filter reached it
(arm C) re-solves OPTIMAL, on a point the checker accepts at 1e-7 with a
worst column violation of 3.45e-08 and a worst row violation of 1.09e-16.
A subset of a feasible system cannot be infeasible, so one of the two
verdicts is a tolerance verdict, and both are consistent with the solver's:
the model is infeasible by less than the feasibility tolerance. Warm from
the IIS-alone basis all three re-solve INFEASIBLE, so a second pass of the
filter would drop them; D264 refuses that pass on cost and
`bench/refusals.txt` carries the reopen condition.

**Arm C is why the output file went stale.** It was added to
`cplex2-replay.c` after `cplex2-replay.txt` was last written, so the
committed output had no OPTIMAL in it at all while D264 and this page both
described one. Re-run at `6cae092` on 2026-09-04; the output here is that
run, and the script is no longer newer than its own result.

## What this does NOT say

It does not say the 28 are minimal in exact arithmetic: the oracle is the
same solver with the same tolerances, solving cold instead of warm. It
says the filter and a cold re-solve agree on every member of 28
instances, and disagree on three members of one.
