# 02-122 — the relative pivot floor in the two primal ratio tests, swept

## The question

`TODO.md` §0 stage 8. Both primal ratio tests accept a blocking row when
`|col[i]| >= PIVOT_MIN`, an **absolute** 1e-9. `bench/measurements/02-120/`
showed what that costs on `pilot87`: the FTRAN returns -1.59e-07 for an entry
that is structurally exactly zero, the ratio test takes that row, the pricing
row reports the truth, and the solve refuses in its own words — *"column 478
prices at 0 in row 790 of the primal phase 1 on a freshly built
factorization; this is a JAOS defect"*. It is the one `ERROR` of the 94.

The shape the stage proposes is a floor relative to the magnitudes the
computation carried, the way `can_move` uses
`NOISE_MARGIN * DBL_EPSILON * column_traffic` on the dual side:

```
min_pivot = max(PIVOT_MIN, PIVOT_MARGIN * DBL_EPSILON * max_i |col[i]|)
```

Its constant is what this directory measures, on both sides.

## The instrument — `census-pivot-scale.sh`, `census.txt`

For every call to either ratio test that chose a row, a `JAOS_DIAG` build
forms

```
r = |col[best]| / (DBL_EPSILON * max_i |col[i]|)
```

and keeps the per-solve minimum and a log10 histogram. **A solve's trajectory
changes under a floor of `C` if and only if that solve's minimum `r` is below
`C`**, so one census sweeps every `C` at once instead of one campaign per
value — the same shape D151 used to predict 103 of 103.

Four slots per solve: dual or forced-primal, crossed with the phase-1 ratio
test and the phase-2/cleanup one. Nothing is billed to the work accumulator
and no solver state is touched; the patch lands in a worktree of HEAD.

### What it found — the gate barely reaches this code

`primal_ratio_test` is reached on the standard gate set by **three instances
of 94**: `wood1p` (169 calls), `pilot87` (1), `etamacro` (1). Their minima are
5.4855, 3.26e+14 and 2.03e+15. On `netlib-infeas` and `netlib-kennington`
(`census-gate-sets.txt`) **it is reached by none at all**.

So **no value of `PIVOT_MARGIN` below 5.4855 can move any gate set.** The
constant is measured entirely on the forced-primal campaign, and the gate's
role here is to confirm the digests do not move.

### The forced-primal campaign — where each setting starts to bite

Smallest `r` per instance, over all four slots. Everything not listed is above
5.4855.

| instance | min r | | instance | min r |
|---|---|---|---|---|
| `pilot87` | 3.3457e-06 | | `scsd1` | 0.073529 |
| `pilot` | 1.63e-05 | | `stair` | 0.078285 |
| `scsd8` | 2.3078e-05 | | `pilot4` | 0.15674 |
| `tuff` | 0.00072158 | | `d6cube` | 0.34184 |
| `greenbea` | 0.0018865 | | `woodw` | 0.54647 |
| `d2q06c` | 0.015055 | | `dfl001` | 0.7525 |
| `perold` | 0.017124 | | `pilot-ja` | 0.77338 |
| `scsd6` | 0.023186 | | `wood1p` | 5.4855 |

`pilot87`'s own worst call is the refusal itself: iteration 17165, row 790,
`|move| = 1.594e-07` against `cmax = 2.1458e+14`.

## The sweep — `sweep-pivot-margin.sh`, `sweep-<C>.txt`

One build, `PIVOT_MARGIN` read once from the environment, so the sweep cannot
measure one binary N times (the trap that broke three of five build
configurations, D154). `C = 0` collapses the floor to `PIVOT_MIN` and is the
control.

**The census predicted the affected set exactly: 15 instances of 15.** `C=3e-6`
moves nothing (below `pilot87`'s 3.3457e-06); `C=1e-5` moves `pilot87` alone;
`C=1e-3` moves the four below 1e-3; `C=1e-1` the ten below 0.1; `C=1` and
`C=5` the same fifteen, because the next instance up is `wood1p` at 5.4855.

### The first implementation was rejected by its own control

Round one computed the column's largest entry in a **separate scan before the
row loop**, and billed it. That is not a neutral accounting change:
`bench/primal` gives the primal solve a budget of **10x the dual's work**, so
charging more work shortens every primal solve. `bnl2` sat at 10.0066x of the
dual and `tuff` at 10.0186x, right on the bar. D203 had already written this
limit down.

The `C = 0` control came out **18 instances** away from the committed record
with four verdicts changed — `bnl2` and `tuff` DISAGREE → overrun, `pilot-ja`
and `standmps` ok → DISAGREE — and **none of it was the floor**. The scan
alone cost a work geometric mean of 1.028065x on the primal solve (worst
`sctap3` at 1.122728x) against 1.000007x on the dual.

Round one's files are kept as `sweep-<C>.txt`; the verdict tallies in them are
measured against a control that had already lost two instances, and only the
affected-set prediction survives from them.

### The second implementation was rejected by `numerics-reviewer`

`sweep-cheap.sh` fixed the work charge and the review then found two defects
in what removing a row does to the callers.

- **`-1` came to mean two things.** It meant "nothing blocks". An emptied
  candidate set made it also mean "rows blocked, none cleared the floor", and
  both callers turn `-1` into a refusal — phase 1's says *"this is a JAOS
  defect"*. The repair for one false refusal could manufacture another.
- **The floor dropped a row from the choice but not from the movement.**
  `*step` came from the floored pass, and both callers use it to decide a
  bound flip, which then runs `xb[i] -= delta * col[i]` over every row
  including the one just called meaningless. That can push a basic past a
  declared bound and still publish `OPTIMAL`.

The review also confirmed what the fast path rests on: the equivalence proof
holds in both tie-break modes and under the `step < 0 → 0` clamp, `dir` being
exactly ±1.0 so the filter and the break test compare the same bits; no caller
reads `*below` on a negative return; the work charge is one per scan; nothing
between the passes writes `s->col`, `s->xb`, `s->basis` or the bounds.

### The implementation that ships — `sweep-candidate.sh`, `cand-<C>.txt`

Pass 0's answer is kept. `*step` is read off **every** row, so the bound-flip
behaviour is exactly the shipping one and the floor decides only what may be
pivoted on. Pass 0's winner stands when the floor leaves nothing, so `-1`
keeps its single old meaning. The break is negated so a NaN `rel` from a
non-finite column stops rather than running an unfiltered second pass.

**Both fixes produce a byte-identical campaign record.** `cand-1.txt` and
`cheap-1.txt` agree on all 94 lines, so neither state is reached by these
instances. The fixes are insurance, not repairs of something that was firing.

### The earlier implementation, and the control that caught it — `sweep-cheap.sh`

The column's largest entry comes out of **the loop that already runs**, and
only the row that loop chose is checked against the floor. A second pass runs
just when that row is below it. This is exact: the winner of a scan is still
the winner over any subset of it that contains that winner, and
`jm_primal_row_wins` breaks ties on the basis variable, which no removal
changes.

**`C = 0` reproduces `bench/results/primal.txt` byte for byte** — verdicts,
iteration counts, objectives, splits and work units — for `cheap-0.txt` and
for `cand-0.txt` alike. So the floor costs nothing when it does not fire, and
every difference at a higher `C` is the floor and only the floor.

### Verdicts, 94 standard instances

| `PIVOT_MARGIN` | ok | DISAGREE | overrun | ERROR |
|---|---|---|---|---|
| 0 (shipping, and byte-identical to it) | 55 | 31 | 7 | 1 |
| 3e-1 | **56** | 30 | 8 | **0** |
| **1** | **56** | 30 | 8 | **0** |
| 2 | **56** | 30 | 8 | **0** |

**The tally is flat from 0.3 to 2**, a factor of about seven, so the value is
not a spike. Which instances move differs inside that range — `pilot-ja`,
`stair` and `woodw` change trajectory at 1 and not at 0.3, all keeping their
verdict — but the outcome does not.

The window is bounded on both sides by measurement rather than by argument:
below 3.3457e-06 the floor decides nothing at all, and above 5.4855 it would
begin to reach the gate.

At `C = 1`, twelve instances move and **not one of them to a worse verdict**:

- `pilot4` DISAGREE → **ok**;
- `pilot87` ERROR → overrun, which is the other seven phase-1 instances' fate
  and not a self-declared defect;
- `pilot` 40487 → 35666 iterations and `pilot-ja` 18536 → 12597, both still
  `ok`; `perold` 7838 → 9981, still `ok`;
- `d2q06c`, `greenbea`, `scsd1`, `scsd6`, `stair`, `tuff`, `woodw` keep the
  verdict they had.

### What it costs — `work-cost.sh`

| solve | work geometric mean, C=0 → C=1 | worst |
|---|---|---|
| dual (what the gate runs) | **1.000000x** | 1.000000x |
| forced primal | **0.995321x** | 1.271581x (`perold`) |

**The dual side is byte-identical on all 94**, which is the census's prediction
measured rather than argued: the lowest ratio anywhere on the dual path is
`wood1p`'s 5.4855, so at `C = 1` the floor never rejects a row there and the
second pass never runs. On the primal campaign the floor is **0.5% cheaper**
on average — the pivots it refuses were costing iterations.

### The shipping build reproduces the sweep's build

The sweep reads `PIVOT_MARGIN` from the environment so one binary serves every
setting; the shipping code is `constexpr double PIVOT_MARGIN = 1.0`. Those are
the same number only if `1.0 * DBL_EPSILON` folds identically either way.
`make primal J=12` on the shipping tree writes `bench/results/primal.txt`
**byte-identical to `cand-1.txt`**, all 94 record lines. It does.

## The gate, and the fourth metric

`make configs` — **all five configurations build and pass**, which is the run
`tests/test_simplex.c` changing requires (D154).

`make netlib netlib-infeas netlib-kennington J=12` — `gate: PASS` and
`0 regressed, 0 improved, 0 new` on each. The summary line hides a lot, so the
reading that decides it is `git diff bench/results/`: **empty**. All three
records come back byte-identical after being regenerated, digests and work
units alike, over 139 instances. That is the census's prediction measured, not
argued.

Work units therefore cannot say what the floor costs the CPU, and seconds on
this host repeat to 6.27% (D93). The instruction count can (D206,
`icount.sh`, `icount.txt`):

| instance | `1fe8bc6` | candidate | ratio |
|---|---|---|---|
| `adlittle` — control, never reaches it | 7755048 | 7754912 | 0.99998 |
| `afiro` — control | 1065142 | 1065026 | 0.99989 |
| `etamacro` — 1 call | 219670091 | 219669993 | 1.00000 |
| **`wood1p` — 169 calls** | 2192253358 | 2192431206 | **1.00008** |
| geometric mean | | | **0.99999** |

The two controls move 0.002% and 0.011% on solves the change cannot reach, so
that is what nothing looks like in this instrument. `wood1p` pays 0.008%, or
178k instructions out of 2.19 billion.

### What the floor does NOT do

**It does not repair `pilot87`.** At `C=1e-5` the refusal stops and the solve
runs out of work instead, at 426850 phase-1 iterations against 17165 before.
At `C=1e-3` and `C=1e-1` **the same refusal comes back at a different row** —
column 669 in row 813, then column 4084 in row 1784. The floor moves where the
mechanism fires; it does not remove the mechanism. What remains is the two
computations of `(B^-1 A_q)_r` disagreeing on a fresh factorization with
nothing to rebuild.
