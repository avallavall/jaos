# 02-512 — a dive's solve prices by dual Devex and stops at the root LP's work

Taken on 2026-11-25 on the tree of 02-511, for J36's root dive: in
02-501's split the root's dive takes 13.8% of the work on the 94 that
never leave the root and leads on most of the 17 where the heuristics
lead.

## The cause

The dive fixes the integer column nearest an integer and solves the copy
again, up to 50 times. With no incumbent it stops at the root heuristics'
share (`MIP_HEUR_ROOT`, `MIP_HEUR_FLOOR`); with one, outside network mode,
nothing stops it but the solve's own limit, and RINS's dive, the same
code over the columns the incumbent and the relaxation agree on, has no
stop either. 02-510's split (`split510.py`) on three instances that hold
an incumbent from the jump or lock rounding (`three-read.txt`, 300 s):

| instance | the dive, s | what it found | nodes |
|---|---|---|---|
| `neos-662469` | 109.4 | nothing | 46 |
| `var-smallemery-m6j6` | 290.0 | nothing | 1 |
| `supportcase7` | 271.7 | nothing | 1 |

On `var-smallemery-m6j6` (`dz.py`'s lines) the root LP takes 2,165
iterations and 0.8 s. The dive's first five solves take 0 to 1,995
iterations, two of them from the slack basis where the copy's presolve
dropped rows; the sixth runs 61,361 iterations, 19,000 of them without a
better infeasibility before the plateau's perturbation, with 57,515
weight restarts, and the time limit stops it. The dive prices by dual
steepest edge, and in a solve from a basis a drifted weight sets the
whole set back to 1, as 02-508 found in the cut re-solves. 401 columns
are fractional at the root and each solve fixes one, so 50 solves could
not reach an integer point anyway.

## The arms

- `dv.py`: the dive's solves price by dual Devex (`devex`), and also
  perturb on 02-510's degenerate run (`both`);
- `dz.py`, switched by environment: Devex (`DZ_DEVEX`), each solve fixing
  ceil(fractional / solves left) columns nearest an integer (`DZ_BATCH`),
  and each solve stopped at a multiple of the root LP's work (`DZ_CAP`);
- `dcap.py`: the cap passed down from the root's own LP work to both of
  the dive's callers, with `DCAP_DEVEX` and `DCAP_BATCH`.

The three at 300 s (`three-read.txt`):

| arm | `neos-662469` | `var-smallemery-m6j6` | `supportcase7` |
|---|---|---|---|
| base | dive 109 s, 46 nodes, 264462 | dive 290 s, 1 node | dive 272 s, 1 node |
| Devex | dive 69 s, 35 nodes | dive 289 s, 1 node | dive 266 s, 1 node |
| Devex, batch | dive 295 s, 1 node | dive 85 s, 7 nodes, -143.06 | dive 257 s, 1 node |
| Devex, batch, cap 1 | dive 1.1 s, 373 nodes, 184390.5 | dive 1.7 s, 185 nodes, -146.16 | dive 11.9 s, RINS's dive 244 s, 1 node |

The cap there held only the root's dive. MIPLIB 3 with the cap on both
callers (`miplib3-arms.txt`, against the tree before):

| arm | moved | work, geometric | total | worst |
|---|---|---|---|---|
| cap | 14 | 0.951x | 0.977x | `p0033` 1.13x, `lseu` 1.08x |
| cap, Devex | 21 | 0.939x | 0.965x | `p0282` 1.17x, `lseu` 1.11x |
| cap, Devex, batch | 21 | 0.942x | 0.983x | `p0282` 1.17x, `lseu` 1.11x |

Every arm passes the gate. Batch fixing adds nothing once the cap ends the
long dives (`dive-batch-fixing` in `bench/refusals.txt`).

The cap and Devex arm was the first built (`dive2.py`) and paired at 300 s
on the 28 below. On 02-491's 20 solved it took `irp` from optimal in 116 s
to the time limit: alone it closes in 5.16e10 work units and 14,977 nodes
against 2.76e10 and 4,555 (`irp-read.txt`). The two trees find the same
incumbents to node 312; the Devex dives then send the tree another way,
and it finds the optimum at node 678 against 348. With the cap alone
`irp` reads its 4,555 nodes and 2.76e10 again, and keeping the cap to the
root's own calls does not move it. Devex in the dive is left out
(`dive-devex`).

## The change

`src/mip.c`: each solve of the dive's copy stops at `MIP_DIVE_SOLVE_CAP`
(1) times the work the root's relaxation took; a solve that stops there
ends the dive. Both callers, the root's dive and RINS's, pass the cap.

Five tests pinned values of the old path and are pinned again on the new
one, each still reading what it reads: ZI rounding's first point on its
knapsack (now -1544 at node 4, a RINS sub-MIP taking -1518 at node 1), the
market-split seeds of the crossover (4) and DINS (3) tests, and the
lattice tree's restart, now after 1,985 nodes against 1,149 (2,005 nodes
in all against 1,169, still under the 25,766 the columns alone take), its
node limit moved with it to 2,001. `tests/cli.sh` reads the same two
lines.

## At 300 s

`pair.sh` runs 02-501's `pair.sh` with 02-511's tree against the change,
both with the round lines, six legs at a time: on the 28 where 02-501's
heuristics or strong branching lead (`heur.txt`) and the five where
02-510's pair still left the heuristics leading (`heur2.txt`), their two
failed pairs again two at a time, and on 02-491's 20 solved
(`pair-record.txt`, `pairsum.txt`, `pairsum-solved.txt`). Load 2 to 11.

| | before | after |
|---|---|---|
| out of the root, of the 33 | 19 | 27 |
| hold an incumbent | 26 | 26 |

Out of the root: `var-smallemery-m6j6` (328 nodes, incumbent -101.16 to
-143.56, bound -155.98 to -154.33), `neos-1445765` (15, -4397 to -7900),
`supportcase7` (7), `roi5alpha10n8` (5), `neos-3555904-turama` (4),
`netdiversion` (3, 3809587 to 1601913), `neos-933966` (2, 48296 to 32304)
and `satellites2-40` (2). `cvs16r128-89` reaches 399 nodes against 4 and
-39 against -7, `neos-662469` 489 against 32 and 184390 against 264462,
and `net12`'s bound 37 to 40. `CMS750_4` holds 549 against 538. Of the 20
every status is the same but `neos-3083819-nubu`'s, optimal in 256 s
against the limit; `swath1` passes its 2.2 GB cap in both legs.

## The gates

`make test` and `make sanitize` pass. `netlib`, `netlib-infeas` and
`netlib-kennington` read 0 regressed, 0 improved, 0 new, 0 at other nodes,
and their records and those of `primal barrier pdlp concurrent warm` are
byte-identical. `make miplib` reads `gate: PASS` and 11 at other nodes, as
`miplib3-arms.txt`'s cap arm reads; `bench/miplib.baseline` is rewritten
from it. `make miplib2017` (`miplib2017-read.txt`): solved 10 and 29
incumbents on both sides, sum 9.3394 to 9.3260, inside 02-474's spread:
`beasleyC3` 759 to 756, `markshare2` 84 to 67, `timtab1` 1007485 to
1003618 and `csched007`'s bound 297 to 303; `mad` 0.118 to 0.129 and
`neos-3627168-kasai` 990498 to 992248.
