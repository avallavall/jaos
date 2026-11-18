# 02-491 — sizing a run of the 240 MIPLIB 2017 benchmark instances

Taken on 2026-11-18 on cc4758dc, to prepare a one-off reading of JAOS
against Mittelmann's MILP benchmark (https://plato.asu.edu/ftp/milp.html,
the page of 28 Oct 2026). It is not a gate. Nothing here is the
comparison. `run.sh` is, and it waits for a quiet machine. Every reading
below was taken while other agents loaded the machine (load average 5 to
15 on its 12 logical CPUs), at one or two jobs, so its seconds are
indicative only.

- `../../miplib2017-bench.manifest`: the 240, pinned, fetched by
  `bench/fetch.sh` into `bench/instances-miplib2017-bench` (git-ignored).
- `mittelmann/`: his page as fetched (`milp.html`), its table
  `12threads.res` and the seven result files of `milp_res12/`.
- `run.sh`: the full run. `summarise.py`: the score and the bridge to his
  table. `highs.opt`: HiGHS on one thread at gap 0. `heavy.txt`: the
  instances that run first, two at a time. `factor.txt`: the 61 instances
  his serial HiGHS solves in 10 to 250 s.
- `sizing/`: `read.txt`, `jaos stats` on all 240 (wall clock, peak
  resident kB, exit code); `jaos-top20.txt`, the 20 largest under JAOS at
  30 s; `highs-factor.txt`, 14 under HiGHS at 120 s and gap 0, and
  `highs-factor-gap1e-4.txt`, the same at HiGHS's default gap;
  `bohle-30s.txt`, `neos-3402454-bohle` alone with a log. The three
  records are `run.sh`'s. `jaos-top20.txt` and `highs-factor-gap1e-4.txt`
  were taken before `run.sh` read the wall clock from CLOCK_MONOTONIC: its
  wall seconds there are `/usr/bin/time`'s, and `highs-factor.txt`'s are
  `/proc/uptime`'s.

## The set

`benchmark-v2.test`: 240 instances, 232 with a proven optimum, one open
(`supportcase22`) and seven infeasible. 639 MB of `.mps.gz`, 7.3 GB as
`.mps`. The fetch took 363 s. All 240 are fixed MPS; 17 have CRLF line
ends. The bound types are UP, LO, FX, FR, MI, PL, BV and LI, three files
have RANGES, and none has OBJSENSE, an objective constant, SOS,
indicators or semi-continuous columns. The 30 rows of
`bench/miplib2017.manifest` are identical here.

The optima are `miplib2017-v31.solu`'s. The current `v37` agrees on 238
and lowers two, `binkar10_1` to 6741.38002393972 and `neos-3754480-nidda`
to 12939.7540104743, as "objective improvements" below the optimum that
v31 lists. HiGHS, SCIP and JAOS stop at v31's values, and so does
Mittelmann: his HiGHS reports 6742.20002 on `binkar10_1`, marked ok,
which v37 would mark a mismatch under the checker's 1e-6.

JAOS reads 238 of the 240. It refuses `leo1` and `leo2`: each has one
line ` FR BOUND1 C0000001 .000000`, a free bound with a value, and the
reader stops on "bound type 'FR' takes no value". HiGHS 1.15.1 reads both
without a warning. On Mittelmann's table serial HiGHS solves `leo1` in
6011 s and not `leo2`, so at any limit below that the refusal costs no
solve that HiGHS makes. Reading is not the cost: all 240 read in 86 s,
`square47` in 15 s at 2.2 GB, `square41` in 8 s at 1.1 GB,
`neos-3402454-bohle` in 7 s at 0.9 GB, and every other file in under 4 s
and 0.6 GB.

## Mittelmann's table

Twelve threads of a Ryzen 9 5900X, 128 GB, 2 hours. The serial HiGHS is
1.15.0 (`modified.highs`), the parallel one 1.15.1; his SCIP is 10.0.0
with SoPlex 8.0.0, the SCIP version of `bench/compare`. He runs
"modified" instances, `p_NAME`: SCIP is used "to randomly perturb
rows/columns and performing a quick presolve". Those files are not
published, so a local run reads the originals, and one instance can move
by more than the machine factor.

His mean is the shifted geometric mean with a shift of 10 s, a failure at
7200 s: that shift reproduces all seven unscaled means of the page from
the result files (COPT 71, SCIP 1003, HiGHSp 549, HiGHS 763, OptVerse 173,
FiberSCIP 688, SCIPCO 586). A solve is status `ok`, solution `ok` or
`--`, and a time of at most 7200 s; that rule reproduces the page's solved
counts, 225, 136, 179, 158, 210, 158 and 160. The footer of
`12threads.res` counts otherwise and reads 192 for HiGHSp and 174 for
FiberSCIP. His files name the MIPLIB test script 1.0.1. Its checker
(`parse.awk`, read here in 1.0.4, the version the site serves) calls an
answer consistent within 1e-4 when the optimum is at most 100 in
magnitude, and within 1e-6 relative otherwise; `summarise.py` scores the
local runs by the same rule.

His solved counts at shorter limits, from the result files:

| limit, s | 60 | 120 | 200 | 300 | 450 | 600 | 1200 | 3600 | 7200 |
|---|---|---|---|---|---|---|---|---|---|
| COPT | 139 | 160 | 178 | 188 | 197 | 197 | 205 | 224 | 225 |
| OptVerse | 101 | 117 | 129 | 145 | 154 | 162 | 184 | 206 | 210 |
| HiGHS parallel | 56 | 70 | 83 | 93 | 110 | 118 | 136 | 160 | 179 |
| SCIP concurrent | 64 | 78 | 89 | 97 | 103 | 112 | 127 | 148 | 160 |
| HiGHS serial | 54 | 65 | 76 | 82 | 89 | 99 | 117 | 142 | 158 |
| FiberSCIP | 58 | 75 | 83 | 89 | 94 | 101 | 119 | 145 | 158 |
| SCIP | 46 | 56 | 67 | 73 | 82 | 86 | 104 | 123 | 136 |

He runs every solver at gap 0. The test script sets `MIPGAP=0.0` and
hands it to each solver (`run_scip.sh` writes `set limits gap`), and in
his files the dual bound equals the primal bound on every solved run with
a solution: 153 of 153 for HiGHS, 133 of 133 for SCIP, 219 of 219 for
COPT. `highs.opt`
sets `mip_rel_gap = 0` for that reason. At HiGHS's default 1e-4 the factor
moves little (below), but `timtab1` ends at 764774 against 764772, a
mismatch under the checker. JAOS keeps its default 1e-6 (shifted), which
the checker's rule already accepts; `JAOS_ARGS='--opt mip_gap=0'` makes it
gap 0 too. `bench/compare/scip_solve.py`, which `run.sh` uses for SCIP,
sets 1e-6.

## Memory

JAOS on the 20 largest by nonzeros, 30 s each (`sizing/jaos-top20.txt`;
`square47`, `square41`, `neos-3402454-bohle` and `neos-2075418-temuka`
alone, the rest two at a time). Seconds are JAOS's own; peaks are
resident GB of 2^30 bytes.

| instance | rows | columns | nonzeros | JAOS s | peak GB |
|---|---|---|---|---|---|
| `square47` | 61591 | 95030 | 27329856 | 34.1 | 3.05 |
| `square41` | 40160 | 62234 | 13566426 | 30.6 | 1.62 |
| `neos-3402454-bohle` | 2897380 | 2904 | 8953800 | 149.3 | 2.19 |
| `neos-2075418-temuka` | 349602 | 122304 | 7610261 | 31.2 | 0.97 |
| `neos-5052403-cygnet` | 38268 | 32868 | 4898304 | 30.4 | 0.89 |
| `supportcase19` | 10713 | 1429098 | 4287094 | 30.4 | 1.05 |
| `neos-5114902-kasavu` | 961170 | 710164 | 4240376 | 32.3 | 1.59 |
| `neos-4647030-tutaki` | 8382 | 12600 | 3953388 | 40.4 | 0.99 |
| `supportcase7` | 6532 | 138844 | 2845545 | 30.1 | 0.40 |
| `roi5alpha10n8` | 4665 | 106150 | 2370224 | 30.2 | 0.37 |
| `supportcase12` | 166781 | 799616 | 2334437 | 30.6 | 1.02 |
| `supportcase22` | 260602 | 7129 | 2228828 | 30.8 | 0.97 |
| `ns1644855` | 40698 | 30200 | 2110696 | 30.3 | 0.43 |
| `neos-5104907-jarama` | 489818 | 345856 | 2053548 | 30.6 | 1.04 |
| `neos-3402294-bobin` | 591076 | 2904 | 2034888 | 30.8 | 1.12 |
| `bab2` | 17245 | 147912 | 2027726 | 30.3 | 0.38 |
| `co-100` | 2187 | 48417 | 1995817 | 30.5 | 0.54 |
| `ns1760995` | 615388 | 17956 | 1854012 | 31.0 | 1.13 |
| `s100` | 14733 | 364417 | 1777917 | 30.2 | 0.47 |
| `savsched1` | 295989 | 328575 | 1770507 | 30.3 | 0.72 |

Every instance fits. The largest peak is `square47`'s 3.05 GB, 2.2 GB of
it the read. `neos-3402454-bohle`'s row is `sizing/bohle-30s.txt`. In the
record it reads `fail`: the hard kill stopped it at 100 s, and `timeout`,
not yet run with `--foreground`, took the rusage with it (below). In 30 s the root relaxation is solved on 16 of
the 20 (a `nodes 1` line); on `square47`, `neos-2075418-temuka`,
`neos-5114902-kasavu` and `neos-3402454-bohle` it is not. The tree a 300 s
run grows on these is not measured here. CLAUDE.md records a MIPLIB tree
past 8 GB on a long run, so `run.sh` caps each job's address space at
`MEM_GB / jobs` and reruns a refused one alone under the whole of it.
HiGHS peaked at 0.6 GB on the factor set. `heavy.txt` holds the four
instances past 1.5 GB: `square47`, `neos-3402454-bohle`, `square41` and
`neos-5114902-kasavu`.

## Limits and clocks

- JAOS's `--time-limit` covers the solve, not the read, and presolve does
  not look at the clock often. On `neos-3402454-bohle` at 30 s it stopped
  after 149.3 s with 0 nodes and 0 iterations, its work all in "the
  rest", the part before the root LP (`sizing/bohle-30s.txt`).
  `neos-4647030-tutaki` stopped at 40.4 s and `square47` at 34.1 s. At
  300 s bohle's presolve ends inside the limit. How far past 300 s the
  root LPs that follow can run is not measured here; the hard kill at
  510 s ends any run that goes that far. Mittelmann's time is the
  process's wall clock, reading included, and `summarise.py` scores the
  same (or the solvers' own seconds with `--solver-time`).
- HiGHS counts its read inside its time limit. At 10 s its reader gave up
  on `neos-3402454-bohle` ("Parser reached timeout"); at 300 s that does
  not happen.
- WSL's clocks disagree. `/usr/bin/time`'s wall clock is realtime, which
  steps back: `savsched1` read in -1.92 s, and a 105 s `timeout` fired at
  98 to 100 s by it. `/proc/uptime` jumps ahead: 165.7 s for the bohle run
  where JAOS's own CLOCK_MONOTONIC read 149.3 s, 7 s read and the exit
  around it, 157 s by `/usr/bin/time`. `run.sh` reads CLOCK_MONOTONIC.
- GNU `timeout` without `--foreground` sends its signal to its whole
  process group, itself included, and `/usr/bin/time` then loses the
  child's figures. `run.sh` runs `timeout --foreground`.

## The machine factor

HiGHS 1.15.1 on one thread, 14 instances his serial HiGHS solves in 11 to
57 s, spread from 560 to 1.16 million nonzeros, 120 s each, one job beside
the bohle probe, load 6 to 9 (`sizing/highs-factor.txt`). All 14 solve.

| instance | HiGHS s here | his s | ratio |
|---|---|---|---|
| `enlight_hard` | 9.93 | 11 | 0.90 |
| `swath1` | 10.48 | 13 | 0.81 |
| `physiciansched6-2` | 5.50 | 21 | 0.26 |
| `beasleyC3` | 17.54 | 21 | 0.84 |
| `n5-3` | 25.80 | 21 | 1.23 |
| `binkar10_1` | 22.57 | 24 | 0.94 |
| `ex10` | 26.93 | 24 | 1.12 |
| `air05` | 21.98 | 25 | 0.88 |
| `fiball` | 63.87 | 27 | 2.37 |
| `mzzv42z` | 30.77 | 31 | 0.99 |
| `neos-1582420` | 29.79 | 33 | 0.90 |
| `nw04` | 52.29 | 39 | 1.34 |
| `timtab1` | 37.37 | 46 | 0.81 |
| `dano3_3` | 60.39 | 57 | 1.06 |

The factor, local seconds over his, is 0.94 on HiGHS's own clock and 1.03
on the process wall clock (`/proc/uptime`'s), geometric spread x1.5. At
gap 1e-4 it read 1.04 on 13 of them. The two far ends, `fiball` at 2.37
and `physiciansched6-2` at 0.26, are wider than any machine difference;
the modified instances are the likely cause. Under this load a job can
share its core with another agent's, and one Zen 5 core of the 9600X
should outrun one Zen 3 core of the 5900X, so a quiet machine should read
below 1. The full run measures its own factor for that reason.

## The plan

- **Limit: 300 s per instance here**, one thread. The limit stays fixed
  here, and his table is cut at 300 / F s, F the factor the same run
  measures (`summarise.py`). At F about 1 his table at 300 s reads COPT
  188, OptVerse 145, serial HiGHS 82, SCIP 73: enough solves to place
  JAOS. His own budget, 2 h scaled by F, would take 80 to 95 hours of
  JAOS alone at 5 jobs. `leo1` and `leo2` count unsolved for JAOS.
- **Jobs: 5.** The 9600X has 6 cores and 12 threads. Two jobs on one core
  share it and slow each other, so 5 leaves one core to the system.
  Memory does not bind: 13 GB over 5 jobs is a 2.6 GB cap each, above
  every peak outside `heavy.txt`, whose four run first, two at a time, at
  6.5 GB each.
- **Time.** At most 240 × 305 s + 86 s of reading is 20.4 h of JAOS, 4.2
  h at 5 jobs with the heavy phase, and less by whatever JAOS solves.
  HiGHS on all 240 adds about 3 h (158 unsolved at 300 s, 82 solved), 7.3
  h for both. HiGHS on `factor.txt` alone adds about 15 minutes. SCIP
  adds about 3 h more.
- **Quiet means quiet.** The load in the record's header must read near 0
  at the start. Other agents' work both slows the jobs and shifts F.

Run from the main checkout under WSL, after `make compare-solvers` has
built HiGHS into `bench/compare/solvers`. The full head-to-head, about
7.3 h:

```
wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && bash bench/measurements/02-491/run.sh -t 300 -j 5"
```

JAOS alone with HiGHS on the factor set, about 4.5 h:

```
wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && bash bench/measurements/02-491/run.sh -t 300 -j 5 -s jaos -o bench/measurements/02-491/full"
wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && bash bench/measurements/02-491/run.sh -t 300 -j 5 -s highs -n bench/measurements/02-491/factor.txt -o bench/measurements/02-491/full-highs"
wsl -d Ubuntu-24.04 -- bash -c "cd /mnt/c/Users/vall-/Desktop/projectes/jaos && python3 bench/measurements/02-491/summarise.py bench/measurements/02-491/full/record.txt bench/measurements/02-491/full-highs/record.txt"
```

Add SCIP with `-s "jaos highs scip"` and `SCIP_PYTHON=~/scipvenv/bin/python3`
in front. A run that stops part way resumes with the same command. The
record is `OUT/record.txt`, and `OUT/logs/` holds every solver's output.
Commit the record and leave the logs. `summarise.py` prints each solver's
solved count (by the checker's rule, by the harness's, and the answers
that disagree with the manifest), its shifted mean and its peak, then F
and his seven solvers beside the local ones, all at 300 / F s on his
machine. The instances are read from `/mnt/c`; the whole set reads in 86
s, so a copy on the WSL disk (`-d`) saves nothing worth having.

## The reading of 2026-11-18

`full/record.txt` is JAOS on all 240 at 21fdf4d3 under `--threads 1`, 5
jobs, 16368 s, load 0.9 at the start. `full-highs/record.txt` is HiGHS on
`factor.txt`, 942 s; builds for the eil33-2 reading below ran beside it
(load 2.2 to 4.2 at its end), so its F is indicative. `summarise.py`:

```
solver    ran solved harness mismatch  sgm10 s  peak GB
highs      61     58      58        0     53.5     1.08
jaos      240     20      20        1    251.0    11.26
machine factor 0.995 (58 instances, spread x1.56)

On Mittelmann's machine at 302 s, over the 240:
M:copt 188, M:optverse 146, M:scipco 97, M:highsp 93, M:fscip 90,
M:highs 82, M:scip 73, here:jaos 20
```

- **One wrong answer.** `eil33-2` ended optimal at 1295.81 in 3 nodes,
  its optimum 934.008. The LP presolve called two of its `= 1` rows
  forcing at a node and fixed the column the optimum takes to 0: its test
  widened the lower side's tolerance by the upper side's bounds, which
  reduced-cost fixing had set near 1e14. Fixed in 02-496; `eil33-2` then
  solves in 33 s.
- **213 at the limit.** 96 of them are still at the root, one node, and
  97 hold no incumbent. 18 hold the reference objective and lack the bound.
- **6 end `numerical_error`**, five of them at the root: `leo1`, `leo2`,
  `ns1830653`, `sp97ar`, `sp98ar` (an incumbent at node 2) and
  `supportcase42`.
- **Memory.** `markshare2`, `markshare_4_0` and `mas74` passed the 2.6 GB
  cap of 5 jobs, against the 30 s sizing; alone at 13 GB they reach the
  limit (`mas74` at 4.8 GB). The largest peak is `opm2-z10-s4`, 11.8 GB.
- `neos859080` is infeasible, and JAOS says so in 5 s.
