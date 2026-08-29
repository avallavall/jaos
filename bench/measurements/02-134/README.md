# 02-134 — the asserts D219 adds to `model.c`, and three attempts at a control that proves anything

`bench/measurements/02-121/model.c.md` listed the contracts the comment purge
kept as prose because other code depends on them. This directory is the
evidence for the assert half of that list.

## What has to be shown

An assert that never fires cannot be told from one that cannot fire. D216 set
the shape for `lu.c`: 0 firings on the candidate, then break the invariant and
watch **the same assert, named**, fire. Both halves or neither.

## The two versions that proved nothing

**Version one reported 0 assertions in all three arms, including both broken
ones.** It read as a clean result. Every arm was dead, and each for a
different reason:

- The pair breaker made `store_basis` leave `start_row_status` null. The very
  next statement in `store_basis` detects exactly that and clears **both**
  arrays, restoring the pair. The breaker was undone by the code it was
  breaking.
- The status breaker made `publish` publish an objective on its non-OPTIMAL
  branch, then ran the standard 94 — where every solve is OPTIMAL and that
  branch never executes.
- Nothing separated "the invariant held" from "`-UNDEBUG` never reached the
  compiler". That is D82's failure in another shape, and it would have made
  every arm read 0 whatever the code did.

**Version two added the canary** — an assert false by construction, in the
function under test. It fires 94 times, so the flag does reach the compiler.
With that settled, the pair arm's 0 firings finally meant something, and what
it meant is that **the gate cannot reach that assert at all**:

- the gate never changes a model's dimensions after solving it, so
  `basis_extend` and `basis_survives_or_goes` never run;
- `publish` calls `jm_model_remember_basis` once per process, and nothing
  reads the pair afterwards.

0 of 94 is what an unreachable assert looks like. `jaos-testing` already says
to check that something runs a function before trusting a green gate about it.
This is the first time that rule caught one of this project's own asserts.

## Version three, and what it says

The unit suite reaches the pair: `test_a_dimension_change_the_solve_can_see`
solves, adds a row, adds a column and deletes both. That arm runs there, with
an unbroken companion arm to show the suite is quiet when the invariant holds.

| arm | what it breaks | where it runs | fired | which assert |
|---|---|---|---|---|
| `canary` | an assert false by construction | gate, 94 | **94** | the canary — `-UNDEBUG` reaches the compiler |
| `live` | nothing | gate, 94 | 0 | — |
| `pair` | the start arrays unpaired, past every guard | unit suite | **1** | `JM_BASIS_PAIRED(m)`, core dumped |
| `pair-live` | nothing | unit suite | 0 | 29 tests, 0 failures |
| `status` | `publish` publishes on a non-OPTIMAL solve | infeasible set, 29 | **19** | `m->solve_status == JAOS_SOLVE_OPTIMAL` |

Each arm has its own worktree, its own `make clean` and its own binary (D82),
and each carries the working tree's `src/` on top of the ref, so the control
runs before the commit as well as after (D217).

`assert-control.txt` is the run. `run-assert-control.sh` exits 2 rather than 1
when the canary is silent, because that is not a failed arm — it is a reading
that is not about asserts at all.

## The fifth contract, which is not an assert

`-ffast-math` is a build error now (`src/jaos_internal.h`).
`jm_two_product_residue` splits a product by Dekker's method and the residue
only exists if the compiler does not reassociate; `-ffast-math` and `-Ofast`
enable `-fassociative-math`, which deletes it and leaves a plausible wrong
answer. Confirmed the only way it can be: compiling with the flag, and
watching the build fail.
