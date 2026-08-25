# 02-100 — the primal clean-up was pricing a row the expensive way, and three instances say what that cost

2026-08-25. Evidence for the `build_pricing_row` extraction. Not a sweep and
not a trade: the answers are identical and the work is lower on two instances
and marginally higher on one.

## What changed

Row r of `B^-1` and row r of `B^-1 M` are what both simplex methods need from
a basis change: the dual picks its entering column out of the pricing row, and
the primal has already chosen one and needs the row to step every other
reduced cost by. `price_and_select` built them and `primal_cleanup` repeated
the work, on a stated reason that has expired — there is more than one caller
with a claim on it now.

**The repeat was not equivalent.** `primal_cleanup` did a dense `jm_lu_btran`
and then built `alpha` column by column through `price_entry`, which costs the
whole matrix however sparse `rho` is. `price_all` walks the row-wise mirror
instead and skips a whole row of the matrix per zero of `rho`, which is the
saving D35 measured and the column view structurally cannot express.

Both now call `build_pricing_row`.

## What it cost, per instance

`make netlib netlib-infeas netlib-kennington J=12`, read with
`record_diff.py` against the committed record and not off the summary line:

```
94 instances compared: 91 bit-identical, 3 moved, 0 digest change(s)

-- no regression --

-- changes within threshold --
  WORK   etamacro    3309456     -> 3308076      (0.9996x)
  WORK   pilot87     17961079189 -> 17961110514  (1x)
  WORK   wood1p      55637071    -> 53867372     (0.9682x)
```

`netlib-infeas` and `netlib-kennington` are bit-identical throughout.

**`0 digest change(s)` is the claim worth having.** Every published solution is
the same to the bit, and the iteration counts are unmoved as well — `etamacro`
571, `pilot87` 38000, `wood1p` 694, all unchanged. So the trajectory is
identical and only the billed work differs, which is what an equivalent
implementation doing less of the same arithmetic looks like.

**The gate's summary line says nothing about any of this.** All three sets read
`0 regressed, 0 improved, 0 new`, because no predicate flipped and nothing
passed the 2.0x work bar. Three instances moved anyway. This is the reason the
per-instance diff is the rule here and the summary is not.

## `pilot87` went slightly UP, and that is the honest half

17961079189 → 17961110514, which is +31325 units on 1.8e+10, or 1.0000017x.
The sparse route bills the pattern ordering `jm_pattern_order` performs —
`nr + words + nrpat` — that a dense BTRAN never charged for. On a `rho` dense
enough that the ordering buys nothing, that bookkeeping is a small net loss.
It is real and it is in the record rather than rounded away.

## Which instances these are

The three that moved are the three whose solves reach `primal_cleanup` far
enough to price a row from it. Nothing else in either set touches the changed
code, which is why 91 of 94 are bit-identical rather than merely close.

## Gate

`make test` and `make sanitize` exit 0. All three sets `0 regressed,
0 improved, 0 new`. `make configs` does **not** apply: nothing in `tests/`
changed and no block behind a build flag was touched.
