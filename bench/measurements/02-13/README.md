# What a sign-respecting inequality implied-free family would reach

The count `TODO.md` §1a asked for, taken 2026-08-17 before building anything.
D107 is the decision it closed. The instrument reads the model as loaded and
changes nothing.

## The instrument

`implied_free_sign.c` is the 02-10 counter with a classification added.
Detection is unchanged: column j live with exactly one entry `a_ij` in row i,
and the box the row implies on `x_j` sits inside the column's own box.

The classification: eliminating such a column forces the row's multiplier,
`y_i = c_j / a_ij`, in minimize-canonical space, the same convention
`src/check.c` judges published duals in. An inequality row restricts the
sign: `y_i > 0` needs a finite row lower bound to bind against, `y_i < 0` a
finite upper. A hit whose forced multiplier points at an infinite row end is
declined. A zero cost forces `y_i = 0`, which every sense admits.

`run-sign-count.sh` refuses to report until two calibrations pass:

- A nine-column hand model where every branch fires: one equality, five
  sign-ok inequalities (one range row, one zero cost, one negative
  coefficient), three declined. Expected `9 9 9 9 1 8 5 3 5 5 1 1`, and the
  run reproduces it.
- The 02-10 values: `maros-r7` 984 hits, `truss` 0, netlib 3321 hits over
  3315 distinct rows. Reproduced exactly, so the detection port is faithful.

## The count

`counts/netlib.txt`, `counts/kennington.txt`, totals in `counts/totals.txt`:

| set | hits | equality rows | inequality | sign-ok | declined | ok rows | ok nonzeros |
|---|---|---|---|---|---|---|---|
| netlib | 3321 | 2980 | 341 | **341** | **0** | 341 | 14094 |
| Kennington | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

The 341 sit on 12 instances, and 304 of them on the six `ship*` models:

```
ship12l 77   ship12s 77   ship08l 50   ship08s 50   ship04l 25   ship04s 25
80bau3b 14   bnl2 9   pilot87 9   scorpion 3   25fv47 1   finnis 1
```

Every `ship*` instance solves below the comparison harness's own 0.05 s
floor. `stocfor3`, the worst instance in the comparison since the P0
re-take, carries zero. The 19 zero-cost hits, the ones whose postsolve must
pick a value instead of computing one, are 9 on `bnl2`, 9 on `pilot87` and 1
on `80bau3b`.

## The declined count is a theorem, not a reading

On a feasible and bounded model, a one-sided inequality hit cannot be
declined. The containment test forces the column bound on the row's open
side to be infinite, and a declined candidate has its cost improving toward
that infinite bound while the row stays feasible along the way. That is an
improving ray, so the model was unbounded. All four sense-and-sign cases
reduce to it. The hand model exercises the decline branch with exactly such
columns, so the branch is shown able to fire; on the 94 feasible bounded
instances it cannot, and it does not.

So the sign condition costs a sign-respecting family nothing in reach. What
shrinks the opportunity is that inequality rows carry a tenth of the count,
not the two thirds §1a assumed: the gap between the 3315 as-loaded rows and
the 1041 the shipped family removes is mostly the margin (§1b owns 1353 of
it) and presolve-time interaction on equality rows, not row sense.
