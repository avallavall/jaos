# 02-110 — the campaign reports which method did the work, and the test that pins it is validated

2026-08-26. `TODO.md` §0's "either way `bench/primal.c` should report the
split", now closed.

## Why

D194, D195 and D196 were each spent working out from outside what a "primal"
campaign's iterations actually are. The runner reported `primal=iters/work`
for a solve the dual finished, because `reenter_after_settling` calls `run()`.
One column shows it.

## What landed

**`sx` gains `n_phase1_iters`, assigned after the phase-1 call in
`run_primal`** — so it is written on every exit, which is the whole point. The
count was previously readable only from `phase 1 reached a feasible point in N
iterations`, printed on **success**, so a phase 1 that ran and did not finish
read as one that never ran. That is what made D194 wrong about eight instances.

Both closing summaries carry it. `bench/primal.c` reads that line and prints
`split=p1:N/p2:N/dual:N` on every record line, **including the overrun
branch** — the branch that needed it most, since every overrun happens inside
phase 1 and the verdict alone does not say so.

## The cross-check

The runner's own summary, over all 94:

```
iterations by method: phase 1 336660 (39.5%), phase 2 97 (0.0%),
                      dual re-entry 515522 (60.5%)
```

**Identical to `bench/measurements/02-108/`**, which measured the same three
figures through a patched worktree and a separate probe. Two instruments, two
code paths, the same numbers.

Per instance, against 02-108 on five named cases — `wood1p` p1:3820 p2:0
dual:0, `pilot4` 2596/1/3323, `truss` 2802/1/2080, `stocfor1` 165/1/50,
`vtp-base` 87/5/1 — all five match exactly. `wood1p` is the one D194 recorded
backwards.

Everything else the campaign reports is unchanged: measured 55, overrun 7,
disagreed 31, errors 1, work geometric mean 3.9023, worst `sctap2` 14.8415.

## The test, and its negative control

`test_the_summary_separates_phase_1_from_phase_2` asserts the dual's line reads
zero on both counts, and that a cold forced-primal solve on a model whose slack
basis is primal infeasible reports a **non-zero** phase-1 count no larger than
its primal count.

**Validated by injecting the defect** (`validate-test.sh`, `validate-test.txt`):
`n_phase1_iters` forced to 0 in a worktree gives exactly one FAIL, and it is
this test, with the message `phase 1 ran from a primal infeasible start and
reported none`.

The first attempt at that control **broke the build instead**, because zeroing
the assignment left `phase1_entered` unused and `-Werror` refused it. A control
that cannot compile proves nothing, so the fault is now two statements and the
script prints every FAIL line rather than only the exit code.

**The non-success path is not reproduced in the suite** and the test says so:
it needs a phase 1 that takes many iterations and then runs out of budget,
which no two-row model reaches. `wood1p` is the named case in the campaign.

## Two defects this change shipped with, both caught before the record

- **`make configs` caught the test helper.** It is used only inside a block the
  two fault builds compile out, so `-Werror=unused-function` refused it there
  while a plain `make test` passed. The same shape as D154 and D-10.
- **`write_result` never learned the three new fields.** `read_result` expected
  17 and the `fprintf` still wrote 14, so every worker's file failed to parse
  and `make primal J=12` reported all 94 as `worker died`. **Invisible at
  `-j 1`**, which runs in process and never crosses that boundary — and `-j 1`
  is where the columns had been cross-checked. The `perl` substitution that
  should have applied it reported success because `perl` exits 0 whether or not
  `s///` matched; every other edit in the change verified its match count and
  this one did not.

## The cost

All three gate sets `gate: PASS`, `0 regressed, 0 improved, 0 new`, every file
in `bench/results/` byte-identical to the committed record. `make configs`
exits 0 on all five configurations.

## Re-running

`validate-test.sh` writes beside itself and replaces this directory's evidence.
Its worktree goes under `mktemp -d`, outside the repository.
