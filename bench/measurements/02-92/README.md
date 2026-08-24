# 02-92 — the refactorization interval, swept for the first time: 64 is not the work minimum and it is `pilot87`'s best accuracy by three orders

D180. No behavioural change: the sweep goes beside the constant in
`src/simplex.c` and into `docs/tolerances.md`, proved object-identical by
`comment_only.sh`.

## Why this ran

`TODO.md` carried the sweep as a standing debt in its own words: the
`REFACTOR_EVERY` 16..256 trajectory sweep is manual, **three of M1's four
defect closures came from it**, and no target automates it. D119 was the
fourth.

`REFACTOR_EVERY` decides how many Forrest-Tomlin updates accumulate before the
basis is factorized again. It changes the numerical trajectory and it must not
change whether an answer is correct. An instance that is right at the shipping
64 and wrong at some other interval is a defect the gate cannot see, because
the gate builds one binary.

**The constant had no sweep in the source and none in `docs/tolerances.md`.**
It is the only interval in the file that did not.

## The control, which is why the rest means anything

`record-netlib-64.txt` is **identical to the committed `bench/results/netlib.txt`
on all 94 instance lines**. The only differing line is the footer, because the
sweep runs without `-b`. So the harness at the shipping setting reproduces the
gate exactly, and every difference at another setting is the setting.

The second control is the canary the script aborts on: six settings, six
distinct binary md5s, and **0 of 94 instances report identical work at every
setting**. A sweep where that is all of them has measured one binary N times
(D82).

## No answer changes verdict at any interval

94 netlib and 29 infeasible instances at six intervals. `objective=ok`,
`checker=ok` and `det=ok` throughout, on every one. **The interval hides no
defect at HEAD.**

## What the cost looks like

| interval | 8 | 16 | 32 | **64** | 128 | 256 |
|---|---|---|---|---|---|---|
| work, geometric mean against 64 | 1.0318 | 0.9484 | **0.9143** | 1.0000 | 1.1873 | 1.5663 |
| worst single instance | 2.267 | 4.430 | 2.819 | — | 5.881 | 9.125 |
| worst instance | `grow22` | `d2q06c` | `grow15` | — | `d2q06c` | `nesm` |

The work row is a geometric mean of per-instance ratios, never a sum: two
instances are 74% of this set's total (D46).

**64 is not the work minimum.** 32 reads 8.6% better on the mean and 16 reads
5.2% better. Both cost a worst case: `grow15` 2.819x at 32, `d2q06c` 4.430x at
16. The mean is flat across 16, 32 and 64 and the worst case is not, which is
the shape D151 chose the warm-repair cap by.

**And 32 costs accuracy that no verdict reports.** `pilot87` goes from
1.044e-07 to 5.329e-05 against Koch, three orders worse, and still passes: the
gate's window is `1e-6 * 301.7 = 3.017e-04`, so 5.329e-05 clears it with 5.7x
to spare. That is `TODO.md` item 5's defect seen from the other side — the
gate cannot see an answer getting worse in absolute terms.

**So 64 stays**, chosen for the worst case and for `pilot87`, and now it has
its measurement on both sides.

## What the sweep found, and it belongs to `pilot`'s item

`pilot`'s distance from Koch, per interval:

| interval | 8 | 16 | 32 | **64** | 128 | 256 |
|---|---|---|---|---|---|---|
| gap to Koch | **0** | 2.312e-05 | **0** | **2.312e-05** | **0** | 5.266e-09 |
| work | 6.83e9 | 5.33e9 | 3.80e9 | 4.72e9 | 4.95e9 | 4.37e9 |

**It is not monotone.** Three of six intervals publish Koch's optimum exactly,
at the shipping tolerance, and the shipping interval is one of the two that do
not. `32` reaches it for **0.805x** of 64's work on that instance.

**And 5.266e-09 is D174's own number.** That entry's `dual_tol` sweep reads
`pilot` at -5.266e-09 for `dual_tol = 1e-9`. Two independent knobs — a
tolerance and a refactorization interval — land on the same value, so both are
selecting among one small set of neighbouring vertices. The published point
takes three distinct values across everything measured so far: Koch exactly,
5.266e-09 away, and 2.312e-05 away.

**This refines D174 and does not overturn it.** Its mechanism stands:
`DUAL_TOL` is what lets the solve stop while a column is still improving. What
is new is that the tolerance does not decide where it stops — the trajectory
does, and the interval perturbs the trajectory enough to reach the optimum
without touching any tolerance.

**Nothing is changed here.** Which vertex `pilot` should publish, and at what
price, is `TODO.md` item 1 and it is the maintainer's call.

## Reproducing

```
bench/measurements/02-92/run-refactor-sweep.sh              # 12 jobs, 8..256
bench/measurements/02-92/run-refactor-sweep.sh 12 32 64     # two settings
```

Each setting gets its own worktree in `$(mktemp -d)` — outside the repository,
because `make clean` is `rm -rf build` and `make configs` runs it between five
configurations, so a worktree under `build/` is deleted mid-campaign by anyone
else's build (D166). The script aborts if the constant did not rewrite, and
aborts if two settings produce the same binary.
