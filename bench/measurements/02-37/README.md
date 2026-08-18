# The honest shift record skips two re-pricings out of 290, and one instance moves

Taken 2026-08-18, closing `TODO.md` §5a's last item. One probe, then one
candidate. **Landed** as D128.

## The question, asked before the change rather than after

`shift[v] += need` records a loan that was never made whenever `need` is below
half an ulp of the cost: the addition leaves the cost where it was. D125
measured that on 167816 of netlib's 1006960 lends — 16.7%.

Recording what the cost actually moved by is not free, and the reason is one
line in `settle_shifts`:

```c
if (!repay_shifts(s))
    return;
```

`repay_shifts` reports whether anything was outstanding, and `settle_shifts`
skips `compute_duals` and `repair_dual_infeasibility` when nothing was. A
phantom loan makes it report true, so it currently forces a re-pricing that
would otherwise be skipped.

**So the question is not whether the record is honest. It is how often the
honest record flips that report from true to false**, because that is the
whole of the behaviour change. The two readings differ only when every
outstanding record is phantom: a column whose cost really moved has
`cost != cost0`, which both readings see.

## The blast radius, measured first

`run-phantom-loan.sh` keeps a shadow record accumulating `moved` beside the
real one, and compares both readings at every call.

| set | solves | `repay_shifts` calls | **flips** | solves with a flip | phantom lends |
|---|---|---|---|---|---|
| netlib | 188 | 258 | **2** | 2 | 167816 of 1006960 (16.7%) |
| infeasible | 38 | **0** | 0 | 0 | 29784 of 697766 (4.3%) |
| Kennington | 32 | 32 | **0** | 0 | 400204 of 2802558 (14.3%) |

Two calls out of 290 across the whole gate. The infeasible set never reaches
`settle_shifts` at all, which is the same fact D123 established from the other
side. The phantom percentages reproduce D125 exactly.

## The change, and the gate agrees with the prediction

```c
const double before = s->cost[v];
s->cost[v] += need;
s->shift[v] += s->cost[v] - before;
s->d[v] = 0.0;
```

`d[v] = 0.0` stays. Removing it was measured and refused (D126); it rounds a
reduced cost to zero when the cost that produced it could not move, and
without it the breach compounds across iterations.

| set | bit-identical | moved | digests moved |
|---|---|---|---|
| netlib (94) | **93** | 1 | **0** |
| infeasible (29) | **29** | 0 | 0 |
| Kennington (16) | **16** | 0 | 0 |

```
WORK   fit1d   1694739 -> 1681313   (0.9921x)
```

**Two flips on two solves, and 188 solves is 94 instances run twice — so one
instance.** `fit1d` is it, and it costs 0.9921x: the re-pricing that no longer
runs. 110 solution digests and 29 infeasibility verdicts unmoved. `make test`
and `make sanitize` exit 0.

The prediction and the outcome agree exactly, which is what a probe is for.

## What it does not claim

**`shift[v]` still cannot be read as how far a cost moved.** It accumulates
separately from `cost`, so the two round apart at the first lend large against
the cost (D122), and D124 showed the totals differ by re-association alone.
What this removes is one specific dishonesty: a record of a move that provably
did not happen.

## Reproducing it

`run-phantom-loan.sh`, beside this file, for the blast radius. For the
candidate: `make clean`, then `make netlib netlib-infeas netlib-kennington
J=12`. `src/` is read and never written by the probe.
