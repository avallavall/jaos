# 02-504 — settling rounds that pass their walk's work

Taken on 2026-11-19 on ccef8dba, for TODO J35's last instance. On
`ns1830653` at 300 s, the warm re-solve after the root's cut round 9 spent
the rest of the solve in its settling rounds (02-497).

## The re-solve alone

The LP is 02-497's dump of that re-solve, 4839 rows, read with its starting
basis: `jaos solve ns.pre10.mps --basis ns.pre10.bas --threads 1`.
`setdbg.py` prints each settling round's iterations and work:

| piece | iterations | work units |
|---|---|---|
| the warm walk | 1,635 | 3.8e9 |
| 32 warm rounds, none settling | to 44,665 | 5.4e10 |
| the cold walk after the restart | 7,167 | 1.2e10 |
| 32 cold rounds, none settling | to 46,867 | 5.0e10 |
| the primal from the settled point | 659 | 3e9 |

Every round flips boxed columns of zero cost and leaves a breach of the
same size, so 64 rounds pay ten times the two walks for nothing; the primal
finishes.

## Two ways out

`variants.py` builds both behind switches (`JAOS_SR_MIN`,
`JAOS_SR_NOPRIMAL`), and `miplib3.sh` reads MIPLIB 3 for each against the
unchanged code (`miplib3-read.txt`):

- **A warm point both tests refuse goes on in the primal**, as a cold one
  does. It moves no MIPLIB 3 instance. On `ns1830653` it is the wrong road:
  that primal settles under `SETTLE_ROUNDS_PRIMAL`, 256 rounds with no
  bound, and with 16 dual rounds before it it had run 17 minutes without
  ending when it was stopped. With 8 it ended its 608 iterations on a
  breach, and the solve restarted cold as before. Not landed
  (`settle-warm-primal` in `bench/refusals.txt`).
- **A dual run's rounds stop once their work passes the walk's**, after
  a number of rounds that always run:

  | rounds before the bound | MIPLIB 3 instances moved | geometric |
  |---|---|---|
  | 1 | 10, from `misc07` 0.007x to `stein27` 85x, `khb05250` 33x | |
  | 4 | 13, `l152lav` 0.86x to `blend2` 1.50x | 1.005x |
  | 8 | 3, `misc03` 1.06x | 1.002x |
  | 16 | 1, `stein45` 1.00x | 1.000x |

  A node solve's walk is short, so from the first round the bound cuts
  settles that used to finish a few rounds later. Eight landed
  (`SETTLE_ROUNDS_FREE`).

With it the re-solve alone ends in 17,007 iterations, 4.12e10 work units and
31.7 s, against 47,526, 1.23e11 and 99.3 s: eight warm rounds, the cold
restart, eight cold rounds, then the primal's 562 iterations to 11161.5139.

## The gates

On the change over ccef8dba:

- `make test` and `make sanitize` pass.
- `netlib`, `netlib-infeas` and `netlib-kennington`: 0 regressed, 0
  improved, 0 new, 0 at other nodes.
- `primal` and `concurrent`: their records are unchanged.
- `pdlp`: 27 agree, up from 25. `d2q06c` and `dfl001` finish from
  overrunning. The geometric mean is 5.80, down from 5.90; `nesm` costs
  1.18x and `pilot87` 1.04x.
- `barrier`: `pilot-we` costs 0.88x, and the geometric mean is 3.277, down
  from 3.283.
- `warm`: `bnl2`'s warm re-solve now restarts cold after its rounds, at
  2.80e7 work units against 1.73e7. The geometric mean is 0.0527, up from
  0.0525.
- `make miplib`: 0 regressed, and 2 instances end at other nodes.
  `dcmulti` takes 415 nodes against 241, 1.29x the work, and `misc03` 0.97x.
- `make miplib2017` at 1e10: 11 solved and 29 incumbents, as before.
  `m17sum.py`'s sum moves from 9.2468 to 9.2647. `binkar10_1` solves in
  0.85x the work, `neos5` in 1.00x. On the rest, as many incumbents
  improve as worsen: `timtab1` 1036198 from 1068960 and `beasleyC3` 762
  from 756.

## `ns1830653` at 300 s

02-491's `run.sh -n` on it, one thread, 300 s, under a load of about 5
(`ns1830653-300s.txt` is the same run with `--log summary`): `time_limit`
at node 1 with no incumbent, as before, and the root's bound at 11162,
from 11130. The root now gets through 11 solves, its LP and ten cut
rounds' re-solves, and they take 99.9% of its work, 1.8e11 units. What
is left is the cost of the cut rounds' re-solves, J36's second share.
