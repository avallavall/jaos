# 02-385 — an infeasible MIP carries its relaxation's certificate again

Taken on 2026-10-21 on the tree of 3414947.

## The regression

02-235's certificate sweep and 02-236's subsystem sweep were run on new
seeds (21 to 23) and on seed 1 again. Both had read clean in September.

| seed 1 | 02-235 (2026-09-14) | HEAD |
|---|---|---|
| MIPs with a certificate / without | 616 / 3 | 502 / 117 |
| P8 broken (a MIP with an infeasible relaxation and no certificate) | 0 | 114 |
| 02-236 P7 broken (a MIP's subsystem differs from its relaxation's) | 0 | 62 |

122f63c had the tree hand the root's ray to the model, once
`jaos_check_certificate` confirmed it against the model as loaded. Since
9ab84a4 (2026-10-04) the root is solved after integer columns' implied
bounds are tightened, so its ray leans on those bounds and fails that
check, and no certificate was published. A MIP proved infeasible before
any node, by the parity test or by an emptied integer box, never reached
that code at all (seed 21, model 1113, now
`tests/data/mip_parity_root.mps`).

## The change

`root_certificate` in `src/mip.c`. When the tree ends infeasible, the
relaxation as loaded (the model with its integer marks taken off, nothing
else changed) is solved once, and its ray is published when it is
infeasible and the ray certifies. It runs when the root's relaxation is
infeasible, and at the end when the tree never solved a root; a feasible
root relaxation means the relaxation as loaded is feasible too, so it does
not run then. The root's own ray is the fallback. Solving the relaxation
as loaded, objective kept, gives the same ray as solving the model without
marks, which is what 02-236's P7 asks.

## The reading

- 02-235's `certs.sh`, six seeds of 2000 (`certs.txt`): every property
  holds on every seed. The MIPs with a certificate and without read 616 /
  3, 614 / 2, 620 / 4, 603 / 3, 591 / 4 and 640 / 5, 02-235's table but
  for one more MIP without on seed 6, infeasible by integrality alone.
  Seeds 21 and 22 read P8 = 0 as well.
- 02-236's `iissub.sh`, six seeds of 1000 (`iissub.txt`): every property
  holds on every seed, P7 included.
- `make netlib netlib-infeas J=12`, `make netlib-kennington J=2` and
  `make miplib J=2`: 0 regressed, 0 improved, and every results file is
  byte-identical, since no gate instance is infeasible at its root.

`make test` and `make sanitize` pass.
