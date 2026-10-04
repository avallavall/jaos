# 02-332 — MIP: MIR rounds past 6 while the root bound rises

Taken on 2026-10-04 on the tree of e15b600, for TODO row J7 ("flat
roots"). The settings are 02-328's; the arms ran from 02-329's `arms.sh`.

## The roots

`rounds.txt` holds the root bound after each round with
`--mir-rounds 20` (round:cuts:bound), on two 2017 roots with a flat start,
`binkar10_1`, and the two MIPLIB 3 trees the extra rounds cost most.

| model | default (6 rounds) | 20 MIR rounds | 20 Gomory rounds | HiGHS root |
|---|---|---|---|---|
| neos-911970 | 24.05 (7.3e8 work) | 43.33 (1.8e9) | 42.65 (1.0e10) | 52.1 |
| neos-3381206-awhea | 415.24 (8.6e8) | 427.56 (7.2e8) | 445.25 (6.1e9) | 451.8 |

`neos-911970` sits at 23.26 for five rounds and then rises every round.
`neos-3381206-awhea` sits at 415.24 for seven rounds, rises to 427.56 by
round 10 and stays there. `gt2` stops moving at round 2 and 18 more rounds
add 1 to 6 cuts each. Gomory rounds cost far more work than MIR rounds for
the same bound.

## The arms

Against `bench/results/miplib.txt` and `bench/results/miplib2017.txt` of
e15b600.

| arm | rule | MIPLIB 3 geo | sum | worst | 2017 solved | incumbents | primal | dual | gap sum |
|---|---|---|---|---|---|---|---|---|---|
| e15b600 | 6 rounds | 1 | 1 | | 3 | 25 | 8.064 | 8.738 | 16.802 |
| b5 | 20 rounds (`-O mip_mir_rounds=20`) | 1.037 | 0.998 | `gt2` 2.515 | 3 | 25 | 9.082 | 8.433 | 17.515 |
| b6 | past 6 while a round lifts the bound by 1e-4 | 1.005 | 0.999 | `p0033` 1.802 | 3 | 25 | 8.077 | 8.460 | 16.536 |

`b6` is the default: `MIP_MIR_MORE` 20 and `MIP_MIR_MORE_STALL` 1e-4,
outside network mode and only when `jaos_set_mip_mir_rounds` is not called.
On the 2017 set `neos-911970`'s bound at the limit goes from 29.76 to 45.00,
`binkar10_1` reaches the reference's point (6742.20, was 6746.64) and its
bound goes from 6713.00 to 6715.38, and `neos-3381206-awhea`'s incumbent
goes from 458 to 461. On MIPLIB 3 `p0033` runs 111 nodes where it ran 37,
`mod008` reads 0.739x and `p0282` 0.907x.

`b5` lifts the same bounds, and `awhea`'s to 428 as well, but
`markshare_4_0`'s incumbent goes from 1 to 4 and `gt2` runs 1191 nodes
where it ran 471.

## MIQP

`miqp-b6.txt` is QPLIB's 17 convex MIQPs at 1e10 work units on the shipped
code, run as 02-325's `miqp.sh` runs them. Every status, incumbent, bound
and node count is 02-328's `miqp-fin.txt`; the work moves by under 3.4e-5
of itself.
