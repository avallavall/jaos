# 02-435 — mirrored columns and negated rows

Taken on 2026-10-22 on the tree of f31f29b. `read.sh` takes the cone
reading, `readlin.sh` the linear one, and `probe.sh` sorts the cone
disagreements; `*-head.txt` are f31f29b's records and `*-new.txt` the
change's.

## The harnesses

`mirrorcone.c` (02-433's `flipcone.c`) and `mirror.c` (02-434's
`flip.c`) build each generated model twice: as it is, and with each
column mirrored and each row negated with chance 1/2. A mirrored column
x is -x: its bounds swap sides and change sign, and its cost and
coefficients change sign. A negated row a'x in [l, u] is -a'x in
[-u, -l], its quadratic part included. Both are exact in floating point
and leave the same model, so the status must match and the objective
with it. Cone heads, SOS members, semi-continuous columns and the
indicator column stay as they are. The masks come from a second random
stream, so the models are the same as 02-431's and 02-434's on the same
seed. Seeds 1 to 10, 20,000 models a seed.

## The reading on f31f29b

| harness | mode | compared | disagree |
|---|---|---|---|
| `mirrorcone.c` | mixed | 198,380 | 17 |
| `mirrorcone.c` | continuous | 200,000 | 132 |
| `mirror.c` | lp | 200,000 | 0 |
| `mirror.c` | mixed | 199,993 | 0 |
| `mirror.c` | qp | 200,000 | 0 |
| `mirror.c` | miqp | 199,809 | 2 |

The cone disagreements are 02-431's kind. `probe.sh` dumps the 16 mixed
status disagreements and the 27 continuous ones of seeds 1 and 2, and
moves every quadratic side and cone head out by 1e-4: both orders of
all 43 then end optimal at the same objective (`cone-*-coneout.txt`).
The 17th mixed one is seed 10's model 8215, one of 02-431's four
objectives that differ.

The two `miqp` ones each end `numerical_error` as they are and with an
answer mirrored. Both fail at the root, after coefficient tightening:

- Seed 2's model 4764. Tightening reads row R4's largest activity from
  implied bounds that carry the primal tolerance's widening, and turned
  the binary C5's coefficient 1 into 8.67e-7 where exact arithmetic
  gives 0. That entry forces scale factors from 2^-11 to 2^9, and the
  barrier's push on the root relaxation does not settle.
- Seed 5's model 5180 (`miqp-5180.mps`). Tightening turned a binary's
  coefficient 2 into 0.50000015 where exact arithmetic gives 0.5. The
  mirrored model has that binary at [-1, 0], which tightening does not
  read, so it is not tightened. The root relaxation
  (`qp-push-5180.mps`) leaves R4 1.5e-7 off its side at the optimum.
  The barrier converges (primal 2e-12, dual 8e-10, gap 3.9e-9 at its
  sixth iteration), its push takes R4 as active, pins the columns that
  make it so, cannot meet the row and cycles, and the conic interior
  point's fallback ends at a point the checker refuses for its duals
  (8.8e-6). With the coefficient at exactly 0.5 the relaxation solves
  at 18.5.

## The change

Coefficient tightening sums, beside a row's largest and smallest
activity, the widening its continuous columns' bounds carry,
`|a|·tol·(1 + |bound|)` each. A tightening whose new coefficient is no
larger than that sum is skipped, because it cannot be told apart from 0.
Skipping a tightening is always valid. `tests/data/miqp_tighten_dust.mps`
is model 4764; it now ends optimal at -11. Model 5180 is taken up in
02-436.

## The reading on the change

`mirror.c` gives f31f29b's lines in `lp`, `mixed` and `qp`. In `miqp`
model 4764 now agrees, and model 5180 is the one disagreement left
(`*-new.txt`). 02-434's `flip.c` gives 02-434's lines in all four modes.
`read.sh` on the change gives f31f29b's cone records byte for byte.

## The gate

`make test` and `make sanitize` pass. `make netlib netlib-infeas J=12`
(94 of 94 and 29 of 29), `make netlib-kennington J=2` (16 of 16) and
`make miplib J=2` (24 of 24) show 0 regressed against their baselines.
One line of `bench/results/miplib.txt` moves: `blend2` takes 13 fewer
work units, 727,044,590, with the same iterations, nodes, objective and
digest, so one tightening there is skipped and the search is the same.
