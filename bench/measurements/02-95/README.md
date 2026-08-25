# 02-95 — `pilot87`'s bound moves because its DUAL solution is not unique, and its primal answer does not move at all

D183. No source change. Two trees, two binaries, one variable.

## The standing debt this is for

`TODO.md` has carried it since D92: "`pilot87`'s suboptimality bound is not
understood — `gap_positive` moves 0.0068–26.7 across D92's variants while every
answer is inside tolerance."

D180 handed it a case the record could not resolve. At `REFACTOR_EVERY` 8 and
256, `pilot87` publishes the **identical objective**, `301.71035883543192`, and
**two different digests** — and `bench/run.c` hashes `x` and `y` into one, so
its record cannot say which half moved.

## The answer: the duals moved, the priced point did not

| | `REFACTOR_EVERY = 8` | `REFACTOR_EVERY = 256` |
|---|---|---|
| objective | 301.71035883543192 | **identical** |
| digest of `x` | `334a6e189adb4b45` | `875faca3a7332c93` |
| digest of `y` | `2e204845a11c08e6` | `a6d5e3366594ebae` |
| `gap_positive` | 0.00139018 | **0.00140689** |
| `relative_suboptimality` | 4.59245e-06 | 4.64766e-06 |
| `unquantified_rays` | **10** | **14** |
| basic members | 2031 of 2030 rows | 2031 of 2030 rows |

**The primal answer is the same to rounding.** 987 of 4883 columns move, and
**738 of them cost exactly zero** — they cannot touch the objective. The 249
that are priced move by at most **4.44e-15**, carrying 1.073e-13 of objective
traffic between them, of which 5.16e-15 survives. The largest single term is
column 15: `c = 2.82765`, `dx = 4.44e-15`.

**The duals are genuinely different.** 1817 of 2030 moved, **166 of them by more
than 1e-9 relative** — a rounding-level change cannot reach that. The largest
relative move is **55.7%** and the largest absolute is **1.79e-04**, on row 1079
going from -3.34e-04 to -1.55e-04. **None changes sign**, so both dual
solutions are feasible, which they have to be.

So: **`pilot87` has a non-unique dual solution.** Two runs land on different
dual vertices while the primal point stays where it is. `gap_positive` is built
from the duals, so it follows them; `unquantified_rays` counts columns whose
multiplier the checker calls zero, so it follows them too — 10 against 14.

**The bound moving is a property of the model, not a defect in the bound.**

## What this does NOT establish

D92's variants moved `gap_positive` from **0.0068 to 26.7**, a factor of 3900.
The two settings here move it by **1.2%**. The mechanism is demonstrated; the
magnitude is not reproduced, and D92's variants are not in this tree. Reading
this as "the 3900x is explained" would be claiming a span that was not
measured.

## Two things the run says in passing

`pilot87` publishes **2031 basic members against 2030 rows at both settings** —
over by one, which is §2's defect and puts it among D179's 24.

The 738 zero-cost columns that move are what a flat face looks like from
outside: free to slide with nothing pricing them.

## Reproducing

```
bench/measurements/02-95/run-split-digest.sh 8 256 pilot87
```

Two worktrees in `$(mktemp -d)`, one binary each, and the objective is the
canary: if the two settings do not publish the same one, the premise is gone
and every comparison below it is about two different answers.

**The cost column is in the dump on purpose.** The first reading of this said
"two genuinely different vertices with the same objective" from the digests
alone, and 987 columns moving while `c'x` holds to the last bit is a claim that
needs the cost beside it. With the cost, most of those columns turn out to
price at nothing and the rest to move at the arithmetic floor, which is a
different statement.
