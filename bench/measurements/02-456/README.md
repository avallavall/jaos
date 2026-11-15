# 02-456 — a stop inside the re-entry resumes its walk; an open conic node keeps a bound pair a column

Taken on 2026-11-15 on the tree of 00e975c with this change, for TODO rows
J30 and J31. `read.sh` takes both readings (`resume-*.txt`, `cone.txt`).

## J30: a stop inside the settling re-entry of a warm-started solve

When the simplex reaches an optimum it settles it: while a reduced cost
breaches its bound, it re-enters the dual simplex from the settled point,
for up to `SETTLE_ROUNDS` rounds. A stop inside a round's dual pass
restored the point the round began from, by a fresh factorisation, and
parked. The resume took back the work units and iterations counted at the
round's start (02-424) and ran the round again. The restore rebuilt the
statuses, the basis and the bounds, but nothing else the round began
with. The reduced costs and the primal values came from a fresh factor
where the straight round had the updated one's. The steepest-edge weights,
their restart counts, the factor's density estimates and the verification
state were those of the stop. A solve from a given basis starts from
guessed weights, which the pass replaces with exact ones after
`DSE_GUESS_RESTARTS` restarts. On 02-455's `node-058`, the straight round
began with one restart counted and the resumed one with four, so the
resumed pass ran on other weights: 7 iterations and 18,612 work units
against 15 and 27,641.

Now a stop inside a round's dual pass parks the state as it stands, like
a stop in any other dual pass, and the resume goes back into that pass at
the same round. The counts are not taken back. The stopped solve
publishes the basis the pass stopped at, where it published the settled
one. A node solve parks nothing, so it still restores the settled point at
such a stop, and the trees do not move.

`test_a_stop_inside_the_settling_reentry_resumes_its_walk` reads
`node-058` (`tests/data/lp_resume_reentry.mps`, `.bas`), where it read one
of the 52 that resumed, stopped at ten points of its re-entry, from 12,500
to 26,000 work units. With 00e975c's `src/simplex.c` the test fails:
27,641 work units expected, 21,516 resumed.

### The reading

`dumpnodes.patch` writes the first 400 node LPs that the tree of
`tests/data/mip_noinc_propagate.mps` solves from a basis, each with that
basis, cut rounds of the root included. `resume-many.c` stops each at 40
evenly spaced work limits of its straight walk and resumes. 14,837 of the
16,000 limits stop a solve.

| library | broken stops | files with one |
|---|---|---|
| 00e975c | 420 | 37 |
| the change | 0 | 0 |

02-455's `resume-warm.sh`: `broken 0`, where 00e975c broke all 8. 02-424's
`read.sh` reads clean: `resume2.c` on seeds 1 to 9 and 02-247's `resume.c`
on seeds 1 to 8, every stop at the reference's status, iterations, work
units and objective.

## J31: the conic tree held about 400 KB a node

A child node copied its parent's list of fixed bounds and appended its
branch's, so a node at depth d held at least d entries. 02-455's
`s2-c01303.mps` (`tests/data/conic_tree_deep_dive.mps`), 34 lines with
three integer columns that have no finite bound on one side or both, dives
with no incumbent for its whole work limit, and the dive keeps an open
sibling at each level, so its memory grew with the square of its depth.

Now a node keeps one entry a column, sorted by column. A child merges its
parent's list with its branch's entries, and a column's last entry wins.
`ct_apply` sets each listed column's bounds with `jaos_set_col_bounds`,
which only stores them, so every relaxation gets the bounds it got before
and the tree is the same.

`cone.txt`, peak resident memory (`/usr/bin/time -f %M`) and the growth of
the heap in use over the solve (glibc's `mallinfo2`, read at every node):

| work limit | nodes | 00e975c resident, heap growth | the change |
|---|---|---|---|
| 2.5e7 | 382 | 5.3 MB, 2.0 MB | 3.1 MB, 0.13 MB |
| 5e7 | 753 | 11.8 MB, 7.1 MB | 3.2 MB, 0.21 MB |
| 1e8 | 1,492 | 37.8 MB, 27.1 MB | 3.3 MB, 0.38 MB |
| 2e9 | 27,369 | 11.3 GB (02-455) | 8.8 MB, 6.2 MB |

The two libraries end at the same status, nodes and work units under each
limit. `test_a_deep_conic_dive_keeps_its_open_nodes_small` solves that
file under 2e8 work units (2,968 nodes) and requires the heap in use to
grow by less than 8 MB. With 00e975c's `src/conictree.c` it grows by
106 MB and the test fails; with the change it grows by 0.7 MB.

## The gates

`make test` and `make sanitize` pass. Against 00e975c's records:

| reading | regressed | what moved |
|---|---|---|
| `netlib`, `netlib-infeas` | 0 | nothing: every line the same |
| `netlib-kennington` | 0 | nothing |
| `cblib` | 0 | nothing |
| `primal`, `barrier`, `concurrent`, `warm` | | nothing |
| `pdlp` | | `dfl001` and `pilot`, which overrun, stop inside the re-entry of the crossover's simplex and bill 293,485 and 2,912,624 fewer work units, the factorisation that restored the settled point; the same iterations |

`miplib` was not taken: `src/mip.c` did not change, and a node solve keeps
the restore.

02-432's `gate.sh`, 00e975c against the change: every line the same in
the four sets that run the conic tree. Those are 02-255's mixed-integer
conic models, with the one `UNSURE` line 00e975c has; 02-326's shaped
models; CBLIB's 80 mixed-integer instances, at the same status,
incumbent, bound, nodes and work; and QPLIB's mixed-integer QCQPs.
02-425's `warmcone.c`: P1, P2 and P3 all 0. 02-427's `warmqc.c`: the 24
optima without duals that 02-452 records under P2, and nothing else.
`make cblib`: 29 of 29, 0 regressed.
