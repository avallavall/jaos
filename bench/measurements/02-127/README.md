# 02-127 — stage 2's width, swept

`sweep-delta.sh` measures the working tree with `jm_harris_pick`'s widening
tolerance read from the environment as a multiple of `primal_tol`, so one
binary serves every setting (D154's trap). `0` is pass two alone: the largest
pivot among **exact** ties, no relaxation. `1` is what D212 shipped. D211's
two counters ride along, per instance.

| width | ok | DISAGREE | overrun | ERROR | `pilot87` phase-1 iters | its worst relative rise |
|---|---|---|---|---|---|---|
| 0 | 59 | 30 | 5 | 0 | 457002 | 7.2291e+11 |
| **0.1** | **61** | **29** | **3** | **1** | **106485** | **1351.31** |
| 1 (shipped) | 60 | 30 | 4 | 0 | 372035 | 8.33038e+13 |
| 10 | 58 | 31 | 5 | 0 | 191834 | 4.61607e+11 |

**Most of the gain is the pivot preference, not the relaxation.** At width 0,
with no relaxation at all, 59 of 94 already agree against 56 before the
change. The relaxation is worth at most two more.

**Width 0.1 is where `pilot87`'s divergence nearly stops.** Its worst
relative rise of the phase-1 objective falls from 8.33e+13 to **1351**, ten
orders of magnitude, on 106485 phase-1 iterations against 372035. Nothing
else measured has moved that number. D211 named it as the reading that says
whether the tiny pivots were the whole story.

> **Read the three widths added below before believing that paragraph.** With
> `0.01`, `0.3` and `0.5` measured, the 1351 is one point between 8.27e+11 and
> 3.28e+16. It is not a trend and the width does not control the divergence.

**And width 0.1 brings back one `ERROR`** — `pilot87` itself, and its message
names the cause, because D209 made these sites say which floor rejected them:

> column 975 prices at 7.74039e-10 in row 758 of the primal phase 1 on a
> freshly built factorization, against a floor of 1e-09

That floor is `PIVOT_MIN`, the **stability** floor D209 identified, not the
relative one. So at 0.1 `pilot87` does not diverge: it brings its objective
under control, runs 106444 phase-1 iterations instead of 372035, and then
stops on a pivot **1.3 times** below an absolute 1e-9 on a fresh
factorization, where there is nothing left to rebuild.

That is a different and much smaller failure than 1e+24, and it is the class
D207 and D209 have been circling from the other side. It also separates the
two questions: the width decides whether the objective stays under control,
and `PIVOT_MIN` decides whether the solve can finish.

## D212's three regressions: two of them are not the relaxation

Read from the four `delta-*.txt` records already here, no new run.

| instance | before D212 | width 0 | 0.1 | 1 | 10 |
|---|---|---|---|---|---|
| `israel` | ok | **ok** | DISAGREE | DISAGREE | DISAGREE |
| `pilot-ja` | ok | DISAGREE | DISAGREE | DISAGREE | DISAGREE |
| `pilotnov` | ok | DISAGREE | DISAGREE | DISAGREE | DISAGREE |

`pilot-ja` and `pilotnov` disagree at width 0, where there is no relaxation at
all. What loses them agreement is the **pivot preference** — pass two taking a
larger pivot than the exact minimum did. Only `israel` belongs to the
relaxation, and every width above 0 loses it.

All three fail the same way, on dual feasibility at the settled point, and the
size of the breach moves with the width without following it: `pilotnov`
breaches its bound by 4.75223 at width 0, by 0.221786 at 0.1 and at 1, and by
95.8043 at 10.

## Every verdict that moves, width against width

| pair | instances that differ |
|---|---|
| 0 vs 1 | `brandy` DIS→ok, `finnis` DIS→ok, `scrs8` DIS→ok, `d6cube` overrun→DIS, `fit1p` ok→DIS, `israel` ok→DIS |
| **0.1 vs 1** | `wood1p` **ok**→DIS, `pilot87` **ERROR**→overrun |
| 10 vs 1 | `bandm` DIS→ok, `pilot` overrun→ok |

Only two instances separate 0.1 from 1. One is a gain for 0.1 (`wood1p`), the
other is the same instance failing in a different way (`pilot87`, which fails
at both).

## Three more widths: the verdict count is flat, and the 1351 was luck

`0.01`, `0.3` and `0.5` were added on 2026-08-27. `0.5` is the value GMSW 1989
actually ship (`delta_i = delta_f / 2`); `0.01` and `0.3` bracket `0.1`.
`sweep-delta.sh` now also runs against a committed HEAD with no working-tree
diff.

**Control.** Width `1` was re-run from HEAD and reproduced the committed
`delta-1.txt` and `counters-1.txt` **byte for byte**. The first four settings
were measured from a working-tree diff, so without this the two halves of the
table could not be compared.

| width | ok | DISAGREE | overrun | ERROR | `pilot87` phase-1 iters | its worst relative rise |
|---|---|---|---|---|---|---|
| 0 | 59 | 30 | 5 | 0 | 457002 | 7.2291e+11 |
| **0.01** | **61** | **29** | 4 | 0 | 298758 | 8.26947e+11 |
| **0.1** | **61** | **29** | 3 | 1 | 106485 | **1351.31** |
| **0.3** | **61** | **29** | 4 | 0 | 287484 | 3.28308e+16 |
| **0.5** | **61** | **29** | 4 | 0 | 381886 | 8.06882e+11 |
| 1 (shipped) | 60 | 30 | 4 | 0 | 372035 | 8.33038e+13 |
| 10 | 58 | 31 | 5 | 0 | 191834 | 4.61607e+11 |

**There is a plateau, and it is exactly the same 61 instances.** At `0.01`,
`0.1`, `0.3` and `0.5` the set of agreeing instances is identical, name for
name. The width buys nothing inside that band. What it buys against the
shipped `1` is **one instance, `wood1p`**, and that is the only verdict that
separates `0.5` from `1`. `0.5` and `0.1` are separated by one instance too:
`pilot87`, which fails at both, as an overrun at `0.5` and an `ERROR` at `0.1`.

**`pilot87`'s 1351 is a single point.** Its neighbours are 8.26947e+11 at
`0.01` and 3.28308e+16 at `0.3`. Across the seven settings the number runs
7.2e+11, 8.3e+11, **1.4e+03**, 3.3e+16, 8.1e+11, 8.3e+13, 4.6e+11 — no order at
all. The width does not control the divergence, and a constant fitted to the
1351 would be a constant fitted to one instance at one setting.

**So the reason to move off `1` is not the campaign.** It is that `1` sits
exactly at the top of the bound the phase-1 argument allows:
`docs/research/harris-primal.md` bounds the width above by `PRIMAL_TOL`,
because a relaxed basic variable ends at most `delta` outside its bound and
that only counts as feasible while `delta <= PRIMAL_TOL`. Shipping the bound
itself leaves zero margin, and GMSW keep their tolerance under theirs. Any
value in the plateau restores the margin at no measured cost.

## The gate does not move at any admissible width

`gate-delta.sh` runs all three gate sets from a worktree at HEAD with the width
made an environment read. `bench/results` is restored from git between
settings, so every width is diffed against the same committed baselines and
never against the width before it. Verbatim output: `gate-log.txt`.

| width | netlib | netlib-infeas | netlib-kennington | instance lines that moved |
|---|---|---|---|---|
| 0 | PASS | PASS | PASS | **0** |
| 0.1 | PASS | PASS | PASS | **0** |
| 0.5 | PASS | PASS | PASS | **0** |
| 1 (shipped) | PASS | PASS | PASS | **0** |
| 10 | PASS | PASS | PASS | **0** |
| **1e9** | **NOT MET** | not reached | not reached | **9** |

Every set reads `0 regressed, 0 improved, 0 new` at the first five settings.
The width changes nothing the gate can see, anywhere from no relaxation at all
to ten times the bound the phase-1 argument allows.

**The 1e9 row is why the five zeroes can be believed.** Five identical clean
results are also what a probe that never reached the code would print, and the
project has been caught by exactly that before. At `1e9` the widening is 100
in the units of `xb`, so pass one admits every candidate and pass two takes the
globally largest pivot whatever its ratio. The gate then breaks, on the one
instance it should:

```
-pilot87  optimal ... work=17961112053 obj=301.71034733311052
+pilot87  optimal ... work=17961477424 obj=301.7103459906541
+pilot87  REGRESSED    checker: yes -> no (optimal -> optimal)
gate: NOT MET     baseline: 1 regressed, 0 improved, 0 new
```

So the instrument reaches the code, and the five zeroes are a measurement.

## The pivot histogram, which the verdict count cannot see

`counters-<W>.txt` carries one line per instance with a histogram of the
phase-1 pivot element by decade: bucket `d` is
`floor(log10(|alpha|)) + 12`, clamped to `[0, 15]`, so buckets 0 to 8 are every
pivot below 1e-3. The first four settings were read for `pilot87`'s `max_rel`
alone and the histogram was left unaggregated.

    for w in 0 0.01 0.1 0.3 0.5 1 10; do
      awk -v W=$w '/^RR /{for(i=1;i<=NF;i++) if($i~/^hist=/){
        n=split(substr($i,6),a,","); for(j=1;j<=n;j++){t+=a[j]; if(j<=9) lo+=a[j]}}}
        END{printf "%-5s %8d pivots  %.4f%% below 1e-3
", W, t, 100*lo/t}' counters-$w.txt
    done

| width | 0 | 0.01 | 0.1 | 0.3 | **0.5** | 1 | 10 |
|---|---|---|---|---|---|---|---|
| phase-1 pivots | 439500 | 534087 | 340787 | 470325 | 605267 | 581525 | 529840 |
| below 1e-3 | 0.2603% | 0.1766% | 0.2579% | 0.2503% | **0.1290%** | 19.0463% | 0.3261% |

**`0.5` is the lowest of the seven.** The obvious objection to narrowing the
window says the opposite: pass one admits fewer candidates, so pass two can
only reach a smaller pivot. These files refute it, and no new campaign was run
to get them.

**Width `1` is the outlier, and it is one instance.** 110587 of its 110759
tiny pivots are `pilot87`'s, over 308118 phase-1 pivots: it alternates between
a pivot near 1e-4 and one above 1e+3 for most of the run. That is a second
reason `1` was a bad setting, independent of the verdict count.

## Why 0.5, when the measurement cannot choose

Inside `0.01` to `0.5` the campaign is one number and the gate is another, both
flat. **No reading here selects a value in the plateau.** What selects one is
the argument, and three things point the same way:

1. **The bound.** `docs/research/harris-primal.md` bounds the width above by
   `primal_tol`. `1` met that bound with equality; `0.5` keeps a factor of two.
2. **The published ratio.** MINOS and SNOPT start EXPAND's tolerance at
   `delta_f / 2`. JAOS holds its width fixed and carries none of the rest of
   EXPAND, so this is one ratio borrowed and not the method — a weaker claim
   than "the published value", and it is stated that way in the source.
3. **`0.5` is a power of two**, so `0.5 * primal_tol` is exact and no
   contraction can round it differently. `0.1` and `0.3` cannot say that, and
   in a solver whose first rule is bit-identical results across machines that
   is a real difference between two settings the campaign cannot separate.

`pilot87`'s divergence chooses nothing, per the section above.

## Open

- **The plateau's upper edge is somewhere in `(0.5, 1]` and is not resolved.**
  Nothing measured between them. The cost of being wrong is one instance of a
  non-gate campaign, so it is not urgent.
- `israel`, `pilot-ja` and `pilotnov` still disagree, and the two mechanisms
  are named above but not diagnosed.
- **Consecutive relaxed steps are bounded by nothing in the code**, only by
  `refresh`. One relaxed step puts a basic at most one width past its bound; a
  row already past its bound has its distance clamped to zero, so the next
  iteration can push it a further width. The reading that would settle it is
  `max_i (xb[i] - up) / width` at entry to `primal_ratio_test`. D213 carries
  it.

**Both scripts here were repointed when D213 landed.** `sweep-delta.sh` and
`gate-delta.sh` patch the width by exact string replacement, and the line they
anchored on stopped existing when the width became a constant. They now anchor
on `const double width = PRIMAL_HARRIS_DELTA * s->primal_tol;`. They must stay
release builds: every width above 1.0 trips `primal_pick`'s assert, and the
bench binaries carry `-DNDEBUG` (`Makefile:107`).

Nothing here changes `PIVOT_MARGIN`; that constant is D207's and was swept
separately in `bench/measurements/02-122/`.
