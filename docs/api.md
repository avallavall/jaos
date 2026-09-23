# The C API

`include/jaos.h` is the public header of JAOS. It declares 200 functions,
the enums and structs they use, and the version macros. This page describes
every function in the order the header declares them. Each statement comes
from the code in `src/`. `docs/cli.md` shows the same calls behind the
command-line tool, and `docs/format-support.md` gives the rules of every
file format.

Every function and type name starts with `jaos_`, and every macro and enum
value starts with `JAOS_`. A model is an opaque `jaos_model`. The caller
holds a pointer to one and reads or changes its contents only through the
functions below.

## Conventions

### Return values and errors

Most calls return a `jaos_status`:

| Value | Meaning |
|---|---|
| `JAOS_OK` | The call did what its entry says. |
| `JAOS_ERR_INVALID_INPUT` | An argument, the model or a file breaks a rule the call checks. |
| `JAOS_ERR_OUT_OF_MEMORY` | An allocation failed. |
| `JAOS_ERR_IO` | A file could not be opened, read or written. |
| `JAOS_ERR_NUMERICAL` | A computation could not reach a result. The entries say when. |

The header numbers these values 0 to 4, in the order of the table.

A call that answers a question returns the answer itself: a count, a flag,
a number or a string. For a null model such a call returns what it returns
for a new, empty model. Every function carries `JAOS_NODISCARD` except
`jaos_model_free` and `jaos_clear_basis`, which return nothing. Under C23
or C++17 the macro expands to `[[nodiscard]]`, so the compiler warns when a
caller ignores a result. Every function also carries `JAOS_API`, which
exports it from a shared library. On Windows it expands to
`__declspec(dllexport)` while `JAOS_BUILD` is defined, and to nothing
otherwise. With GCC or Clang on other systems it expands to
`__attribute__((visibility("default")))`. The signatures on this page leave
both macros out.

`jaos_model_error(m)` returns the message of the last failure on `m`. The
message names the row, column, file line or value at fault. It stays until
a later message replaces it. `jaos_solve`, `jaos_iis` and `jaos_feasrelax`
clear it when they start, and some calls clear it when they succeed. The
model holds it in a buffer of 256 bytes, so a longer message is cut to its
first 255 bytes. Not every failure writes a message. A null pointer or an
index out of range often returns `JAOS_ERR_INVALID_INPUT` alone.

### Ownership and lifetimes

`jaos_model_new` creates a model and `jaos_model_free` frees it.
`jaos_model_copy` and `jaos_iis_model` also create models. The caller frees
those with `jaos_model_free` as well.

The library copies every array and string it receives. The caller may free
or reuse them as soon as the call returns. The library keeps only the
callback functions and their `user` pointers.

The caller supplies every output array. An array over the columns holds
`jaos_num_col(m)` values, and an array over the rows holds `jaos_num_row(m)`
values. Any other size comes from a count call that the entry names. An
output that the entry calls optional may be null, and the call then skips
it.

The strings that the library returns stay valid for these spans:

- `jaos_version`, `jaos_status_str`, `jaos_solve_status_str` and
  `jaos_option_name` return static strings. They stay valid for the life of
  the program.
- `jaos_model_error` returns a pointer into the model. The pointer stays
  valid until `jaos_model_free`. Its text changes when a later call writes a
  message.
- Five getters return exact values as strings that the model owns:
  `jaos_exact_col_value`, `jaos_exact_row_dual`, `jaos_exact_objective`,
  `jaos_exact_row_multiplier` and `jaos_exact_col_direction`. The model
  frees these strings at the next `jaos_solve`, `jaos_verify`,
  `jaos_verify_basis`, `jaos_exact_certificate` or
  `jaos_exact_unbounded_ray`. It also frees them at an edit that clears the
  answer, when `jaos_load_lp` or a reader replaces the model, and at
  `jaos_model_free`.
- The `line` that a log callback receives is valid only during that call.
  So is the `col_value` array of an incumbent or node event.

### Indices, infinity and edits

Rows and columns count from 0, in the order they were loaded or added.
Cones and SOS sets count from 0 in the order they were added. A deletion
keeps the order of what remains and numbers it from 0 again.

`jaos_infinity()` returns IEEE positive infinity. A missing lower bound is
`-jaos_infinity()`, and a missing upper bound is `jaos_infinity()`. Costs,
matrix entries and the objective constant must be finite. A bound may be
infinite, and no bound may be NaN.

An edit to the model's data clears the last answer. `jaos_status_of` then
returns `JAOS_SOLVE_NOT_RUN`, and the solve counters read 0.
`jaos_objective`, `jaos_solution` and every other call that needs an answer
fail until the next solve. Setting a name, an option, a MIP start or a basis
does not clear the answer. The basis that the last solve left stays on the
model as the start of the next solve. `jaos_clear_basis` removes it.

### Options

Each solve setting has a setter and an option name. `jaos_set_option` sets
any of them from text, and `jaos_get_option` reads any of them back as
text. `jaos_get_option` is the read path for the settings. Only the thread
count and the algorithm also have getters of their own. `jaos_read_options`
applies a file of settings. Option names and word values match in any case.
The options belong to the model. `jaos_model_copy` copies them, and
`jaos_load_lp` and the file readers keep them.

The table lists every setter with its option name, in the order that
`jaos_option_name` gives. Through `jaos_set_option`, an integer takes
decimal digits. A number takes C-locale floating-point text such as `1e-6`
or `inf`. A boolean takes `true` or `false`, `on` or `off`, `yes` or `no`,
and `1` or `0`. A word takes one of the words shown. The default is the
value `jaos_get_option` reports before any setter runs.

| Setter | Option | Value | Default |
|---|---|---|---|
| `jaos_set_work_limit` | `work_limit` | integer | 0, no limit |
| `jaos_set_time_limit` | `time_limit` | number | 0, no limit |
| `jaos_set_primal_tolerance` | `primal_tolerance` | number | 1e-7 |
| `jaos_set_dual_tolerance` | `dual_tolerance` | number | 1e-9 |
| `jaos_set_algorithm` | `algorithm` | `dual`, `primal`, `barrier`, `pdlp`, `concurrent` | `dual` |
| `jaos_set_log_level` | `log_level` | `off`, `summary`, `progress`, `detail` | `off` |
| `jaos_set_mip_gap` | `mip_gap` | number | 1e-6 |
| `jaos_set_mip_node_limit` | `mip_node_limit` | integer | 0, no limit |
| `jaos_set_mip_tree_batch` | `mip_tree_batch` | integer | 1 |
| `jaos_set_mip_branching` | `mip_branching` | `pseudocost`, `most-fractional` | `pseudocost` |
| `jaos_set_mip_reliability` | `mip_reliability` | integer | 0 |
| `jaos_set_mip_probe_cap` | `mip_probe_cap` | number | 0, no cap |
| `jaos_set_mip_probe_depth` | `mip_probe_depth` | integer | -1, every depth |
| `jaos_set_mip_cut_rounds` | `mip_cut_rounds` | integer | 1 |
| `jaos_set_mip_cut_depth` | `mip_cut_depth` | integer | 3 |
| `jaos_set_mip_cut_drop` | `mip_cut_drop` | boolean | true |
| `jaos_set_mip_node_cut_cap` | `mip_node_cut_cap` | integer | 4 |
| `jaos_set_mip_cover_rounds` | `mip_cover_rounds` | integer | 4 |
| `jaos_set_mip_cut_stall` | `mip_cut_stall` | number | 0 |
| `jaos_set_mip_node_cut_stall` | `mip_node_cut_stall` | number | 0 |
| `jaos_set_mip_root_cut_drop` | `mip_root_cut_drop` | boolean | true |
| `jaos_set_mip_cover_lift` | `mip_cover_lift` | boolean | false |
| `jaos_set_mip_mir_rounds` | `mip_mir_rounds` | integer | 6 |
| `jaos_set_mip_node_mir` | `mip_node_mir` | boolean | false |
| `jaos_set_mip_mir_aggregate` | `mip_mir_aggregate` | integer | 0 |
| `jaos_set_mip_dive` | `mip_dive` | boolean | false |
| `jaos_set_mip_dive_child` | `mip_dive_child` | `nearer`, `up`, `down`, `pseudocost` | `nearer` |
| `jaos_set_mip_dive_backtrack` | `mip_dive_backtrack` | integer | 0 |
| `jaos_set_mip_dive_gap` | `mip_dive_gap` | number | 0 |
| `jaos_set_mip_dive_degrade` | `mip_dive_degrade` | number | 0 |
| `jaos_set_mip_dive_heuristic` | `mip_dive_heuristic` | integer | 50 |
| `jaos_set_mip_dive_heuristic_depth` | `mip_dive_heuristic_depth` | integer | 0 |
| `jaos_set_mip_rins` | `mip_rins` | integer | 0 |
| `jaos_set_mip_feaspump` | `mip_feaspump` | integer | 20 |
| `jaos_set_mip_pump_general` | `mip_pump_general` | boolean | false |
| `jaos_set_mip_pump_obj` | `mip_pump_obj` | number | 0.5 |
| `jaos_set_mip_pump_always` | `mip_pump_always` | boolean | false |
| `jaos_set_mip_rcfix` | `mip_rcfix` | boolean | false |
| `jaos_set_mip_tighten` | `mip_tighten` | boolean | true |
| `jaos_set_mip_probing` | `mip_probing` | boolean | false |
| `jaos_set_mip_probing_cap` | `mip_probing_cap` | number | 1 |
| `jaos_set_mip_clique_fix` | `mip_clique_fix` | boolean | false |
| `jaos_set_mip_conflicts` | `mip_conflicts` | boolean | true |
| `jaos_set_mip_symmetry` | `mip_symmetry` | boolean | false |
| `jaos_set_mip_orbital` | `mip_orbital` | boolean | true |
| `jaos_set_mip_propagate` | `mip_propagate` | integer | -1, the tree's own choice: 4 on a quadratic objective, 0 otherwise |
| `jaos_set_mip_propagate_depth` | `mip_propagate_depth` | integer | -1, every node |
| `jaos_set_mip_heuristics` | `mip_heuristics` | boolean | true |
| `jaos_set_mip_pool_size` | `mip_pool_size` | integer | 1 |
| `jaos_set_mip_cutoff` | `mip_cutoff` | number | `inf`, no cutoff |
| `jaos_set_mip_clique_rounds` | `mip_clique_rounds` | integer | 4 |
| `jaos_set_mip_zero_half_rounds` | `mip_zero_half_rounds` | integer | 0 |
| `jaos_set_mip_flow_cover_rounds` | `mip_flow_cover_rounds` | integer | 0 |
| `jaos_set_mip_local_branching` | `mip_local_branching` | integer | 0 |
| `jaos_set_mip_node_select` | `mip_node_select` | integer | 1 |
| `jaos_set_mip_restart` | `mip_restart` | boolean | false |
| `jaos_set_threads` | `threads` | integer | 1 |

The calls that take data have no option name. These are the model's own
setters, `jaos_set_mip_start`, `jaos_set_basis` and the four callback
setters.

### Determinism

JAOS gives bit-identical results on every machine and in every run. The
same model with the same options gives the same answer, the same basis, the
same `jaos_work_units` and the same files. A time limit is the one
exception. Where `jaos_set_time_limit` stops a run depends on the clock, so
two runs stopped by it can differ. `jaos_set_work_limit` stops a run at the
same point on every machine, because the solver counts work units in its
kernels. `docs/work-units.md` describes that count. `jaos_solve_time` reads
the clock, so its value changes from run to run.

### Threads

`jaos_set_threads` changes four things:

- The concurrent solve, `JAOS_ALGORITHM_CONCURRENT`, runs its three methods
  at once from its second round on. It does so only when the thread count is
  above 1 and no work limit is set. When a work limit is set, it runs the
  three methods one after another in every round. Its first round always
  runs them one after another. Each method runs on one thread, so the
  concurrent solve's barrier computes its Cholesky factor on one thread.
- The barrier computes its Cholesky factor in blocks of rows. It spreads a
  block over up to that many threads, at most 64, when the block holds
  enough work.
- The conic branch and bound solves the relaxations of one round on up to
  that many threads, each on its own copy of the model.
- The linear branch and bound does the same.

`jaos_set_mip_tree_batch` sets the size of a round in both trees. At its
default of 1, a round holds one node.

In each case the answer and `jaos_work_units` are the same at any thread
count. Only `jaos_solve_time` changes. Everything else runs on the thread
that called `jaos_solve`.

The library holds no global state, so different models can be solved in
different threads at the same time. Calls on one model are not
synchronised, so two threads must not use the same model at once. The
progress callback runs on the threads of the concurrent solve when it runs
its methods at once. It also runs on the threads of the linear branch and
bound when that tree solves a round on more than one thread. In both cases
two threads can call it at the same time. The log, incumbent and node
callbacks run on the thread that called `jaos_solve`.

### Values that overflow

A model with finite data can still produce a value that a double cannot
hold, when a product or a sum grows past the largest double. Every optimum
passes one test before the solve publishes it. When the objective, a column
value, a reduced cost, a row activity or a row dual is not finite, the solve
ends `JAOS_SOLVE_NUMERICAL_ERROR`. `jaos_model_error` then names the value
and its index. When the terms of a row overflow and their sum is not a
number, the checker counts an infinite violation of the row's finite
bounds, so `primal_feasible` reads false. The call still returns `JAOS_OK`.

### Log lines

The log callback receives one line per call, with the line's level and
without a trailing newline. A line reaches the callback only when the
model's log level is at least the line's level. The default level is
`JAOS_LOG_OFF`, so a callback alone receives nothing. The library formats
each line in a buffer of 1024 bytes. A line longer than 1023 bytes reaches
the callback cut to its first 1023 bytes. Logging does not change an
answer.

## Version, status words and infinity

**`jaos_version`**\
`const char *jaos_version(void)`\
Returns `JAOS_VERSION_STRING`, the version the library was built as. The
macros `JAOS_VERSION_MAJOR`, `JAOS_VERSION_MINOR` and `JAOS_VERSION_PATCH`
hold its three numbers as integer constants, and `JAOS_VERSION_STRING`
joins them with dots. In release 0.4.0 they are 0, 4 and 0.

**`jaos_status_str`**\
`const char *jaos_status_str(jaos_status s)`\
Returns a short English name for a call status: `"ok"`, `"invalid input"`,
`"out of memory"`, `"i/o error"` or `"numerical error"`. A value outside the
enum gives `"unknown status"`.

**`jaos_solve_status_str`**\
`const char *jaos_solve_status_str(jaos_solve_status s)`\
Returns a short English phrase for a solve outcome, such as `"optimal"` or
`"work limit reached"`. A value outside the enum gives `"unknown status"`.

**`jaos_infinity`**\
`double jaos_infinity(void)`\
Returns IEEE positive infinity. Pass it, or its negation, for a bound that
does not exist.

## Model life cycle

**`jaos_model_new`**\
`jaos_status jaos_model_new(jaos_model **out)`\
Creates an empty model and stores it in `*out`. The model has no rows, no
columns and every option at its default. The call fails when `out` is null
or the allocation fails, and a failed allocation leaves `*out` null.

**`jaos_model_free`**\
`void jaos_model_free(jaos_model *m)`\
Frees the model and everything it holds, including the answer and the
strings it returned. A null `m` does nothing.

## Loading a linear program

The arrays describe the objective `col_cost'x + obj_offset`, minimised or
maximised, subject to `row_lower <= A x <= row_upper` and
`col_lower <= x <= col_upper`. The matrix `A` is stored column by column.
Column `j` holds the entries `a_start[j]` to `a_start[j+1] - 1` of
`a_index` and `a_value`, and `a_index` holds row indices. So `a_start` has
`num_col + 1` entries. It starts at 0, ends at `num_nz` and never
decreases. It may be null when `num_nz` is 0.

**`jaos_load_lp`**

```c
jaos_status jaos_load_lp(jaos_model *m,
    int64_t num_col, int64_t num_row,
    jaos_obj_sense sense, double obj_offset,
    const double *col_cost,
    const double *col_lower, const double *col_upper,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);
```

Replaces the whole model with the linear program the arrays describe. It
keeps the options and the callbacks. It discards everything else: the names,
the model's name, the integer marks, the quadratic parts, the cones, the SOS
sets, the indicator rows, the MIP start, the basis and the answer. It drops
zero entries and sorts each column by row index. It fails with
`JAOS_ERR_INVALID_INPUT` and no message when a count is negative, `sense` is
neither `JAOS_MINIMIZE` nor `JAOS_MAXIMIZE`, or a needed array is null. It
fails the same way when a cost, an entry or `obj_offset` is not finite, or a
bound is NaN. It also fails when `a_start` breaks the rules above, or a
column names a row out of range or twice.

## Sizes, costs, bounds and the objective

Each call here that returns a `jaos_status` fails with
`JAOS_ERR_INVALID_INPUT` when `m` is null or an index is out of range. Each
setter clears the last answer when it succeeds.

**`jaos_num_col`**\
`int64_t jaos_num_col(const jaos_model *m)`\
Returns the number of columns.

**`jaos_num_row`**\
`int64_t jaos_num_row(const jaos_model *m)`\
Returns the number of rows.

**`jaos_num_nz`**\
`int64_t jaos_num_nz(const jaos_model *m)`\
Returns the number of stored matrix entries. The model stores no entry equal
to 0.

**`jaos_col_cost`**\
`jaos_status jaos_col_cost(const jaos_model *m, int64_t col, double *cost)`\
Stores the cost of column `col` in `*cost`. It fails when `cost` is null.

**`jaos_col_bounds`**\
`jaos_status jaos_col_bounds(const jaos_model *m, int64_t col, double *lower, double *upper)`\
Stores the bounds of column `col` in `*lower` and `*upper`. Both outputs are
optional.

**`jaos_row_bounds`**\
`jaos_status jaos_row_bounds(const jaos_model *m, int64_t row, double *lower, double *upper)`\
Stores the bounds of row `row` in `*lower` and `*upper`. Both outputs are
optional.

**`jaos_set_col_cost`**\
`jaos_status jaos_set_col_cost(jaos_model *m, int64_t col, double cost)`\
Sets the cost of column `col`. It fails when `cost` is not finite.

**`jaos_set_col_bounds`**\
`jaos_status jaos_set_col_bounds(jaos_model *m, int64_t col, double lower, double upper)`\
Sets both bounds of column `col`. It accepts a lower bound above the upper
one, and the simplex reports such a model infeasible. It fails when a bound
is NaN.

**`jaos_set_row_bounds`**\
`jaos_status jaos_set_row_bounds(jaos_model *m, int64_t row, double lower, double upper)`\
Sets both bounds of row `row`. The rules of `jaos_set_col_bounds` apply.

**`jaos_objective_sense`**\
`jaos_status jaos_objective_sense(const jaos_model *m, jaos_obj_sense *sense)`\
Stores `JAOS_MINIMIZE` (0) or `JAOS_MAXIMIZE` (1) in `*sense`. It fails
when `sense` is null.

**`jaos_objective_offset`**\
`jaos_status jaos_objective_offset(const jaos_model *m, double *offset)`\
Stores the objective's constant term in `*offset`. It fails when `offset` is
null.

**`jaos_set_objective_sense`**\
`jaos_status jaos_set_objective_sense(jaos_model *m, jaos_obj_sense sense)`\
Sets the objective's sense. It fails when `sense` is neither `JAOS_MINIMIZE`
nor `JAOS_MAXIMIZE`.

**`jaos_set_objective_offset`**\
`jaos_status jaos_set_objective_offset(jaos_model *m, double offset)`\
Sets the objective's constant term. It fails when `offset` is not finite.

## Names

A row or a column without a name of its own answers to a positional name.
Column `j` is `C<j+1>` and row `i` is `R<i+1>`, counting from 1. The
objective is `COST` until it gets a name. A name is 1 to `JAOS_NAME_MAX`
bytes, which is 255, with no whitespace and no control character. A buffer
of `JAOS_NAME_MAX + 1` bytes holds any name the model returns. The model
accepts two rows or two columns with one name. The file writers that write
names refuse them. A new name does not clear the answer.

Each call here fails with `JAOS_ERR_INVALID_INPUT` when `m` is null or an
index is out of range.

**`jaos_col_name`**\
`jaos_status jaos_col_name(const jaos_model *m, int64_t col, char *buf, int64_t cap)`\
Copies the name of column `col` into `buf`, with its terminating zero. `cap`
is the size of `buf`. The call fails when `buf` is null or `cap` is not
larger than the name's length, and the message then says how many bytes the
name needs.

**`jaos_row_name`**\
`jaos_status jaos_row_name(const jaos_model *m, int64_t row, char *buf, int64_t cap)`\
Copies the name of row `row` into `buf`. It fails as `jaos_col_name` does.

**`jaos_objective_name`**\
`jaos_status jaos_objective_name(const jaos_model *m, char *buf, int64_t cap)`\
Copies the objective's name into `buf`. It fails as `jaos_col_name` does.

**`jaos_set_col_name`**\
`jaos_status jaos_set_col_name(jaos_model *m, int64_t col, const char *name)`\
Gives column `col` the name `name`. A null or empty `name` removes the
column's own name, so the column answers to its positional name again. The
call fails when `name` breaks the rule above.

**`jaos_set_row_name`**\
`jaos_status jaos_set_row_name(jaos_model *m, int64_t row, const char *name)`\
Gives row `row` the name `name`, under the rules of `jaos_set_col_name`.

**`jaos_set_objective_name`**\
`jaos_status jaos_set_objective_name(jaos_model *m, const char *name)`\
Gives the objective the name `name`. A null or empty `name` restores
`COST`. The call fails when `name` breaks the rule above.

**`jaos_col_index`**\
`jaos_status jaos_col_index(jaos_model *m, const char *name, int64_t *col)`\
Finds the column that answers to `name` and stores its index in `*col`. A
positional name finds a column only when that column has no name of its
own. When two columns share a name, the call finds the one with the lower
index. The first call builds a lookup table, which is why `m` is not const.
The call fails with a message when no column answers to `name`.

**`jaos_row_index`**\
`jaos_status jaos_row_index(jaos_model *m, const char *name, int64_t *row)`\
Finds the row that answers to `name`, under the rules of `jaos_col_index`.

## Integer and semi-continuous columns

Each call here fails with `JAOS_ERR_INVALID_INPUT` when `m` is null or `col`
is out of range. A setter clears the last answer when the mark changes.

**`jaos_set_col_integer`**\
`jaos_status jaos_set_col_integer(jaos_model *m, int64_t col, bool is_integer)`\
Marks column `col` integer or continuous. The column's bounds do not
change, so a binary column is an integer column with bounds 0 and 1. The
call refuses to clear the mark of a column that switches an indicator row.

**`jaos_col_integer`**\
`jaos_status jaos_col_integer(const jaos_model *m, int64_t col, bool *is_integer)`\
Stores in `*is_integer` whether column `col` is integer. It fails when
`is_integer` is null.

**`jaos_set_col_semicontinuous`**\
`jaos_status jaos_set_col_semicontinuous(jaos_model *m, int64_t col, bool is_semi)`\
Marks column `col` semi-continuous or clears the mark. A semi-continuous
column is 0 or lies between its bounds. A column with both marks is
semi-integer.

**`jaos_col_semicontinuous`**\
`jaos_status jaos_col_semicontinuous(const jaos_model *m, int64_t col, bool *is_semi)`\
Stores in `*is_semi` whether column `col` is semi-continuous. It fails when
`is_semi` is null.

## Quadratic objective

With a quadratic part, the objective is `c'x + ½ x'Qx` plus the constant,
with `Q` symmetric. The model holds the diagonal of `Q` and its strict lower
triangle. `jaos_solve` checks when it starts that the objective is convex.
`Q` must be positive semi-definite when minimising and negative
semi-definite when maximising, and the solve refuses the model otherwise.
The solve also refuses a quadratic objective under the primal simplex, PDLP
or the concurrent solve. Under the default algorithm the barrier solves
such a model.

**`jaos_set_col_quadratic`**\
`jaos_status jaos_set_col_quadratic(jaos_model *m, int64_t col, double q)`\
Sets the diagonal entry `Q[col][col]` to `q`, so the objective gains
`½ q x_col²`. It clears the last answer when the value changes. It fails
when `m` is null, `col` is out of range or `q` is not finite.

**`jaos_col_quadratic`**\
`jaos_status jaos_col_quadratic(const jaos_model *m, int64_t col, double *q)`\
Stores the diagonal entry of column `col` in `*q`. The entry is 0 when the
column has none. The call fails when `q` is null or `col` is out of range.

**`jaos_set_quadratic`**\
`jaos_status jaos_set_quadratic(jaos_model *m, int64_t num_nz, const int64_t *rows, const int64_t *cols, const double *values)`\
Replaces all of `Q` with `num_nz` triplets and clears the last answer.
`num_nz = 0` removes the quadratic part. A triplet `(i, j, v)` with
`i != j` sets both `Q[i][j]` and `Q[j][i]` to `v`. Diagonal triplets on one
column add up, and zero values are skipped. The call fails when an index is
out of range, a value is not finite, or one pair appears twice in either
order.

**`jaos_quadratic_nz`**\
`int64_t jaos_quadratic_nz(const jaos_model *m)`\
Returns the number of entries that `jaos_quadratic` fills: the nonzero
diagonal entries plus the pairs of the strict lower triangle.

**`jaos_quadratic`**\
`jaos_status jaos_quadratic(const jaos_model *m, int64_t *rows, int64_t *cols, double *values)`\
Fills the three arrays with the stored entries of `Q`, each with
`rows[k] >= cols[k]`. The entries come column by column, with each column's
diagonal entry first. The arrays must hold `jaos_quadratic_nz(m)` entries.
The call fails when any of the three is null, even for an empty `Q`.

## SOS sets

A model with an SOS set is a MIP.

**`jaos_add_sos`**\
`jaos_status jaos_add_sos(jaos_model *m, int type, int64_t n, const int64_t *cols, const double *weights)`\
Adds a special ordered set over the `n` columns in `cols`, with the weights
in `weights`. In a set of type 1, at most one member is nonzero. In a set of
type 2, at most two members are nonzero, and they are adjacent in weight
order. The call fails when `type` is not 1 or 2, `n` is below 1, a member is
not a column or appears twice, or a weight is not finite or equals another.

**`jaos_num_sos`**\
`int64_t jaos_num_sos(const jaos_model *m)`\
Returns the number of SOS sets.

## Cones

A cone constrains an ordered list of columns `(x0, x1, ...)`. A quadratic
cone holds `x0 >= ||(x1, ...)||`. A rotated cone holds
`2 x0 x1 >= ||(x2, ...)||²` with `x0 >= 0` and `x1 >= 0`. The conic interior
point of `src/conic.c` solves a model with a cone or a quadratic row. The
dual and barrier algorithm settings allow such a model, and `jaos_solve`
refuses it under the other three. A column in a cone cannot be deleted until
the cone is deleted.

**`jaos_add_cone`**\
`jaos_status jaos_add_cone(jaos_model *m, jaos_cone_type type, int64_t n, const int64_t *cols)`\
Adds a cone over the `n` columns in `cols` and clears the last answer.
`type` is `JAOS_CONE_QUADRATIC` (1) or `JAOS_CONE_ROTATED` (2). The enum
starts at 1, so 0 is not a cone type. `cols[0]` is `x0`, and for a rotated
cone `cols[1]` is `x1`. The call fails when `cols` is null or `type` is
neither `JAOS_CONE_QUADRATIC` nor `JAOS_CONE_ROTATED`. It also fails when
`n` is below 1 for a quadratic cone or below 2 for a rotated one, or when a
member is not a column or appears twice.

**`jaos_num_cones`**\
`int64_t jaos_num_cones(const jaos_model *m)`\
Returns the number of cones.

**`jaos_cone`**\
`jaos_status jaos_cone(const jaos_model *m, int64_t k, jaos_cone_type *type, int64_t *n, int64_t *cols)`\
Stores the kind, the size and the columns of cone `k`. Each output is
optional, and `cols` must hold `*n` entries. The call fails when `k` is out
of range.

**`jaos_delete_cones`**\
`jaos_status jaos_delete_cones(jaos_model *m, int64_t num_del, const int64_t *cones)`\
Removes the `num_del` cones listed in `cones` and clears the last answer.
The other cones keep their order and are numbered from 0 again. The call
fails when an index is out of range or appears twice.

## Quadratic rows

A row may carry a quadratic part beside its linear one, so its activity is
`a'x + ½ x'Qx`. The triplets follow the rules of `jaos_set_quadratic`.
`jaos_solve` refuses a row that has a quadratic part and two finite bounds.
It also refuses a quadratic part that is not convex on the row's finite
side.

**`jaos_set_row_quadratic`**\
`jaos_status jaos_set_row_quadratic(jaos_model *m, int64_t row, int64_t num_nz, const int64_t *rows, const int64_t *cols, const double *values)`\
Replaces the quadratic part of row `row` with `num_nz` triplets and clears
the last answer. `num_nz = 0` removes the part. The call fails when `row` or
a column index is out of range, a value is not finite, or one pair appears
twice.

**`jaos_row_quadratic_nz`**\
`int64_t jaos_row_quadratic_nz(const jaos_model *m, int64_t row)`\
Returns the number of stored entries in the quadratic part of row `row`. It
returns 0 when `row` is out of range.

**`jaos_row_quadratic`**\
`jaos_status jaos_row_quadratic(const jaos_model *m, int64_t row, int64_t *rows, int64_t *cols, double *values)`\
Fills the arrays with the stored entries of the quadratic part of row
`row`, each with `rows[k] >= cols[k]`. The entries are ordered by column and
then by row. The arrays must hold `jaos_row_quadratic_nz(m, row)` entries,
and they may be null when the row has none. The call fails when `row` is out
of range, or when the row has entries and an array is null.

## Indicator rows and reading an SOS set

**`jaos_set_row_indicator`**\
`jaos_status jaos_set_row_indicator(jaos_model *m, int64_t row, int64_t col, int value)`\
Makes row `row` hold only while column `col` equals `value`, which is 0 or
1. While the column has the other value, the row is free. A negative `col`
removes the row's indicator. The call fails when `col` is not a column,
`value` is not 0 or 1, or `col` is not marked integer.

**`jaos_row_indicator`**\
`jaos_status jaos_row_indicator(const jaos_model *m, int64_t row, int64_t *col, int *value)`\
Stores the indicator column and value of row `row`. It stores `-1` and `0`
when the row has none. Both outputs are optional, and the call fails when
`row` is out of range.

**`jaos_sos`**\
`jaos_status jaos_sos(const jaos_model *m, int64_t k, int *type, int64_t *n, int64_t *cols, double *weights)`\
Stores the type, the size, the members and the weights of SOS set `k`. The
members come in increasing weight order. Each output is optional, and `cols`
and `weights` must hold `*n` entries. The call fails when `k` is out of
range.

## MIP options: gap, cuts, dives and heuristics

The branch and bound reads the settings in this section and in the next
three. A model with no integer structure ignores them. The table in
[Options](#options) gives each option name and default. Unless its entry
says otherwise, a setter here fails only when `m` is null. A setter that
takes an `int64_t` count restores the default for a negative value. A setter
that takes `int on` restores the default for a negative value, turns the
feature off for 0 and turns it on for a positive value.

The conic branch and bound of `src/conictree.c` solves a MIP with cones or
quadratic rows. It reads only the gap, the node limit, the tree batch, the
branching rule, the cutoff, the rounding switch, the count of the dive
heuristic and the MIP start. Its pool holds the incumbent alone.

**`jaos_set_mip_gap`**\
`jaos_status jaos_set_mip_gap(jaos_model *m, double gap)`\
Sets the relative gap that ends the search. A node closes when its bound is
within `gap * (1 + |incumbent|)` of the incumbent. The default is 1e-6, and
0 restores it. The call fails when `gap` is negative or not finite.

**`jaos_set_mip_dive`**\
`jaos_status jaos_set_mip_dive(jaos_model *m, bool on)`\
Turns diving on or off. It is off by default. While diving, the tree solves
next the child that `jaos_set_mip_dive_child` picks and puts the sibling in
the open set, until a node is pruned or integral.

**`jaos_set_mip_cut_rounds`**\
`jaos_status jaos_set_mip_cut_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of Gomory mixed-integer cuts at the root. Each round adds
one cut per fractional integer column of the relaxation's basis, and the
cuts stay for the whole tree. The default is 1, and 0 turns them off.

**`jaos_set_mip_cut_depth`**\
`jaos_status jaos_set_mip_cut_depth(jaos_model *m, int64_t depth)`\
Gives one round of Gomory cuts to every node below the root whose depth is
at most `depth`. The root is at depth 0. A node's cuts hold in its subtree
only. The default is 3, and 0 keeps the cuts at the root.

**`jaos_set_mip_cut_drop`**\
`jaos_status jaos_set_mip_cut_drop(jaos_model *m, bool on)`\
When on, the default, a node's cut leaves the relaxation of the nodes under
it once it does not bind there. When off, every node under it keeps the
cut. The setting matters only when nodes below the root get cuts.

**`jaos_set_mip_node_cut_cap`**\
`jaos_status jaos_set_mip_node_cut_cap(jaos_model *m, int64_t cap)`\
Keeps at most `cap` cuts at each node below the root, the most efficacious
ones. The default is 4, and 0 means no cap. The root's rounds have no cap.

**`jaos_set_mip_cover_rounds`**\
`jaos_status jaos_set_mip_cover_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of knapsack cover cuts at the root. The cuts come from every
row whose columns are all binary. The default is 4, and 0 turns them off.

**`jaos_set_mip_clique_rounds`**\
`jaos_status jaos_set_mip_clique_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of clique cuts at the root. The cuts come from the conflict
graph of the all-binary rows. The default is 4, and 0 turns them off.

**`jaos_set_mip_zero_half_rounds`**\
`jaos_status jaos_set_mip_zero_half_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of zero-half cuts at the root. The default is 0, off.

**`jaos_set_mip_flow_cover_rounds`**\
`jaos_status jaos_set_mip_flow_cover_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of flow cover cuts at the root. The default is 0, off.

**`jaos_set_mip_cut_stall`**\
`jaos_status jaos_set_mip_cut_stall(jaos_model *m, double fraction)`\
Ends the root's cut rounds after a round that moved the bound by less than
`fraction * (1 + |bound|)`. The default is 0, which never ends them early,
and a negative value restores it. The call fails when `fraction` is NaN or
infinite.

**`jaos_set_mip_node_cut_stall`**\
`jaos_status jaos_set_mip_node_cut_stall(jaos_model *m, double fraction)`\
Gives no further cut round to a node whose own round moved its bound by less
than `fraction * (1 + |bound|)`, or to the nodes under it. The default is 0,
never, and a negative value restores it. The call fails when `fraction` is
NaN or infinite.

**`jaos_set_mip_root_cut_drop`**\
`jaos_status jaos_set_mip_root_cut_drop(jaos_model *m, int on)`\
Lets a root cut leave the relaxation below a node where it does not bind, as
a node's own cut does. It is on by default.

**`jaos_set_mip_cover_lift`**\
`jaos_status jaos_set_mip_cover_lift(jaos_model *m, int on)`\
When on, each cover cut gets Balas's lifted coefficients. When off, the
default, each cover cut is extended by every heavier item.

**`jaos_set_mip_mir_rounds`**\
`jaos_status jaos_set_mip_mir_rounds(jaos_model *m, int64_t rounds)`\
Sets the rounds of mixed-integer rounding (MIR) cuts on the model's rows at
the root. The default is 6, and 0 turns them off.

**`jaos_set_mip_dive_backtrack`**\
`jaos_status jaos_set_mip_dive_backtrack(jaos_model *m, int64_t times)`\
Lets a dive resume from the deepest sibling it left, up to `times` times per
dive. The default is 0. The setting matters only while diving is on.

**`jaos_set_mip_dive_gap`**\
`jaos_status jaos_set_mip_dive_gap(jaos_model *m, double fraction)`\
Lets a dive resume only while the waiting sibling's bound is within
`fraction * (1 + |best open bound|)`. The default is 0, no condition, and a
negative value restores it. The call fails when `fraction` is NaN or
infinite.

**`jaos_set_mip_node_mir`**\
`jaos_status jaos_set_mip_node_mir(jaos_model *m, int on)`\
Adds MIR cuts over a node's own bounds to the node's round of Gomory cuts,
under the same cap. It is off by default.

**`jaos_set_mip_mir_aggregate`**\
`jaos_status jaos_set_mip_mir_aggregate(jaos_model *m, int64_t rows)`\
Lets an MIR row absorb up to `rows` other rows before it is rounded. Each
absorbed row substitutes out a continuous column. The default is 0, the
single-row form.

**`jaos_set_mip_dive_heuristic`**\
`jaos_status jaos_set_mip_dive_heuristic(jaos_model *m, int64_t solves)`\
Sets the most solves that the dive heuristic takes. On a copy of the
relaxation, the dive fixes the integer column nearest an integer and solves
again. An integral point it reaches is judged like any heuristic point. The
default is 50, and 0 turns it off. In a model with cones or quadratic rows,
each solve fixes half of the fractional integer columns.

**`jaos_set_mip_dive_heuristic_depth`**\
`jaos_status jaos_set_mip_dive_heuristic_depth(jaos_model *m, int64_t depth)`\
Runs the dive heuristic at every node down to depth `depth`. The root is at
depth 0. The default is 0, the root only.

**`jaos_set_mip_rins`**\
`jaos_status jaos_set_mip_rins(jaos_model *m, int64_t solves)`\
Sets the most solves of RINS. RINS fixes the integer columns on which the
incumbent and a node's relaxation agree, and dives on the rest. It runs once
per distinct incumbent. The default is 0, off.

**`jaos_set_mip_local_branching`**\
`jaos_status jaos_set_mip_local_branching(jaos_model *m, int64_t size)`\
Sets the neighbourhood of local branching. For each distinct incumbent, a
small tree solves the model with one more row: at most `size` binary columns
may take the other value than they have in the incumbent. The small tree
stops at `MIP_LOCAL_BRANCHING_NODES` nodes, and a better point it finds
becomes the incumbent. The default is 0, off. A negative value restores the
default.

**`jaos_set_mip_node_select`**\
`jaos_status jaos_set_mip_node_select(jaos_model *m, int64_t rule)`\
Sets which open node the tree takes next. 0 takes the lowest bound. 1,
the default, takes the lowest estimate: the node's bound plus, for
each fractional integer column of its parent's relaxation, the smaller
of its two pseudocost gains, with the branching column's own gain in
the child's direction; every `MIP_ESTIMATE_BOUND_EVERY`-th pick still
takes the lowest bound. A negative value restores the default. The call
fails when `rule` is above 1.

**`jaos_set_mip_restart`**\
`jaos_status jaos_set_mip_restart(jaos_model *m, int on)`\
Turns the restart on or off. It is off by default. After the root, when an
incumbent exists, the reduced costs of the root relaxation fix every integer
column they can against the incumbent's value. When they fix at least
`MIP_RESTART_FRAC` of the integer columns, the tree stops and starts again
from the root with those columns fixed and the incumbent as its start; the
second tree gets the work, time and nodes the first one left, and the model's
bounds are put back afterwards. A negative value restores the default.

**`jaos_set_mip_feaspump`**\
`jaos_status jaos_set_mip_feaspump(jaos_model *m, int64_t rounds)`\
Sets the rounds of the feasibility pump at the root. Each round rounds the
relaxation's point. It then solves the copy for the point nearest that
rounding in the L1 norm. The default is 20, and 0 turns it off. The pump
runs only while no incumbent exists.

**`jaos_set_mip_pump_general`**\
`jaos_status jaos_set_mip_pump_general(jaos_model *m, int on)`\
Gives the pump an extra column and two rows for each general integer
column. The distance of such a column to its rounding then counts wherever
the rounding lies. It is off by default.

**`jaos_set_mip_pump_obj`**\
`jaos_status jaos_set_mip_pump_obj(jaos_model *m, double decay)`\
Blends the model's objective into each round of the pump. The weight starts
at 1 and is multiplied by `decay` each round. The default is 0.5, 0 gives
the plain pump, and a negative value restores the default. The call fails
when `decay` is NaN or at least 1.

## Presolve report, model statistics and the MIP test

**`jaos_presolve_result`**\
`jaos_status jaos_presolve_result(const jaos_model *m, jaos_presolve_report *out)`\
Fills `out` with what presolve did in the last solve of a continuous model
without cones or quadratic rows. `num_row`, `num_col` and `num_nz` give the
size of the model the solver ran on, and `rounds` counts the presolve
rounds. Each other field counts one kind of reduction: `fixed_col`,
`empty_row`, `empty_col`, `singleton_row`, `singleton_col`,
`free_col_singleton`, `forcing_row`, `redundant_row`, `implied_free_col`,
`tightened_bound`, `duplicate_row`, `duplicate_col`, `dominated_col` and
`aggregated_col`. `aggregated_col` counts the implied free columns
substituted out of an equation, each taking the equation with it. A branch
and bound or a conic solve leaves the report as it was. Before any solve,
every field is 0. The call fails when `m` or `out` is null.

**`jaos_model_statistics`**\
`jaos_status jaos_model_statistics(const jaos_model *m, jaos_model_stats *out)`\
Fills `out` with counts that describe the model. `num_row`, `num_col` and
`num_nz` give its size. The fields `equality_row`, `ranged_row`,
`one_sided_row` and `free_row` sum to the rows. The fields `fixed_col`,
`ranged_col`, `one_sided_col` and `free_col` sum to the columns.
`integer_col` and `binary_col` count the integer and the binary columns. A
binary column is an integer column whose bounds, rounded inward, are 0 and
1. `empty_row` and `empty_col` count the rows and columns with no entry, and
`obj_nz` counts the nonzero costs. `min_abs` and `max_abs` range over the
matrix entries, and `obj_min_abs` and `obj_max_abs` over the nonzero costs.
`semicontinuous_col`, `sos_set`, `indicator_row` and `cone_set` count those
structures. `quadratic_col` counts the columns with a nonzero diagonal entry
in `Q`, and `quadratic_row` counts the rows with a quadratic part. The call
fails when `m` or `out` is null, or when memory runs out.

**`jaos_model_has_integer`**\
`bool jaos_model_has_integer(const jaos_model *m)`\
Returns true when the model has an integer column, an SOS set, or a
semi-continuous column whose lower bound is above 0. `jaos_solve` uses this
test to choose the branch and bound. The call returns false for a null
model.

## MIP start, cutoff, bound fixing, symmetry and limits

The rules at the start of
[MIP options: gap, cuts, dives and heuristics](#mip-options-gap-cuts-dives-and-heuristics)
apply here too.

**`jaos_set_mip_start`**\
`jaos_status jaos_set_mip_start(jaos_model *m, const double *col_value)`\
Gives the branch and bound a starting point of `jaos_num_col(m)` values. The
tree judges it at the root like any heuristic point. The point becomes the
first incumbent only when it is integral, satisfies every bound and row, and
is better than the cutoff. A null `col_value` removes the start, and the
call fails when a value is not finite. A call that fails also removes the
stored start.

**`jaos_set_mip_cutoff`**\
`jaos_status jaos_set_mip_cutoff(jaos_model *m, double cutoff)`\
Prunes every node whose relaxation cannot improve on the objective value
`cutoff`, from the first node on. The tree also takes no incumbent that does
not improve on it. The value is in the model's own sense. A cutoff better
than the optimum makes the search end `JAOS_SOLVE_INFEASIBLE`. An infinity
removes the cutoff, and NaN fails.

**`jaos_set_mip_pump_always`**\
`jaos_status jaos_set_mip_pump_always(jaos_model *m, int on)`\
Runs the feasibility pump at the root even when an incumbent exists. It is
off by default.

**`jaos_set_mip_rcfix`**\
`jaos_status jaos_set_mip_rcfix(jaos_model *m, int on)`\
Turns reduced-cost fixing at the root on or off. It is off by default. Once
an incumbent exists, it moves an integer column's far bound to the furthest
integer the column's reduced cost allows. Every node inherits the new bound,
and `jaos_mip_result` counts such columns in `fixed_cols`.

**`jaos_set_mip_tighten`**\
`jaos_status jaos_set_mip_tighten(jaos_model *m, int on)`\
Turns coefficient tightening at the root on or off. It is on by default. It
shrinks by the row's slack each coefficient of a binary column that cannot
make its one-sided row tight on its own. For a positive coefficient in a
`<=` row it shrinks the row's bound too. The set of integer points does not
change.

**`jaos_set_mip_probing`**\
`jaos_status jaos_set_mip_probing(jaos_model *m, int on)`\
Turns probing on or off. It is off by default. After the root solve, it
tries each binary column that is fractional there at 0 and at 1, and
propagates the rows. A setting that makes a row impossible fixes the column
the other way, and the bounds that both settings imply are kept.

**`jaos_set_mip_probing_cap`**\
`jaos_status jaos_set_mip_probing_cap(jaos_model *m, double multiple)`\
Stops the root's probing at `multiple` times the work of the root solve. The
default is 1, 0 means no cap, and a negative value restores the default. The
call fails when `multiple` is NaN or positive infinity.

**`jaos_set_mip_clique_fix`**\
`jaos_status jaos_set_mip_clique_fix(jaos_model *m, int on)`\
Turns clique fixing on or off. It is off by default. At each node, a binary
column fixed to one value fixes every literal that the root's clique table
puts in conflict with it. A node that holds both sides of a conflict closes
without a solve.

**`jaos_set_mip_conflicts`**\
`jaos_status jaos_set_mip_conflicts(jaos_model *m, int on)`\
Turns conflict analysis on or off. It is on by default. At a node whose
relaxation is infeasible, it reads the Farkas proof over the branchings on
the path. When the proof needs only binaries fixed on the path, at most 32
of them, it adds a row that forbids that combination for the rest of the
search.

**`jaos_set_mip_symmetry`**\
`jaos_status jaos_set_mip_symmetry(jaos_model *m, int on)`\
Runs symmetry detection at the root. `jaos_mip_result` reports the
generators and orbits it finds. It is off by default. Orbital fixing, which
is on by default, runs the same search, so this switch matters only with
`jaos_set_mip_orbital` off.

**`jaos_set_mip_orbital`**\
`jaos_status jaos_set_mip_orbital(jaos_model *m, int on)`\
Turns orbital branching and fixing on or off. It is on by default. It uses
the symmetries that the root's search finds to fix whole orbits of binary
columns at 0.

**`jaos_set_mip_propagate`**\
`jaos_status jaos_set_mip_propagate(jaos_model *m, int64_t rounds)`\
Sets the passes of bound propagation at each node before its relaxation is
solved. A pass reads the rows over the node's bounds and closes the node
when a row admits no point. It also tightens the integer bounds that the
rows imply. A negative value, and the default, leave the choice to the
tree: 4 passes on a model with a quadratic objective, 0 otherwise.
`jaos_get_option` reads this unset value back as -1, so a replayed option
file keeps the tree's choice.

**`jaos_set_mip_propagate_depth`**\
`jaos_status jaos_set_mip_propagate_depth(jaos_model *m, int64_t depth)`\
Sets the deepest node at which propagation runs. The root is at depth 0. A
negative value, the default, means every node.

**`jaos_set_mip_dive_degrade`**\
`jaos_status jaos_set_mip_dive_degrade(jaos_model *m, double frac)`\
Lets a dive go on into a child only while the node's bound stays within
`frac * (1 + |b|)` of its parent's bound `b`. The default is 0, no
condition, and a negative value restores it. The call fails when `frac` is
NaN or infinite.

**`jaos_set_mip_heuristics`**\
`jaos_status jaos_set_mip_heuristics(jaos_model *m, bool on)`\
Turns the rounding heuristic on or off. It is on by default. It rounds each
fractional node's relaxation to the nearest integers and offers the point as
an incumbent. The dive heuristic, RINS and the pump have their own setters.

**`jaos_set_mip_node_limit`**\
`jaos_status jaos_set_mip_node_limit(jaos_model *m, int64_t nodes)`\
Stops the branch and bound when its node count reaches `nodes`. The solve
then ends `JAOS_SOLVE_NODE_LIMIT` and keeps its incumbent. The default is 0,
no limit. The call fails when `nodes` is negative.

## MIP tree options

**`jaos_set_mip_tree_batch`**\
`jaos_status jaos_set_mip_tree_batch(jaos_model *m, int64_t nodes)`\
Sets how many open nodes a branch and bound takes in one round. The round's
relaxations are solved on up to `jaos_set_threads` threads. The tree takes
their answers in the round's own order, so the tree does not depend on the
thread count. The default, 1, is the tree that takes one node at a time.
Above 1 the search changes. The conic tree reaches an optimum with less
work on the models it finishes, and a run it stops at a limit tends to hold
a better bound and a worse incumbent. The linear tree (since 2026-09-22)
solves each node of a round from the node's own basis on a copy of the
tree's model. It then takes the nodes in order and solves each again from
its copy's final basis, so the cuts and conflicts one node finds reach the
nodes after it. The linear tree takes one node per round on a model with a
quadratic objective, and once the work or time limit is spent. A round
holds at most 64 nodes. The call fails when `nodes` is below 1.

**`jaos_set_mip_branching`**\
`jaos_status jaos_set_mip_branching(jaos_model *m, jaos_branching rule)`\
Sets the branching rule. `JAOS_BRANCH_PSEUDOCOST` (0), the default, scores
each column by the objective gain that a unit move in each direction has
cost so far. `JAOS_BRANCH_MOST_FRACTIONAL` (1) takes the column farthest
from an integer. Any other value fails.

**`jaos_set_mip_reliability`**\
`jaos_status jaos_set_mip_reliability(jaos_model *m, int64_t branches)`\
Sets how many branches in each direction a column needs before its
pseudocost is trusted. Below that count, strong branching solves the
children of up to 8 candidates per node, and their gains start the
pseudocosts. The default is 0, never. The setting applies to pseudocost
branching only.

**`jaos_set_mip_probe_cap`**\
`jaos_status jaos_set_mip_probe_cap(jaos_model *m, double multiple)`\
Stops each strong-branching child solve at `multiple` times the work of the
node's own relaxation. The default is 0, no cap, and a negative value
restores it. The call fails when `multiple` is NaN or positive infinity.

**`jaos_set_mip_probe_depth`**\
`jaos_status jaos_set_mip_probe_depth(jaos_model *m, int64_t depth)`\
Limits strong branching to the nodes down to depth `depth`. The root is at
depth 0. A negative value, the default, allows every depth.

**`jaos_set_mip_dive_child`**\
`jaos_status jaos_set_mip_dive_child(jaos_model *m, jaos_dive_child rule)`\
Chooses which child a dive solves first. `JAOS_DIVE_NEARER` (0), the
default, takes the side the fraction is closer to. `JAOS_DIVE_UP` (1) and
`JAOS_DIVE_DOWN` (2) fix the side. `JAOS_DIVE_PSEUDOCOST` (3) takes the side
with the smaller expected loss in the objective. Any other value fails.

## MIP results and the solution pool

**`jaos_mip_result`**\
`jaos_status jaos_mip_result(const jaos_model *m, jaos_mip_report *out)`\
Fills `out` with the counts of the last branch and bound: `nodes`,
`lp_solves`, `cuts`, `heuristic_points`, `first_incumbent_node`,
`fixed_cols`, `tightened`, `symmetry_generators` and `symmetry_orbits`. It
also gives `has_incumbent`, and `incumbent`, the incumbent's objective or 0
when `has_incumbent` is false. `bound` is the best objective that an open
node could still reach. At `JAOS_SOLVE_OPTIMAL` the bound equals the
incumbent's objective. The call fails when `m` or `out` is null.

**`jaos_mip_incumbent`**\
`jaos_status jaos_mip_incumbent(const jaos_model *m, double *col_value, double *objective)`\
Copies the best integer point of the last branch and bound and its
objective. It works for any outcome, so a tree stopped by a limit still
gives its incumbent. Both outputs are optional. The call fails when the last
solve found no integer point.

**`jaos_set_mip_pool_size`**\
`jaos_status jaos_set_mip_pool_size(jaos_model *m, int64_t size)`\
Keeps the `size` best distinct integer points that the branch and bound
finds. Two points are distinct when they differ on an integer column. The
default is 1, the incumbent alone, and a negative value restores it. A size
of 0 fails.

**`jaos_mip_pool_count`**\
`jaos_status jaos_mip_pool_count(const jaos_model *m, int64_t *count)`\
Stores in `*count` how many points the pool holds after the last solve. It
fails when `m` or `count` is null.

**`jaos_mip_pool_solution`**\
`jaos_status jaos_mip_pool_solution(const jaos_model *m, int64_t k, double *col_value, double *objective)`\
Copies pool entry `k` and its objective. Entry 0 is the best, and the other
entries follow in order. Both outputs are optional. The call fails when `k`
is not below the pool's count.

## Model name and copy

**`jaos_model_name`**\
`jaos_status jaos_model_name(const jaos_model *m, char *buf, int64_t cap)`\
Copies the model's name into `buf`. The name is `JAOS` unless it was set or
read from a file. The call fails as `jaos_col_name` does.

**`jaos_set_model_name`**\
`jaos_status jaos_set_model_name(jaos_model *m, const char *name)`\
Sets the model's name under the rules for names. A null or empty `name`
restores `JAOS`.

**`jaos_model_copy`**\
`jaos_status jaos_model_copy(const jaos_model *src, jaos_model **out)`\
Creates a new model that holds everything `src` holds except the answer:
the data, the names, the marks, the quadratic parts, the cones, the options,
the callbacks, the MIP start and the start basis. The caller frees the copy
with `jaos_model_free`. The call fails when a pointer is null or memory runs
out, and `*out` is then null when `out` is not.

## Matrix entries, rows and columns

`jaos_add_cols` takes its entries column by column, as `jaos_load_lp` does.
`jaos_add_rows` takes them row by row. New row `k` holds the entries
`ar_start[k]` to `ar_start[k+1] - 1` of `ar_index` and `ar_value`, and
`ar_index` holds column indices. Both calls drop zero values, and the start
array may be null when `num_nz` is 0. Every edit here clears the last
answer. After an addition or a deletion, a stored start basis stays only
while its number of basic entries equals the number of rows.

**`jaos_col_entries`**\
`jaos_status jaos_col_entries(const jaos_model *m, int64_t col, int64_t *count, int64_t *index, double *value)`\
Stores the number of entries of column `col` in `*count`. It copies their
row indices and values in increasing row order. `index` and `value` are
optional, so a first call with both null gives the size. The call fails when
`count` is null or `col` is out of range.

**`jaos_row_entries`**\
`jaos_status jaos_row_entries(jaos_model *m, int64_t row, int64_t *count, int64_t *index, double *value)`\
Does the same for row `row`, with the column indices in increasing order.
The first call after an edit builds a row-wise copy of the matrix, which is
why `m` is not const. That build can fail with `JAOS_ERR_OUT_OF_MEMORY`.

**`jaos_coefficient`**\
`jaos_status jaos_coefficient(const jaos_model *m, int64_t row, int64_t col, double *value)`\
Stores the entry at row `row` and column `col` in `*value`. It stores 0 when
there is no entry. The call fails when an index is out of range or `value`
is null.

**`jaos_set_coefficient`**\
`jaos_status jaos_set_coefficient(jaos_model *m, int64_t row, int64_t col, double value)`\
Sets the entry at row `row` and column `col`. A value of 0 removes the
entry. The call fails when an index is out of range or `value` is not
finite.

**`jaos_add_cols`**

```c
jaos_status jaos_add_cols(jaos_model *m, int64_t num_new,
    const double *col_cost, const double *col_lower, const double *col_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);
```

Appends `num_new` continuous columns without names, with the given costs,
bounds and entries. A stored MIP start gets a value for each new column: the
lower bound when it is positive, the upper bound when it is negative, and 0
otherwise. A stored start basis puts each new column at a finite bound, and
the call removes the basis when a new column is free. The call fails when a
cost is not finite, a bound is NaN, a row index is out of range, a value is
not finite, or a new column names a row twice. `num_new = 0` with
`num_nz = 0` does nothing.

**`jaos_add_rows`**

```c
jaos_status jaos_add_rows(jaos_model *m, int64_t num_new,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *ar_start, const int64_t *ar_index,
    const double *ar_value);
```

Appends `num_new` rows without names, with the given bounds and entries. A
stored start basis makes each new row basic. The call fails when a bound is
NaN, a column index is out of range, a value is not finite, or a new row
names a column twice. `num_new = 0` with `num_nz = 0` does nothing.

**`jaos_delete_cols`**\
`jaos_status jaos_delete_cols(jaos_model *m, int64_t num_del, const int64_t *cols)`\
Deletes the `num_del` columns listed in `cols`, with their entries, names,
marks and quadratic terms. It removes them from their SOS sets, and it
removes a set that is left empty. The call fails when an index is out of
range or appears twice. It also fails when a column switches an indicator
row or belongs to a cone, so such a row or cone must be deleted first.

**`jaos_delete_rows`**\
`jaos_status jaos_delete_rows(jaos_model *m, int64_t num_del, const int64_t *rows)`\
Deletes the `num_del` rows listed in `rows`, with their entries, names,
indicators and quadratic parts. The call fails when an index is out of range
or appears twice.

## Reading and writing files

Every reader replaces the model's contents with the file's model, as
`jaos_load_lp` does. It keeps the options and the callbacks. A reader takes
a gzip file wherever it takes a plain one, and it decides from the file's
first two bytes. It returns `JAOS_ERR_IO` when the file cannot be opened or
read. It returns `JAOS_ERR_INVALID_INPUT` when the file breaks the format,
with a message that names the line where it can. `docs/format-support.md`
gives the rules of each format.

Every writer checks the model before it writes. It fails without leaving a
file when the format cannot express the model, and the message names the row
or column at fault. A writer that writes names refuses two rows or two
columns with one name, and the objective counts as a row. Numbers are
written with the fewest of 15, 16 or 17 significant digits that read back
as the same double.

A path that ends in `.gz` gives a gzip file from every writer but
`jaos_write_proof`. The writers that compress are the six model writers,
`jaos_write_solution`, `jaos_write_sol_ampl`, `jaos_write_mps_basis`,
`jaos_write_point`, `jaos_write_point_values`, `jaos_write_duals` and
`jaos_write_dual_values`. `jaos_write_proof` writes plain text whatever the
path's name.

Every writer takes a non-const model, for two reasons. It records a
failure's message on the model, where `jaos_model_error` reads it, and
clears that message when it succeeds. Some writers also build the row-wise
copy of the matrix on first use, as `jaos_row_entries` does. A writer
changes nothing else: the model's data and its last answer stay as they
were.

**`jaos_read_mps`**\
`jaos_status jaos_read_mps(jaos_model *m, const char *path)`\
Reads an MPS file in free or fixed layout. It reads the integer markers,
every bound type, ranges, SOS sets, indicators, the quadratic objective,
quadratic rows and cones.

**`jaos_read_lp`**\
`jaos_status jaos_read_lp(jaos_model *m, const char *path)`\
Reads a CPLEX-style LP file. It reads bounds, the general, binary and
semi-continuous sections, SOS sets, indicators, and quadratic parts in the
objective and in constraints.

**`jaos_read_nl`**\
`jaos_status jaos_read_nl(jaos_model *m, const char *path)`\
Reads AMPL's text `.nl` format. It reads a body of degree two or less:
sums, differences, products, division by a nonzero constant, powers 1 and
2, unary minus and sumlist. The body's constant moves into the row's bounds
or the objective's constant. Its quadratic part becomes the objective's `Q`
or the row's quadratic part. In the objective, its linear part is added to
the `G` coefficients. In a row, its linear terms are added to the `J`
coefficients of the same column, so `(x+1)^2 + 3*x <= 5` reads as
`x^2 + 5x <= 4`; a sum that comes out exactly 0 leaves no entry. The column names come from the `.col`
file beside the file and the row names from the `.row` file, each when it
is there and complete. The `x` segment becomes the MIP start of a model
with integer columns. The call refuses a binary `.nl` file, any other
operator, a product of degree three or more, and integer variables in
nonlinear terms.

**`jaos_write_mps`**\
`jaos_status jaos_write_mps(jaos_model *m, const char *path)`\
Writes the model as free-format MPS, the one format that carries every
structure a model holds. It refuses a row whose lower bound is above its
upper one or whose two bounds are the same infinity. It refuses a ranged row
that no RANGES entry rebuilds exactly, and a column bound at an infinity of
the wrong sign. It also refuses a semi-continuous column with no finite
upper bound, and a row or objective named `'MARKER'`, quotes included.

**`jaos_write_lp`**\
`jaos_status jaos_write_lp(jaos_model *m, const char *path)`\
Writes the model as a CPLEX-style LP file. A name that the LP reader could
not read back as one token is written under a spelled name, and a comment at
the top of the file maps it. The writer refuses cones, a quadratic row with
two finite bounds, a bound at an infinity of the wrong sign, a row whose
lower bound is above its upper bound, and an empty row in a model with no
columns.

**`jaos_write_nl`**\
`jaos_status jaos_write_nl(jaos_model *m, const char *path)`\
Writes AMPL's text `.nl` format, with the names in `.col` and `.row` files
beside it. The format lists integer columns last, so a model whose integer
columns are not last reads back with its columns in that order. The writer
refuses SOS sets, semi-continuous columns, indicator rows, a quadratic
objective, quadratic rows and cones.

**`jaos_read_qplib`**\
`jaos_status jaos_read_qplib(jaos_model *m, const char *path)`\
Reads the QPLIB text format of Furini et al. (2019). It reads a quadratic
objective, quadratic rows, and continuous, integer and binary variables.

**`jaos_write_qplib`**\
`jaos_status jaos_write_qplib(jaos_model *m, const char *path)`\
Writes the QPLIB text format. It refuses SOS sets, semi-continuous columns,
indicator rows and cones. It also refuses a name that holds `#` or `!` or
starts with `%`, because a QPLIB reader takes such a name for a comment.

**`jaos_read_cbf`**\
`jaos_status jaos_read_cbf(jaos_model *m, const char *path)`\
Reads the Conic Benchmark Format of CBLIB, versions 1 to 3, with its
linear, quadratic and rotated cones and its integer marks. A cone over
affine expressions becomes new free columns defined by equality rows, with
the cone over those columns. Semidefinite, power and exponential parts are
refused, and a refused file leaves the model unchanged.

**`jaos_write_cbf`**\
`jaos_status jaos_write_cbf(jaos_model *m, const char *path)`\
Writes CBF version 3, which carries no names. A column bound other than 0
or an infinity becomes a row, and a ranged row becomes two rows. The file
therefore reads back as an equivalent model with the same optimum. The
writer refuses a quadratic objective, quadratic rows, SOS sets,
semi-continuous columns, indicator rows, and a bound at an infinity of the
wrong sign.

**`jaos_read_osil`**\
`jaos_status jaos_read_osil(jaos_model *m, const char *path)`\
Reads OSiL XML in either matrix layout. It reads the variable types `C`,
`B`, `I`, `S` and `D`, and the quadratic terms of the objective and of the
rows. It refuses nonlinear expressions, SOS blocks and a second objective.

**`jaos_write_osil`**\
`jaos_status jaos_write_osil(jaos_model *m, const char *path)`\
Writes the model as OSiL XML, quadratic terms included. It refuses SOS sets,
cones and indicator rows.

**`jaos_write_solution`**\
`jaos_status jaos_write_solution(jaos_model *m, const char *path)`\
Writes JAOS's own solution file. After an optimum the file holds the values,
the duals and the basis. After an infeasible or unbounded answer it holds
the certificate, and the basis the solve stopped on when there is one. A
model with cones adds the cone duals. The call fails when the last solve
reached none of these answers or a value to write is not finite.

**`jaos_write_sol_ampl`**\
`jaos_status jaos_write_sol_ampl(jaos_model *m, const char *path, const char *message)`\
Writes the `.sol` text file that AMPL reads after a solve, whatever the
outcome. A `message` that is neither null nor empty opens the file.
Otherwise the file opens with the version, the status and the objective.
The file carries the row duals of a continuous optimum, and the column
values of an optimum or of a stopped tree's incumbent.

## The error message

**`jaos_model_error`**\
`const char *jaos_model_error(const jaos_model *m)`\
Returns the message of the last failure on `m`, or an empty string when
there is none. The pointer stays valid until `jaos_model_free`. A null `m`
gives an empty string.

## Solve options

These setters change how `jaos_solve` works, and none of them clears the
answer. A new tolerance or algorithm discards the working state of a stopped
solve, so the next solve starts again from the stored basis. Each setter
fails when `m` is null.

**`jaos_set_work_limit`**\
`jaos_status jaos_set_work_limit(jaos_model *m, int64_t units)`\
Stops a solve after `units` work units, and the solve ends
`JAOS_SOLVE_WORK_LIMIT`. The default is 0, and 0 or a negative value means
no limit. The stop happens at the same point on every machine.

**`jaos_set_threads`**\
`jaos_status jaos_set_threads(jaos_model *m, int64_t threads)`\
Sets how many threads a solve may use. The concurrent solve, the barrier's
Cholesky factor and the rounds of both branch and bounds use them, as
[Threads](#threads) describes. The default is 1. The call fails when
`threads` is 0 or negative.

**`jaos_threads_of`**\
`int64_t jaos_threads_of(const jaos_model *m)`\
Returns the thread count. It is 1 unless it was set.

**`jaos_set_time_limit`**\
`jaos_status jaos_set_time_limit(jaos_model *m, double seconds)`\
Stops a solve after `seconds` of wall-clock time, and the solve ends
`JAOS_SOLVE_TIME_LIMIT`. The default is 0, and 0 or a negative value means
no limit. The call fails when `seconds` is NaN. This is the one setting that
makes a result depend on the machine. In a MIP, every relaxation the tree
solves, at a node, in a cut round or in a heuristic, gets only the time that
is left.

**`jaos_set_primal_tolerance`**\
`jaos_status jaos_set_primal_tolerance(jaos_model *m, double tol)`\
Sets how far a variable may lie outside its bounds and still count as
feasible, in the scaled space the solver works in. The default is 1e-7, and
0 restores it. The call fails when `tol` is negative or not finite.

**`jaos_set_dual_tolerance`**\
`jaos_status jaos_set_dual_tolerance(jaos_model *m, double tol)`\
Sets how far a reduced cost may lie on the wrong side of zero, in the same
scaled space. The default is 1e-9, and 0 restores it. The call fails when
`tol` is negative or not finite.

**`jaos_set_algorithm`**\
`jaos_status jaos_set_algorithm(jaos_model *m, jaos_algorithm alg)`\
Chooses the method for a continuous model: `JAOS_ALGORITHM_DUAL` (0), the
default, `JAOS_ALGORITHM_PRIMAL` (1), `JAOS_ALGORITHM_BARRIER` (2),
`JAOS_ALGORITHM_PDLP` (3) or `JAOS_ALGORITHM_CONCURRENT` (4). The concurrent
solve runs the dual simplex, the primal simplex and the barrier on three
copies under growing work budgets. It takes the first answer in that order.
It runs the three at once only when the thread count is above 1 and no work
limit is set, as [Threads](#threads) describes. The branch and bound of a
linear model solves its relaxations with the primal simplex under
`JAOS_ALGORITHM_PRIMAL` and with the dual simplex under any other setting.
Any value outside the enum fails.

**`jaos_algorithm_of`**\
`jaos_algorithm jaos_algorithm_of(const jaos_model *m)`\
Returns the algorithm set on the model. It is `JAOS_ALGORITHM_DUAL` unless
another was set.

**`jaos_set_option`**\
`jaos_status jaos_set_option(jaos_model *m, const char *name, const char *value)`\
Sets the option `name` from the text `value` through the option's setter,
so the setter's checks apply. [Options](#options) gives the syntax of each
kind of value. The call fails with a message when the name is unknown or the
value does not parse.

**`jaos_get_option`**\
`jaos_status jaos_get_option(const jaos_model *m, const char *name, char *buf, int64_t cap)`\
Writes the option's current value into `buf` as text. An option that was
never set gives its default. Integers are decimal, and numbers have 17
significant digits or read `inf` or `-inf`. Booleans read `true` or `false`,
and words read as `jaos_set_option` takes them. The call fails when the name
is unknown, `buf` is null, or `cap` is too small for the text and its
terminating zero.

**`jaos_read_options`**\
`jaos_status jaos_read_options(jaos_model *m, const char *path)`\
Applies an options file through `jaos_set_option`. Each line holds a name
and a value, which `=` or `:` may separate. `#` starts a comment, and blank
lines are skipped. The call stops at the first bad line and returns that
line's error, with the path and the line number in front of the message.
The lines before it stay applied. A file that does not open gives
`JAOS_ERR_IO`.

**`jaos_num_options`**\
`int64_t jaos_num_options(void)`\
Returns the number of option names, which is 57.

**`jaos_option_name`**\
`const char *jaos_option_name(int64_t k)`\
Returns the name of option `k`, for `k` from 0 to `jaos_num_options() - 1`,
in the order of the table in [Options](#options). It returns null for any
other `k`.

## Log and callbacks

The header declares the four callback types:

```c
typedef void (*jaos_log_fn)(void *user, jaos_log_level level, const char *line);
typedef jaos_callback_action (*jaos_progress_fn)(const jaos_progress *p, void *user);
typedef jaos_callback_action (*jaos_incumbent_fn)(const jaos_incumbent *inc, void *user);
typedef jaos_callback_action (*jaos_node_fn)(jaos_node *ev, void *user);
```

The log function takes `user` first and returns nothing. The other three
take `user` last and return a `jaos_callback_action`. The node event is not
const, so that the callback can set its `branch_col`.

A callback stays on the model until it is replaced, and a null function
removes it. `jaos_model_copy` copies the callbacks with the options. A
callback that returns `JAOS_CALLBACK_CONTINUE` (0) lets the solve go on. A
callback that returns `JAOS_CALLBACK_STOP` (1) ends the solve, which then
reports `JAOS_SOLVE_INTERRUPTED`. Each setter fails when `m` is null.

**`jaos_set_log_callback`**\
`jaos_status jaos_set_log_callback(jaos_model *m, jaos_log_fn cb, void *user)`\
Sets the function that receives the solver's log lines. The function gets
`user` back on each call. Lines arrive only when the log level is above
`JAOS_LOG_OFF`, as [Log lines](#log-lines) describes.

**`jaos_set_log_level`**\
`jaos_status jaos_set_log_level(jaos_model *m, jaos_log_level level)`\
Sets how much the log says: `JAOS_LOG_OFF` (0), the default,
`JAOS_LOG_SUMMARY` (1), `JAOS_LOG_PROGRESS` (2) or `JAOS_LOG_DETAIL` (3).
Any other value fails.

**`jaos_set_progress_callback`**\
`jaos_status jaos_set_progress_callback(jaos_model *m, jaos_progress_fn cb, void *user)`\
Sets a function that the solver calls while it iterates. The `jaos_progress`
event gives the iteration count in `iterations`, the work units so far in
`work_units`, and in `primal_infeasibility` a measure of primal
infeasibility that the running method keeps. The simplex and PDLP
call it every 64 iterations, and the barrier calls it at every iteration.
The conic interior point does not call it. In a branch and bound the counts
run over the whole tree.

**`jaos_set_incumbent_callback`**\
`jaos_status jaos_set_incumbent_callback(jaos_model *m, jaos_incumbent_fn cb, void *user)`\
Sets a function that a branch and bound calls each time it takes a new
incumbent. The `jaos_incumbent` event gives the node, the objective, the
best bound, the point of `num_col` values, and `by_rounding`. `by_rounding`
is true when a heuristic or the MIP start found the point.

**`jaos_set_node_callback`**\
`jaos_status jaos_set_node_callback(jaos_model *m, jaos_node_fn cb, void *user)`\
Sets a function that the branch and bound calls at every node once its
relaxation is solved, and at every point a heuristic would make an
incumbent. The `jaos_node` event gives the node, its depth, its objective
and bound, the point, whether the point is integral, and `branch_col`.
`branch_col` is the column the tree will branch on. The callback may set it
to another integer column that is fractional at the point, and the tree
ignores any other value. `jaos_solve` refuses a node callback on a model
with cones or quadratic rows and integer structure.

**`jaos_node_add_row`**\
`jaos_status jaos_node_add_row(jaos_node *ev, int64_t nnz, const int64_t *index, const double *value, double lower, double upper)`\
Adds the row `lower <= sum of value[k] * x[index[k]] <= upper` from inside a
node callback, through the event that the callback received. The row holds
for the rest of the search. When the row cuts off the node's point, the tree
solves the node again and calls the callback again, up to 1000 rounds per
node. The tree does not take an integral point that the row cuts off. The
call fails when `ev` or its `internal` field is null, an index is out of
range, a value is not finite, or a bound is NaN. It also fails when `lower`
is above `upper`, `lower` is positive infinity, or `upper` is negative
infinity.

## Solving and reading the answer

**`jaos_solve`**\
`jaos_status jaos_solve(jaos_model *m)`\
Solves the model and records the outcome, which `jaos_status_of` reports.
It clears the message and the exact values first. It then picks the method
from the model:

- The conic interior point of `src/conic.c` solves a model with cones or
  quadratic rows. With integer structure too, the conic branch and bound of
  `src/conictree.c` solves it.
- The branch and bound of `src/mip.c` solves any other model with integer
  structure.
- The barrier solves a continuous model with a quadratic objective. When the
  barrier ends `JAOS_SOLVE_NUMERICAL_ERROR`, the conic interior point solves
  the model again.
- The method that `jaos_set_algorithm` chose solves any other continuous
  model.

The call returns `JAOS_OK` when it recorded an outcome, and
`JAOS_SOLVE_NUMERICAL_ERROR` is such an outcome. It returns
`JAOS_ERR_INVALID_INPUT` when it refuses the model before it solves. It
refuses a non-convex quadratic objective or quadratic row, and a quadratic
row with two finite bounds. It refuses a quadratic objective, a cone or a
quadratic row under the primal simplex, PDLP or the concurrent solve. It
refuses a model with cones or quadratic rows and integer structure when
that model also has SOS sets, semi-continuous columns, indicator rows or a
node callback.
It returns `JAOS_ERR_OUT_OF_MEMORY` when memory runs out, and
`JAOS_ERR_NUMERICAL` when a method fails in a way it cannot record as an
outcome.

In a branch and bound, a node below the root whose relaxation fails is set
aside with its bound, and the search goes on. When a node was set aside,
the solve ends `JAOS_SOLVE_OPTIMAL` only when the incumbent is within the
gap of every bound set aside. A search that would end
`JAOS_SOLVE_INFEASIBLE`, or `JAOS_SOLVE_OPTIMAL` without that, ends
`JAOS_SOLVE_NUMERICAL_ERROR` instead, and `jaos_model_error` names the first
node set aside. `bound` in `jaos_mip_result` then includes the bounds set
aside.

A simplex solve stopped by a limit or a callback keeps its working state on
the model. The next `jaos_solve` goes on from where it stopped. An edit,
`jaos_set_basis`, `jaos_clear_basis`, or a new algorithm or tolerance
discards that state.

**`jaos_status_of`**\
`jaos_solve_status jaos_status_of(const jaos_model *m)`\
Returns the outcome of the last solve:

| Value | Meaning |
|---|---|
| `JAOS_SOLVE_NOT_RUN` | No solve since the model was built or last edited. |
| `JAOS_SOLVE_OPTIMAL` | An optimum, within the gap for a MIP. |
| `JAOS_SOLVE_INFEASIBLE` | The model has no feasible point. |
| `JAOS_SOLVE_UNBOUNDED` | The objective improves without limit. |
| `JAOS_SOLVE_WORK_LIMIT` | The work limit stopped the solve. |
| `JAOS_SOLVE_TIME_LIMIT` | The time limit stopped the solve. |
| `JAOS_SOLVE_NUMERICAL_ERROR` | The solve reached no answer it could publish. `jaos_model_error` says why. |
| `JAOS_SOLVE_INTERRUPTED` | A callback returned `JAOS_CALLBACK_STOP`. |
| `JAOS_SOLVE_NODE_LIMIT` | The node limit stopped a branch and bound. |

The header numbers these values 0 to 8, in the order of the table.

**`jaos_objective`**\
`jaos_status jaos_objective(const jaos_model *m, double *out)`\
Stores the objective value of the optimum in `*out`, with its constant. It
fails unless the last solve ended `JAOS_SOLVE_OPTIMAL`, because no other
outcome has an objective.

**`jaos_solution`**

```c
jaos_status jaos_solution(const jaos_model *m, double *col_value,
    double *row_activity, double *row_dual, double *col_dual);
```

Copies the optimum. `col_value` receives the column values and `col_dual`
the reduced costs, `jaos_num_col(m)` values each. `row_activity` receives
the row activities and `row_dual` the row duals, `jaos_num_row(m)` values
each. Every output is optional. The call fails unless the last solve ended
`JAOS_SOLVE_OPTIMAL`. For a MIP the point is the incumbent, with its integer
columns rounded to integers.

## The basis

A basis gives each column and each row one of four statuses:
`JAOS_BASIS_BASIC` (0), `JAOS_BASIS_AT_LOWER` (1), `JAOS_BASIS_AT_UPPER` (2)
or `JAOS_BASIS_FREE` (3). `JAOS_BASIS_FREE` marks a nonbasic variable with no
finite bound. A row's status describes its activity `A_i x`. A valid basis
has exactly `jaos_num_row(m)` basic entries.

**`jaos_basis`**\
`jaos_status jaos_basis(const jaos_model *m, jaos_basis_status *col_status, jaos_basis_status *row_status)`\
Copies the basis the last solve left. Both outputs are optional. The call
fails when the last solve left no basis. The simplex leaves one at an
optimum, at an infeasible or unbounded answer, and at a stop by a limit or a
callback. The conic interior point leaves none, and a branch and bound
leaves one only at an optimum.

**`jaos_set_basis`**\
`jaos_status jaos_set_basis(jaos_model *m, const jaos_basis_status *col_status, const jaos_basis_status *row_status)`\
Stores a basis as the start of the next solve. Both arrays are needed
unless their count is 0. The call fails when a status is not one of the four
or the number of basic entries is not `jaos_num_row(m)`. When it succeeds,
it discards the working state of a stopped solve.

**`jaos_clear_basis`**\
`void jaos_clear_basis(jaos_model *m)`\
Removes the stored start basis, so the next solve starts from the slack
basis. It also discards the working state of a stopped solve. A null `m`
does nothing.

## Answer files

A reader here checks a solution file against the model. The counts must
equal the model's. Each record's name must be the name the model gives that
index, its own or the positional one. The readers take a gzip file wherever
they take a plain one, and the writers compress when the path ends in
`.gz`. No reader installs what it reads. Pass a basis to `jaos_set_basis`,
and pass a ray to a checker.

**`jaos_read_solution`**

```c
jaos_status jaos_read_solution(jaos_model *m, const char *path,
    double *objective, double *col_value, double *col_dual,
    jaos_basis_status *col_status, double *row_activity, double *row_dual,
    jaos_basis_status *row_status);
```

Reads an optimum's solution file written for this model and fills the
outputs. Every output is optional. The call fails when the file's status is
not optimal, when the file does not match the model, or when it breaks the
format.

**`jaos_read_certificate`**\
`jaos_status jaos_read_certificate(jaos_model *m, const char *path, jaos_solve_status *status, double *row_ray, double *col_ray)`\
Reads a certificate file. `*status` becomes `JAOS_SOLVE_INFEASIBLE` or
`JAOS_SOLVE_UNBOUNDED`. An infeasible file fills `row_ray` with one Farkas
multiplier per row, and an unbounded file fills `col_ray` with one direction
entry per column. Every output is optional, and the call fails on an
optimum's file.

**`jaos_read_cone_duals`**\
`jaos_status jaos_read_cone_duals(jaos_model *m, const char *path, double *cone_dual)`\
Reads the cone duals of a solution file into `cone_dual`, one value per cone
member, cone after cone. The call fails when the model has no cones or the
file has no `cone` records.

**`jaos_read_basis`**\
`jaos_status jaos_read_basis(jaos_model *m, const char *path, jaos_basis_status *col_status, jaos_basis_status *row_status)`\
Reads the basis from a solution file of either kind. Both outputs are
optional. The call fails when a certificate file carries no basis.

**`jaos_write_mps_basis`**\
`jaos_status jaos_write_mps_basis(jaos_model *m, const char *path)`\
Writes the basis the last solve left as an MPS basis file, the format in
which solvers exchange a basis. The call fails when the last solve left no
basis, or when two columns or two rows share a name.

**`jaos_read_mps_basis`**\
`jaos_status jaos_read_mps_basis(jaos_model *m, const char *path, jaos_basis_status *col_status, jaos_basis_status *row_status)`\
Reads an MPS basis file for this model, from JAOS or from another solver.
The cards may come in any order, and both outputs are optional. The call
refuses an unknown card, a name the model does not have, a second card for
one variable, and a card on a bound the variable does not have. A refused
read leaves the outputs untouched.

**`jaos_write_point`**\
`jaos_status jaos_write_point(jaos_model *m, const char *path)`\
Writes the optimum's column values as a point file, one `NAME VALUE` line
per column. The call fails unless the last solve ended optimal.

**`jaos_write_point_values`**\
`jaos_status jaos_write_point_values(jaos_model *m, const char *path, const double *col_value)`\
Writes the given column values as a point file, whatever the last solve did.
The call fails when a value is not finite or two columns share a name.

**`jaos_write_duals`**\
`jaos_status jaos_write_duals(jaos_model *m, const char *path)`\
Writes the optimum's row duals in the shape of a point file, one
`NAME VALUE` line per row. The call fails unless the last solve ended
optimal.

**`jaos_write_dual_values`**\
`jaos_status jaos_write_dual_values(jaos_model *m, const char *path, const double *row_dual)`\
Writes the given row values in the same shape. The call fails when a value
is not finite or two rows share a name.

**`jaos_read_point`**\
`jaos_status jaos_read_point(jaos_model *m, const char *path, double *col_value)`\
Reads a point file into `col_value`. It also reads the solution files that
Gurobi, MIPLIB, SCIP, HiGHS and CPLEX write, and it detects the shape from
the file. Every column must appear exactly once. In the MIPLIB and SCIP
shapes a missing column is 0.

**`jaos_read_duals`**\
`jaos_status jaos_read_duals(jaos_model *m, const char *path, double *row_dual)`\
Reads row multipliers into `row_dual` from a file of the same shapes, by row
name.

**`jaos_solution_file_status`**\
`jaos_status jaos_solution_file_status(jaos_model *m, const char *path, jaos_solve_status *status)`\
Stores the status that a solution file holds: `JAOS_SOLVE_OPTIMAL`,
`JAOS_SOLVE_INFEASIBLE` or `JAOS_SOLVE_UNBOUNDED`. It reads and checks the
whole file against the model first, so it fails where the readers fail.

## Solve counters

**`jaos_work_units`**\
`int64_t jaos_work_units(const jaos_model *m)`\
Returns the work units the last solve used. A branch and bound counts its
whole tree. A solve that went on after a stop counts from its first start.
The count is 0 before a solve and after an edit.

**`jaos_iterations`**\
`int64_t jaos_iterations(const jaos_model *m)`\
Returns the iterations of the last solve, counted the same way.

**`jaos_solve_time`**\
`double jaos_solve_time(const jaos_model *m)`\
Returns the wall-clock seconds of the last solve. It is the one number that
JAOS reports and that changes between runs.

## Checking answers and certificates

The checker shares no algorithm with the solver. It calls only the
library's arithmetic helpers (the compensated sum, the exact product's
residue, the rounding) and the rule that says whether a model is a MIP.
It judges a point, duals or a
certificate from the model as loaded, in the model's own units, so it can
judge an answer from any solver. `tol` must be finite and not negative, and
a call fails with `JAOS_ERR_INVALID_INPUT` otherwise.

**`jaos_check_solution`**

```c
jaos_status jaos_check_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, double tol,
    jaos_check_report *out);
```

Judges the point `col_value`, and the row duals when `row_dual` is not
null, and fills `out`. The primal half always runs:

- `max_col_violation` is the largest bound violation of a column.
- `max_row_violation` is the largest bound violation of a row.
  `max_row_violation_relative` divides each row's violation by the larger
  of 1 and the sum of its terms' magnitudes.
- `max_integrality_violation` covers the integer columns and the SOS sets.
- `max_cone_violation` is the largest distance of the point from a cone,
  divided by the larger of 1 and the largest magnitude in that cone.
- `primal_objective` is the objective at the point, with its constant and
  its quadratic term.
- `primal_feasible` is true when the column, relative row, integrality and
  cone violations are all within `tol`.

The dual half runs only when `row_dual` is given and the model has no
integer structure and no cones. `checked_duals` says whether it ran. When
it did not run, its fields stay 0 or false. When it ran:

- `max_dual_violation` is the largest sign violation of a row dual or a
  reduced cost.
- `dual_objective` is the dual objective.
- `objective_gap` is the complementarity gap at the model's bounds, taken
  as an absolute value and divided by
  `1 + |primal_objective| + |dual_objective|`.
- `gap_positive` and `gap_negative` sum the positive terms of the gap and
  the magnitudes of its negative terms. They also count terms at the bounds
  that the checker derives from the rows where the model has none.
- `relative_suboptimality` is `gap_positive / (1 + |primal_objective|)`.
- `dropped_terms` counts the row duals and reduced costs whose sign points
  at an infinite bound, so that the dual objective has no term for them.
  `max_dropped_multiplier` is the largest of them in magnitude.
  `gap_certified` is true when `dropped_terms` is 0.
- `certified_suboptimality` looks at each column whose reduced cost points
  at an infinite bound. It takes the reduced cost's magnitude times the
  step the column can make that way before a finite row side stops it, and
  keeps the largest such product.
- `unquantified_rays` counts those columns that no row stops and whose
  reduced cost is within `tol`.
- `dual_feasible` is true when `max_dual_violation` and `objective_gap` are
  within `tol`.

The call fails when `col_value` is null on a model with columns, or when an
input value is NaN.

**`jaos_check_conic_solution`**

```c
jaos_status jaos_check_conic_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, const double *cone_dual,
    double tol, jaos_check_report *out);
```

Judges an answer as `jaos_check_solution` does, and takes the cone duals
too, one value per cone member, cone after cone. The dual half then also
asks each cone's dual to lie in the dual cone. The call fails when the model
has cones and `cone_dual` is null.

**`jaos_cone_dual`**\
`jaos_status jaos_cone_dual(const jaos_model *m, int64_t k, double *z)`\
Copies the dual of cone `k` from the last solve, one value per member.
After an optimum it is the cone's dual. After an infeasible answer it is the
cone part of the certificate. The call fails when `k` is out of range or the
last solve published no cone duals.

**`jaos_certificate`**\
`jaos_status jaos_certificate(const jaos_model *m, double *row_ray)`\
Copies the Farkas ray behind an infeasible answer, one multiplier per row.
The call fails unless the last solve ended `JAOS_SOLVE_INFEASIBLE` with a
ray. A branch and bound publishes a ray only when its root relaxation is
infeasible.

**`jaos_check_certificate`**\
`jaos_status jaos_check_certificate(const jaos_model *m, const double *row_ray, double tol, jaos_certificate_report *out)`\
Judges a Farkas ray `y` over the rows. The rows combined by `y` require
`y'Ax >= inf_rows`, and the column bounds cap `y'Ax` at `sup_columns`.
`certified` is true when `gap = inf_rows - sup_columns` is larger than
`tol * (1 + |sup_columns| + |inf_rows|)`. The call fails when an entry of
`row_ray` is not finite.

**`jaos_check_conic_certificate`**\
`jaos_status jaos_check_conic_certificate(const jaos_model *m, const double *row_ray, const double *cone_ray, double tol, jaos_certificate_report *out)`\
Judges the certificate of an infeasible model with cones: the row
multipliers and the cone part, one value per cone member. The call fails
when the model has cones and `cone_ray` is null.

**`jaos_unbounded_ray`**\
`jaos_status jaos_unbounded_ray(const jaos_model *m, double *col_ray)`\
Copies the direction behind an unbounded answer, one value per column. The
call fails unless the last solve ended `JAOS_SOLVE_UNBOUNDED` with a
direction.

**`jaos_check_ray`**\
`jaos_status jaos_check_ray(const jaos_model *m, const double *col_ray, double tol, jaos_ray_report *out)`\
Judges an unbounded direction `d`. It reports the objective's rate `c'd` in
`rate`. `max_col_escape` says how far `d` leaves a finite column bound or a
cone. `max_row_escape` says how far `d` leaves a finite row side. For a
quadratic row with matrix `Q_i`, it also takes the largest entry of
`Q_i d` that is not zero within the tolerance. `curvature` is the curvature
`d'Qd` of the objective. `certified` is true when the rate improves the
objective beyond the tolerance, no bound, row or cone is left, and the
curvature is zero within the tolerance. The call fails when an entry of
`col_ray` is not finite.

## Infeasible subsystem and feasibility relaxation

**`jaos_iis`**\
`jaos_status jaos_iis(jaos_model *m, jaos_iis_side *row_side, jaos_iis_side *col_side, jaos_iis_report *out)`\
Finds an irreducible infeasible subsystem after a solve that ended
`JAOS_SOLVE_INFEASIBLE`. The subsystem is a set of row and column bound
sides that is infeasible on its own and feasible without any one of them.
The optional arrays receive `JAOS_IIS_NONE` (0), `JAOS_IIS_LOWER` (1),
`JAOS_IIS_UPPER` (2) or `JAOS_IIS_BOTH` (3) for each row and column.
`JAOS_IIS_BOTH` is `JAOS_IIS_LOWER | JAOS_IIS_UPPER`, and the library tests
the two bits apart. `out` receives the member count in `members`, the
candidates in `candidates`, the solves in `solves`, the work units in
`work_units`, and in `from_certificate` whether the search started from the
solve's Farkas ray. Integer marks, semi-continuous marks, SOS sets,
indicator rows and the quadratic objective are left out, so the subsystem
explains the linear relaxation. The call fails on a model with cones or
quadratic rows. It returns `JAOS_ERR_NUMERICAL` when the relaxation turns
out feasible, or when a re-solve stops at a limit. The work and time limits
apply to each re-solve.

**`jaos_iis_model`**\
`jaos_status jaos_iis_model(const jaos_model *m, const jaos_iis_side *row_side, const jaos_iis_side *col_side, jaos_model **out)`\
Builds a new model from the sides that `jaos_iis` marked. Each member side
keeps its value, every other side becomes infinite, and every cost becomes
0. The new model has no integer or semi-continuous marks, SOS sets,
indicator rows or quadratic objective. Rows with no member side are
removed, and so are columns with no entries and no member bound. Names stay
and indices change. The caller frees `*out`, and the call fails when a
pointer is null or a side is not one of the four values.

**`jaos_feasrelax`**\
`jaos_status jaos_feasrelax(jaos_model *m, jaos_relax_scope scope, double *row_move, double *col_move, jaos_relax_report *out)`\
Finds the smallest total move of bounds that makes the model feasible.
`scope` is `JAOS_RELAX_ROWS` (1), `JAOS_RELAX_COLS` (2) or `JAOS_RELAX_BOTH`
(3), which lets the row bounds, the column bounds or both move. The enum
starts at 1, so 0 is not a scope. The optional arrays `row_move` and
`col_move` receive each move. A negative move lowers a lower bound, and a
positive move raises an upper bound. Adding every move to its bound gives a
feasible model. `out` receives the total in `total`, the counts of rows and
columns moved in `rows_moved` and `cols_moved`, and the largest move in
`largest`. `at_row` or `at_col` says where the largest move is, and the
other one is -1. Both are -1 when nothing moved. `out` also receives the
work units in `work_units` and the status of the solve in `status`. The
call solves an elastic copy and does not change the model. It fails on a
model with cones or quadratic rows or with an unknown `scope`, and returns
`JAOS_ERR_NUMERICAL` when the copy's solve does not end optimal.

## Ranging

Ranging reads the basis behind the last optimum. It applies to a continuous
linear model whose last solve ended optimal. It refuses a quadratic
objective, cones, quadratic rows and integer structure. Each interval says
how far one number may move, with everything else held, before that basis
stops being optimal. An end with no limit is infinite, and every output
array is optional.

**`jaos_cost_ranging`**\
`jaos_status jaos_cost_ranging(jaos_model *m, double *lower, double *upper)`\
Fills, for each column, the interval `[lower, upper]` that its cost may
take.

**`jaos_rhs_ranging`**\
`jaos_status jaos_rhs_ranging(jaos_model *m, double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi)`\
Fills, for each row, the interval `[lower_lo, lower_hi]` that its lower
bound may take and the interval `[upper_lo, upper_hi]` that its upper bound
may take.

**`jaos_bound_ranging`**\
`jaos_status jaos_bound_ranging(jaos_model *m, double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi)`\
Fills the same two intervals for the bounds of each column.

## Exact verification

These calls work in exact rational arithmetic with a fixed budget of
`JM_EXACT_LIMBS` limbs of 32 bits, which is 4096 bits in a default build. A
report's `bound_bits` is the size a result needs, and `capacity_bits` is the
size the arithmetic holds. The exact getters return decimal strings that
the model owns. Each string is an integer or a ratio of two integers.
`jaos_verify` and `jaos_verify_basis` refuse a quadratic objective, cones,
quadratic rows and integer structure.

**`jaos_verify`**\
`jaos_status jaos_verify(jaos_model *m, jaos_verify_report *out)`\
Proves or refutes that the basis behind the last optimum is optimal, in
exact arithmetic and with no tolerance. `out->status` is
`JAOS_PROOF_OPTIMAL` (0), `JAOS_PROOF_BROKEN` (1) or `JAOS_PROOF_REFUSED`
(2). `JAOS_PROOF_REFUSED` means the numbers do not fit the budget.
`out->stage` is `JAOS_PROOF_STAGE_NONE` (0) unless the proof broke. A broken
proof names its stage, `JAOS_PROOF_STAGE_RANK` (1), `JAOS_PROOF_STAGE_PRIMAL`
(2) or `JAOS_PROOF_STAGE_DUAL` (3). At the primal or the dual stage, `at_row`
or `at_col` names the row or column that breaks it, and the other one is
-1. `violation` is then how far that basic value lies outside its bounds,
or the magnitude of the reduced cost or row dual whose sign is wrong.
`blocks` and `largest_block` give the number and the largest size of the
diagonal blocks the basis splits into. `terms` counts the exact products
the proof took, and `bytes_held` is the size in bytes of the largest dense
block it held. The call fails unless the last solve ended optimal with a
basis, and it returns `JAOS_ERR_NUMERICAL` when it cannot judge.

**`jaos_verify_basis`**\
`jaos_status jaos_verify_basis(jaos_model *m, const jaos_basis_status *col_status, const jaos_basis_status *row_status, jaos_verify_report *out)`\
Runs the same proof on a basis that the caller supplies, with no solve. The
basis may come from another solver's file through `jaos_read_mps_basis`.
The call fails when an array is null, a status is not one of the four, or
the number of basic entries is not `jaos_num_row(m)`.

**`jaos_exact_col_value`**\
`jaos_status jaos_exact_col_value(const jaos_model *m, int64_t col, const char **out)`\
Stores in `*out` the exact value of column `col` in the basis that
`jaos_verify` or `jaos_verify_basis` proved optimal. The call fails when
`col` is out of range or no proof holds exact values.

**`jaos_exact_row_dual`**\
`jaos_status jaos_exact_row_dual(const jaos_model *m, int64_t row, const char **out)`\
Stores in `*out` the exact dual of row `row` from the same proof. It fails
as `jaos_exact_col_value` does.

**`jaos_exact_objective`**\
`jaos_status jaos_exact_objective(const jaos_model *m, const char **out)`\
Stores in `*out` the exact objective from the same proof. The call fails
when there are no exact values. It also fails when the objective did not fit
the budget while the values did.

**`jaos_exact_certificate`**\
`jaos_status jaos_exact_certificate(jaos_model *m, jaos_exact_ray_report *out)`\
Derives the exact Farkas multipliers of an infeasible answer from the basis
the solve stopped on. `out->derived` is true when it succeeds. The call
returns `JAOS_OK` with `derived` false when `bound_bits` exceeds
`capacity_bits`. `blocks`, `largest_block`, `terms` and `bytes_held` mean
what they mean in `jaos_verify_report`. `at_row` is the row at the basis
position where the published ray is largest, or -1 when a column holds that
position. The call fails unless the last solve ended infeasible with a ray
and a basis. It returns `JAOS_ERR_NUMERICAL` when the basis cannot give the
ray, for example when the basis is singular.

**`jaos_exact_row_multiplier`**\
`jaos_status jaos_exact_row_multiplier(const jaos_model *m, int64_t row, const char **out)`\
Stores in `*out` the exact multiplier of row `row` that
`jaos_exact_certificate` derived. The call fails when `row` is out of range
or no exact certificate is held.

**`jaos_exact_unbounded_ray`**\
`jaos_status jaos_exact_unbounded_ray(jaos_model *m, jaos_exact_ray_report *out)`\
Derives the exact direction of an unbounded answer from the basis the solve
stopped on. It reports as `jaos_exact_certificate` does, and leaves
`at_row` at -1. The call fails unless the last solve ended unbounded with a
direction and a basis.

**`jaos_exact_col_direction`**\
`jaos_status jaos_exact_col_direction(const jaos_model *m, int64_t col, const char **out)`\
Stores in `*out` the exact direction entry of column `col` that
`jaos_exact_unbounded_ray` derived. The call fails when `col` is out of
range or no exact direction is held.

## Proof files

A proof file holds an answer with every number an exact rational.
`docs/format-support.md` describes its format.

**`jaos_write_proof`**\
`jaos_status jaos_write_proof(jaos_model *m, const char *path)`\
Writes the proof file of the last answer. For an optimum it writes the
values that `jaos_verify` proved, and it fails when there are none. For an
infeasible or unbounded answer it writes the exact ray, which it derives when
it can, and the published doubles otherwise. It then checks the file with
`jaos_check_proof` and deletes it when the check does not certify it. It
never compresses the file.

**`jaos_check_proof`**\
`jaos_status jaos_check_proof(jaos_model *m, const char *path, jaos_proof_report *out)`\
Reads a proof file and judges it from the model alone, in exact arithmetic.
For an optimum it checks primal feasibility, dual feasibility and the
objective the file claims. `primal`, `dual` and `objective` say which of the
three held, and `out->certified` is true when all three hold. For a
certificate it checks the ray. `bad_row` and `bad_col` name the first row
and column that break a check, or -1. `kind` says what the file proves:
`JAOS_PROOF_FILE_OPTIMAL` (0), `JAOS_PROOF_FILE_INFEASIBLE` (1) or
`JAOS_PROOF_FILE_UNBOUNDED` (2). `terms` counts the exact products the check
took. The call refuses a model with a quadratic objective, cones, quadratic
rows or integer structure. It returns `JAOS_ERR_NUMERICAL` when the
arithmetic outgrows its budget.
