# 02-66 — `row_traffic` accumulates what a finite end absorbed, and the assert that says so

2026-08-20. `TODO.md`'s first smaller item: `row_traffic[i]` saturates to
`+inf` and never recovers.

## What was asked

The relaxation of a cost-0 bounded singleton column adds
`max(|cmax|, |cmin|)` to `row_traffic[i]`. A cost-0 singleton column need only
be non-free, so one of its bounds may be infinite, and the product with the
coefficient is then infinite. `TODO.md` had the repair already stated — "what
should accumulate is the finite part actually subtracted from
`cur_rl`/`cur_ru`" — and asked for a measurement rather than a patch.

## What the probe reads, before the repair

`traffic.txt`, produced by `run-traffic-probe.sh` at `f0ffec8`. The probe
carries a SECOND accumulation beside the shipped one, so one run reports both
and changes nothing. Counters are summed over every presolve run in the set,
and the gate solves each instance twice.

| | netlib (94) | infeas (29) | Kennington (16) |
|---|---|---|---|
| saturating sites | 9556 | 1408 | 2 |
| rows reaching the frozen-row test | 16618 | 1894 | 602 |
| of those, `row_traffic == inf` | **9008** | 1344 | 2 |
| rows at exactly zero margin | 234 | 48 | 0 |
| of those, saturated | **220** | 12 | 0 |
| infinite value READ by either consumer | **0** | **0** | **0** |
| worst repaired traffic on a zero-margin row | **660** | 1 | — |
| largest repaired traffic, any row | 1e7 | 50303 | — |

**Three things it settles.**

**The instrument is dead on more than half the rows it exists for**: 9008 of
netlib's 16618 frozen rows carry `+inf`.

**No consumer ever reads a saturated row**, on any of the three sets. The
file already said this was unreachable and gave the argument — the only
saturating site sets `row_frozen[i]`, `row_frozen` is never cleared, and both
consumers skip a frozen row. It is measured now rather than argued.

**The repaired accumulation recovers the number the row really carries.**
`greenbea` row 57 reads **660**, which is the figure the frozen-row test's own
comment names for that row ("660 of magnitude subtracted"), where the shipped
form reads `+inf`.

## One claim in `TODO.md` was wrong

It said "**all** 117 standard-set rows that reach the frozen-row test at
exactly zero margin carry `row_traffic == inf`". The probe reads 220 of 234
over two solves, so **110 of 117** per solve, not 117 of 117. The item's
substance is unaffected; the word "all" is not.

## The repair, and why it is two repairs

```c
const bool lo_absorbs = isfinite(cur_rl[i]);
const bool hi_absorbs = isfinite(cur_ru[i]);
...
double moved = 0.0;
if (lo_absorbs && isfinite(cmax) && fabs(cmax) > moved) moved = fabs(cmax);
if (hi_absorbs && isfinite(cmin) && fabs(cmin) > moved) moved = fabs(cmin);
row_traffic[i] += moved;
```

The infinite term is the visible half. The second half is independent of
infinity: an end that was ALREADY infinite is not subtracted from at all, so
the magnitude aimed at it moved nothing. A `<=` row taking `cmax = 5` against
`cur_rl = -inf` used to charge 5 for a subtraction that never happened.

**That second failure mode is this site's alone**, and the source says so: the
other two producers subtract the same term from both ends, so charging it in
full is exact there whichever end was finite. Only this site aims different
magnitudes at the two ends.

The repaired value is never larger than the old one.

## The cost: nothing, on all three sets

Gate at the candidate commit, against the parent's committed record:

```
netlib             94 instances compared: 94 bit-identical, 0 moved, 0 digest change(s)
netlib-infeas      29 instances compared: 29 bit-identical, 0 moved, 0 digest change(s)
netlib-kennington  16 instances compared: 16 bit-identical, 0 moved, 0 digest change(s)
```

`gate: PASS` with `0 regressed, 0 improved, 0 new` on each. That is the
expected result for a change with no reachable read, and it is the check
rather than the expectation.

The assert was moved after the campaign ran.
`.claude/skills/jaos-measure/scripts/comment_only.sh src/presolve.c df2054e`
reports the release object unchanged, because `-DNDEBUG` removes an `assert`,
so the campaign carries over verbatim.

## The assert, and where the review moved it

`numerics-reviewer` reviewed the diff and returned one finding that changed
the change.

**The assert was at the one producer that can no longer poison the
accumulator.** After the repair this site's own contribution is finite by
construction, so `assert(isfinite(row_traffic[i]))` there could only fail
because of a term added at one of the other two producers — and a row poisoned
by those need never host a cost-0 singleton column. "0 aborts on 139" would
have meant "the traffic was finite at rows that had a cost-0 bounded singleton
column", not "the traffic is finite".

It is now a sweep over every row after the round loop, and the predicate is
the property a consumer needs:

```c
assert(!(isfinite(cur_rl[i]) || isfinite(cur_ru[i])) || isfinite(row_traffic[i]));
```

Validated both ways, and the negative control was re-run against this
placement rather than inherited from the earlier one:

- repaired tree, `-UNDEBUG`, all 139 instances: **0 aborts**
- old accumulation with this sweep: **45 of 94** netlib instances abort

It fires wherever an end is finite, which is not everywhere: a row whose two
bounds were already infinite absorbs nothing, so the old form saturated it and
the sweep stays quiet there.

## What was refuted — the structural argument for the assert

The obvious defence of the assert is that an overflow at the other two
producers drives BOTH ends to the same infinity, so the antecedent is false and
it cannot fire on a model jaos accepts. **That is wrong**, and it was about to
be written into the entry.

`row_traffic` sums magnitudes; the bounds sum signed values. Two terms of
opposite sign cancel in the ends and add in the budget. Confirmed here at
`-O0 -ffp-contract=off`, from `rl = ru = 0`:

```
after 1e+308  : rl=-1e+308 ru=-1e+308 traffic=1e+308
after -1e+308 : rl=0       ru=0       traffic=inf
either end finite=1  traffic finite=0  ASSERT FIRES=1
```

A model producing it, every value of which passes jaos's validation
(coefficients finite, bounds non-NaN, no magnitude cap):

```
min x3  s.t.  R: 1e308*x0 - 1e308*x1 + x2 + x3 == 0
x0 in [1,1], x1 in [1,1], x2 in [0,5], x3 in [0,5], cost(x3) = 1
```

The two fixed columns leave `cur_rl = cur_ru = 0` and `row_traffic[R] = +inf`,
and R keeps degree 2 so no row rule removes it.

So the assert rests on **measured headroom**, not on an argument: the largest
traffic any row of the three sets carries is 1e7, against a `DBL_MAX` of
1.8e308. The measured number is what survives someone changing the fixed-column
site; the argument would not have. Found by `numerics-reviewer`.

## What the sweep does NOT cover, and it is queued rather than hidden

The two live reads happen inside the round loop; the sweep runs after it.
Traffic only grows, so the sweep is strictly stronger on that half. The
antecedent goes the other way: a row whose end was finite when it was read and
is infinite by the end passes the sweep. `TODO.md` carries this with the two
unreachable-branch asserts.

## Reproducing it

```
bash bench/measurements/02-66/run-traffic-probe.sh          # the readings, at f0ffec8
bash bench/measurements/02-66/run-assert-both-ways.sh       # 0 of 139, then 45 of 94
bash .claude/skills/jaos-measure/scripts/comment_only.sh src/presolve.c df2054e
```

The probe matches the text of the UNREPAIRED accumulation, so it applies at
`f0ffec8` and not at HEAD. It builds its own detached worktree and never
touches the main tree.

**Re-run against commit `22e2d9d`**, the tree this evidence was taken on. The script anchors on source that later commits rewrote; `make record-check` knows it is pinned.
