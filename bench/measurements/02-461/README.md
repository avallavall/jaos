# 02-461 — strong-branching children on the node's kept factors

Taken on 2026-11-16 for TODO row J7, on the tree of 709228b with this
change. `read.sh` takes the landed readings with HEAD's `bench/run`, one
solve at a time: `miplib3/kept.txt` and `full/kept.txt`. `full/base.txt`
and `full/rel4cap1.txt` are 02-459's records, whose library is the same
as 709228b's. The other records come from scratch builds described below.

## What was built

TODO J7 and refusal D293 named the step: strong-branching probes as a
few dual steps on the node's factors, since each probe was a full LP
solve, presolve and factorisation included. Now, under
`--reliability N`:

- When a node's relaxation ends optimal in the dual simplex, with no
  aggregation and no primal pass, the simplex keeps its state: the basis,
  the factors, the primal and dual values, and the presolved model when
  the node was presolved (`jm_kept` in `src/simplex.c`).
- Each probe (`jm_probe`) restores that state and changes the one
  column's bound in the reduced model, clipped to what presolve
  tightened. It runs the dual simplex from there, with a work cap counted
  from the probe's own start. Neither presolve nor the node's solve is
  repeated, and the node is not solved again after its probes.
- A probe that ends optimal gives the child's objective. A probe stopped
  by its cap gives the Lagrangian bound of its last basis (02-459's
  `stop_bound`). A column that the node's presolve removed is probed by
  a solve of its own, as before.
- Any change to the model drops the kept state.
- Strong branching skips a node while it has taken more than
  `MIP_PROBE_SHARE` (0.1) of the tree's work.

`tests/test_mip.c` checks these, with and without presolve at the node:
- every kept probe gives the status and bound of a fresh solve of the
  same child;
- a capped probe's bound is valid;
- the node's own answer does not move;
- a limit left on the node's model does not stop a probe;
- a column removed by presolve is handed back.

The defaults do not change: reliability stays 0.

## MIPLIB 3, against `bench/miplib.baseline`

Every instance at its optimum, under `--reliability 4 --probe-cap 1`:

| build | total work | nodes | past 2x |
|---|---|---|---|
| HEAD's probes, full solves (02-459) | 1.155x | 174,497 to 139,318 | `p0201` 4.7x, `l152lav` 2.8x, `misc03` 2.6x, `mod008` 2.2x, `enigma` 2.1x |
| kept factors (landed) | 0.796x | 174,497 to 163,310 | `misc03` 2.9x |

The kept probes take far less on `gt2` (447 nodes to 5, 0.14x), `blend2`
and `p0282` (0.56x), `misc07`, `dcmulti` and `l152lav` (0.65x to 0.72x).
They take more on `mod008` and `p0201` (1.5x) and on `misc03`, whose tree
grows from 248 nodes to 622. `misc03` is past 2x under every probing
form read so far (02-459: 2.6x to 4.2x), so the probes stay behind the
switch.

## MIPLIB 2017, 30 instances at 1e10 work units

02-328's `m17sum.py`, lower is better:

| build | primal | dual | sum |
|---|---|---|---|
| base (default, no strong branching) | 7.0744 | 6.5961 | 13.6705 |
| HEAD's probes, full solves (02-459) | 9.2592 | 6.5275 | 15.7867 |
| kept factors (landed) | 6.8427 | 6.5188 | 13.3615 |

The kept probes beat base by 2.3%, and the dual side by 1.2%. Of J7's
seven instances that hold the optimum as incumbent, none closes:

- The dual gap narrows on `pk1` (0.256 to 0.204), `mas76` (0.0073 to
  0.0058), `gen-ip054` (0.0062 to 0.0057), `assign1-5-8` (0.0935 to
  0.0926) and `binkar10_1` (0.0036 to 0.0035).
- `neos5` and `gen-ip002` read the same.
- `gen-ip054` loses its optimal incumbent (primal 0.0026).

`timtab1`'s incumbent comes from 0.68 to 0.41 and its dual gap from 0.295
to 0.263. `neos-3754480-nidda` (0.028 to 0.141) and `graphdraw-domain`
(dual 0.295 to 0.316) read worse.

## The first reading, and what it missed

The first build of the same idea read MIPLIB 2017 at 14.7393, worse than
base, and MIPLIB 3 at 0.875x with no instance past 2x (`full/first.txt`,
`miplib3/first.txt`). The check below found every probe it finished right.
It also found three kinds of probe that taught nothing, or taught a false
zero:

- **The cap was read against the node's own limit, from the node's
  start.** Each probe adds to that count. In a tree with a work limit, a
  heuristic's sub-MIP or the main tree near its budget, the later probes
  of a node stopped before their first iteration. Their bound was the
  parent's, and their pseudocost learnt a gain of zero.
- **A column the node's presolve had removed was not probed at all**: in
  the check's solve of `blend2`, 24,568 times.
- **A probe that ended optimal was dropped** when a reduced cost within
  tolerance pointed at an infinite bound, so the Lagrangian bound was
  undefined.

The landed build fixes all three. It reads 13.3615 and 0.796x, and
`misc03` passes 2x again.

## Two ablations

- **Branching at once on a column whose probe child is infeasible,** on
  the first build: MIPLIB 3 goes from no instance past 2x to `enigma` at
  2.4x (895 nodes to 4,438), `miplib3/first-forced.txt`. It is not landed.
- **Skipping, rather than solving, the probes of columns the node's
  presolve removed,** on the landed build: MIPLIB 3 goes from 0.796x to
  0.822x (`miplib3/kept-no-fallback.txt`), all of it `blend2`. `misc03`
  reads the same 2.9x, so its growth is the probing itself.

## The check

`check.patch` (`patch -p0`, then `JAOS_KEPT_CHECK=1`) runs at every kept
probe an uncapped kept probe, and a fresh solve of the same child on a
copy of the node's model. It compares the two.

On 15 MIPLIB 3 instances under `--reliability 4 --probe-cap 1`, built
with assertions:
- 5,907 probes, none of them disagreeing on status, or on the bound by
  more than 1e-6 relative, and none left undecided.
- 2,233 capped probes, each with a bound on the correct side of the
  child's optimum.

The check changes the trees of `p0282` (979 nodes against 891) and
`bell5` (20,451 against 20,459). A probe refactors from the LU the last
probe left, whose sparsity estimates and kept pattern are its own, so an
extra probe moves the later ones. This is deterministic, but not free of
history.

On `blend2`, the check's tree reaches a node relaxation whose postsolve
trips the debug-build check on row activities
(`ps_verify_row_activities`). That is a node solve, not a probe. The
same build without the check, and the ASAN build, solve `blend2` in the
landed 4,212 nodes.

## The verdict

Landed behind `--reliability`, where it replaces the full-solve probes:
on both sets it beats the probing it replaces. It stays off by default
because `misc03` takes 2.9x the work. `bench/refusals.txt`'s D293 says
what reopens it as the default.
