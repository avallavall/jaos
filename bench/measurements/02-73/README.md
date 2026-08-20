# 02-73 — the fourth read of the same running difference, and the one the count cannot reach

D163. Opened by `numerics-reviewer` reviewing D162, which landed one commit
earlier.

## What D162 left

D162 put a shift count on three reads of `cur_rl[i]`/`cur_ru[i]`. The review
found a fourth and a fifth thing:

| | | |
|---|---|---|
| **FOLD** | the singleton row's fold judges `cur_rl[i] / a` and still counted a fixed eight ulps | **repaired here** |
| **CHAIN** | the fold then FIXES a column at that value, and the receiving row is charged one shift at its own traffic | **open, `TODO.md`** |

Both are wrong answers. Both were constructed by the review from the source and
confirmed here by running them.

`src/presolve.c`'s comment at the frozen-row test read "Repaired (D162)" while
three of the four reads carried the count. `docs/tolerances.md` said "every
site that judges a running difference" in the same commit, and listed the fold
— the one site that did not have it. That sentence was false for one commit and
D163 says so.

## The models — `fold-and-chain.txt`, `fold-and-chain.c`, `run-fold-and-chain.sh`

All five share one trick: `2^-25` is a quarter of an ulp of 1e9, so a column
fixed at `2^-25` subtracted from an accumulator of magnitude 1e9 rounds back to
where it started. 256 of them lose `2^-17 = 7.6294e-6`, and every value
involved is a dyadic rational a double holds exactly.

**And the oracle arbitrates all five**, which is what D162's own model could not
do. Its removals were the whole row, so the solver's own summation made the
same error; here the row keeps a live column and the reference build reaches
the feasible point.

| model | the parent (D162) | this tree | the oracle |
|---|---|---|---|
| **FOLD** feasible | **INFEASIBLE** | optimal, 999999999.99999237 | optimal, **999999999.99999237** |
| FOLD control, 1e-3 out | INFEASIBLE | INFEASIBLE | infeasible |
| **END** feasible | optimal | optimal | optimal |
| EDGE control, 2e-4 out | INFEASIBLE | INFEASIBLE | infeasible |
| **CHAIN** feasible | **INFEASIBLE** | **INFEASIBLE** | optimal, 1.1920928955078125e-07 |

The repaired FOLD objective matches the oracle **to the last bit**.

### FOLD — the fourth read

```
x_big + (256 columns fixed at 2^-25) == 1e9,   x_big in [0, 1e9 - 2^-17]
```

Feasible exactly at `x_big = 1e9 - 2^-17`. Round 1 removes the smalls and loses
every one, so `cur_rl` stays at 1e9 against a truth of `1e9 - 7.6294e-6`. Round
2 folds the row onto `x_big` and asks whether `[1e9, 1e9]` meets
`[0, 1e9 - 2^-17]`: `1e9 > (1e9 - 2^-17) + 1.77636e-6` fires.

The scale there was already right — `ps_bound_scale` of the fold's own pair, or
`row_traffic[i] / |a|`, whichever is larger. Only the count was missing, and it
takes the end `tightens_lo`/`tightens_hi` says the running difference supplied.
Scaling by both ends would be D161's shape and D162 records what that costs.

**The cost is what the COLLAPSE below the test now admits.** The gap that
branch accepts is whatever the refusal let through, so a wider window puts a
larger residue on the row — `|a|` times the gap, because D158's clamp puts the
column back inside its own box and leaves the row carrying it. On this model
that is 5.86e-5 where it was 1.78e-6. Nothing on the three sets reaches it:
D158 measured 0 collapses in 100018 folds and this leaves that at 0.

### END — the term D162's own test never exercised

```
x_big + (256 smalls) + w1 + w2 == 1e9,  w1, w2 in [0, 2^-23] cost 1
```

The two live columns keep the row at degree 3, so clause 1 judges it instead of
the fold. `row_traffic` is `2^-17`, below the window's floor of 1, and the bound
is 1e9 — so **the whole window comes from `ps_end_scale`**, the term D162 added
in its second revision. Replace that call with a constant 1.0 and the window
falls from 5.862e-5 to 1.776e-6 against a residue of 7.391e-6.

D162's own test lands `cur_rl` at 7.75e-6, where `ps_end_scale` reads its floor,
so the traffic half carried everything there and neutering `ps_end_scale` left
the suite green. Confirmed by running it: with `ps_end_scale` returning 1.0,
D162's test passes and this one fails. The pair is complete now.

### EDGE — the control the widening needed

D162's control sat 1e-2 from feasible against a window of 1.18e-4, a factor of
85. That says the window is not absurd, not that it refuses a shortfall near
its own edge. EDGE is END moved **2e-4** out against a window of 5.862e-5, a
factor of 3.5, and it is refused on every build.

This matters at clause 1 in particular: `src/presolve.c`'s own comment says a
row rescued there reaches FORCING with its condition already true and is pinned
and DELETED, so a genuine infeasibility missed at clause 1 is not re-tested
downstream.

### CHAIN — what a count cannot cover, and it stays open

```
row S:  x1 + (256 y_s fixed at 2^-25) == 1e9         x1 in [1e9-1, 1e9+1]
row R:  x1 + w1 + w2 == 1e9 - 63*2^-23               w1, w2 in [0, 2^-23]
```

Feasible exactly at `x1 = 1e9 - 2^-17`, `w1 = 2^-23`, `w2 = 0`.

Round 1 empties row S of its smalls and loses them all. Round 2 folds row S and
**fixes x1 at 1e9**, which is wrong by 7.6294e-6 and passes the fold's own test
without firing, because `new_lo == new_hi` there. Round 2's column pass then
subtracts that wrong value from row R, which is charged **one** shift at its own
traffic. Clause 1 refuses row R.

The count is right — one subtraction happened — and the scale is right. The
window is short because `v` itself was wrong, and no term in `ps_shift_excess`
knows that. **A wider window is not the repair.** What it needs is an error
weight carried instead of a count, `row_err[i] += |a| * err(v)`, with the fold
recording `err` when it fixes a column. That is a design and it is `TODO.md`'s.

## The gate

`gate: PASS` on all three sets with `0 regressed, 0 improved, 0 new`; 94, 29 and
16 instances **bit-identical to the committed records**, 0 digest changes. Five
build configurations.

Both new tests were validated against the tree that must fail them: at D162
with this test file copied in, the FOLD test fails and the END test passes;
with `ps_end_scale` neutered to a constant 1.0, the END test fails and D162's
own test still passes.

## What this record does not settle

- **CHAIN.** Confirmed as a wrong answer with the oracle disagreeing, repair
  not attempted here.
- **The three sets still cannot choose a window shape.** 0 rows are near any
  boundary and 139 of 139 are bit-identical under every shape tried across
  D162 and D163. Every verdict in both entries comes from a constructed model.
