# 02-141 — the model.c tests, and two ways a control can look like it passed

D229. Four tests in `tests/test_model.c`, one per sentence `src/model.c`
states about its two derived copies — the row-wise mirror and the scaling —
and about what a mutation may leave behind. `run-model-controls.sh` is the
proof each goes red when its sentence breaks, and `controls.txt` is the
record.

Seven arms, five breakers, all behaved. Getting there took three runs, and
the two failures on the way are worth more than the green result.

| arm | unit suite | verdict |
|---|---|---|
| intact | 34 tests, 0 failures | — |
| a matrix change keeps the row-wise mirror | 2 failures | its test is red |
| a bound or a cost throws both copies away | 1 failure | its test is red |
| the scaling reads a cost | 1 failure | its test is red |
| `jaos_delete_rows` refuses to empty a column | 4 failures | its test is red |
| `jaos_add_cols` leaves a column descending | 2 failures | its test is red |
| the recipe again, nothing broken | 34 tests, 0 failures | — |

## What the four tests state

- **Only a matrix change discards the derived copies.** The five operations
  that touch the matrix must discard both; `jaos_set_col_cost`,
  `jaos_set_col_bounds` and `jaos_set_row_bounds` must discard neither. Both
  halves are in one test, because a version that discards everything is never
  wrong and costs a re-scale on every branch of a branch-and-bound loop.
- **The scaling reads no bound and no cost.** Change every cost and every
  bound, recompute from scratch, and every factor has to come back identical
  to the bit. That is what makes the sentence above safe.
- **A column left empty by `jaos_delete_rows` is not an error.** It is still
  a column, with its cost and its bounds, and the derived copies rebuild on
  the model as it now stands.
- **Row order inside a column survives a chain of mutations.** The example is
  loaded with a column deliberately unsorted, so the load has to sort it, and
  every operation after that has to keep it sorted — `jaos_set_coefficient`
  binary-searches the column and the delete paths merge rather than re-sort.

## The first failure: a test that could not have caught its own defect

The scaling arm came back **green**, which is a control reporting that the
test would not notice the defect.

It was the test's fault. It changed each cost from one non-zero value to
another, and the break was keyed on `cost != 0.0` — so the doubling applied
before and after and cancelled out. The test now moves a cost **across
zero**, flips a sign, moves a magnitude seven orders, and takes a bound to
infinity, so any way of reading a cost or a bound shows up.

It also asserts, before comparing anything, that at least one factor is not
1.0. Comparing all-ones against all-ones would pass whatever the scaling
read.

## The second failure: a break that was overwritten before it could do anything

The repaired test still came back green, and this time the breaker was the
problem. It had been inserted at `m->scale_valid = true;` — which in
`src/scale.c` runs **before** the factors are computed, not after. The
doubling ran on freshly allocated memory and the real computation then wrote
over all of it.

Two green runs, two different causes, and both read exactly like a passing
control. The breaker sits after the dispatch now, and doubling was chosen
because it keeps every factor an exact power of two, so `scale.c`'s own
assert still holds and only the test can catch it.

## What the overlaps say

Three arms turn other tests red as well, and that is recorded rather than
tidied away:

- keeping the row-wise mirror also breaks
  `test_a_dimension_change_the_solve_can_see`;
- refusing to empty a column also breaks `test_deleting_renumbers_what_survives`
  and two others;
- an unsorted column also breaks
  `test_added_columns_append_and_leave_the_rest_alone`.

An arm passes when its **own** test is red; the file lists everything else
that moved with it, which is how a reader can see which contracts are shared.

## Running it

```
bash bench/measurements/02-141/run-model-controls.sh
```

Each arm is its own worktree of `HEAD` plus the working-tree copies of
`src/model.c`, `src/scale.c` and `tests/test_model.c`, so a break never
touches this tree. Exit 0 only when every arm behaved.
