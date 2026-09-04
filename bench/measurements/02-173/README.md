# 02-173 — what exact evaluation says that the checker's arithmetic does not

D267. `jm_exact_evaluate` (`src/exact.c`) over every gate instance's
published point, beside `jaos_check_solution` on the same point. The
checker sums in `long double`, uncompensated (D268); this does not round
at all. The question is whether the difference reaches anything.

It is asked because D262 found one case by accident: the checker's
objective was 790 ulps out on `finnis`, because `long double` holds 64 bits
and a binary64 product needs 106. One case found that way is a reason to
ask the population.

## What is here

| file | what it does |
|---|---|
| `exact-cover.c` / `run-exact-cover.sh` | one line per instance: rows, products formed, the distance in ulps between the checker's objective and the exact one, both worst-row violations, and the seconds the exact walk took. Writes `exact-cover.txt` |

## The reading

**110 of the 139 instances have an optimum to evaluate**, the other 29
being the infeasible set. Of those 110:

- **0 refused for limbs.** Not one instance exhausted `JM_EXACT_LIMBS`, so
  128 limbs covers this whole population and the capacity has a measurement
  behind it now rather than an argument.
- **0 objectives differ from the checker's.** Not one ulp, anywhere. The
  plain `long double` sum in `src/check.c` is holding the objective on every
  gate instance, and `finnis` no longer differs because D261 removed the lent
  bounds that were carrying 3.2e12 of traffic into it.
- **75 of 110 worst-row violations differ**, of which 33 differ in the
  first three significant digits. The exact figure is the larger one on 24
  of those 33 and the smaller on 9. The largest ratio is **7.12x on
  `cre-c`** (1.6e-14 against 1.14e-13) and the largest absolute gap is
  **9.99e-12 on `fffff800`**.

**No verdict moves.** Every difference is at 1e-11 or below and the
checker's bar is 1e-7, so the gate reads the same either way. That is the
result, and it is why nothing is exposed: see `bench/refusals.txt`.

## The cost, which was the other half of the question

Development numbers, not for any baseline. `ken-13` is 28632 rows and
121425 products and takes **0.02 s**; the largest, `pds-20`, takes 0.01 s.
Nothing on the set reaches a tenth of a second.

The reason it is that cheap is the representation. A dyadic rational is
`m * 2^e` and adding two of them is a shift and an addition, where a
general rational would need a gcd and two divisions per term. The product
count is also far below the nonzero count — `cre-a` forms 2515 products
against 14987 nonzeros — because a term with a zero coefficient or a zero
variable is skipped, and at an optimum most variables are zero. Skipping a
zero product loses nothing.

## What this does NOT say

It does not say the checker is right, only that it agrees here. Both
figures come from the same matrix and the same published point; a model
built wrongly by the loader would be evaluated wrongly by both.

It does not say the row-violation gap is harmless in general. It says no
instance on this population sits near enough to the tolerance for a 7x
difference in a 1e-13 quantity to reach it. `bench/refusals.txt` carries
the condition that would reopen that.
