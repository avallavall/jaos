# The comparison harness

Times JAOS against HiGHS, SoPlex and Clp on the Netlib standard set,
against HiGHS and SCIP on MIPLIB 3 and the MIPLIB 2017 reading, against
HiGHS and Clp on the Maros-Meszaros QPs, and against SCIP on the
continuous CBLIB instances. Nothing here is part of what JAOS ships. Its
record is `results/`, and it carries seconds, which `bench/results/` never
does.

## Run it

```
make compare-solvers
make compare COMPARE_ARGS='-t P0'
```

`make compare-solvers` runs `fetch-solvers.sh`, whose `build_*` functions
fetch and build each competitor, pinned by checksum in the `*.manifest`
files. Run it once first: `make compare` builds HiGHS alone, so without it
SoPlex and Clp are not timed. Rung `P0` is every solver's own presolve on,
the dual simplex forced, no crash basis, one thread. Do not run bare
`make compare`: it defaults to rung T0, which turns the other solvers'
presolve off.

The MIP comparison runs one set at a time:

```
SCIP_PYTHON=/path/to/python bash bench/compare/run-mip.sh \
    -m bench/miplib2017.manifest -d bench/instances-miplib2017 -t 20
```

`SCIP_PYTHON` names a Python that has `pyscipopt`; without it SCIP is left
out. `-x EXT` reads `NAME.EXT` instead of `NAME.mps`. For anything but
`mps` or `mps.gz` HiGHS is left out, since it reads neither CBF nor the
other formats.

The QP comparison is `bash bench/compare/run-qp.sh`, which reads
`bench/maros-meszaros.manifest` by default and takes the same `-m`, `-d`
and `-t`. The conic comparison is `run-mip.sh -x cbf.gz` over
`bench/cblib.manifest`.

## The rungs

JAOS runs with its defaults on every rung: the dual simplex, presolve on.
The rungs set the other solvers.

| tier | the others |
|---|---|
| T0 | dual forced, presolve off |
| **P0** | dual forced, presolve on. The rung JAOS is judged on |
| T1 | free to pick primal or dual |
| T2 | presolve on |
| T3 | stock defaults |

Each of T1 to T3 changes one setting of the others against T0. T0 to T3
were defined when JAOS had no presolve, and JAOS now runs with presolve
on. Use P0.

## The LP reading

`results/P0.txt`, 2026-09-24 on tree 6c79039, geometric mean of
per-instance ratios over the instances above a 0.05 s floor:

| | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 1.91x | 0.57x | 1.54x |
| iterations | 1.15x | 0.48x | 1.04x |
| time per iteration | 1.66x | 1.17x | 1.49x |
| JAOS faster on | 0 of 16 | 14 of 17 | 4 of 14 |
| worst instance | `stocfor3` 11.2x | `truss` 1.8x | `stocfor3` 10.0x |

`summarise.py` recomputes the same figures from the record. SoPlex's and
Clp's objectives on `pilot87` miss the reference, so that instance counts
against HiGHS only.

## The MIP reading

`run-mip.sh` solves each instance with JAOS, HiGHS 1.15.1 and SCIP 10.0
(through `pyscipopt` 6.2.1), 20 s each and one thread. All three stop at
the relative gap JAOS stops at, 1e-6 (`MIP_GAP`): `highs-mip.opt` and
`scip_solve.py` set it for the other two. An instance counts as solved
when the solver ends optimal, or at its gap limit, with the objective at
the reference within `1e-6 * max(|ref|, 1)`. `summarise_mip.py` prints the
solved counts, the shifted geometric mean of the seconds (a shift of 1 s,
an unsolved instance counted at the limit), and the ratio of the shifted
means.

`results/mip-miplib.txt` and `results/mip-miplib2017.txt`, 2026-09-24 on
tree 6c79039, on an otherwise idle machine:

| set | JAOS | HiGHS | SCIP | JAOS / HiGHS | JAOS / SCIP |
|---|---|---|---|---|---|
| MIPLIB 3, 24 instances | 23 solved, 0.93 s | 24, 0.64 s | 24, 0.57 s | 1.18x | 1.23x |
| MIPLIB 2017, 30 instances | 0, 20.00 s | 8, 13.60 s | 8, 12.29 s | 1.44x | 1.58x |

## The QP and conic readings

`run-qp.sh` solves the 138 Maros-Meszaros QPs with JAOS (its barrier),
HiGHS 1.15.1 (its QP solver, `highs-qp.opt`: one thread, tolerances 1e-7)
and Clp 1.17.11 (`-barrier`), 20 s each. The conic reading runs JAOS and
SCIP over the 29 continuous CBLIB instances. SCIP has no CBF reader in
this build, so `scip_cbf.py` reads the file and hands SCIP each cone as a
quadratic constraint. Both are summarised by `summarise_mip.py` with the
MIP reading's rule.

`results/qp-maros-meszaros.txt` and `results/conic-cblib.txt`, 2026-09-23
on tree f7b65c9, on an otherwise idle machine:

| set | JAOS | HiGHS | Clp | JAOS / HiGHS | JAOS / Clp |
|---|---|---|---|---|---|
| Maros-Meszaros, 138 QPs | 133 solved, 0.23 s | 96, 2.04 s | 119, 0.69 s | 0.40x | 0.72x |

| set | JAOS | SCIP | JAOS / SCIP |
|---|---|---|---|
| CBLIB continuous, 29 instances | 27 solved, 1.54 s | 2, 18.13 s | 0.13x |

JAOS refuses `values`, whose Q is not positive semi-definite. SCIP is a
mixed-integer nonlinear solver and takes a cone as a general nonlinear
constraint, so the conic reading sets JAOS against a different kind of
method. A conic interior-point solver would be the fair rival.

## Rules

- A time without a verified answer is discarded. Every competitor's
  objective is checked within the gate's tolerance: against the Koch
  reference on LP, and against the reference in the manifest on MIP.
- On LP all four solvers run at a primal tolerance of 1e-7. The
  competitors' dual tolerance is 1e-7, and JAOS's is its default, 1e-9
  (`DUAL_TOL`). On MIP the three share the relative gap 1e-6 and one
  thread.
- Every result file names the machine in its header. The LP harness also
  marks a run under WSL, which is a development number, and a tree with
  uncommitted changes. `run-mip.sh` writes neither mark.
- The harness repeats to about 1.4% on the development host (commit
  54737cc).
