# 02-467 — proximity search, refused

Taken on 2026-11-16 on c64b8fe with `proximity.patch`, for TODO row J32.
`read.sh` applies the patch to a copy of HEAD and takes the readings one
solve at a time: `miplib3/proximity.txt` and `full/proximity.txt`.
`full/before.txt` is 02-466's record.

## What was built

Proximity search, after Fischetti and Monaci (2014). With an incumbent, a
sub-MIP keeps every row and adds one: the original objective at most the
incumbent's less δ. Its own objective is the Hamming distance to the
incumbent over the free binaries.

δ is the objective's step when the costs lie on a grid, and otherwise
1e-3 of (1 + |incumbent|) (`MIP_PROX_DELTA`).

It runs once per new incumbent, so each point it finds sets off the next
one. It has its own share of the tree's work (`MIP_PROX_SHARE`, 0.1),
apart from RINS. Each call is capped like any sub-MIP:
`MIP_SUBMIP_WORK` times the model's size, and `MIP_SUBMIP_NODES` nodes.
`J32_PROX=1` turns it on.

A first form ran as a third pass inside the RINS sub-MIP's 10% share, and
found nothing. RINS left it 6,000 to 500,000 work units a call, short of
its sub-MIP's root LP.

## The readings

MIPLIB 3, against `bench/miplib.baseline`: 0.977x, no instance past 2x.
`blend2` reads 0.78x and `mod008` 0.89x.

MIPLIB 2017, 02-328's `m17sum.py`:

| | primal | dual | sum |
|---|---|---|---|
| before (02-466) | 6.8396 | 6.5210 | 13.3607 |
| proximity search | 7.8883 | 6.5617 | 14.4500 |

It finds points where its sub-MIP is cheap. On `glass4` the incumbent
falls from 4.70e9 through 4.20e9 to 4.0e9 in four calls, against an
optimum of 1.2e9, though the gap read at 1e10 stays 1.0.

The incumbent improves on:
- `neos-3046615-murg`, 0.047 to 0.019 of the optimum;
- `mas74`, 0.081 to 0.032;
- `neos-911970` and `tr12-30`;
- `binkar10_1`, which gets its optimum back.

It worsens on:
- `markshare_4_0`, which loses its optimal incumbent, 0 to 1.0;
- `neos-3754480-nidda`, 0.056 to 0.128;
- `beasleyC3`, 0.028 to 0.058;
- `mad`, 0.297 to 0.316.

The dual side loses 0.04, the most of it on `graphdraw-domain` and
`pk1`, whose trees get less of the work. On `mad` each call's sub-MIP
spends 10 to 48 million work units and stops at its root, finding no
point. Without `markshare_4_0`, the sum still reads about 13.45, worse
than 13.36.

## The verdict

Refused: `bench/refusals.txt`'s `proximity-search` line says what
reopens it.
