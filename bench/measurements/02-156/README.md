# 02-156 — an approximate edge rule for the primal, derived, measured, refused

D244. The maintainer's decision on 2026-09-01 was to derive a pricing rule
here rather than buy the paper Devex is in. The rule was derived
(`docs/research/approximate-edge-pricing.md`), built, and swept. **It loses
to Dantzig on this set at every setting, and the code is not kept.**

## What is here

| file | what it does |
|---|---|
| `run-edge-sweep.sh` | patches the constant, runs the campaign, and reports paired against Dantzig |
| `edge-sweep.txt` | the sweep, as run |
| `runs/` | one per-instance campaign record per setting, plus Dantzig's |
| `candidate.diff` | the rule as measured, so a retry starts from this and not from scratch |

`run-edge-sweep.sh` restores `src/simplex.c` and `bench/results/primal.txt`
itself. About twelve minutes.

## The sweep

Paired over the instances that come back `ok` under both the setting and
Dantzig. A ratio below 1 means the edge rule used less.

| DRIFT | paired | iters | work | ok | disagree | overrun |
|---|---|---|---|---|---|---|
| Dantzig | — | 1.0000 | 1.0000 | **61** | 30 | 3 |
| 1.0000001 | 60 | **1.0000** | 1.1089 | 60 | 30 | 4 |
| 1.5 | 59 | 0.9948 | 1.1033 | 59 | 31 | 4 |
| 2.0 | 60 | 0.9458 | 1.0462 | 60 | 30 | 4 |
| 4.0 | 58 | 1.0365 | 1.1182 | 59 | 31 | 4 |
| 16.0 | 53 | 0.9834 | 1.0186 | 53 | 37 | 4 |
| 1e300 | 57 | 0.9513 | **0.9335** | 59 | 34 | 1 |

## The control, and what it cost to get right

The first row is the control. At a ratio just above 1 every weight is reset
at the end of nearly every pivot, so pricing always reads weights of 1, so
`gain^2 / 1` orders the candidates exactly as `gain` does and the rule must
choose what Dantzig chooses. **It reads 1.0000 on iterations, exactly**, so
the recurrence, the update walk and the pricing change are all doing what
they are meant to.

**The first version of this sweep was not paired and its control failed.**
It read 1.9901 against Dantzig's 2.0079 and the two look like different
rules. They are not: the campaign bounds the primal at ten times the dual's
work per instance, this rule spends work of its own, and one instance
crossed that bound. The two geometric means were then over two different
populations. Every figure here is over the instances `ok` under both, and
the size of that set is in the table.

## What the sweep says

**The maintenance costs 10.9% work for nothing at the control.** That row
changes no decision — the iteration ratio is exactly 1 — and still costs
1.1089 in work. That is the price of carrying the weights, measured
directly, and everything the rule buys has to be paid for out of it.

**It does reduce iterations, by up to 5%.** 0.9458 at DRIFT 2 and 0.9513 at
"never reset" are real reductions on a paired set.

**Only "never reset" nets a work saving**, at 0.9335. Every other setting
spends more work than it saves.

**And every setting agrees with the dual on fewer instances than Dantzig.**
61 becomes 53 to 60. The campaign's own rule is that agreement is the gate
and speed is only the report, so that decides it.

## Where the lost agreement goes, which is the one thing in the rule's favour

The failure modes do not change, only how many instances reach them:

| | Dantzig | never reset |
|---|---|---|
| the settled point is not dual feasible | 29 | 32 |
| phase 1's total infeasibility rises | 1 | 2 |

**The rule creates no new way to fail.** Both families are the forced
primal's own open problems and 30 instances already reach them under
Dantzig. One of the losses is not even a pricing decision: at the control,
where no decision changes at all, an instance still moves from `ok` to
`overrun` because the weight maintenance pushed it past the work bound.

## Refused, and what would reopen it

**Refused at every swept setting.** The code is removed and
`candidate.diff` holds it as measured.

**The comparison is being made on a method that already fails a third of the
set**, and that is the reopen condition: when the forced primal's
"settled point is not dual feasible" family is fixed, the pricing question
can be asked on a set where agreement is not already broken. Re-run
`run-edge-sweep.sh` then, and start from **"never reset"**, which is the row
that reduced both iterations and work.

The derivation stays in `docs/research/approximate-edge-pricing.md`. It cost
nothing to keep, it is ours, and a retry should not have to do it twice.
