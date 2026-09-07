# 02-215 — why `klein2`'s pivots disagree

D335 made `klein2` answer from its own infeasible basis by restarting
cold, and said the mechanism underneath was not diagnosed. This is the
diagnosis. Nothing is repaired here.

## What is here

| file | what it is |
|---|---|
| `diag.py` | the instrumentation: one line per declined and per taken pivot, guarded by `JAOS_DIAG`, applied to a copy of the tree |
| `run.sh` | builds that copy under `mktemp -d`, runs `klein2` warm, and summarises the trace |
| `trace-summary.txt` | its output |

Neither touches the repository's own build. `jaos-debug`'s rule: the
patch is a script that asserts its anchor matched exactly once, the
binary is built outside `build/`, and nothing is left behind.

## The shape, from the log alone

Cold, `klein2` answers INFEASIBLE in 262 iterations with **0 stability
rebuilds**. Warm from its own basis the total primal infeasibility falls
1.67e6 → 26389 → 23635 → **2940.36 by iteration 3000, and then never
moves again** for 103246 iterations. Bland's rule comes on at iteration
8266 and changes nothing.

## It is cycling, and the period is two

83680 declines and 106463 takes. They are not spread out:

```
declines   41799 × (r=143, leaving 61, entering 77)
           41798 × (r=253, leaving 66, entering 77)
              83 × everything else

takes      41799 × (r=112, leaving 55, entering 54)
           41799 × (r=112, leaving 54, entering 55)
           21325 × everything else, all early
```

The two repeated takes are inverses: position 112 takes variable 54, then
gives it back to 55. Fourteen consecutive trace lines from the middle of
it:

```
DECL 41260 r=253 leave=66 q=77 alpha=-4.8174738865449891e-09 col=-1.3969838619232178e-09 rel=0.710017 upd=1
TAKE 41260 r=112 leave=55 q=54 alpha=-0.66666666596443391 theta=1.7881393451452538e-06 upd=0
DECL 41261 r=143 leave=61 q=77 alpha=-3.6960288727173812e-09 col=-3.5215634852647781e-09 rel=0.0472035 upd=1
TAKE 41261 r=112 leave=54 q=55 alpha=-1.4999999613795512 theta=2.9156606153322726e-05 upd=0
DECL 41262 r=253 leave=66 q=77 alpha=-4.8174738865449891e-09 col=-1.3969838619232178e-09 rel=0.710017 upd=1
TAKE 41262 r=112 leave=55 q=54 alpha=-0.66666666596443391 theta=1.7881393451452538e-06 upd=0
...
```

**Every number repeats bit for bit.** The two declined pivot elements are
the same two doubles on all 83597 repetitions, and the two steps are the
same two doubles on all 83597. That is a cycle, not a stall, and the two
have different cures (`jaos-debug`).

## The defect

**`PIVOT_MIN` admits a pivot element that `LU_AGREE_TOL` then refuses,
and nothing removes the candidate in between.**

`PIVOT_MIN` is 1e-9 and every declined element is 1e-9 to 3e-8 — it
clears the floor by a factor of about four. At that magnitude the element
is not a number, it is what is left after cancellation: the two
independent computations of it, the pricing row's BTRAN and the entering
column's FTRAN, disagree by **1% to 71%** relatively.

```
relative disagreement   >= 1      : 4
                        0.1 - 1   : 41811
                        0.01 - 0.1: 41821
                        1e-3 - 1e-2: 23
                        < 1e-3    : 21
```

`-1.3969838619232178e-09` is exactly 3 × 2^-31, which is the shape of a
difference that cancelled rather than of a coefficient.

D86's stability trigger is right to refuse it. What it does about it is
ask for a refactorization and hand the iteration back unspent — and the
candidate is still there, still passes the ratio test's floor, and is
selected again.

## Why the refusal turns into a cycle rather than a retry

The guard against looping in the trigger is "take the pivot when
`n_updates == 0`", so a declined pivot cannot be declined twice in a row.
It is not declined twice in a row. What happens instead is:

1. With updates applied, the pricing picks row 253 (or 143). Its element
   is noise. The trigger declines it and asks for a rebuild.
2. The rebuild lands, and the pricing is re-run on a fresh factorization.
   **It now picks a different row**, 112, whose element is 0.67 or 1.5 and
   perfectly trustworthy. That pivot is taken.
3. One Forrest-Tomlin update later `n_updates` is 1 again, the pricing
   picks 143 (or 253) again, and step 1 repeats with the other member of
   the pair.

So the solve alternates between two bases, and each visit costs one
declined pivot plus one real one.

**Bland's rule cannot break this**, and the reason is exact: Bland's
anti-cycling argument assumes the pivot the rule selects is the pivot
that is performed. Here the rule selects 253 or 143 and the trigger
refuses it; the pivot actually performed is whatever a re-pricing on a
fresh factorization picks. The rule is switched on and is not in control
of the sequence.

## What would close it

Two candidates, and both are constants with two sides to sweep, so
neither is a one-line change:

- **Make the two thresholds agree.** A pivot the trigger will refuse
  should not be offered by the ratio test. `PIVOT_MIN` at 1e-9 against
  `LU_AGREE_TOL` at 1e-5 relative is the mismatch, and the sweep has to
  show what raising the floor costs the instances that use small pivots
  legitimately.
- **Remove the declined candidate for the retry.** The trigger knows
  which (row, column) it refused; excluding it from the next pricing
  would make the refusal progress instead of repeat. That needs somewhere
  to keep the exclusion and a rule for when it expires.

## Reproducing

```
make cli
bash bench/measurements/02-215/run.sh
```

About two minutes. The instrumented binary and its trace go to a
`mktemp -d` and nothing is written into the repository.
