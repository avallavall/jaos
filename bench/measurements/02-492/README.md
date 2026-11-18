# 02-492 — strong branching on threads, and every core by default

Taken on 2026-11-18 on cc4758dc and on the change above it, for TODO row
J12 and the SPECS row "Parallel MIP solve". The machine has 12 cores and
15 GB, and five other agents were running on it the whole time, at a load
of 9 to 12. Every wall-clock number below is inflated and noisy: read it as
an indication, and take it again on a quiet machine with `speedup.sh`. Work
units, nodes and answers do not depend on the load.

## Where the tree's work goes

`shares-miplib.txt` and `shares-miplib2017.txt` are the `branch and bound
work` line of `--log summary` on cc4758dc, one thread, at 3e9 work units on
MIPLIB 3 and 1e9 on the 2017 set. The shares are of the tree's own work:

| phase | MIPLIB 3, typical | MIPLIB 2017 at 1e9, typical |
|---|---|---|
| node relaxations | 50% to 86% | 67% to 99% (`glass4` 37%, `csched007` 25%) |
| heuristics | 12% to 31% (`enigma` 74%, `p0033` 56%) | 12% to 20% (`csched007` 65%, `graphdraw-domain` 59%) |
| strong branching | 4% to 15% | 0% to 13% |
| cuts, propagation, the rest | under 10% on most | under 12% on most |

Strong branching is capped at a tenth of the tree's work
(`MIP_PROBE_SHARE`), and it reaches the cap on about half of each set. So
running the probes on threads, with the tree unchanged, saves at most a
tenth of the wall time. The node relaxations are the lever, and with the
tree unchanged they run one after another.

## What runs on threads now

1. **A probe starts from the node's own state.** A node's probes ran on
   one copy of its final simplex state, each restored from the one before:
   the factor's FTRAN density estimate, the order of each row's entries in
   the row-wise matrix, the phase-1 cost marks and the settle buffers came
   from the probe before. So a probe's pivots and work depended on which
   probes ran before it. Now the first probe of a node clones the whole
   state into a template (`sx_clone`), and every probe is restored from it
   in full (`sx_assign`, `probe_ready` in `src/simplex.c`). A probe is a
   function of the node and its own column, bound and cap.
2. **The probes on lanes.** `jm_probe_many` takes a node's probes and runs
   them on up to `--threads` lanes: the calling thread on the kept state,
   each other lane on its own copy of the template, kept on the model
   between nodes. A lane takes the next probe from a shared counter, and
   the answers land in the probes' own slots, so which lane ran which
   probe does not matter. `kept_probes` in `src/mip.c` then reads them in
   candidate order, as the one-thread loop did. When a work limit is near,
   a probe's cap depends on the work of the probes before it; the tree
   then reads the caps in order and solves again, on the calling thread,
   every probe whose cap differs.
3. **A pool of workers.** Starting three threads and joining them takes
   270 to 330 µs on this machine (`thr.c` in the session, not kept), more
   than a node's probes take on most of MIPLIB 3. `jm_pool` in
   `src/sys.c` starts the workers once per tree and wakes them per node.
4. **When to spread.** A node spreads its probes over one more thread per
   `MIP_PROBE_LANE_WORK` (150000) work units of probes, estimated from the
   work per probe of the node that probed before; a tree's first probing
   node runs on one thread (`sweep.txt`). On this machine, below 150000
   `bell5` and `blend2` probe slower on four threads than on one, and
   `misc07`, `binkar10_1`, `tr12-30`, `mad` and `l152lav` probe 1.6x to
   2.5x faster at any value from 50000 to 400000. `stein45` probes slower
   on four threads at every value: 5.3 µs per thousand work units on lanes
   against 3.0 on one thread. That is not understood; the load is a
   candidate. The sweeps took the probe cap as the first node's estimate;
   a sub-MIP that probes once then started a pool for one node, so the
   first node now probes on one thread.
5. **Every core by default.** `jaos_threads_of` returns the core count
   when no count was set. The bench tools set one thread per solve, so a
   `-j` run does not ask for J times the cores; `-O threads=N` overrides it.

`make tsan` is new: the five suites that start threads (`test_chol`,
`test_concurrent`, `test_conic`, `test_mip`, `test_symmetry`) under
ThreadSanitizer, run with address randomisation off, which this kernel
needs.

## The tree moved once

Restoring each probe in full changes some probes' work and pivots, so the
tree moved, once, for every thread count. Against `bench/miplib.baseline`,
one thread (`miplib3-t1.txt`): 24 of 24 solved and checked, no regression, 8 at
other nodes (`bell3a` 0.97x, `bell5` 0.98x, `blend2` 0.89x, `dcmulti` 1.03x,
`misc03` 0.93x, `misc07` 1.16x, `stein27` 1.00x, `stein45` 1.04x), and the
work of the other 16 moved by less than 0.1%. Over the 24 the work reads
0.999x (geometric mean) and 1.045x in total, `misc07` and `stein45` the
total's weight.

The 2017 set at 1e10 work units (`miplib2017.txt`), one thread, against
`bench/results/miplib2017.txt` with 02-328's `m17sum.py`: 9 solved and 29
with an incumbent as before, the primal gap sum 5.5284 to 5.1919, the dual
6.1232 to 6.1521, the sum 11.6517 to 11.3440. Of the nine solved,
`gen-ip054` reads 0.89x, `pk1` 0.99x, `binkar10_1` 1.02x, `exp-1-500-5-5`
1.01x and the rest 1.00x. `neos-2657525-crna`'s incumbent goes from 825.06
to 208.23, `timtab1`'s from 1216454 to 990499 and `markshare2`'s from 138
to 89; `supportcase26`'s goes from 1802.19 to 1836.20.

## The same tree at every thread count

`bench/run` at `-O threads=1`, 2, 4 and 12, two solves at a time: the
records of MIPLIB 3 to completion (`miplib3-t1.txt`) are byte-identical at
the four counts, every line's status, iterations, work, nodes, solution
digest and basis digest, and so are the records of the 2017 set at 2e9
work units (`miplib2017-2e9-t1.txt`; `determinism.txt`). `make netlib
netlib-infeas J=4` and `make netlib-kennington J=2` read as their
baselines, no instance at other nodes: no LP solve takes a probe.

`tests/test_mip.c` asserts it twice. `test_probes_on_lanes_give_what_one_
probe_at_a_time_gives` runs a node's probes on one lane, on three lanes and
one at a time in reverse order, and compares each probe's outcome, bound
bits, work and pivots. `test_strong_branching_on_threads_keeps_the_tree`
solves two MIPs at 1 and 3 threads with the lanes forced on and compares
the objective's bits, nodes, solves, work and point. `tests/cli.sh` and
`python/test_jaos.py` check a MIP at 1 and 3 threads and the default count.

## Wall time

`speedup-loaded.txt` is `speedup.sh` at 1e9 work units, one solve at a
time, at a load of 3 to 4.5 from the other agents. Wall time is the
solver's own `time` line: `/usr/bin/time`'s elapsed time under WSL read
below the user time on one thread and once below zero. Against one thread,
four threads read 0.89x to 1.17x and eight 0.86x to 1.14x; every status,
objective, work, node, bound and incumbent line agrees. CPU over wall is
0.92 to 0.95 on one thread and 0.94 to 1.06 on four and eight, so the
lanes add a few percent of work done at once, as the probe shares above
allow. The spread is the noise: three repeats of `misc07` on one thread
read 3.01 s to 3.24 s, and the same instance read 1.08x and then 0.92x at
four threads. At this budget the strong-branching lanes give no speedup
that this machine can tell from the noise.

## Rounds of nodes without the second solve

`rounds.txt` is `--tree-batch` 1, 4 and 8 on one thread at 4e9 work units,
on the tree of this change, with the work of each round node's second
solve counted apart (a counter in `bb_tree`, not kept). The rounds as they
are cost 1.32x MIPLIB 3's work at batch 4 and 1.21x at batch 8 (geometric
mean over the 19 instances whose tree moves), with 2 past 2x at batch 4.
Without the second solves the same trees would cost 1.13x and 1.035x, none
past 2x, from 0.52x (`blend2`) to 1.61x (`p0201`) at batch 8. That is an
estimate: the round's lanes solve without the node's propagation and
without the incumbent's cutoff, so a version that takes the lane's solved
copy in place of the second solve must run both on the lane, and its tree
differs again. It is the next piece, because the node relaxations are most
of the work and a round runs them at once.

## Reading it again

`speedup.sh` runs a fixed list at 1, 4 and 8 threads, one solve at a time,
and prints wall time, CPU time, CPU over wall, the speedup against one
thread, and whether the status, objective, work, nodes, bound and
incumbent lines agree. Run it on a quiet machine after `make cli`.
