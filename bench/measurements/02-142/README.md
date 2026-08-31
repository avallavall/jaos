# 02-142 — the last three tests of the debt, and an off-by-one that is a segfault

D230. Three tests for the three contracts `src/jaos_internal.h` states with
nothing beside them. `run-internal-controls.sh` is the proof each goes red
when its sentence breaks, and `controls.txt` is the record.

Eight arms, four breakers, two of them run twice. All behaved.

| arm | unit suite | what caught it |
|---|---|---|
| intact | 104 tests, 0 failures, exit 0 | — |
| `jm_pattern_order` leaves its bitmap dirty | **exit 134** | the entry assert |
| the same break under `-DNDEBUG` | **5 failures** | the tests |
| `jm_pattern_order` keeps a position at `limit` | **exit 134** | the entry assert |
| the same break under `-DNDEBUG` | **exit 139** | the hardware |
| `jm_nonbasic_insert` writes the wrong word | **4 failures** | the tests |
| the iteration split is never written back | **2 failures** | the tests |
| the recipe again, nothing broken | 104 tests, 0 failures, exit 0 | — |

## What the three tests state

- **`jm_pattern_order` sorts, deduplicates, drops what the limit excludes,
  and leaves its bitmap clean.** Ascending is not a preference: every
  consumer of a pricing row breaks its ties by scan position. The bitmap
  being clean on the way out is what lets the next caller skip clearing it,
  so the test calls it a second time with the **same, unzeroed** bitmap and
  requires the same answer.
- **The nonbasic bitmap matches a rebuild after a basis change.** The bitmap
  is maintained by hand at every site that moves a variable in or out of the
  basis, and `jm_nonbasic_build` is the only thing that writes it wholesale,
  so the property worth testing is that the two agree word for word. The
  variables moved are 64 and 65, which straddle a word boundary. The test
  also builds a bitmap from the status array *before* the change and requires
  it to differ, because otherwise the comparison is two copies of one call.
- **The primal iteration split is written on an interrupted exit.**
  `test_the_summary_separates_phase_1_from_phase_2` covers the successful
  exit and says in its own comment that the abandoned one is not reproduced
  there. This is that path: a forced primal solve stopped by a watcher. D194
  published a wrong count for eight netlib instances precisely because a
  phase 1 that ran and did not finish reported nothing.

## The finding: the same break, three different things catching it

D227 established that an arm which aborts on an assert is only half a
control — it proves the assert fires, not that anything catches the defect
where asserts are compiled out. So both `jm_pattern_order` breaks were run
again under `-DNDEBUG`, which is what `RELEASE_CFLAGS` carries. The two gave
different answers.

**The dirty bitmap is caught by the tests.** Five of them go red without any
assert, four of which existed before this campaign. So that contract is
guarded in every build.

**The off-by-one is caught by neither.** With asserts it aborts at 134; with
asserts compiled out the suite exits **139**, which is a segfault. The reason
is what the bound is for: `limit` sizes the bitmap at `(limit + 63) / 64`
words, so a position equal to `limit` indexes word `limit >> 6`, and when
`limit` is a multiple of 64 that word is one past the end. The range test is
the only thing keeping the write inside the allocation.

That is a better answer than a failing test. It says the defect cannot be
silent: a debug build stops on the assert and a release build stops on the
hardware. Compare D227's clamp, where the release build was completely quiet
and one comparison in the caller was the whole guard.

## Running it

```
bash bench/measurements/02-142/run-internal-controls.sh
```

Each arm is its own worktree of `HEAD` plus the working-tree copies of
`src/simplex.c` and `tests/test_simplex.c`. Exit 0 only when every arm
behaved, including the ones that have to fail.
