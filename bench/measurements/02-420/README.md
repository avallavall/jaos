# 02-420 — exact solving of generated decimal LPs

Taken on 2026-10-22 on the tree of af76198, with the change. `read.sh`
takes the reading (`decimals.txt`).

## The harness

`decimals.c` takes 02-235's LPs, as 02-411's `scaled.c` does, and writes
them in decimals: each cost and matrix entry has one or two places, each
column bound one and each row side two, and the planted point still holds
every row exactly in decimal. Each model is solved in floating point,
exactly on its doubles, and exactly on its decimals. The decimal exact
solve must end optimal, infeasible or unbounded (P1), with the
floating-point status (P2), with an exact objective within 1e-9 of the
floating-point one (P3), and with a proof file that `jaos_check_proof`
certifies on a fresh copy of the model (P4). A failing model is written
out as MPS.

## Three defects

The first reading, on seeds 1 to 3 at 1000 models each, failed P1 on 686
models and P4 on 8. All three causes were older than the decimal reading:
the binary exact solve has them too.

- **A presolve verdict leaves no basis.** When presolve decides that a
  model is infeasible, there is no basis for the exact certificate to come
  from, and the exact solve ended `numerical_error`. Every P1 failure the
  harness printed had this cause, and the change clears all 686.
- **A postsolved basis can give multipliers that do not certify.** On a
  model that presolve reduces and the simplex then finds infeasible, the
  exact certificate derived from the postsolved basis did not hold, though
  the exact solve reported infeasibility proved. Without presolve the same
  model certifies.
- **The exact ray lost its column scale.** `jaos_exact_unbounded_ray`
  solved the scaled basis and did not take back the power of two each
  column was scaled by. On one model the derived ray moved column C13 by
  2 where the floating-point ray, which holds, moves it by 1.

In exact mode, an infeasible or unbounded answer from a model that
presolve decided, reduced or aggregated is now solved again with presolve
and the aggregator off. The exact ray takes back its column scale. And
the exact solve judges each certificate and ray it derives with the proof
file's exact test before it reports a proof; one that does not hold ends
`numerical_error` with that reason.

## The reading

With the change, seeds 1 to 3: every P1 to P4 holds on all 3000 models
(1590 optimal, 1344 infeasible, 66 unbounded).

The exact solve on the doubles ends with another status on 32 of the
3000. On 27 the doubles' data are infeasible over the rationals, as
02-358 found for `sierra`. On 5 the certificate or ray derived from the
basis does not hold over the doubles' data, and the solve now ends
`numerical_error` there instead of reporting a proof. The decimal reading
proves all 32.
