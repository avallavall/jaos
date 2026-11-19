# 02-501 — where the root's 300 s go on the 96 that never leave it

Taken on 2026-11-19 for TODO row J36: 96 of MIPLIB 2017's 240 benchmark
instances end 02-491's reading at the root, one node, 73 of them with no
incumbent (`02-491/full/record.txt`, status `time_limit` and `nodes 1`;
`root96.txt`). Every reading here is 300 s on one thread, `--threads 1
--time-limit 300 --log summary`, on builds of 7b7dd94e; the change was
then rebased on 0555ab39 (J35's settling, J37's time-limit fix, J12's
lanes) and its identity checks and gates ran on that tree. The machine
was shared with three other agents (load 4 to 12 on 12 logical CPUs), so
the seconds are indicative and the work units are the measure.
`s250r10` and `satellites2-60-fs` (10.9 and 9.5 GB in 02-491) are left out
for the shared machine's memory, so the split reads 94.

The folder:
- `split.patch`: the measuring build. It logs the root's work and seconds
  by piece (`root split:` and `root piece:` lines): the MIP presolve and
  setup, the symmetry search, the LP presolve, the root LP with its
  iterations, probing, the clique table, each separator of the cut rounds,
  the cut rounds' LP re-solves, each root heuristic, strong branching, and
  the piece the time limit passed in. It applies to 7b7dd94e and does not
  move a path.
- `pair.sh`: the reading. Each instance runs under the two builds, the two
  legs queued next to each other so that they run at the same time and
  load, each leg's line with the load average at its start and end. Both
  legs carry the same `--time-limit`: until the defect reported on
  2026-11-19 is fixed (an unreached time limit moves `sp150x300d`'s path on
  7b7dd94e), a run under a time limit is only compared with another under
  the same limit.
- `split.py`: the table of shares from the logs; `pairsum.py`: the two
  legs side by side; `speed.py`: the root LP's speed in each pair.
- `record-pairs-heavy.txt` (the four heaviest, two at a time),
  `record-pairs.txt` (the rest, three at a time), `record-before.txt` (the
  before build alone on the instances the pairs left out, by 02-491's
  `run.sh` with the binary given): the records; `split.txt`: the split of
  every instance read, in work units, with every piece's total at its end;
  `split-seconds.txt` the same in seconds; `pairsum.txt` and `speed.txt`:
  the pairs.

## The split

Over the 94, the share of all work units and of all seconds, and the
instances each piece leads (`split.txt`, `split-seconds.txt`):

| piece | work | seconds | leads (work) | leads (seconds) |
|---|---|---|---|---|
| root LP | 32.2% | 45.4% | 40 | 40 |
| the cut rounds' LP re-solves | 44.8% | 31.3% | 34 | 34 |
| heuristics | 17.0% | 14.1% | 17 | 14 |
| strong branching | 4.1% | 2.7% | 3 | 2 |
| the separators | 1.1% | 4.1% | 0 | 4 |
| symmetry | 0.2% | 1.9% | 0 | 0 |
| presolve, LP and MIP | 0.0% | 0.1% | 0 | 0 |
| probing, clique table | 0.0% | 0.0% | 0 | 0 |

The LP, at the root and in the cut rounds' re-solves, leads on 74 of the
94 and takes 77% of the work. Of the heuristics, the root dive takes 13.8%
of all work and leads on most of the 17. The 300 s run out in the root LP
on 33, in a cut round's re-solve on 29, in the dive on 13, in strong
branching on 9, in the clique separator on 2, in another heuristic on 5,
and in the tree on 3: on this base `neos-2746589-doon`,
`neos-5195221-niemur` and `ns1208400` leave the root (5, 7 and 19 nodes),
which 02-491's 21fdf4d3 did not. 23 of the 94 hold an incumbent. Two
pieces cost seconds their work units do not show: the clique separator
bills 0.00% of the work and takes 3.5% of the seconds, and the symmetry
search 0.2% against 1.9% (its refinement bills one pass a round while a
pass sorts every vertex).

## Why the root LP costs what it costs

The root LP is JAOS's dual simplex with dual steepest edge (the MIP root
never takes the barrier). Against HiGHS 1.15.1's dual on the same LP
relaxations (`solve_relaxation`, one thread, the same machine and load):

| instance | HiGHS iterations | HiGHS s | JAOS iterations | JAOS s |
|---|---|---|---|---|
| `mzzv11` | 62,165 | 19.1 | 63,258 | 74.4 |
| `qap10` | 51,988 | 5.6 | over 140,486 | over 60 |
| `dano3_3` | 59,126 | 12.2 | over 63,491 | over 60 |

So on these three a JAOS iteration costs 3.9x to 4.7x HiGHS's, and on
`qap10` it also takes 2.7x the iterations or more (HiGHS's presolve drops
its 272 dependent rows; JAOS's root LP presolve removes nothing). The MIP's
root LP takes far more iterations than the same relaxation solved alone:
`mzzv11`'s root LP runs 248,058 iterations in its 300 s and does not end
(`split.txt`), where the relaxation alone ends in 63,258; `dano3_3`'s ends
after 280,779 and `qap10`'s after 379,018. The root LP is solved as a node
(`cfg.node_solve`), which among other things never replaces drifted
steepest-edge weights by exact ones (`DSE_GUESS_RESTARTS`: "never in a
MIP node") and leaves the root's own presolve and coefficient tightening
in front of it; which difference costs the iterations is not read here. A gprof
build (`-pg -fno-inline`) of ab51180f splits the iteration:

- `mzzv11`, 40 s: the dense back-substitution with U 33%, the
  factorizations 12% (250 in 16,211 iterations, one every 64 updates, the
  `REFACTOR_EVERY` that `pilot87`'s accuracy set), BTRAN 15% (its reach
  walk 7%), the pricing row 7%. The forward solves are dense: their answers
  hold 56% (the entering column), 61% (the bound flips) and 69% (the
  steepest-edge row) of the 9,499 rows, on a factor of 21,276 + 47,376
  nonzeros over a basis of 43,328.
- `qap10`, 60 s: factorizations 18% (1,900), the dense forward solves 26%,
  BTRAN 20% (its reach walk 12%).
- `dano3_3`, 60 s: forward solves 22%, the pricing row 11%, the ratio test
  over a dense pricing row 17%, factorizations 9%.
- `neos-5114902-kasavu` (961,170 rows) and the other models with 10^5 to
  10^6 rows are the opposite: their solves are hyper-sparse (answers of
  1e-4 of the rows), and the time went to passes over every row that the
  work units do not bill: each solve permuted its right-hand side in and
  its answer out over all rows, the BTRAN found its roots by scanning every
  slot, the iteration copied and cleared the column, the flips and the
  steepest-edge row with full `memcpy`/`memset`, the Forrest-Tomlin update
  shifted every slot behind the replaced one, and the row choice read every
  row. Twelve or so such passes an iteration made a 961k-row iteration
  cost 7 ms against 1 ms of billed work.
- `neos-3402294-bobin` (591,076 rows, 2,904 columns): the forward solves
  are half dense, so its iterations cost what their arithmetic costs.

## The change

Every answer and every work unit stays the same; only the unbilled passes
go:
- the forward and backward solves take the pattern of their right-hand
  side (`jm_lu_ftran_in`, `jm_lu_btran_in`), read and clear only those
  entries in a scratch vector kept zero, and return the entries they wrote;
  the dual simplex hands them the entering column's rows, the leaving row,
  the bound flips' rows and the steepest-edge row's pattern, and clears
  each vector by the list of what its last solve wrote, falling back to a
  full clear whenever another path wrote the vector;
- the kept spike of the Forrest-Tomlin update is cleared by its pattern;
- the update walks the eliminated row's entries in the order of U through
  a heap instead of every position behind the replaced slot, and the
  replaced slot takes a new position after the last instead of shifting
  every slot behind it (the pattern sort then bills its words as the order
  without gaps would);
- the rows outside their bounds are kept in a bitmap that `pviol_set`
  updates, so the row choice reads them in index order, the same order and
  sums, and still bills a pass over every row;
- the symmetry search's colour refinement sorts each colour class on its
  own, in place of every vertex at every pass: the classes come out of the
  previous pass in colour order, so the order and the colours are the
  same.

Identity: on 0555ab39, the change and the base print the same lines but
`time` on all 94 Netlib LPs, Kennington's 16, MIPLIB 3's 24 at 3e8 work
units and MIPLIB 2017's 30 at 5e8, and an assert build (`-UNDEBUG`, which
checks every cleared vector against the full copy it replaces) agrees on
Netlib and on MIPLIB 2017 at 1e8. The gates on the rebased tree: `make
test` and `make sanitize` pass; `netlib`, `netlib-infeas`,
`netlib-kennington` and `miplib` read `gate: PASS` and `baseline: 0
regressed, 0 improved, 0 new, 0 at other nodes`; `miplib2017` reads its
`gate: NOT MET` as before, with no baseline, and `m17sum.py` reads solved
11, incumbents 29, primal 4.1525, dual 5.0942, sum 9.2468 on both sides;
and every record of `bench/results/` those targets and `primal`,
`barrier`, `pdlp`, `concurrent` and `warm` write comes out byte-identical
to the committed one. Kennington's 16 took 35.5 s against 49.4 s in the
identity run.

At a fixed work limit, the root LP's seconds (one leg beside two other
solves; ab51180f):

| instance | work | root LP s before | after |
|---|---|---|---|
| `neos-5114902-kasavu` | 1e10 | 51.3 | 12.2 |
| `ns1760995` | 3e9 | 21.2 | 12.3 |
| `supportcase22` | 5e9 | 28.0 | 25.1 |
| `neos-3402294-bobin` | 4e9 | 20.8 | 18.1 |
| `mzzv11` | 1e10 | 22.8 | 21.4 |

and `neos-5114902-kasavu`'s symmetry search at 3e9 takes 14.4 s against
30.7 s. A cache of the factor's arrays across refactorizations was not
built: with glibc keeping freed blocks (`MALLOC_MMAP_THRESHOLD_`,
`MALLOC_TRIM_THRESHOLD_`), `kasavu`'s root LP read 6.86 s against 7.05 s.
Ranking the refinement's classes by a hash of their keys was built and is
slower (`symmetry-hash-ranks` in `bench/refusals.txt`).

## The pairs at 300 s

A sample, not the 94: the four heaviest by memory (`neos-5114902-kasavu`,
`square41`, `square47`, `academictimetablesmall`; `k1mushroom` refused its
6 GB cap in both legs) two at a time, then the others in 02-491's order,
largest file first, three at a time, stopped once 50 pairs were complete.
That order puts every instance of 90,000 rows or more in the sample and a
spread of the dense ones after them (`record-pairs*.txt`). `bohle` ran its
before leg alone for the split and has no pair.

The paths are the same in both legs, so a leg can only reach further. No
instance changes category: in the 55 pairs (`k1mushroom`'s and
`triptim1`'s legs refused their caps) none leaves the root in either leg, 9 hold an incumbent in both, and none holds a different one
(`pairsum.txt`). One more root LP ends inside the 300 s, `rail01`'s
(164,473 iterations against 158,658 unfinished). The root LP's speed
(`speed.txt`: where neither leg's root LP ended, the after leg's iterations
over the before leg's; where both ended, the before leg's seconds over the
after leg's):

| | pairs | geometric mean | range |
|---|---|---|---|
| 90,000 rows or more | 31 | 1.226 | 0.98 to 1.96 |
| fewer rows | 21 | 1.010 | 0.83 to 1.52 |

The two below 0.95 are root LPs of 10 s (`neos-4413714-turia`,
`academictimetablesmall`), where the seconds of two legs under a shared
load differ by that much.

The largest: `radiationm40-10-02` 1.96 (135.5 s to 69.2 s),
`supportcase12` 1.83 (249.1 s to 135.8 s), `snp-02-004-104` and
`neos-5049753-cuanza` 1.75, `thor50dday` 1.52, `neos-5114902-kasavu` 1.43
(26,993 iterations to 38,641), `buildingenergy` 1.43, `ns1760995` 1.37.
At 300 s `kasavu`'s gain is smaller than at 1e10 work, because its later
iterations' forward solves cross the 10% density at which they take the
dense path, which passes over every row as before.

## What is left

The largest share is the LP's iterations, at the root and in the cut
rounds' re-solves, and on most of the 94 they are dense: their cost is
billed arithmetic, which this change does not touch. The next step is in
`TODO.md`'s J36 row.
