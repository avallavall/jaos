# 02-94 — `plato-nug` solves one of three, and presolve reaches nothing at all on the one that does

D182. No source change.

## What was asked

`TODO.md` §4 has carried `plato-nug` as "unmeasured rather than unsolvable"
since D115, and nobody had checked the sentence. It is the one shape this tree
does not have: every model JAOS reads today is economic, transport or
stochastic, and a QAP relaxation is none of those. A set that cannot say
whether it solves cannot be part of an argument about model population.

## The answer: one of three

| instance | rows x cols | result |
|---|---|---|
| **`nug08-3rd`** | 19728 x 20448 | **solves**, 34424 iterations, **294654930775 work units** |
| `nug20` | 15240 x 72600 | **did not finish in 3600 s** |
| `nug30` | 52260 x 379350 | **did not finish in 1800 s** |

The seconds are a stopping rule and not a cost: they make "did not finish in
T" a statement someone can check. The work units are the cost and they are in
`record-nug08-3rd.txt`.

**The family is not ordered by rows.** `nug20` has 4488 FEWER rows than
`nug08-3rd` and 3.5x the columns, and it does not finish in five times the
time `nug08-3rd` needs.

`nug08-3rd`'s answer is clean: `checker=ok`, `det=ok`, `cert=yes`,
`Q=8.89e-10`, `rsub=4.14e-12`, and it publishes 214.00000000040001 with no
reference optimum to score against.

**So `plato-nug` is not a practical fourth set and one instance of it is
usable.** That is a different answer from "unsolvable" and from "unmeasured",
and it is what §4 needed.

## What the one that solves says — `presolve-reach.txt`

**Presolve removes nothing at all on `nug08-3rd`.** Not a row, not a column,
not a nonzero: `19728/20448/139008 -> 19728/20448/139008`.

That is worth a table, because §4's whole argument is that the population
decides the verdict. From the committed records, no run needed:

| set | n | median rows removed | median nonzeros removed | removes nothing at all |
|---|---|---|---|---|
| netlib | 94 | **9.04%** | 6.35% | 8 of 94 |
| Kennington | 16 | 12.57% | **21.57%** | 0 of 16 |
| `plato-pds` | 8 | **2.93%** | 1.40% | 0 of 8 |
| `plato-fome` | 4 | **0.00%** | 2.06% | 0 of 4 |
| `plato-nug` | 1 | **0.00%** | **0.00%** | **1 of 1** |

**JAOS's presolve reaches a median of 0 to 3% of rows on the plato sets against
9% on netlib.** §4 quotes Galabova 2023 for the opposite direction — HiGHS's
presolve speed-up is 1.10 on netlib against 1.67 on a modern set, so its
presolve is worth *more* there. The two are not a matched comparison: that set
is Mittelmann's benchmarks plus four industrial models, not these. But the
shape of the gap is stated rather than assumed now, and it points the way §4
said it would.

The eight netlib instances where presolve also removes nothing are `degen2`,
`degen3`, `fit1d`, `fit2d`, `scsd1`, `scsd6`, `scsd8` and `truss`.

## The instrument was wrong first, and its output looked finished

The first version of `run-nug.sh` filtered the runner's console output with
`grep -vE` on a leading bracket, to drop the per-instance timing prefix. **The
runner prefixes the RECORD line with that same bracket**, so the only line
carrying work units, the digest and the checker numbers was the one thrown
away — and the summary line survived, so the output read as a finished
measurement with an instance count and a `gate: PASS`.

The record comes from `-o` now and never from the console.

## Reproducing

```
bench/measurements/02-94/run-nug.sh 3600 nug08-3rd     # ~730 s
bench/measurements/02-94/run-presolve-reach.sh         # reads records, no run
```
