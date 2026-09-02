# 02-165 — the certificate crosses back through presolve

D256. Under the default build presolve reduces or proves most infeasible
models, and until this decision none of those published a ray. Now a
reduced solve's ray is lifted through the arena into the caller's rows
or columns, and each presolve proof site seeds one signed unit on the
row or column it refused, lifted the same way. The population arm below
is the claim, and it is 02-164's arm re-run under the default build: the
29 reference infeasibles publish a ray the checker certifies, and the
two feasible controls refuse one.

## What is here

| file | what it does |
|---|---|
| `cert-population.c` | 02-164's driver with the build flag dropped, a prover column (zero iterations is presolve alone), the relative margin `rel = gap / (1 + |sup| + |inf|)`, and a thin arm |
| `run-cert-population.sh` | builds the DEFAULT library and driver in a temp dir outside the repo, runs the 29 plus 2 controls |
| `cert-population.txt` | the reading: 28 certified at 1e-7, 1 thin, 0 refused, 2 controls |

## The reading

| | |
|---|---|
| certified at the population's 1e-7 bar | 28 of 29 |
| of those, proved by presolve alone and seeded at the site | 9 |
| thin: a proof at 1e-9, not at 1e-7 | 1, `gran`, `rel = 4.07e-08` |
| refused, or a control that handed out a ray | 0 |

The thin one is what a site-seeded ray is. Presolve refuses a row at
eight ulps of its own traffic (`itol` in the activity pass), and the lift
carries exactly that margin into the caller's space: `gran`'s row is
refused by 8e-06 against two halves of 97.7. The simplex's ray for the
same model under the reference build combines rows and reads
`rel = 0.266` (02-164). A caller who wants a fatter proof of a
presolve-proved verdict can solve the reference build; the thin one is
a proof at the tolerance it names, not a wrong one.

Two readings moved between the builds for a reason the record should
carry: `bgdbg1` reads `gap=10` here against `35.5` in 02-164, and
`ceria3d` `0.25` against `0.0625`, because presolve's site proves each
with one row and the simplex with several. Both certify at 1e-7.
