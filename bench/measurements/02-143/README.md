# 02-143 — are the indirect loops long enough to prefetch in?

The question that comes before `TODO.md` item 2, and it is not the one the
item asks.

Ainsworth & Jones schedule prefetches at `offset = c(t - l) / t` with `c`
around 64 (ACM TOCS 36(3) 2019, section 4.4 for the formula and section 7.6
for the constant). For the two-load chain these sites have — a stride load of
an index, then an indirect load through it — that is one prefetch 64
iterations ahead and one 32 ahead.

**An inner loop that runs three times cannot use a look-ahead of 64.** The
prefetch index is past the end on every iteration, so the change is added
instructions and nothing else. The paper answers that case in section 4.6
with loop hoisting, which is a different and much larger change.

So the first measurement is not "does prefetching help". It is "is there a
loop to prefetch in".

## What was counted

Three sites, all of the shape `y[idx[p]] -= val[p] * w`:

| site | where |
|---|---|
| `ftran-L` | the L scatter in `ftran_prefix`, `src/lu.c` |
| `ftran-U` | the U scatter in `jm_lu_ftran_sparse`, `src/lu.c` |
| `pricing` | the row-wise loop in `price_all`, `src/simplex.c` |

The patch only increments counters, so the solve is bit-identical — and that
is checked rather than asserted: the patched build's `bench/results` output
came back **byte-identical to an unpatched build** of the same commit. A
census whose probe changed the solver would be measuring a different one.

The report is called by hand before the worker's `_exit(0)`, because
`bench/run`'s workers run no `atexit` handler (D228 is where that cost a
whole run).

## The answer

94 standard instances, 32.5 billion inner-loop iterations across the three
sites.

| site | total iterations | in loops longer than 64 | longer than 32 |
|---|---|---|---|
| `ftran-U` | 17,367,275,360 | **70.0%** | 79.4% |
| `pricing` | 9,925,788,478 | **41.0%** | 60.4% |
| `ftran-L` | 5,191,708,320 | **25.8%** | 59.5% |

**The technique applies.** The busiest site by a factor of two spends seven
of every ten iterations in a loop where a 64-ahead prefetch lands inside the
loop it was issued in. Weighted across the three sites, 54.1% of all
iterations are in loops longer than 64 and 70.4% in loops longer than 32.

`triplen.txt` carries the full histogram, in buckets of 1, 2, 3-4, 5-8, 9-16,
17-32, 33-64, 65-128 and 129+.

## Two things the histogram says beyond the headline

**The distribution is not the average.** `ftran-U` averages 13.8 iterations
per entry, which on its own would have killed the idea. The mean is
misleading because the work is not where the entries are: 56% of its
iterations happen in loops of 129 or more, which are 3.3% of its entries.

**`ftran-U` enters 463 million times with nothing to do.** Those are empty
columns: they pay the loop setup and run zero iterations. That is 37% of its
entries and 0% of its iterations, and it is a separate observation from
prefetching — it is a candidate for a different change, and nothing here
measures whether skipping them would pay.

## What this does not say

Nothing about whether prefetching helps. It says only that the look-ahead
fits, which is the precondition. What the change costs is a miss count
(`tools/icount.sh -m`), because every prefetch is a retired instruction and
the instruction count reports a working prefetch change as worse (D225).

## Running it

```
bash bench/measurements/02-143/run-triplen.sh 12
```

Two arms in their own worktrees: the census, and a plain build whose record
it must match.
