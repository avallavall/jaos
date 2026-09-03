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

**`cplex2` keeps three row sides a cold re-solve does not need**, rows 34,
41 and 203's lower bounds, out of 338 members. `cplex2-replay.txt` shows
why: with the kept set the filter held at row 41, dropping that side
re-solves OPTIMAL, cold and warm alike, on a point the checker accepts at
1e-7 with a worst row violation of 9.99e-10; the final subsystem without
that side re-solves INFEASIBLE with a ray the checker certifies. A subset
of a feasible system cannot be infeasible, so one of the two verdicts is a
tolerance verdict, and both are consistent with the solver's: the model is
infeasible by less than the feasibility tolerance. Warm from the IIS-alone
basis all three re-solve INFEASIBLE, so a second pass of the filter would
drop them; D264 refuses that pass on cost and `bench/refusals.txt` carries
the reopen condition.

## What this does NOT say

It does not say the 28 are minimal in exact arithmetic: the oracle is the
same solver with the same tolerances, solving cold instead of warm. It
says the filter and a cold re-solve agree on every member of 28
instances, and disagree on three members of one.
