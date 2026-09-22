# The comparison harness

Times JAOS against HiGHS, SoPlex and Clp on the Netlib standard set, and
against HiGHS and SCIP on MIPLIB 3 and the MIPLIB 2017 reading. Nothing
here is part of what JAOS ships. Its record is `results/`, and it carries
seconds, which `bench/results/` never does.

## Run it

```
make compare COMPARE_ARGS='-t P0'
```

`build_*` fetches and builds each competitor, pinned by checksum in the
`*.manifest` files. Rung `P0` is every solver's own presolve on, the dual
simplex forced, no crash basis, one thread. Never run bare `make compare`: it
defaults to rung T0, which was defined when JAOS had no presolve.

The MIP comparison runs one set at a time:

```
SCIP_PYTHON=/path/to/python bash bench/compare/run-mip.sh \
    -m bench/miplib2017.manifest -d bench/instances-miplib2017 -t 20
```

`SCIP_PYTHON` names a Python that has `pyscipopt`; without it SCIP is left
out.

## The rungs

| tier | JAOS | the others |
|---|---|---|
| T0 | dual simplex, no presolve | dual forced, presolve off. Historical |
| **P0** | dual simplex, presolve | dual forced, presolve on. The rung JAOS is judged on |
| T1 | unchanged | free to pick primal or dual |
| T2 | unchanged | presolve on |
| T3 | unchanged | stock defaults |

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

The reading before, `results/P0-2026-09-21.txt` on tree 6ae3966, read 3.46x,
1.01x and 2.76x per solve. The aggregator (2e04b47) did most of the
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

2026-09-21, tree 3086162, `results/mip-miplib.txt` and
`results/mip-miplib2017.txt`:

| set | JAOS | HiGHS | SCIP | JAOS / HiGHS | JAOS / SCIP |
|---|---|---|---|---|---|
| MIPLIB 3, 24 instances | 23 solved, 1.43 s | 24, 0.70 s | 24, 0.64 s | 1.43x | 1.48x |
| MIPLIB 2017, 30 instances | 0, 20.00 s | 8, 14.38 s | 7, 12.87 s | 1.37x | 1.51x |

JAOS leaves l152lav at the limit on MIPLIB 3. On the 2017 set it finishes
none of the eight the rivals finish. A first run left HiGHS on its own
thread count and a gap of 1e-4 and SCIP at a gap of 0;
`bench/measurements/02-284/` keeps that run as `mip-*-unequal.txt`.

## Rules

- A time without a verified answer is discarded. Every competitor's objective
  is checked against the Koch reference within the gate's tolerance.
- Tolerances are equalised: JAOS runs at the stricter of HiGHS's 1e-7 and
  SoPlex's 1e-6. On MIP the three share the relative gap 1e-6 and one
  thread.
- Every result line names the machine. A number taken under WSL is a
  development number.
- The harness repeats to about 1.4% on this host, measured from JAOS's own
  cross-rung ratio.
