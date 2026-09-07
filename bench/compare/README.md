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

`results/P0.txt`, 2026-08-30, geometric mean of per-instance ratios over the
instances above a 0.05 s floor:

| | vs HiGHS 1.15.1 | vs SoPlex 8.0.3 | vs Clp 1.17.11 |
|---|---|---|---|
| time per solve | 3.60x | 1.12x | 2.96x |
| iterations | 1.78x | 0.73x | 1.56x |
| time per iteration | 2.02x | 1.52x | 1.90x |
| JAOS faster on | 1 of 17 | 10 of 21 | 1 of 14 |
| worst instance | `stocfor3` 27.4x | `grow22` 14.8x | `stocfor3` 22.8x |

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
