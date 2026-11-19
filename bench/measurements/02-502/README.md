# 02-502 — a time limit that is set and never reached leaves the run as it was

TODO row J37, a Tier 1 defect found on 7b7dd94e. `sp150x300d` with
`jaos solve --threads 1 --work-limit 100000000` ends optimal at 69 in 2 nodes
and 16,715,755 work units; the same with `--time-limit 1200` ended
`work_limit` at 100,000,399 units with 512 nodes and bound 68, the same from
run to run, and a dev build (`-UNDEBUG`) of it aborted at `src/presolve.c`'s
`ps_verify_row_activities`. A limit that is set and not reached must leave
every result, every work count and every node as the run without it.

Taken on 2026-11-19 under WSL on the Ryzen 5 9600X, which had been up 6,000
to 11,000 s, while other agents loaded the machine. Before is 7b7dd94e, after
is the change on it.

- `sp150x300d.txt`: `sp150x300d` before and after, with no limit, at 1200 s
  and at 1e6 s, and with `--no-mip-presolve` with no limit and at 1200 s.
- `check.txt`: the points the runs at 1200 s return on 7b7dd94e, and the
  default run's, judged by `jaos check`.
- `trace.patch`: the temporary trace, on 7b7dd94e's `src/presolve.c`, that
  prints every basic row whose published activity the row-activity check
  rejects, in the reduced model and in the model as given, and the replay's
  records. `activity.txt`: its lines on `sp150x300d`.
- `miplib3-7b7dd94e-5000.txt`: MIPLIB 3 on 7b7dd94e under `-O
  time_limit=5000`, against its record.
- `dev-miplib3.txt`: the 24 of MIPLIB 3 under the dev build of the change,
  with and without the MIP presolve.
- `sweep.sh SET OUT BIN LIMIT...` runs `BIN`, a `bench/run`, on a set
  (`m3`, `m17` at the gate's `-L 1e10`, `netlib`, `infeas`) at each `LIMIT`:
  `none` without a limit, a number as `-O time_limit=LIMIT`, the MIP sets
  under `flock ~/mipgate.lock` at `-j 2`. It says whether each record is
  byte for byte the first. `sweep-7b7dd94e.txt`: its readings on the change
  over 7b7dd94e.

## The branch that moved

`budget()` (`src/mip.c`) gives an LP the time left of the tree's limit,
counted from `m->mip_started`, the second the tree started. `bb_tree` sets
that field for the model it searches. When the MIP presolve substitutes
columns out or mirrors them, `jm_branch_and_bound` searches the presolved
copy and then publishes the copy's optimum on the model as given through
`republish_at_the_incumbent`, an LP over the continuous columns at the
integer point. Its `budget()` read the given model's `mip_started`, which no
tree had set: zero, the origin of the monotonic clock. The time left was the
limit less the seconds since boot, and on a machine up longer than the limit
it was negative, so the LP got `DBL_MIN` and stopped at its first look. The
republish failed, and `jm_branch_and_bound` took the path for a presolved
tree it cannot use: it searched the model as given from the start, under the
limit's time and what the first tree left of the work.

On `sp150x300d` the presolved tree ended optimal at 16,715,755 units, and the
search of the model as given took the 83,284,245 left and stopped at
100,000,399 with 512 nodes, the bound at 68: the run `--no-mip-presolve`
makes, cut at that work (`sp150x300d.txt`). At 1e6 s the limit exceeded the
seconds since boot and nothing moved, which is why a far limit hid it. On
MIPLIB 3 at 5000 s, below the machine's 7,728 s up, `egout` is the one that
moves (`miplib3-7b7dd94e-5000.txt`): 2,543,926 work units from 1,169,208,
and the record's second solve, the determinism check, `DIVERGED`, since the
first solve's second tree had set the field by then and the second solve's
republish passed.

The fix sets `m->mip_started` where `jm_branch_and_bound` starts its clock,
the start the MIP presolve, the presolved tree and the searches after it
already count from. The other reads were checked and are right:

- `budget()` sets an LP's limit only when the tree has one. Without one the
  LP keeps its own, none, and with one it gets what is left.
- The presolve and the aggregator read a deadline counted from the LP's own
  entry, and the MIP presolve one counted from `jm_branch_and_bound`'s.
- Sub-MIPs and strong branching's threaded probes take what is left from
  `mip_started` inside the tree that set it, and the lattice's republish
  from its own first tree's start.
- Every other read compares the clock with a deadline, which is zero without
  a limit (`jm_deadline`), and `jm_past` then reads no clock. With a far
  limit it reads the clock and decides nothing before the deadline.

The sweeps below, at 1e6 s and at 5000 s, a limit below the seconds since
boot that a start of zero would already have passed, found no other branch.

## The presolve assert

Not a postsolve fault, and not the limit's: `--no-mip-presolve` with no
limit aborts the dev build at the same point, since the search of the model
as given is that run. A node relaxation of 494 rows and 600 columns, which
presolve reduces to 241 rows and 288 columns, solves optimal, and the
reduced solve publishes its row 197 basic at 3.6652636481449008e-10, the
value of the row's logical in the basis, while its two columns make
3.6655348831604595e-10 (`activity.txt`). The 2.7e-14 between them is the
simplex's: the logical's value comes out of the basis, with the error of the
solves that made it, and the row's own traffic, 3.7e-10, does not bound that
error. The replay copies the activity to row 448 of the model as given and
adds nothing to it, both columns being kept, and the check, whose window for
a basic row is rounding on the row's traffic, 1.78e-15 here, blamed the
replay for it. 02-62 left out the rows whose logical rests on a bound for the
same reason.

The check now adds, for a row the reduced model keeps, what the reduced
solve published on it: the gap between its activity and its column sum, and
the rounding of that sum. A removed row keeps the window it had, and a replay
that moves a kept row moves it past both. It runs in the debug build only,
so the release objects are those of the change without it.

The points the runs return pass `jaos check` (`check.txt`): the work-limit
stop's incumbent at 1200 s and the optimum of the run at 1200 s with no work
limit, which searched the model as given in 591 nodes, both at 69 with no
column or row violation, `dual_feasible no` as for the default run's
optimum, whose duals are those of the LP at the integer point. Under the dev
build of the change, `sp150x300d` passes its asserts with and without the
limits and the MIP presolve, and so do the 24 of MIPLIB 3 at 200 million
units with and without the MIP presolve (`dev-miplib3.txt`).

## The tests

- `test_a_time_limit_never_reached_leaves_the_tree_as_it_was`
  (`tests/test_mip.c`) solves `pass_through_path`, whose optimum the
  presolved tree finds, and `binary_cover` to its node limit, with no limit,
  at 1e6 s and at half the clock's reading, which a start of zero has passed,
  and compares the status, work, iterations, nodes, solves, cuts, fixed
  columns, bound, incumbent and point. On 7b7dd94e the path reads 4963 work
  units at half the clock against 519.
- `tests/cli.sh` solves `tests/data/mp_path.lp` with no limit, at 1e6 s and
  at half the seconds since boot, and compares the reports less `time`. On
  7b7dd94e, `work_units 4963` against 519 and `symmetry_generators 0`
  against 2.
- `test_a_kept_row_carries_the_residual_of_the_reduced_solve`
  (`tests/test_presolve.c`) hands the postsolve a reduced optimum whose basic
  row sits 1e-13 off its column sum; the check before aborts the test binary,
  and after it the row as given carries the same 1e-13.

## The sweeps

`sweep-7b7dd94e.txt`, on the change over 7b7dd94e, every record against the
one without a limit, which is byte for byte 7b7dd94e's record. The machine
was up 7,254 to 10,994 s, so 5000 s lay below the seconds since boot
throughout.

| set | limits | records |
|---|---|---|
| MIPLIB 3 (24) | 1e6, 5000 | byte-identical |
| MIPLIB 2017 gate (30), `-L 1e10` | 1e6, 5000 | byte-identical |
| netlib (94) | 1e6, 5000 | byte-identical |
| netlib-infeas (29) | 1e6, 5000 | byte-identical |
