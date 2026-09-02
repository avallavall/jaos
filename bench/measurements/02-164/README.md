# 02-164 — the Farkas ray behind INFEASIBLE, published and re-judged

D254. `jaos_certificate` hands out the refused row's signed, unscaled
B^-T e_r; `jaos_check_certificate` judges it from the model alone in the
original space. The population arm below is the claim: all 29 reference
infeasibles, under the reference build where the simplex proves every
one, publish a ray the checker certifies — and two feasible controls
refuse a certificate, which is what proves the driver can tell the arms
apart.

## What is here

| file | what it does |
|---|---|
| `cert-population.c` | the driver: solve, expect INFEASIBLE, fetch the ray, check it; `feasible:` prefixes the control arm |
| `run-cert-population.sh` | builds the reference library and driver in a temp dir outside the repo, runs the 29 plus 2 controls |
| `cert-population.txt` | the reading: 29 certified, 2 controls, 0 bad |

## What the first firing population taught

13 of 29 died on `gap=-inf`: roundoff read as a load-bearing infinity.
Three repairs, each measured by rerunning the population:

1. Rows whose own slack is basic carry a rho entry that is exactly zero
   by B^-1 B = I; the computed entry is roundoff and is zeroed at
   publication (13 → 2 bad).
2. Entries below PIVOT_MIN are ones the ratio test itself refuses to
   read, so no capacity of those rows entered the walk's argument;
   zeroed at publication (2 → 0 bad; the per-row diagnostics that
   located this family are in the driver).
3. The checker gives each (A'y)_j the traffic-scaled floor
   `jaos_check_solution` already documents, because it recomputes sums
   whose structural zeros come back as noise.

None of the three can manufacture a proof: the checker re-judges the
published ray from the model's own bounds, which is also what makes a
ray leaning on a lent artificial bound refusable.
