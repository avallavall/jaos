# The comparison harness

Times JAOS against HiGHS, SoPlex and Clp on the Netlib standard set. Nothing
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

## The rungs

| tier | JAOS | the others |
|---|---|---|
| T0 | dual simplex, no presolve | dual forced, presolve off. Historical |
| **P0** | dual simplex, presolve | dual forced, presolve on. The rung JAOS is judged on |
| T1 | unchanged | free to pick primal or dual |
| T2 | unchanged | presolve on |
| T3 | unchanged | stock defaults |

## The reading

`results/P0.txt`, 2026-09-21 on tree 6ae3966, geometric mean of
per-instance ratios over the instances above a 0.05 s floor:

| | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 3.46x | 1.01x | 2.76x |
| iterations | 1.63x | 0.63x | 1.37x |
| time per iteration | 2.12x | 1.60x | 2.02x |
| JAOS faster on | 1 of 18 | 13 of 22 | 1 of 16 |
| worst instance | `stocfor3` 33.0x | `grow22` 8.1x | `stocfor3` 23.6x |

`summarise.py` recomputes the same figures from the record to within the
last digit's rounding. SoPlex's and Clp's objectives on `pilot87` miss the
reference, so that instance counts against HiGHS only.

The three rivals disagree about the iteration count and agree about the cost
of one iteration. The iteration is what costs.

## Rules

- A time without a verified answer is discarded. Every competitor's objective
  is checked against the Koch reference within the gate's tolerance.
- Tolerances are equalised: JAOS runs at the stricter of HiGHS's 1e-7 and
  SoPlex's 1e-6.
- Every result line names the machine. A number taken under WSL is a
  development number.
- The harness repeats to about 1.4% on this host, measured from JAOS's own
  cross-rung ratio.
