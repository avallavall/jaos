# 02-09 — presolve reads the model's sense, and three windows stop judging

What this directory decided (D103):

1. Presolve never read `m->sense`, so every cost-direction and dual-sign rule
   in it was inverted on a MAXIMIZE model.
2. `PRESOLVE_TIGHTEN_EPS` was the window at three sites that each ask whether
   a residue is rounding. It is `PRESOLVE_ROUND_ULPS` at all three now, and
   the old constant is deleted.
3. What presolve is worth, in work units and in seconds, with a negative
   control beside the seconds.

Tree: `3f8f4e5` for the campaign and the timing. The probes and the residue
run were taken on the same tree.

## `probes/` — the five models

Each is the minimum case for one defect. Build both ways from the repo root,
and the two binaries must differ before either is believed:

```
FLAGS="-std=c23 -ffp-contract=off -O2 -g -Iinclude -Isrc"
gcc-14 $FLAGS <probe>.c src/*.c -o /tmp/on  -lm
gcc-14 $FLAGS -DJAOS_NO_PRESOLVE <probe>.c src/*.c -o /tmp/off -lm
```

| probe | before the fix | after | reference |
|---|---|---|---|
| `maxprobe` maximise | dual `[2, 0]`, redcost `[1, 0]` | `[2, 1]`, `[0, 0]` | `[2, 1]`, `[0, 0]` |
| `maxprobe` empty col `[0,5]` | OPTIMAL obj 0 | obj 5 | obj 5 |
| `maxprobe` empty col `[-inf,5]` | UNBOUNDED | obj 5 | obj 5 |
| `probe_emptyrow_window` gap 0.5 | OPTIMAL, rowviol 0.5 | INFEASIBLE | INFEASIBLE |
| `probe_emptyrow_window` gap 1.5 | OPTIMAL, rowviol 1.5 | INFEASIBLE | INFEASIBLE |
| `probe_collapse_window` scale 1e6 | OPTIMAL, colviol 2.5e-4 | INFEASIBLE | INFEASIBLE |
| `probe_collapse_window` scale 1e9 | OPTIMAL, colviol 0.2 | INFEASIBLE | INFEASIBLE |
| `probe_frozen_window` | assert `want_lo <= want_hi`, or OPTIMAL with colviol 0.5 under `-DNDEBUG` | INFEASIBLE | INFEASIBLE |

`canary_ulps.c` is not a defect probe. It is the sweep's canary: four fold
conflicts against a column bound of 1e9, chosen so that adjacent settings of
`PRESOLVE_ROUND_ULPS` produce different verdict strings.

## `residues/` — why 8 ulps, measured rather than argued

`diag_patch.py` patches a COPY of `src/presolve.c` and prints, at each of the
three window sites, the residue that site is about to judge divided by
`DBL_EPSILON * scale`. The printed number is therefore the residue in ulps of
that site's own scale, directly comparable with the ulp count in the window.
The repository is never modified. Build outside it.

Checks that the instrument reached the binary: all three probe strings present
in the built binary, and a non-zero probe count over the full set. An earlier
version checked `afiro` alone and refused the run, correctly — `afiro` reaches
none of the three sites, and an empty run is not a reading.

32240 probe lines over the three sets:

| site | lines | residue > 0 |
|---|---|---|
| emptied row | 13150 | 0 |
| fold collapse | 8 | 8 |
| frozen row | 19082 | 4 |

`positive-residues.txt` holds all twelve. Each appears twice because the
runner re-solves for the determinism check, so they are six distinct sites,
and **every one is on `netlib-infeas`**. The smallest is 3.69e8 ulps.

The probe measures the violating direction only, so a feasible row reads
exactly 0 by construction. What the run establishes is therefore:

> No feasible model on these 139 puts a residue anywhere in the interval
> (0, 3.69e8 ulps] at any of the three sites. The window may be set anywhere
> in that interval and no verdict on this population changes.

Evidence on both sides, which is what the constant needed. Too tight: nothing
to hit. Too loose: the smallest real violation clears 8 by 7.7 decades.

## `sweep/table.txt` — PRESOLVE_ROUND_ULPS

`make clean` between every setting. Columns are the ones the two sweeps in
`src/presolve.c` already report.

```
ulps  canary  instances solved obj_ok checker_ok rows_removed cols_removed
1     RRRR    94        94     94     94         7598         24695
2     ARRR    94        94     94     94         7598         24695
4     ARRR    94        94     94     94         7598         24695
8     AARR    94        94     94     94         7598         24695
16    AARR    94        94     94     94         7598         24695
64    AAAR    94        94     94     94         7598         24695
256   AAAA    94        94     94     94         7598         24695
```

Flat across all seven, and the flatness is a reading rather than a broken
instrument for two separate reasons. The canary flips four times inside the
grid. And the seven binaries have seven distinct md5s, which is the check that
matters: the canary's four conflicts do not separate 2 from 4 or 8 from 16,
because the grid steps by 2 in places and by 4 in others.

## `campaign/` — the three sets, both settings

`on-*` is the shipping build, `off-*` is `EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE`,
`make clean` between them, `J=12`.

**The change is a no-op on the gate.** Every `on-*` record is bit-identical to
the record committed at `3f8f4e5`: 94, 29 and 16 instances, 0 digests moved.

**The control passes.** Every `off-*` record reads `0 regressed, 0 improved,
0 new` against its committed baseline, which predates presolve. That is D96's
requirement met.

**What presolve is worth.** `work-*.txt` carry the per-instance ratios;
geometric means, `on/off`, so below 1.0 means presolve saves:

| set | work | iterations |
|---|---|---|
| netlib | 0.810x | 0.867x |
| netlib-kennington | 0.651x | 0.832x |
| netlib-infeas | 0.084x | 0.241x |

The infeasible figure is not a speed claim about the simplex: ten of those
models are decided inside presolve and the simplex never starts.

**Every iteration ratio in that column was taken with `geomean.py
--plus-one`, and on the infeasible set the convention decides the answer.**
Ten of the 29 finish inside presolve at 0 iterations, which a plain ratio
cannot divide by, so without the flag the script drops them and reports
0.9667x over the remaining 19. 0.241x is the honest number because it keeps
all 29 and the ten are the whole point of the reduction; 0.9667x is a
statement about the instances presolve did not decide. The two are far enough
apart that quoting either without naming the convention is a defect, and it
was one here until an independent re-read asked which had been used.

**Two instances get much worse, and it is presolve, not this change.**
`grow22` 11.16x work and 2179 to 16381 iterations; `grow7` 8.56x and 544 to
4804. The `on-*` records are bit-identical to what was already committed, so
these arrived with presolve. Handed to `TODO.md`.

Those four counts were each one too high in five documents until an
independent re-read caught them. `geomean.py --plus-one` adds one to every
iteration count before taking the ratio, and the shifted values were copied
out of its table as though they were the raw counts. The ratios were never
wrong; only the counts beside them were.

## `timing/` — seconds, and the control that qualifies them

`-j 1`, both binaries in one session, minimum over three alternating rounds.
`pairs-*.txt` are `name off_s on_s`, the input `geomean.py --pairs` takes.

| | instances | geometric mean | range |
|---|---|---|---|
| movers | 6 | **0.2915x** | 0.1884x to 0.4302x |
| control | 4 | **0.9934x** | 0.9814x to 1.0031x |

The control instances are `maros-r7`, `truss`, `degen3` and `sc205`, whose
work ratio is 1.0000x to four decimals — presolve provably cannot speed them
up, so whatever they read is this host in this session. Every mover is far
outside that band.

**The 0.2915x is not a whole-set figure and must not be quoted as one.** The
six were chosen because presolve removes the most there. The whole-set number
is the work ratio above.
