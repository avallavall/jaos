# 02-114 — the hand-over has 55000x of margin, and `s->col`'s contract stops being a comment

2026-08-26. `TODO.md` §0's remainder list, the last two items.

## The hand-over's margin, measured rather than assumed

Phase 1 stops when `primal_phase1_costs` returns exactly 0.0, meaning every
basic is inside `primal_tol` **of the `xb` it carried through the pivots**.
`run_primal` then refreshes — recomputing `xb` from a fresh factorization — and
re-checks the worst violation against the same `primal_tol`, exactly, with
nothing between them. A JAOS defect is reported if it fails.

So the question is how far `xb` moves across that refresh, against the bar it
is judged by (`handover-census.txt`):

| | |
|---|---|
| instances that reach the hand-over | **86 of 94** |
| refreshed violation exactly **0.0** | **62 of 86** |
| non-zero | 24 |
| **largest** | `ganges` at **1.81899e-12** |
| next | `greenbeb` 6.43e-13, `sierra` 4.55e-13, `greenbea` 2.29e-13 |
| `primal_tol` | 1e-07 |
| **worst as a fraction of the bar** | **0.000018** |

**The margin is 55000x, not zero.** The eight instances that never reach the
hand-over are the eight that never leave phase 1 (D195).

**No repair.** Widening the check would be a second constant with nothing
behind it. **Reopen conditions**: any instance whose ratio passes about 0.01, a
change to `primal_tol`, or a starting basis that is not the slack basis —
a crossover's arrives with drift a cold start never has.

## `s->col`'s contract is an assert now

`primal_bound_flip` reads `B^-1 M_q` out of `s->col` where the ratio test left
it. **`s->col` has five other writers**, two of which alias it as `rhs`, and the
contract was a comment. `jaos-testing`'s rule is that an invariant another
piece of code depends on is an assert or a test, and this project has a
documented case of a correct, prominent warning comment being violated by new
code and costing weeks.

The check recomputes the column into **its own buffer** — never the shared
scratch, which would corrupt what it observes — and compares **bit for bit**,
because the claim is that nothing wrote it rather than that something wrote
something close. `s->work` is saved and restored, so a debug build bills what
the release build bills. One FTRAN per flip, compiled out by `-DNDEBUG`.

### It is reached, and it catches a violation

Both questions in one experiment (`validate-assert.sh`,
`validate-assert.txt`): `s->col[0]` perturbed immediately before the check.

```
tests/test_simplex.c:3560: src/simplex.c:4172: primal_bound_flip:
    Assertion `chk[i] == s->col[i]' failed.
Aborted (core dumped)
```

**So the suite does reach the flip**, which a green assert alone would not have
told anyone, and the assert does its job.

## The cost

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical — the shipping build compiles the assert out.
`make configs` exits 0 on all five configurations, `sanitize` included, which is
where the check actually runs.
