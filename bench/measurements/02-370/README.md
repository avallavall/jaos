# 02-370 — a shorter trial before the nodes skip presolve, refused

Taken on 2026-10-08 on the tree of 98c60b6, for TODO row J7.

## The idea

A tree counts the simplex iterations of its first
`MIP_NODE_PRESOLVE_TRIAL` (50) node relaxations, and when they average
more than `MIP_NODE_PRESOLVE_ITERS` (20) the later nodes skip the LP
presolve, which breaks their warm start (02-352). On `csched008` the first
50 nodes took 747.5 iterations each, and the tree reached only 57 nodes in
1e10 work units. A shorter trial would stop presolving such nodes sooner.
`trial.diff` puts the trial length behind `JAOS_NPT`.

## The reading

`arms.sh` (MIPLIB 3 at J=2, the 2017 set at 1e10 work units). The base
reads MIPLIB 3 all solved and the 2017 gap sum 13.48.

| arm | trial | MIPLIB 3 geometric | sum | worst | 2017 gap sum |
|---|---|---|---|---|---|
| `npt10` | 10 | 1.042x | 0.987x | `enigma` 4.26x | 14.05 |
| `npt25` | 25 | 1.134x | 1.049x | `enigma` 18.58x | 13.20 |

Both move `enigma`'s tree past the gate's 2x, and the shorter trial also
raises the 2017 gap sum. Refused as `node-presolve-trial`.
