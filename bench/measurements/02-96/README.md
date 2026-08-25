# 02-96 — `DUAL_TOL` is 1e-9, all four wrong points are gone, and Kennington's price is new

D184. `src/simplex.c`: `DUAL_TOL` from `1e-7` to `1e-9`. Taken on the
maintainer's decision, on D174's sweep, with the Kennington figure below put in
front of them before the baselines were rewritten.

## What changed and why

`pilot` published a point **2.31e-05 above the optimum** — 1.87e+08 times the
floor arithmetic sets for that model, and the only netlib instance HiGHS,
SoPlex and Clp all beat. D174 measured the cause and the repair and left the
call open because it costs work.

## The four instances the change is for

| | before | after |
|---|---|---|
| `pilot` | 2.312e-05 | **5.266e-09** |
| `pilot87` | 1.044e-07 | **exactly Koch** |
| `scsd6` | 1.118e-09 | **exactly Koch** |
| `etamacro` | 1.315e-08 | **1.137e-13** |

`pilot87` and `scsd6` publish the reference to the last bit.

## The price, and half of it is new

| set | work, geometric mean | worst instance |
|---|---|---|
| netlib | **1.0339x** | `d2q06c` **5.319x** |
| **Kennington** | **1.0976x** | **`pds-20` 4.815x** |
| infeasible | — | 0 regressed, 29 refusals unmoved |

**netlib's 1.0339x is D174's own prediction to four figures.** Kennington's is
not in D174 at all: that sweep ran on netlib, and `CLAUDE.md` is explicit that
Kennington is where a change that scales badly shows. `pds-20` goes from
6.15e9 to 2.96e10 work units.

Instances past `bench/run.c`'s 2.0x per-instance bar: `agg3` 2.28x, `d2q06c`
5.32x, `nesm` 2.17x, `perold` 2.80x, `pilot-ja` 2.21x on netlib, and `pds-20`
4.81x on Kennington.

**`gate: PASS` on all three sets.** Every instance still solves with
`shape=ok`, `objective=ok`, `checker=ok` and `det=ok`. The regressions are cost
against the baseline, and no answer got worse.

Geometric means of per-instance ratios throughout, never a sum: two instances
are 74% of the standard set's total (D46). The ratio of totals reads 1.0047x on
netlib and 1.9210x on Kennington and is not the result.

## D177's floor is why two of the regressions are visible

`bnl1` reports its suboptimality bound going 7.09e-15 -> 1.57e-14 and `scsd1`
4.3e-17 -> 1.96e-16. **Both sit far under the old `RSUB_FLOOR = 1e-9`, so at
the floor this project shipped until yesterday neither would have been
reported.** That predicate watched 4 solves of 110 before D177 and watches 84
now.

## A units conflation, found in review, measured, and dead

`DUAL_TOL` is read in **two different units**:

```c
a RATE bound, at every other site:  s->d[v] < -s->dual_tol
an OBJECTIVE bound, at one:         can_move(), wrong_way * |other - value| > dual_tol
```

`can_move`'s comment argues at length that the **product** is the right
quantity to test, because it has no space — `publish` divides `d` by the same
gamma it multiplies the value by — and **never says what the product is
compared against**. It is compared against a threshold documented and swept as
a bound on a rate. Tightening by two decades therefore makes that one site 100x
more eager, which is not what was decided.

**Measured, and it changes nothing.** A variant holding `can_move` at 1e-7
while every other reader goes to 1e-9:

- **94 of 94 instances publish the same digest**
- work geometric mean **1.0000x**, best and worst both 1.000x
- all four target instances identical, gap for gap

Two distinct binaries — md5 `61cf9bfdaedb` against `8f822d1c9f46` — and the
script aborts if the patch does not rewrite the line, so the null is a null and
not an unbuilt variant.

**The likely reason is structural and is not measured here.** `can_move` feeds
`anything_to_move`, and its own comment says what such columns need is a primal
pivot — which `SPECS.md` has as missing. A test whose consequence does not
exist yet decides nothing.

So the conflation is real, recorded, and costs nothing today. Giving that site
its own constant would be fitting a number with no sweep behind it, which is
what `CLAUDE.md` forbids.

## Reproducing

```
bench/measurements/02-96/run-dual-tol.sh
```

Runs the three sets on the working tree and then builds the `can_move` variant
in a worktree under `$(mktemp -d)`, outside the repository.

## What was NOT done

**No independent verdict was taken.** `CLAUDE.md` asks for `jaos-measurer` to
judge a finished candidate in a context that did not produce the numbers, and
this session was instructed not to spawn subagents. The per-instance evidence
is above and in `dual-tol.txt` so that judgement can still be made.
