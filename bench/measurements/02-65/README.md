# 02-65 — Three of the five build configurations were broken, and nothing said so

2026-08-19. Started as `TODO.md` §5, a stale comment in `src/presolve.c`.
Verifying that the comment was stale needed the reference build, and the
reference build did not compile.

## What was asked

`TODO.md` §5 said the singleton-column replay's comment claims a frozen row is
never revisited for infeasibility, names `min x0 s.t. x0 + x1 = 100, x0 in
[4,4], x1 in [0,3]` as a model that publishes `x1 = 96`, and that the model
reports INFEASIBLE at HEAD. Either the defect closed and nobody updated the
comment, or the example never showed what it claimed.

**It closed.** `git log -S` puts the claim in `541f7dd` and the repair in
`7587ecd`, both 2026-08-14, the repair second. The repair is the frozen-row
feasibility test at the end of `jm_presolve_run`, and it carries four tests:
`test_a_frozen_row_that_cannot_be_satisfied_is_infeasible`,
`test_the_frozen_row_model_agrees_with_the_reference_build`,
`test_a_frozen_row_that_is_exactly_satisfiable_is_not_refused` and
`test_a_frozen_row_missed_at_scale_is_refused`. The comment is corrected, not
deleted: the site still cannot detect an infeasible model, and saying where
that now happens is worth more than saying nothing.

## What the check for it found

`make test EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE` does not compile at HEAD, and
neither do either of the two fault builds.

`make` decides what to rebuild from timestamps and does **not** track a change
in `EXTRA_CFLAGS`. Run right after a plain `make test`, with no source file
changed, it re-runs the plain binaries and exits 0. That is how `TODO.md` came
to state that all three of `make test`, the reference build and `make sanitize`
exit 0 at HEAD. Two of the three claims were true.

Blamed to `6db3bc6` (D151), which added `repair_fires_at` for the warm-repair
cap test without the fault-build guard the rest of `tests/test_presolve.c`
carries. Under any configuration that ignores that test the helper is defined
and never called, and `-Werror=unused-function` fails the build.

**This is the second occurrence.** D-10's own record says
`make_frozen_row_infeasible_model` broke `-DJAOS_PRESOLVE_FAULT_OFFBYONE` the
same way until 2026-08-15, and adds the reason nothing noticed: "the plain
build and the reference build are the two the loop actually runs". This time
the reference build broke too.

## The three repairs, each measured on its own

`stages.txt`, produced by `run-stages.sh`. The three repairs live in three
different files, so a stage is a `git stash` of a subset.

| stage | plain | reference | OFFBYONE | WRONGDUAL |
|---|---|---|---|---|
| 0 — HEAD | 221 pass | **no compile** | **no compile** | **no compile** |
| 1 — + the unused-function guard | 221 pass | 220 pass | **3 aborts**, 69 ran | **14 fail** |
| 2 — + the row-activity check's fault skip | 221 pass | 220 pass | **16 fail** | **14 fail** |
| 3 — + the positive-test guards | 221 pass | 220 pass | 163 pass, 0 fail | 160 pass, 0 fail |

None of the three is redundant, and stage 1 is why the other two were found at
all: a configuration that does not compile hides every defect behind it.

- **The unused-function guard** wraps `g_warm_repairs`, `count_warm_repair`
  and `repair_fires_at` in the same condition their only test already uses.
- **The row-activity check's fault skip.** D153's check compares published row
  activities against published columns. A fault build makes the replay wrong
  on purpose, so the check fired and aborted three of the eight test binaries
  before any negative test could report — 69 of 236 tests ran. Skipped under
  either fault build rather than weakened: the predicate is untouched on every
  build that ships.
- **The positive-test guards.** `tests/test_simplex.c` carried **no** fault
  guard at all, where `tests/test_presolve.c` carries thirty. Fifteen of the
  sixteen failures went through one helper, `solve_and_verify`, so the guard
  is in the helper; the sixteenth,
  `test_settling_up_reaches_the_optimum_a_shifted_basis_hid`, asserts an exact
  answer directly and takes its own.

## The green is checked, because green alone would prove nothing here

A guard that swallowed a negative test would leave a fault build green for the
wrong reason. `negative-tests.txt`:

- Under `-DJAOS_PRESOLVE_FAULT_OFFBYONE`, **8 of 8** off-by-one negative tests
  PASS and none is ignored.
- Under `-DJAOS_PRESOLVE_FAULT_WRONGDUAL`, **2 of 2** wrong-dual negative
  tests in `tests/test_presolve.c` PASS and neither is ignored.
- The control: on the plain build every one of those ten is IGNORED, which is
  what makes them negative tests rather than tests that pass anyway.

`test_check.c`'s `test_t1_flags_wrong_dual_sign` and
`test_t2_flags_wrong_dual_magnitude` appear in the same listing because their
names match. They are not fault-build tests — they build a wrong dual by hand
and pass on every configuration. Counted separately for that reason.

## The gate campaign carries over, measured

`presolve.o` at HEAD and on the repaired tree share an md5 under the release
flags, so the campaign at `01aca61` still describes the bytes about to land.
The row-activity check lives inside `#ifndef NDEBUG` and `-DNDEBUG` removes
it, so the release build never contained the code that changed.

`.claude/skills/jaos-measure/scripts/comment_only.sh` is that check, made
reusable. It replaces a one-off in `02-30` that hardcoded a since-deleted
worktree path and only handled `src/simplex.c`.

## Three ways this comparison reports a difference that is not one

All three were hit, in order, before the script gave a usable answer.
`build-reproducibility.txt`:

1. **`-flto`, which is the DEFAULT.** Two builds of ONE unedited tree produce
   **12 of 12 different object md5s**. The `.gnu.lto_*` sections carry a
   per-compilation seed. At `LTO=0` the same two builds are byte-identical.
   So `md5sum build/release/*.o` across two trees compares nothing at all
   under the shipping flags.
2. **`-g`.** Debug info records line numbers, so adding or removing a comment
   LINE changes `.debug_line` while `.text` is untouched. `02-30`'s script
   already knew this one.
3. **The source file's basename.** GCC writes it into the object as an
   `STT_FILE` symbol, so compiling the reference copy as `ref.c` differs from
   `presolve.c` on identical code. Found by dumping sections one at a time
   after the disassembly came back identical and the md5 did not.

`comment_only.sh` handles all three: it drops `-g` and `-flto`, and writes the
reference copy under its own basename in a temp directory.

## What is now guarded against a third occurrence

`make configs` builds all five configurations, each after `make clean`, and
fails if any one does. Validated both ways: it exits 0 on the repaired tree
and 2 with `tests/test_presolve.c` reverted to HEAD. It is not part of `make
test` because it costs five full rebuilds.

## Reproducing it

```
make configs                                     # the guard itself
bash bench/measurements/02-65/run-configs-sweep.sh   # HEAD vs repaired, + the controls
bash bench/measurements/02-65/run-stages.sh          # each repair on its own
bash .claude/skills/jaos-measure/scripts/comment_only.sh src/presolve.c
```

Both sweep scripts stash and restore the repaired files, so the tree has to be
otherwise settled before either runs.
