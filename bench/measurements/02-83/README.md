# 02-83 — the exact objective of the published point, and what `finnis` really has left

D173. No source change. An offline oracle that rounds nowhere, and what it
says about the four instances whose published point is not the optimum.

## Why the checker could not answer this

`TODO.md` carried `finnis` as "publishes a point that is not the exact optimal
vertex", 7.62e-05 from Koch's optimum after D172, and named what it needed:
exact rational arithmetic over the published `x` and `c`, because
`jaos_check_solution` cannot be the oracle. The checker multiplies in
`long double`, whose 64-bit mantissa cannot hold a binary64 product's 106
bits, so it carries the same per-term rounding D172 removed one layer out.

`exact-objective.c` adds every `c_j * x_j` into a binary fixed-point
accumulator 5632 bits wide. Each product of two doubles is a dyadic rational
of at most 106 bits, so the sum is exact and nothing rounds until the value is
printed.

## The instrument was validated before any reading — `selftest.txt`, `validate.txt`

Two ways, because a table of confident wrong numbers looks exactly like a
table of right ones.

**Against values written down** (`run-exact-objective.sh` runs this first and
refuses to take a reading if it fails): D172's model, where every double sum of
the terms gives 0 and the answer is 1; D169's model, where a naive sum gives 0
and the answer is 256; the exact 55-digit expansion of the double nearest 0.1;
and the two ends of the exponent range, `2^-1074 * 2^-1074` and
`DBL_MAX * DBL_MAX`. **The first run of it failed**, on a value this session
had transcribed with four digits wrong — which is the case the second check
exists for.

**Against an implementation sharing no code with it**
(`validate-against-fractions.py`, `run-validate.sh`): Python's `fractions` is
exact by construction, and `float.fromhex` reads the same bits. The two agree
to all 20 printed places on `finnis` (614 terms), `pilot` (3652), `pilot87`
(4883) and `afiro` (32).

## `jaos_objective` is the correctly rounded exact objective, on all 110

| | netlib | Kennington |
|---|---|---|
| published within half an ulp of exact | **94 of 94** | **16 of 16** |
| worst | **0.493 ulp**, `ship08l` | 0.476 ulp, `cre-a` |
| the checker within half an ulp of exact | 93 of 94 | 16 of 16 |
| worst | **790.3 ulps**, `finnis` | 0.476 ulp, `cre-a` |

**`refeps` means nothing on Kennington and the column is there only because
the program prints it.** That manifest's references are eight significant
digits from netlib (`ken-13` is `-1.0257395e+10`), not Koch's exact rationals,
so the whole set reads 1e6 to 1e8 and every one of those figures is the
reference's own decimal. Kennington's contribution to this record is the ulp
column above, the row residuals (worst 5.855e-12, `pds-20`) and the column
residuals (worst 2.071e-14, `cre-d`; `pds-20` reads 5.049e-28, which is
D171's own figure from an instrument that shares no code with it).

**D172's "neither number can be called more right than the other" is
settled, and the published one is right.** On `finnis` `jaos_objective` is
0.338 ulps from exact and `jaos_check_solution`'s `primal_objective` is 790.
The 2.2992e-08 D172 measured between them is the checker's error, not a
shared floor. Nothing is left to repair in the sum.

## `finnis` is REFUSED — its gap is the model's own conditioning

`finnis` carries `sum |c_j x_j| = 3.198e+12` against an objective of 1.7e+05,
which is the worst cancellation in the set. One eps of that traffic is
7.10e-04, and **no double objective of any point can be placed nearer than
half of it**.

| `finnis` | |
|---|---|
| exact `c'x + c0` | 172791.06567185125715632010… |
| gap to Koch | 7.62397e-05 |
| **the same gap in units of `eps * sum |c_j x_j|`** | **0.107** |
| worst exact row residual | 8.439e-07, rows 23 and 43 |
| those rows' traffic | 2e+10 each |
| **the residual relative to it** | **4.2e-17 and 3.0e-17**, under one eps |

The point is as feasible and as optimal as binary64 allows on this model.
`priced`, the residual weighted by each row's own multiplier, is 2.888e-05
and two rows are all of it. **There is nothing here to repair and the item is
closed.**

## What the same reading found instead — `exact-objective-netlib.txt`

`refeps` is `(exact - reference) / (eps * sum |c_j x_j|)`: the gap to Koch
measured against the floor arithmetic sets for that model. Over the 93
instances with a Koch optimum (`e226` is excluded — its 7.113 is the
objective constant `docs/format-support.md` owns, not a gap):

| `|refeps|` | instances |
|---|---|
| **<= 0.5 — as near as a double can be placed** | **68** |
| 0.5 to 10 | 20 |
| 10 to 1e4 | 1 |
| **above 1e4** | **4** |

The four, with what the checker says about each:

| | refeps | gap to Koch | `gap_positive` | `gap_certified` | row residual |
|---|---|---|---|---|---|
| **`pilot`** | **1.87e+08** | **2.31e-05** | **0.0386** | **yes** | 3.55e-13 |
| `pilot87` | 1.53e+06 | 1.04e-07 | 7.68e-04 | no | 1.51e-12 |
| `scsd6` | 9.97e+04 | 1.12e-09 | 9.73e-15 | no | 9.59e-17 |
| `etamacro` | 2.74e+04 | 1.31e-08 | 1.34e-07 | yes | 1.71e-13 |

The row residuals are at the arithmetic floor on all four, so these points
are feasible and suboptimal rather than infeasible. `pilot`'s published
objective is **higher** than Koch's on a minimization, which is the direction
a solve that stopped early moves in.

**`pilot` is the only netlib instance where every other solver in
`bench/compare` disagrees with JAOS.** From `bench/compare/results/P0.txt`:
HiGHS `-5.5748972927e+02`, SoPlex `-5.57489729e+02`, Clp `-557.4897293`, all
of them Koch's value, against JAOS's `-557.489706168`. On `pilot87` JAOS is
1.04e-07 out where HiGHS matches Koch, and SoPlex and Clp are both marked
WRONG at 301.71068.

## The library already certifies `pilot`, and nothing reads it

`jaos_check_solution` reports `gap_positive = 0.0386` on `pilot` with
`gap_certified = yes`, which is a proof that `P - P* <= 0.0386`. **Among the
53 instances where the certificate is complete, that is four orders of
magnitude above the next one** (`grow22`, 1.797e-06), and it belongs to the
one instance every other solver beats. The bound is loose by a factor of 1670
against the true 2.31e-05, and it is still the strongest signal on the set.

**The gate cannot see any of this.** `objective_accepted` in `bench/run.c` is
`|got - ref| <= 1e-6 * max(|ref|, 1)`, which on `pilot` is a window of
5.57e-04 — the 2.31e-05 passes it with 24 times to spare, and `bench/run.c`
never reads `gap_positive` at all.

`scsd6` is the case the header of `jaos.h` warns about, seen in the wild:
`gap_positive` is 9.73e-15 where the true gap is 1.12e-09, five orders
smaller, and `gap_certified` is `no`. An uncertified bound bounds nothing.

## Reproducing

```
bench/measurements/02-83/run-exact-objective.sh          # netlib, ~2 min
bench/measurements/02-83/run-exact-objective.sh bench/netlib-kennington.manifest
bench/measurements/02-83/run-exact-objective.sh -v bench/netlib.manifest \
    bench/instances/finnis.mps            # per-row residuals on stderr
bench/measurements/02-83/run-validate.sh                 # the second oracle
```

A run naming its own instances prints and writes no record, so a one-instance
check cannot leave a file that looks like a campaign.

## What this does not close

- **`pilot` and the other three.** Recorded in `TODO.md` with what each needs.
  Nothing was changed in the solver here and no campaign was run, because
  nothing in `src/` was touched.
- **Whether the gate should read `gap_positive`.** It is one instance
  separating cleanly on this set, which is not a threshold. `TODO.md` carries
  it with what it would need.
