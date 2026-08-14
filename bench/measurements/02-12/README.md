# The implied free column singleton: what it cost, and the margin's sweep

Raw readings behind **D106**. Nothing here is a plan. The verdict lives in
`DECISIONS.md`; what stayed open lives in `TODO.md` §1.

## The sweep — `PRESOLVE_IMPLIED_FREE_ULPS`

`make clean` between every setting, because make cannot see a `CFLAGS` change
and D82 was one binary measured six times.

```
    ulps  maros-r7 presolve                       maros-r7 work  checker  rows-
       0  3136/9408/144848->2152/7440/100486          316766250       94   9992
       1  3136/9408/144848->2156/7448/100650          328053926       94   8639
       8  3136/9408/144848->2156/7448/100650          328053926       94   8639
      64  3136/9408/144848->2156/7448/100650          328053926       94   8639
    4096  3136/9408/144848->2156/7448/100650          328053926       94   8639
```

`rows-` is rows removed across the whole standard set by every family
together. Before this family it read 7598, so the family adds **1041** at any
setting above zero and **2394** at zero.

**One step, at zero, and four decades of nothing above it.** The constant is a
switch, not a dial. The firing is bimodal: an implied box is either
comfortably inside the column's own box or exactly at its bound, and across
94 instances almost nothing lands in a 1e-12 relative band.

`solved`, `objective ok` and `checker ok` all read 94 at every setting,
including zero.

### The two checks that make the table evidence

**The canary is in the instance, not in a model built for it.** 4 of
`maros-r7`'s 984 candidate rows sit at exact equality, so zero must remove 984
and anything above it 980. It does. That is what says the `EXTRA_CFLAGS` path
and the rebuild both work.

**The canary separates 0 from the rest and nothing else**, which is the same
gap D103 recorded for `PRESOLVE_ROUND_ULPS`. So the plateau rests on the
second check — five settings, five distinct objects:

```
     0  8e9d3cd9986e1f22a619cbdc70abcff3
     1  97f42ef3db25d066da5a3568ae113157
     8  3adc020a1f20e2b165436264e7e3e164
    64  9df7742c7106c0bc006d9abc84c5383d
  4096  cd36817ec773e6dd480e1c930366977c
```

Built with
`gcc-14 -std=c23 -ffp-contract=off -O2 -DNDEBUG -Iinclude -Isrc -DJAOS_PRESOLVE_IMPLIED_FREE_ULPS_VALUE=$U -c src/presolve.c`.

### Why 8 ships and not 0

Not correctness. At zero the set still reads 94 `objective ok` against Koch's
exact rationals, and an objective that is too good because a real bound was
dropped is exactly what that predicate catches.

Cost. Zero against 8, work, geometric mean of per-instance ratios (D46):

```
  GEOMETRIC MEAN       0.9627x
  best  bore3d         0.2524x     360244 -> 90934
        wood1p         0.4166x
        25fv47         0.6420x
  worst d2q06c         2.2163x     4.28597e+08 -> 9.49907e+08
        greenbea       1.9923x
  ratio of totals      1.0146x     <- NOT the result
```

`d2q06c` at 2.2163x crosses `bench/run.c`'s own `WORK_REGRESSION_FACTOR` of
2.0, so zero would make the gate report a regression. `TODO.md` §1b carries
what is left of the question, which is narrower than "what should the
constant be": it is whether the window's `max(1, scale)` floor should exist.

## What the family cost at the shipping setting

Against the record at `98b605b`, work, geometric mean of per-instance ratios:

```
  instances averaged   94
  GEOMETRIC MEAN       0.9527x
  best  maros-r7       0.0156x    21010708013 -> 328053926, 10479 -> 2576 iters
  worst greenbeb       1.5126x      379164967 -> 573519868
  ratio of totals      0.6105x     <- NOT the result
```

Eleven instances improve, six get worse, 57 are unchanged to the bit. The
mean is almost entirely `maros-r7`: it carries 0.0443 of the 0.0484 that
`-ln(0.9527)` is made of.

Where it fires, and what it removes:

```
  bandm        rows -    5  cols -    5  nz -    27
  beaconfd     rows -   14  cols -   14  nz -   550
  bore3d       rows -    3  cols -    3  nz -     9
  capri        rows -   10  cols -   10  nz -    68
  forplan      rows -    5  cols -    5  nz -    18
  greenbea     rows -    1  cols -    1  nz -     2
  greenbeb     rows -    3  cols -   10  nz -    85
  maros        rows -    3  cols -    3  nz -   386
  maros-r7     rows -  980  cols - 1960  nz - 44198
  modszk1      rows -    2  cols -    4  nz -     8
  scfxm1       rows -    1  cols -    1  nz -     2
  scfxm2       rows -    2  cols -    2  nz -     4
  scfxm3       rows -    3  cols -    3  nz -     6
  standata     rows -    2  cols -    2  nz -   768
  standmps     rows -    2  cols -    2  nz -   768
  tuff         rows -    2  cols -   12  nz -   102
  vtp-base     rows -    3  cols -    3  nz -    42
  TOTAL        rows -1041  cols -2040  nz -47043
```

17 of 94. `bench/measurements/02-10/`'s counter read 3315 rows over 56
instances on the model as loaded; the difference is the equality-row
restriction and the original-degree-1 restriction, both deliberate, and it is
`TODO.md` §1a rather than a shortfall.

The other two sets:

| set | what moved |
|---|---|
| Kennington | **nothing — 16 of 16 bit-identical.** The counter read 0 candidates there before the family was written |
| infeasible | 29 of 29 still refused. `gosh` 1.282x work (18928 -> 23927 iterations), `pang` 0.8223x. `presolve` moved on those two only |

## The defect this uncovered, which was older than it

The first campaign came back with three checker failures:

```
greenbeb   row 900      rowrel 0.0135    col 1.43e-27
modszk1    row 1.67e+05 rowrel 0.317     col 4.55e-13
tuff       row 27.7     rowrel 0.646     col 4.73e-30
```

A published point inside every column's own box that misses a row by 900.
`JM_PS_SINGLETON_COL` and `JM_PS_FREE_COL_SINGLETON` write the activity of
their own row and stop; each fires on a column of live degree one, so the
other rows that column touches are dead, and the share owed to them was never
added. This family is the first record that takes a number back out of a dead
row's activity. Repaired in `ps_add_to_other_rows`; all three read `checker
ok` afterwards. The full argument is in D106.

## Files

| | |
|---|---|
| `sweep/netlib-<ulps>.txt` | the standard-set record at each setting |
| `sweep/run-<ulps>.log` | the runner's own output for each |
