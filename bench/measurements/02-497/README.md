# 02-497 — six MIPLIB 2017 instances that ended `numerical_error`

Taken on 2026-11-19 on 97541e88 (TODO J35). 02-491's reading of the 240
MIPLIB 2017 benchmark instances ended `leo1`, `leo2`, `ns1830653`,
`sp98ar` and `supportcase42` at the root and `sp97ar` at node 2
`numerical_error`.

## The failing solves

Each ends on the same refusal: a dual simplex solve from a cold start
whose settled point breaches `DUAL_TOL` in the model's units by more than
`DUAL_TOL` of its terms, "the settled point is not dual feasible". Each
failing LP was written out with its starting basis (a hook not landed)
and fails the same way alone.

| instance | the LP | rows | refused at | what breaches |
|---|---|---|---|---|
| `leo1` | root, the re-solve after cut round 3 | 788 | 1.39e-8 | a row's dual |
| `leo2` | two relaxations in the tree | 814, 642 | 9.31e-9, 8.84e-8 | rows' duals |
| `sp97ar` | the relaxation of node 2 | 2156 | 7.45e-9 | rows' duals |
| `sp98ar` | root, after the cut rounds | 1819 | 2.04e-9 | rows' duals |
| `supportcase42` | the root relaxation | 18439 | 6.66e-8 | 77 rows' duals |
| `ns1830653` | root, the re-solve after cut round 9, warm | 4662 | 1.68e-9 | boxed columns |

**A row's dual read absolutely (`leo1`, `leo2`, `sp97ar`, `sp98ar`).**
Their costs run from 1e6 to 3e7. A row whose dual is zero at the optimum
gets from the BTRAN a few ulps of those costs, 1.9e-9 (2^-29) to 8.6e-8.
A column's breach was read against its terms, `|c_j| + sum |a_ij
y_i|`, but a row's against 1, so each of these points was refused on a
row. Three things made it worse:

- The settling rounds kept the best point by its absolute breach. On
  `leo1`'s root LP round 0 breached 7.45e-9 on a row and round 1 1.86e-8
  on a column whose terms are 2.7e7, 6.8e-16 of them; round 0 was kept,
  and refused.
- The cleanup pivots such a row into the basis, and the noise lands on
  another: on `sp98ar` rows 15559 and 16856 trade places for 32 rounds
  with the same breach, 1.40e-9.
- A best point restored and refactored reads again. With the ranking
  below alone, `leo1` failed at a node instead: its best point, kept at
  no breach, breached 2.79e-9 on a row once restored and refactored.

**A walk the rounds do not finish (`supportcase42`).** After the dual's
shifts are repaid, 77 rows' duals breach by up to 3.6e-7, real ones, not
rounding. With nothing to flip, each round pivots each breached row in
once, under cost shifting, and repaying those shifts leaves a point that
breaches by 1e-7 to 1e-5. The objective falls from 7.3435391 to
7.3435388 over the 32 rounds, so the rounds are walking a primal path a
pass at a time; the best, round 30's, breaches 4.9e-9 and is refused.

**Rounds that cost a solve each (`ns1830653`).** The re-solve starts from
the last round's basis. Read alone, as the 4839-row re-solve of a run
with the row reading, which fails the same way without the primal, its
settled point breaches 1e-8 on boxed columns of zero cost whose rows'
duals are near zero; each round flips them, and the dual takes about 1400
iterations to repair the rows, its shifts leaving new breaches of the
same size: 32 rounds, 44665 iterations, then a cold restart whose 32
rounds end at 5.5e-9, 4.0e-9 of its terms.

## The change (`src/simplex.c`)

- A row's breach is read through the columns the row meets. Moving its
  dual by `d` moves each of those columns' reduced costs by `d |a_ij|`,
  so the row reads `d |a_ij| / max(1, terms_j)` at its worst column, in
  the model's units. A row with a column whose terms are no larger than
  its `|a_ij|`, or with no column, reads `d` as before.
- The settling rounds rank a point by the test that publishes it: within
  `DUAL_TOL` first, by objective; then within `DUAL_TOL` of its terms;
  then the rest. The last two by their absolute breach, as before.
- A cold settled point that fails both goes on in the primal simplex from
  its basis, then settles as a primal run does (`SETTLE_ROUNDS_PRIMAL`).
  The primal ends when it stalls, the point where it would take Bland's
  rule, and the solve then ends `numerical_error` as before, saying so.

None of the three changes a point the old test published: the ranking
differs only between a point the old test refused and one it publishes,
and the primal starts only where the old solve ended `numerical_error`.
A warm solve that restarts cold may skip restoring a refused point it
kept before, one refactorization. The row reading reads every column's
terms, one pass over the matrix, only when a row breaches; like the rest
of the settled-point test, it bills no work units.

`tests/test_simplex.c` holds two of the generated LPs below:
`lp_row_dual_noise.mps` (5 rows, refused on a row's dual of 2.98e-8 with
costs near 2e8) and `lp_settle_primal_finish.mps` (the primal takes 21
iterations from the settled point).

## Generated LPs

`gen.py` writes LPs whose rows with a zero dual at the optimum all hold
with equality, at a cost scale of 1e6 to 1e9 over 3, 7, 11 or 13.

| seeds | `numerical_error` on 97541e88 | after | same objective as `--exact` |
|---|---|---|---|
| 0 to 1999 | 236 | 236 optimal | 236 |
| 2000 to 9999 | 876 | 875 optimal, 1 `numerical_error` | 875 |

874 of the 876 need only the row reading and the ranking; seeds 6744 and
7613 go on in the primal. 7613 ends optimal; on 6744 the primal stalls
at a total dual infeasibility of 1.0076e-9 and the solve ends
`numerical_error` after 1040 iterations, against 60 before. Run to its
iteration guard, that primal took 18001 iterations and ended on a library
error (refusal `settle-primal-to-guard`).

`jaos solve --check` at 1e-7 refuses 88 of the 1111 optima, 22 and 66,
on a row's multiplier of 1.0e-7 to 1.1e-6, with objectives of 5e9 to
2.3e11 and gaps under 1.7e-15: the checker reads a row's multiplier
against 1, and at those costs a row's dual carries that much rounding.
Their objectives match `--exact` too.

## The six at 300 s

02-491's `run.sh -n six.txt -s jaos -j 1` with `--threads 1`
(`six/record.txt`; its tree reads `unknown` because git under WSL cannot
read a worktree's link; it was 4e9f5c2c with this change). The load was
7.5 to 10 from other work, so the nodes are indicative. Bounds from the
logs:

| instance | 02-491 | now | incumbent | bound | reference |
|---|---|---|---|---|---|
| `leo1` | `numerical_error`, 1 node, 1.3 s | `time_limit`, 6652 nodes | 425120732 | 396667355.49 | 404227536.16 |
| `leo2` | `numerical_error`, 5 nodes, 23.6 s | `time_limit`, 6587 nodes | 592193010.34 | 395079830.32 | 404077441.12 |
| `ns1830653` | `numerical_error`, 1 node, 89.2 s | `time_limit`, 1 node | none | 11130 | 20622 |
| `sp97ar` | `numerical_error`, 2 nodes, 57.7 s | `time_limit`, 201 nodes | 683504233.6 | 654433678.16 | 660705645.759 |
| `sp98ar` | `numerical_error`, 1 node, 5.3 s | `time_limit`, 372 nodes | 534517051.2 | 526905773.58 | 529740623.2 |
| `supportcase42` | `numerical_error`, 1 node, 68.1 s | `time_limit`, 360 nodes | 27.463697549 | 7.3435388161 | 7.75863072227 |

`ns1830653` spends the rest of the 300 s in that warm re-solve's 32
rounds; alone, its LP takes 151 s: 44665 iterations warm, then 2202 cold
and 659 in the primal to 11161.513946091609. That is TODO J35 now.

## The gates

On the tree rebased on 7b7dd94e, against its records.

- `make test` and `make sanitize` pass; `windows-test` skips the Python
  suite run natively, the host's Windows Python reading no `PYTHONPATH`.
- `netlib`, `netlib-infeas`, `netlib-kennington J=2` and `miplib J=2`:
  `gate: PASS`, `baseline: 0 regressed, 0 improved, 0 new, 0 at other
  nodes`, each, and their records unchanged to the byte.
- `primal barrier pdlp concurrent warm J=12`: disagreed 0, rejected 0,
  errors 0 in each, and their records unchanged to the byte.
- `miplib2017 J=2`: `m17sum.py` reads solved 11, incumbents 29, primal
  4.1525, dual 5.0942, sum 9.2468 before and after. One line moves:
  `glass4`, at the work limit with the same
  131864 nodes, incumbent and bound, 1425705 to 1425714 iterations and
  10000000067 to 10000002865 work units. The tree is the same; its
  heuristics' sub-MIPs read otherwise, mutation 75 work units more and
  the sub-MIP boxed by the incumbent's distances 1490 fewer.

On 4e9f5c2c and on 87475d8a every record held to the byte. On ab51180f
`glass4` moved by 3 iterations and `neos-3754480-nidda` by 10 work units,
each at the same nodes, incumbent and bound.
