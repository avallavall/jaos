# 02-495 — rounds of nodes that solve each node once: the design

Written on 2026-11-18 on 40e925ab for TODO row J12, from the code and from
the readings of 02-264, 02-290 and 02-492. Nothing was built or run for
it, because a timing run held the machine. The numbers below are those
folders' readings and the bars the implementation must meet. Line numbers
are `src/mip.c` at 40e925ab unless a file is named.

## The round today

Under `--tree-batch N` above 1 the tree pops up to N open nodes in pick
order (9852–9897), and `bb_round_solve` (1266):

- copies the tree's LP once per node with `jaos_model_copy`, which loads
  the matrix again, copies every name, clears `node_keep`, and leaves the
  copy to build its row-wise matrix and its scaling at its first solve;
- applies the node's bounds, cuts and basis (`node_apply`) and nothing
  else: no orbital fixing, no clique fixing, no propagation, no cutoff at
  the incumbent;
- starts its threads with `jm_thread_start` every round (starting and
  joining three costs 270 to 330 µs, 02-492), deals the nodes out by
  `k % lanes`, and leaves the progress relay on lane 0's copies;
- bills each copy's solve, keeps its final basis in the node
  (`bb_take_basis`) and frees the copy.

The tree then takes each node as if it were alone: `node_apply` on its
own LP, the fixings, propagation, and a solve with the cutoff, from the
copy's final basis. That second solve refactors, and pivots again wherever
the fixings or the cutoff moved the node: 2% to 36% of the tree's work at
batch 8 (02-492's `rounds.txt`). Rounds of 8 cost 1.21x MIPLIB 3's work
(geometric mean). Without the second solves they would cost about 1.035x,
an estimate from lanes that lacked the fixings and the cutoff.

## The decisions

1. A lane runs its node up to and including the LP solve: the bounds,
   orbital and clique fixing, propagation, the indicator rows, and the
   solve with the cutoff at the incumbent the round started with. The rest
   of the node stays in the tree and runs in round order: incumbents,
   conflicts, cuts, heuristics, reduced-cost fixing, strong branching on
   the pseudocosts as they stand at that node, branching and children.
2. The lane's LP is the node's LP. The tree keeps one LP per round
   position. It takes node k on position k's LP, already solved, with its
   factor kept for the probes. There is no second solve, no copy per node,
   and no factor to move.
3. A round takes its nodes in today's pick order, up to N, and its merge
   drops a node that an incumbent from the same round closes. That waste
   is counted, not avoided.
4. N is a constant, 8. By default, rounds start once a tree's own work
   units show that its node relaxations are expensive enough. An explicit
   `--tree-batch N` keeps today's meaning.
5. A round's lanes split the work that is left. The tree bills all of it
   before the first merge.
6. Sub-MIPs, ZI, the probes, the limits, the lattice tree, orbital
   branching, restarts and the conic tree keep their behaviour, with the
   changes listed in section 6.
7. Section 7 sets the go/no-go bars.
8. Stage 1 lands under `--tree-batch` with the default untouched
   (section 8).

## 1. What a lane does, and what stays in round order

### What a lane reads

The lane phase reads only state that nothing writes until every lane has
returned. The merges start after the last lane returns, and the next round
is not started during them.

| state | written by | a lane uses it for |
|---|---|---|
| the MIP model `m` and its row-wise copy | nobody after the root. `propagate_bounds` builds the row-wise copy lazily, so `jm_model_ensure_rowwise(m)` runs once before the first round | propagation, indicator rows |
| `ilo`, `ihi` | only node 1 (reduced-cost fixing, root propagation) | `node_apply`, orbital fixing |
| the cut pool `pool` | merges only, by appending | the node's cut rows |
| the log of permanent rows (section 2) | merges only, by appending | bringing its LP up to date |
| `sym` | only node 1, which runs the second symmetry search | orbital fixing |
| the clique table `ctab` | only node 1 | clique fixing |
| `og`, `gap`, `gap_shift`, `cut_key` | constants | the cutoff |
| `bk0`, the incumbent key at the round's start | the snapshot | `cut_at = prune_at(&og, bk0, gap, gap_shift)` and the cutoff test |
| the propagation passes | the snapshot: `propagate`, or `noinc_propagate` when the round started with no incumbent and with `nodes` at `MIP_NOINC_DIVE_AFTER` or more | propagation |
| `node_no_presolve`, `node_devex`, `node_keep` | the snapshot, copied into each position's LP config at the round's start | the solve |
| the work share, the time left | the snapshot (section 5) | the solve's limits |
| node k | lane k alone | everything. Orbital fixing adds to its fixes |
| position k's LP and its `nfixed`, `nperm`, `in_copy` | lane k alone | the solve |
| `plo`, `phi`, `ouf`, `ozero`, `cstack` | each lane's own copy | scratch |

A lane writes nothing the tree owns. That covers `m` itself, including
its error text and its log: `jm_set_err` and `jm_log` go to the lane's own
LP or into its record. It also covers the pseudocosts, the incumbent, the
open set, the counters, the heuristic scheduler `gh`, the solution pool
and the probe crew. Each position's LP runs on one thread, with `log_cb`,
`progress_cb` and `progress_user` cleared. Position 0 is the tree's own
LP, so its progress relay is cleared for the lane phase and set back
afterwards. A lane never calls `jm_pool_run`, so the pool never nests.

### What a lane does

The block from `node_apply` to the end of the node's solve (9954–10075)
becomes one function that writes a record. The one-node path calls it on
the tree's LP, where the block is today, and applies the record at once.
A lane calls it on position k's LP. The function takes the node's number
as a parameter, because its tests read it (`nodes > 1`, the no-incumbent
passes). The one-node path passes `nodes` as it stands after today's
`nodes++`, and a lane passes the round's starting count plus one. The
`nodes++` itself and the copy of the node's cuts into `act` stay in the
tree. In order, the function:

1. brings position k's LP up to the log of permanent rows;
2. runs `node_apply`;
3. runs orbital fixing when `orbital_on` and `sym.ngen > 0`, adding fixes
   to node k;
4. runs clique fixing. A contradiction ends the lane with *infeasible by a
   clique*;
5. runs propagation with the snapshot's passes. A contradiction ends the
   lane with *infeasible by propagation*;
6. runs `apply_indicators`;
7. solves with `node_cutoff = cut_at`. If the solve stops at the cutoff
   and its stop bound meets the gap test against `bk0`, the lane ends with
   *cut off* and that bound. If the bound does not meet the test, the node
   is solved again without the cutoff, as the tree does now (10062–10075).

The record holds:

- the outcome: infeasible by a clique, infeasible by propagation, cut off
  with its bound, solved, or failed;
- the solve's status and its `jaos_status`;
- the work, split into propagation (`PH_PROP`) and the solve (`PH_LP`);
- the iterations, the solves, and the solve's own work `node_work`, which
  the probe cap reads;
- the counts `orbital_fixed`, `clique_fixed` and `tightened`.

The solution, the basis and the kept simplex state stay in position k's
LP.

### What the tree does in round order

The merge of node k is the rest of today's node body, from 10076 to the
children, unchanged. For the merge's duration, the tree's `lp`, `nfixed`,
`nperm` and `in_copy` point at position k's. The merge runs these steps:

1. **Test the node's key.** Test node k's key (its parent's bound) against
   the incumbent now, as 9902 does. If an earlier merge of the round found
   an incumbent that closes the node, the node is freed and not counted,
   and its lane work counts as waste.
2. **Count the node.** `nodes++`, then the record's counters, its log
   lines (*infeasible by a clique*, *by propagation*), and its work into
   the phase sums (section 5).
3. **Test against new permanent rows.** If the log of permanent rows grew
   after lane k started, test the lane's point against the new rows
   (section 2).
4. **Close the node if the newer incumbent does.** If the record says cut
   off, or the lane's objective is closed by the incumbent now, the node
   is a cut-off node: `pseudocost_learn` from that bound, `cutoff_nodes++`,
   and the node ends. In the one-node tree, the newer incumbent's cutoff
   would have stopped that solve. A merge that skipped this step would run
   heuristics and sub-MIPs on a node that the check at 11874 then drops.
   This also counts as waste.
5. **Bookkeeping and the special outcomes.** The node-presolve trial
   bookkeeping, a failed relaxation set aside, infeasibility and its
   conflict, and an unbounded relaxation, all as now.
6. **The rest of the node, as now, on position k's LP:**
   - pseudocost learning from the lane's objective and `select_branch`;
   - the sub-MIPs and dives;
   - the node cuts and their re-solve;
   - the callbacks;
   - rounding, ZI and the scheduled heuristics;
   - reduced-cost fixing against the incumbent now;
   - the integral test;
   - strong branching and orbital branching;
   - the children and the open set.

Strong branching stays in the merge, not on the lane, on purpose. It reads
`pc_n` to choose what to probe, and it learns into `pc_sum` and `pc_n`. On
a lane it would read the pseudocosts the round started with, and two nodes
of one round would probe the same unreliable column twice. In the merge it
reads the pseudocosts that the round's earlier nodes left, as a single
node does now. It already spreads its probes over threads from position
k's kept state (`kept_probes` at 1678, `jm_probe_many`), and it is capped
at a tenth of the tree's work (`MIP_PROBE_SHARE`). Reliability branching
as Achterberg, Koch and Martin define it depends on the counts being
current: a column is probed until it has `MIP_RELIABILITY` readings.

### Why the outcome does not depend on lane timing

- Lane k's record and position k's LP depend only on the snapshot, node k,
  and position k's LP as its previous lane left it. Position k serves the
  k-th node of every round, so its history depends on the tree, not on
  the threads. The state an LP carries between solves matters (the FTRAN
  density estimate in `lu_spare`, the row order after cuts come and go),
  as 02-492 found for the probes, and it is deterministic for this reason.
  The position of a node must never be chosen by which thread is free.
- Threads take positions from a shared counter, as `probe_lane_run` does,
  so which thread runs which position changes nothing. Positions are the
  tasks, and threads only run them. This is the split between
  simultaneous tasks and threads that Berthold, Farmer, Heinz and
  Perregaard describe for Xpress, and here the number of tasks is fixed.
- The merges run in position order on the calling thread.
- Work units are integers, and lane k's record is added at a fixed place
  in a fixed order.
- No callback fires on a lane, so a user's callback sees the same calls
  at every thread count. Today's
  `test_a_round_calls_progress_only_on_the_calling_thread` asserts fewer
  calls on three threads than on one. It becomes an equality.
- At one thread the lanes run one after another on the calling thread and
  give the same records.

## 2. The tree takes the lane's LP as the node's LP

### Positions

The tree holds `MIP_BATCH_MAX` positions, of which a round uses N.
Position 0 is the tree's own LP. Positions 1 to N−1 are copies of it,
made at the first round. Each copy is billed `nnz + nr + nc`, as the
root's MIR probe bills its copy (`docs/work-units.md`). Each position
carries its own `nfixed`, `nperm` and `in_copy`, and the length of the log
it holds. `node_apply` on a position changes only what differs, as it does
on the tree's LP now. The copies are freed with the tree.

Nothing is transferred. During node k's merge, position k's LP is the
tree's LP:

- its solution fills `x` and the reduced costs;
- its `sol_col_status` and `sol_row_status` become the children's basis
  through `node_child` (694), exactly as now;
- its kept simplex state (`node_keep = reliability > 0`) serves the
  strong-branching probes.

Whichever position takes a child warm-starts it from those basis
statuses, as any node is today: the LP refactors from them. Moving the
factor into the tree's LP would first need that LP to hold the node's rows
and bounds, and position k's LP already does. No open node stores a
factor, which at one factor per open node would not fit in memory.

A tree whose round is 1 uses position 0 alone, so the default tree is
today's tree.

### Permanent rows

`steer_flush` (8571) adds rows for the whole tree: conflicts from
infeasible nodes, and rows that a user callback adds. Today it adds them
to the one LP. With positions, every permanent row goes through a log, a
row buffer in the tree that is appended in merge order. The invariant is
that **a position's permanent rows are always a prefix of the log**.

- `steer_flush` on position k first brings the position up to the log,
  then appends to the log and to position k.
- A position catches up by the steps `steer_flush` takes now (delete its
  cut rows, add the missing permanent rows, add its cut rows back), billed
  the same way, `rb->nnz + nc`.
- A node solved on a position that was behind keeps an `nperm` smaller
  than the tree's. `node_apply` already handles a node whose `nperm` is
  below the LP's, by marking the rows in between as basic (833–846).

A lane brings its position up to the log before `node_apply`, so a lane
misses only rows that the merges of its own round add. Before node k's
merge reads its point, it tests that point against those rows, billing
their nonzeros. If the point satisfies them, it is optimal for the LP with
them too, and the merge goes on with position k as it is. If it violates
one, the merge brings position k up to the log and solves node k again
there, billed and counted as a re-solve. This matters for correctness. A
user's lazy row, once added, must hold at every later node. A conflict row
that a later node violates marks a node the conflict proves infeasible.

### The probes' spare states

`jm_probe_many` keeps up to `PROBE_LANES_MAX − 1` cloned simplex states
on the LP it probes (`probe_spare`). With N positions there would be N
sets. The tree keeps one set and lends it to position k for node k's
probes. The loan is done in `src/mip.c`, so `src/simplex.c` does not
change. The spare re-clones when the rows differ, as it does now between
nodes with different cut lists.

### Memory

Per open node, nothing changes: the basis statuses (`nc + nr` values of
`jaos_basis_status`), 25 bytes per fix and 8 per cut.

Per round, N LPs stay alive for the whole tree. The estimate below comes
from the layout of `jaos_model_copy` and `SX_ARRAYS` with every element at
8 bytes, which is an upper bound. Per position:

- the model: about 32 bytes per nonzero, and 100 per row and per column;
- its kept simplex state: 40 bytes per nonzero, 400 per row, 220 per
  column;
- a second state of the same size while its probes run (the template);
- the factor;
- the presolved node LP, while node presolve is on.

That is about 110 to 150 bytes per nonzero and 1 kB per row and column,
per position, or N times that per round. The 30 instances of the 2017 set
have at most 2500 columns, a few MB per position. The 240-instance bench
reaches 1.19e7 nonzeros, rows and columns for `neos-3402454-bohle`, past
1 GB per position. Rounds therefore have a size cap (section 4). The
implementation reads peak RSS (`/usr/bin/time -f %M`) at batch 1 and at
batch 8.

### What goes away

These go away: `bb_round_solve`'s copies, `bb_lane_run`, the per-round
`jm_thread_start`, `bb_take_basis` on the normal path, and the second
solve. The lanes run on one `jm_pool` per tree: the one `kept_probes`
already starts (`sb_crew.pool`), created with
`min(threads, max(N, 2 * MIP_STRONG_CANDIDATES)) − 1` workers. A
position's lane runs on any of them.

## 3. Which nodes a round takes

A round takes nodes as now: `open_pick` in pick order, which is the best
estimate with a best-bound pick every `MIP_ESTIMATE_BOUND_EVERY` (5)
picks. The pick drops a node that the incumbent closes (9879). A round
takes N nodes, or fewer when the open set holds fewer or a limit is near
(section 5). The merge order is the pick order.

The waste comes in three kinds:

- **Nodes closed after their lane.** An earlier node of the same round
  finds an incumbent that closes the node at its merge (merge steps 1 and
  4). Each improvement of the incumbent wastes at most N − 1 lanes. The
  2017 records show 0 to 22 heuristic incumbents per tree (`heur=` in
  `bench/results/miplib2017.txt`), most of them over thousands of nodes.
  The
  implementation counts this waste in the summary as `rounds: …, … nodes
  closed after their lane, … work units in them`.
- **Lost cutoffs.** A lane solves to optimality a node that the newer
  incumbent's cutoff would have stopped early, so those pivots are not
  saved.
- **Search order.** A node's children enter the open set after the rest
  of its round, so up to N − 1 picks happen before them.

The third kind moves the tree rather than the work per node, and it is
the one 02-492's estimate measured: 0.52x to 1.61x per instance at batch
8, and 1.035x geometric. Lodi and Tramontani report trees with that kind
of spread from changes as small as a permutation of the rows. The estimate
is therefore read in the mean, and per instance only against the gate's
2x.

Considered and not taken:

- **Nodes far apart**, one per subtree or every k-th of the open set.
  With the incumbent fixed, best-first search opens the nodes whose bound
  is under it in any order. Spreading a round apart buys only incumbents
  found sooner, and it costs nodes that the one-node search never opens:
  the anomalies of Lai and Sahni, and of Li and Wah. The heuristics
  already look for incumbents.
- **Best bound plus dives.** Lanes that each plunge several levels make a
  subtree per lane, as in ParaSCIP and FiberSCIP, with branching done on
  the lane from stale pseudocosts. That would also parallelize the
  merge's work. Clausen and Perregaard found depth-first variants ahead of
  best-first in parallel because they find incumbents sooner. This is
  stage 4, and the no-incumbent dive after `MIP_NOINC_DIVE_AFTER` nodes is
  its first use.
- **Merging integral nodes first.** A lane can tell that its point is
  integral and satisfies the SOS sets and indicator rows. Merging those
  nodes first, then the rest in pick order, lets the new incumbent close
  the rest before their heuristics, probes and children run. It is
  deterministic and nearly free, and it is read in stage 3 on its own.

A round never crosses a boundary that the one-node tree checks between
nodes:

- **The node limit.** `want` already shrinks to it. The same goes for the
  lattice tree's soft node limit `mip_node_soft` when an incumbent exists.
- **A dive.** A round never forms while `next` or a resumable dive stack
  is in play (9842–9852). If a merge sets `next`, for example when the
  no-incumbent dive starts at node 1000, the dive runs on position 0. The
  round's other nodes wait with their records still valid, as round nodes
  wait today.
- **An MIQP.** `want = 1`, as now.

If the tree stops at a limit during a round, the unmerged nodes are freed
at the end, as now (12473). Their keys are in `best_bound`
(`bb_round_bound`), which the final bound includes (12277–12281). That is
the guard 02-264 added to the conic tree.

## 4. The round size and when rounds run

N is `MIP_ROUND_NODES` = 8, the same on every machine. Threads beyond 8 do
not help the lanes, though they still help the probes, up to 16. On fewer
threads the positions queue. The choice of 8 has two reasons:

- 02-492's estimate reads 1.035x at 8 against 1.13x at 4, both inside the
  spread of the tree's shape.
- The bound on the speedup grows little past 8. With the merge's share s,
  the bound is `1 / (s + (1 − s)/N)`. At s = 0.28 (`misc07`) it reads 2.7x
  at 8 and 3.1x at 16.

The setting `mip_tree_batch` takes three kinds of value:

- 1: one node at a time;
- N ≥ 2: rounds of N from the first node after the root (today's meaning);
- 0 (stage 2, the new default): rounds of `MIP_ROUND_NODES` once the
  tree's own readings call for them.

The automatic rule switches rounds on at the first pick where all of
these hold:

1. at least `MIP_ROUND_MIN_NODES` (20) nodes past the root are solved;
2. their relaxations have spent at least `MIP_ROUND_TRIAL_WORK` (5e7)
   work units, read as the `PH_LP` sum after the root;
3. their mean work per node is at least `MIP_ROUND_NODE_WORK`;
4. the model's `nnz + nr + nc` is under `MIP_ROUND_SIZE`, so that N
   positions fit in memory (section 2);
5. the tree is neither a sub-MIP (`cfg.mip_sub`) nor an MIQP.

Once on, rounds stay on. Every input is a work unit, a count or the
model's size, never the thread count or a clock. The values in brackets
are starting points for the sweep. The sweep sets `MIP_ROUND_NODE_WORK`
(section 7), and each constant goes into `docs/tolerances.md` with the
reading that set it.

The trial in work (condition 2) keeps small trees off. `p0201` (80 nodes)
and `khb05250` (59) spend under 2e7 work units in their relaxations, root
included, so they never reach it, and they are where rounds move the tree
most (`p0201` reads 1.61x at batch 8). A trial counted in nodes alone
would also keep the most expensive nodes off: `csched008` spends 6e7 per
node and ends at 157 nodes at 1e10, and `ic97_potential` ends at 487.

With one thread the same rule gives the same rounds, run one lane after
another. Three things keep one thread near 1.0x on MIPLIB 3:

- No node is solved twice, and the lane's solve has the fixings and the
  cutoff that the one-node solve has.
- On MIPLIB 3 the rule fires only where nodes are expensive. Rough
  relaxation work per node (the baseline's work over its nodes, times
  02-492's `lp` share, which includes the root): `l152lav` 9.3e5,
  `dcmulti` 2.4e5, `misc07` 1.3e5, `blend2` 5.3e4, `stein45` 3.1e4,
  `bell3a` 1.2e4, `bell5` 1.0e4. At `MIP_ROUND_NODE_WORK` = 1e5 the rule
  fires on `l152lav` after about 54 nodes and on `misc07` after about 376.
  `dcmulti`'s 267 nodes may reach the trial only near their end. At batch
  8 from the root, without the second solve, those two read 1.11x and
  0.73x, which is 0.99x geometric over the 24 if nothing else moves.
- On the 2017 set the rule fires where nodes are expensive. By the same
  rough reading of the records at 1e10, it fires on 13 of 30:
  `neos-2657525-crna`, `assign1-5-8`, `neos-911970`, `mad`, `tr12-30`,
  `neos-3627168-kasai`, `binkar10_1`, `csched007`, `csched008`,
  `ic97_potential`, `beasleyC3`, `exp-1-500-5-5` and `p200x1188c`. Their
  records at 1e10 move with the tree's shape and are judged instance by
  instance against 02-474's spread.

The sweep may show rounds paying in wall time at `MIP_ROUND_NODE_WORK` =
0. In that case the rule keeps only conditions 1, 2, 4 and 5. That is the
"round cheap enough where a node takes a few pivots" that SPECS still
lists, and its bar is the same 1.0x.

## 5. Work under a work limit

Let L be the work limit and W0 the tree's work at the round's start.

- **Each lane's limits.** Lane k's solve gets `max(1, (L − W0) / n − p_k)`
  work units, where n is the round's size and p_k is the lane's
  propagation work. The n lanes then spend at most `L − W0` together, and
  for n = 1 this is the budget `budget()` sets now. The time limit is
  `time_left`, as now.
- **Billing.** The tree adds every lane's work when the last lane
  returns, in position order: `phase_to(PH_PROP)` and the p_k, then
  `phase_to(PH_LP)` and the solve work, then `phase_to(PH_OTHER)`. Every
  check in the merges (`budget_gone`, the probe share, the sub-MIP share,
  `heur_stop`) sees that total, which is the same at every thread count.
- **A lane stopped by its share.** If a lane's solve stopped at its share
  while the total is still under L, the lane is solved again in its merge
  with what is left, billed and counted as a re-solve. Once the total
  reaches L, the tree stops at the next node's check, as now.
- **Nothing is unbilled.** The round's work is reported whole. A lane that
  ran is billed even when its node is closed or left unmerged.

The tree can pass L by one merge, as it can pass it by one node now.
`work_units` is the same at every thread count, and `docs/work-units.md`
loses its paragraph on the second solve.

## 6. The other pieces

| piece | under rounds |
|---|---|
| sub-MIPs (02-488) | run in the merge, in order, under the same `MIP_SUBMIP_EVERY` and `MIP_SUBMIP_SHARE` tests. The nested tree has `cfg.mip_sub` and keeps one node at a time (condition 5). It may start its own probe pool while the tree's workers sleep. Handing it the tree's pool is stage 3 |
| ZI rounding (02-489) | runs in the merge. It reads `lp->sol_row`, which is position k's. It moves to the lane in stage 3, reading the scheduler's counts from the snapshot |
| threaded probes (02-492) | run in the merge, on position k's kept state, with the spare lent (section 2) and the tree's one pool. The cap reads the lane's `node_work` |
| the time limit (02-494) | each lane's solve gets the time left, and the merge reads the clock at every node. A lane stopped by the clock ends the tree at its merge, with its round's keys in the bound. A run that a time limit stops is not the same at every thread count, now or after this change |
| the lattice tree (02-476, 02-478) | the first tree's soft node limit bounds a round (section 3). The reduced tree's soft work check runs at each merge, and `m2` inherits the setting |
| orbital branching | orbital fixing runs on the lane, reading `sym`. The widening at branching runs in the merge |
| restarts | the root restart runs at node 1, before any round. In-tree restarts are off (`MIP_RESTART`), and one would free the round as a stop does |
| the conic tree | unchanged: its rounds already solve each node once, cold. `--tree-batch N` still sets both trees. The automatic rule belongs to the linear tree, and 0 means one node at a time in the conic tree, for 02-264's reason |
| conflicts, lazy rows, callbacks | run in the merge, with rows going through the log (section 2) |
| the node-presolve and node-MIR trials | are decided over the first 50 nodes, before the rule can fire. Under an explicit batch the lanes read the settings as they stand at the round's start |

## 7. Verification

**Stage 1, to land with the default untouched:**

1. `make test && make sanitize && make tsan`, and `make python-test`.
2. The default reads its records. Run `make netlib netlib-infeas J=12`,
   then `make netlib-kennington J=2`, then `make miplib J=2`: each reads
   its baseline, with 0 at other nodes. `make miplib2017` is
   byte-identical to `bench/results/miplib2017.txt`. The refactor into the
   lane function is committed first, and it must pass this check on its
   own. `src/simplex.c` does not change, so the primal, barrier, PDLP,
   concurrent and warm readings are not re-taken.
3. The determinism matrix at `-O mip_tree_batch=8` and at 3: `bench/run
   -j 2` at `-O threads=1`, 2, 4 and 12, on MIPLIB 3 to completion and the
   2017 set at 2e9. Every record is byte-identical to the one-thread
   record, as in 02-492's `determinism.txt`.
4. The tests:
   - `test_mip.c` solves at batch 8 on 1, 2 and 3 threads with the lanes
     forced on, and compares the objective's bits, the nodes, solves and
     work, and the point;
   - a model whose conflicts appear mid-round, so that positions fall
     behind the log;
   - a lazy-row callback whose row an earlier node of the round adds;
   - a work limit that stops a round: the bound is at most the optimum and
     the status is `work_limit`;
   - the progress-call count is equal at 1 and 3 threads;
   - `tests/cli.sh` compares `--tree-batch 8` at `--threads 1` and 4;
   - `python/test_jaos.py` does the same at both layers.
5. MIPLIB 3 at batch 8, one thread, against `bench/miplib.baseline`: 24
   solved, no `REGRESSED` line, re-solves at most 1% of round nodes, and
   work at most 1.21x geometric (today's rounds). Stage 1 lands if it does
   no worse than today's rounds. Going on to stage 2 needs items 6 to 8.

**To go on to stage 2:**

6. MIPLIB 3 at batch 8, one thread: work at most 1.05x geometric (the
   estimate is 1.035x). Between 1.05x and 1.10x, read the waste counts
   before stage 2. Past 1.10x is a no-go: find the cause first.
7. The 2017 set at 1e10, batch 8, one thread: 9 solved, 29 with an
   incumbent, and `m17sum.py`'s sum at most 11.88, the top of 02-474's
   spread (it is 11.34 now).
8. The speedup, on a quiet machine: after the 240-instance run, with the
   load under 1. Use 02-492's `speedup.sh` with `--tree-batch 8` passed
   through (a copy in the implementation's folder with an `ARGS`
   variable), at `THREADS="1 2 4 8"` and 1e9:
   - every status, objective, work, nodes, bound and incumbent line is the
     same at every count;
   - 8 threads against 1: at least 1.8x geometric over the list, and at
     least 1.3x on each instance whose node relaxations are 70% or more of
     its tree. Under 1.4x geometric is a no-go for stage 2, and stage 3
     comes first;
   - 4 threads against 1: at least 1.5x geometric;
   - 1 thread at batch 8 against 1 thread at batch 1: the wall-time ratio
     at most 1.10x the work ratio, since the positions cost little at one
     thread;
   - peak RSS at batch 8 at most 1.5x batch 1 on the list.

   The bound for each instance, from 02-492's shares with the merge's
   share s = 1 − lp − prop and perfect balance, is 2.4x (`binkar10_1`) to
   4.1x (`stein45`) at 8 threads.

**Stage 2, the default:**

9. Sweep `MIP_ROUND_NODE_WORK` over 0, 2.5e4, 5e4, 1e5, 1.5e5 and 3e5
   with `speedup.sh` at 8 threads. Take the smallest value at which no
   instance the rule turns on reads below 1.0x.
10. MIPLIB 3 at the default: no `REGRESSED` line, at most 1.02x geometric,
    none past 1.5x, and the summary names where the rule fired. The 2017
    set as in item 7, and the determinism matrix (item 3) at the default.
    Run `make cblib` if `conictree.c` changed.
11. `speedup.sh` at the default thread count: at least 1.8x geometric at 8
    threads on the instances where the rule fires.

## 8. Stages

**Stage 1: rounds solve each node once, under `--tree-batch`.** It lands
on its own.

1. Move the block from `node_apply` to the end of the solve (9954–10075)
   into one function that writes a record. The one-node path calls it in
   place. Verify with item 2: no reading moves.
2. Add the positions, the log of permanent rows with its prefix
   invariant, the spare lent to the probes, and the pool shared with the
   probes. The lanes take positions from a counter, with callbacks and
   logs off on lanes.
3. Run the merge on position k's LP. Add the test of the lane's point
   against the log's new rows, and the re-solve when a share stopped the
   lane or a row is violated.
4. Delete the copies and the second solve.
5. Add a summary line: rounds, round nodes, nodes closed after their lane
   and their work, re-solves.
6. Add the tests (item 4). In the same commit, update
   `docs/work-units.md`, `docs/api.md` (Threads), `docs/cli.md`
   (`--tree-batch`, `--threads`), `docs/tolerances.md` (`MIP_BATCH_MAX`'s
   reason) and the SPECS row "Deterministic parallel tree search".

Verify with items 1 to 5, and read items 6 to 8. Stage 1's readings go
below this design in this README.

**Stage 2: rounds by default.** Make `mip_tree_batch` 0 the default, with
the rule of section 4, its constants and the size cap. That touches:

- the C API, where 0 is accepted and becomes the default;
- the CLI: `--tree-batch 0`, which `tests/cli.sh` refuses at line 984
  today;
- both Python layers;
- `test_conic.c`'s refusal of 0 (line 1939);
- `docs/`.

Verify with items 9 to 11. The SPECS row "Parallel MIP solve" loses its
"Missing".

**Stage 3: more of the node on the lane.** This comes first if item 8
shows the merge's share limiting the speedup. Each piece is read on
MIPLIB 3 and the 2017 set as stage 1 is:

- the node cuts (local Gomory to depth 3, node MIR) and their re-solve
  move to the lane. The cuts enter the pool in round order, and `mir_live`
  is read from the snapshot;
- the lane computes the rounding and ZI candidates, which are offered in
  round order;
- integral nodes are merged first;
- the sub-MIPs get the tree's pool.

**Stage 4, if the probes or the search order still limit the speedup:**
probes on the lanes from the snapshot's pseudocosts, merged in order; and
rounds of dives, each lane plunging its own node, first for the
no-incumbent dive.

## Papers

- T.-H. Lai and S. Sahni, Anomalies in parallel branch-and-bound
  algorithms, Communications of the ACM 27(6), 594–602, 1984. With k
  nodes at a time a branch and bound can open nodes the sequential search
  never opens, and can take more or less than 1/k of its time. It is why
  the round's 1-thread work is a bar of its own, and why far-apart rounds
  are not taken.
- G.-J. Li and B. W. Wah, Coping with anomalies in parallel
  branch-and-bound algorithms, IEEE Transactions on Computers C-35(6),
  568–573, 1986.
- J. Clausen and M. Perregaard, On the best search strategy in parallel
  branch-and-bound: best-first search versus lazy depth-first search,
  Annals of Operations Research 90, 1–17, 1999. In parallel, the
  depth-first variants beat best-first on bound computations because they
  hold better incumbents sooner. This is the case for stage 4's dives.
- Y. Shinano, T. Achterberg, T. Berthold, S. Heinz and T. Koch, ParaSCIP:
  a parallel extension of SCIP, Competence in High Performance Computing
  2010, Springer, 135–148, 2012; and Y. Shinano, S. Heinz, S. Vigerske and
  M. Winkler, FiberSCIP — a shared memory parallelization of SCIP, INFORMS
  Journal on Computing 30(1), 11–30, 2018. Both keep a subtree per solver
  and split the tree dynamically. FiberSCIP has a deterministic mode. It
  is the shape of stage 4, not of stage 1.
- T. Berthold, J. Farmer, S. Heinz and M. Perregaard, Parallelization of
  the FICO Xpress-Optimizer, Optimization Methods and Software 33(3),
  518–529, 2018. A deterministic tree search that separates the
  simultaneous tasks from the threads that run them. Here the tasks are
  the round's positions, with their number fixed.
- T. Ralphs, Y. Shinano, T. Berthold and T. Koch, Parallel solvers for
  mixed integer linear optimization, Handbook of Parallel Constraint
  Reasoning, Springer, 283–336, 2018; and T. Koch, T. Ralphs and Y.
  Shinano, Could we use a million cores to solve an integer program?,
  Mathematical Methods of Operations Research 76(1), 67–93, 2012. Both
  name the overheads of a parallel tree: search (the waste counts of
  section 3), idle time (lane imbalance and the merge) and communication
  (none here, since the lanes share memory and merge at one barrier).
- T. Achterberg, T. Koch and A. Martin, Branching rules revisited,
  Operations Research Letters 33(1), 42–54, 2005; and T. Achterberg,
  Constraint Integer Programming, PhD thesis, TU Berlin, 2007.
  Reliability branching, whose counts must be current, is why strong
  branching stays in the merge.
- A. Lodi and A. Tramontani, Performance variability in mixed-integer
  programming, Tutorials in Operations Research, INFORMS, 1–12, 2013.
  Trees that move by such factors from small changes are why the round's
  work is read in the geometric mean, and per instance only against 2x.
