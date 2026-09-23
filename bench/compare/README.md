# The comparison harness

Times JAOS against HiGHS, SoPlex and Clp on the Netlib standard set, and
against HiGHS and SCIP on MIPLIB 3 and the MIPLIB 2017 reading. Nothing
here is part of what JAOS ships. Its record is `results/`, and it carries
seconds, which `bench/results/` never does.

JAOS is timed against other solvers on LP and MIP only. A QP rung and a
conic rung are a row in `TODO.md`.

## Run it

```
make compare-solvers
make compare COMPARE_ARGS='-t P0'
```

`make compare-solvers` runs `fetch-solvers.sh`, whose `build_*` functions
fetch and build each competitor, pinned by checksum in the `*.manifest`
files. Run it once first: `make compare` builds HiGHS alone, so without it
SoPlex and Clp are not timed. Rung `P0` is every solver's own presolve on,
the dual simplex forced, no crash basis, one thread. Never run bare
`make compare`: it defaults to rung T0, which was defined when JAOS had no
presolve.

The MIP comparison runs one set at a time:

```
SCIP_PYTHON=/path/to/python bash bench/compare/run-mip.sh \
    -m bench/miplib2017.manifest -d bench/instances-miplib2017 -t 20
```

`SCIP_PYTHON` names a Python that has `pyscipopt`; without it SCIP is left
out. `-x EXT` reads `NAME.EXT` instead of `NAME.mps`; for anything but
`mps` or `mps.gz` HiGHS is left out, since it reads neither CBF nor the
other formats.

## The rungs

| tier | JAOS | the others |
|---|---|---|
| T0 | dual simplex, no presolve | dual forced, presolve off. Historical |
| **P0** | dual simplex, presolve | dual forced, presolve on. The rung JAOS is judged on |
| T1 | as T0 | free to pick primal or dual. Historical |
| T2 | as T0 | presolve on. Historical |
| T3 | as T0 | stock defaults. Historical |

T0 to T3 were last run on 2026-08-11 on tree e467810, before JAOS had
presolve. Each of T1 to T3 changes one setting of the others against T0.
The harness now solves JAOS with its defaults, presolve on, so a new run of
those rungs would no longer change one thing at a time. `results/` also keeps three older records: `P0-2026-08-14.txt` and
`T2-2026-08-14.txt` (tree fd1bd6d) and `P0-2026-08-17.txt` (tree
a88e99b).

## The reading

`results/P0.txt`, 2026-09-22 on tree 7311fa3, geometric mean of
per-instance ratios over the instances above a 0.05 s floor:

| | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 2.03x | 0.66x | 1.93x |
| iterations | 1.14x | 0.45x | 1.06x |
| time per iteration | 1.78x | 1.48x | 1.83x |
| JAOS faster on | 1 of 19 | 15 of 20 | 4 of 16 |
| worst instance | `stocfor3` 18.3x | `truss` 1.9x | `stocfor3` 15.7x |

`summarise.py` recomputes the same figures from the record. SoPlex's and
Clp's objectives on `pilot87` miss the reference, so that instance counts
against HiGHS only.

The reading before, `results/P0-2026-09-21.txt` on tree 6ae3966, reads
3.47x, 1.01x and 2.77x per solve in `summarise.py`. The harness's own
summary block printed 3.46x, 1.01x and 2.76x, because it rounds each
per-instance ratio to two decimals before the mean. The aggregator (2e04b47) did most of the
difference: it took out rows and columns the dual used to pivot on, so the
iteration counts fell from 1.63x, 0.63x and 1.37x. One iteration still
costs 1.5x to 1.8x what it costs each rival.

## The MIP reading

`run-mip.sh` solves each instance with JAOS, HiGHS 1.15.1 and SCIP 10.0
(through `pyscipopt` 6.2.1), 20 s each and one thread. All three stop at the
relative gap JAOS stops at, 1e-6 (`MIP_GAP`): `highs-mip.opt` and
`scip_solve.py` set it for the other two. An instance counts as solved when
the solver ends optimal, or at its gap limit, with the objective at the
reference within `1e-6 * max(|ref|, 1)`. `summarise_mip.py` prints the
solved counts, the shifted geometric mean of the seconds (a shift of 1 s,
an unsolved instance counted at the limit), and the ratio of the shifted
means.

2026-09-23 (evening), tree d6245e0, `results/mip-miplib.txt` and
`results/mip-miplib2017.txt`, on an otherwise idle machine:

| set | JAOS | HiGHS | SCIP | JAOS / HiGHS | JAOS / SCIP |
|---|---|---|---|---|---|
| MIPLIB 3, 24 instances | 23 solved, 0.93 s | 24, 0.66 s | 24, 0.59 s | 1.17x | 1.22x |
| MIPLIB 2017, 30 instances | 0, 20.00 s | 8, 13.69 s | 8, 12.28 s | 1.43x | 1.58x |

Since d6245e0 a node keeps the fixed column its starting basis holds
basic, so its LP starts from the parent's basis whole
(`bench/measurements/02-304/`). On MIPLIB 3 that took JAOS from 22 solved
at 1.18 s (tree 12180a6, that morning: 1.34x and 1.40x) to 23 at 0.93 s:
`bell5` now solves in 1.1 s at 14767 nodes. `l152lav` stays at the limit,
where the rivals need 1.7 s, and `bell3a` takes 8.8 s against 0.24 s and
0.74 s. On the 2017 set JAOS stays at the limit on all 30; the change shows
there in the gap at the limit (`bench/results/miplib2017.txt`), not in
the seconds. The reading of 2026-09-21 (tree 3086162) is kept as
`results/mip-*-2026-09-21.txt`: 23 solved, 1.43x and 1.48x on MIPLIB 3;
1.37x and 1.51x on the 2017 set.

## The QP and conic readings

`run-qp.sh` solves the 138 Maros-Meszaros QPs with JAOS (its barrier),
HiGHS 1.15.1 (its QP solver, `highs-qp.opt`: one thread, tolerances 1e-7)
and Clp 1.17.11 (`-barrier`), 20 s each. The conic reading is `run-mip.sh
-x cbf.gz` over the 29 continuous CBLIB instances with JAOS and SCIP; SCIP
has no CBF reader in this build, so `scip_cbf.py` reads the file and hands
SCIP each cone as a quadratic constraint. Both are summarised by
`summarise_mip.py` with the MIP reading's rule: solved means optimal (or
at the gap limit) with the objective within `1e-6 * max(|ref|, 1)` of the
reference, and the times are shifted geometric means with a 1 s shift, so
instances that take well under a second weigh little.

2026-09-23, tree f7b65c9, `results/qp-maros-meszaros.txt`, on an otherwise
idle machine:

| set | JAOS | HiGHS | Clp | JAOS / HiGHS | JAOS / Clp |
|---|---|---|---|---|---|
| Maros-Meszaros, 138 QPs | 133 solved, 0.23 s | 96, 2.04 s | 119, 0.69 s | 0.40x | 0.72x |

JAOS stops at the limit on `cont-300` and the three `cvxqp*_l`, and it
refuses `values`, whose Q is not positive semi-definite. HiGHS stops at the
limit on 21, ends with a solve error on 9, and ends "Optimal" at an
objective off the reference on 5 (`dpklo1` at 0.7125 against 0.3701,
`qbore3d` at 3102.14 against 3100.20), where JAOS and Clp agree with the
reference.

2026-09-23, tree f7b65c9, `results/conic-cblib.txt`:

| set | JAOS | SCIP | JAOS / SCIP |
|---|---|---|---|
| CBLIB continuous, 29 instances | 27 solved, 1.54 s | 2, 18.13 s | 0.13x |

JAOS stops at the limit on `nql180` and `qssp180`. SCIP is a
mixed-integer nonlinear solver: it takes a cone as a general nonlinear
constraint, not through a conic interior point, so this reading sets
JAOS against a different kind of method, and a conic interior-point solver
would be the fair rival. SCIP reaches the limit on 25, ends on
`chainsing-1000-1` at 30.17980 against the reference 30.18016 (outside the
1e-6 rule), prints nothing on `chainsing-1000-2`, and on
`sched_100_100_scaled` reports 65.9 s under a 20 s limit. A first run left HiGHS on its own
thread count and a gap of 1e-4 and SCIP at a gap of 0;
`bench/measurements/02-284/` keeps that run as `mip-*-unequal.txt`.

## Rules

- A time without a verified answer is discarded. Every competitor's objective
  is checked within the gate's tolerance: against the Koch reference on LP,
  and against the reference in the manifest on MIP.
- On LP all four solvers run at a primal tolerance of 1e-7. The
  competitors' dual tolerance is 1e-7, and JAOS's is its default, 1e-9
  (`DUAL_TOL`). On MIP the three share the relative gap 1e-6 and one
  thread.
- Every result file names the machine in its header. The LP harness also
  marks a run under WSL, which is a development number, and a tree with
  uncommitted changes. `run-mip.sh` writes neither mark.
- The harness repeats to about 1.4% on this host, measured from JAOS's own
  cross-rung ratio (commit 54737cc, 2026-08-10; not re-taken).
