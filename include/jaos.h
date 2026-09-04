/* JAOS — Just Another Optimization Solver.
 *
 * Public API. This is the only public header; everything not declared here is
 * internal and carries no stability promise.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JAOS_H
#define JAOS_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* JAOS itself is built as C23, but this header is consumed by whatever
 * compiler the caller uses, so attribute use degrades gracefully. */
#if defined(__cplusplus) && __cplusplus >= 201703L
  #define JAOS_NODISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
  #define JAOS_NODISCARD [[nodiscard]]
#else
  #define JAOS_NODISCARD
#endif

#define JAOS_VERSION_MAJOR 0
#define JAOS_VERSION_MINOR 2
#define JAOS_VERSION_PATCH 0
#define JAOS_VERSION_STRING "0.2.0"

/* Runtime library version, e.g. "0.1.1". Static storage, never NULL. */
JAOS_NODISCARD const char *jaos_version(void);

/* Result of a library call. Every fallible function returns one of these;
 * data leaves through parameters, never through the return value. */
typedef enum jaos_status {
    JAOS_OK = 0,
    JAOS_ERR_INVALID_INPUT,  /* an argument or file content JAOS rejects */
    JAOS_ERR_OUT_OF_MEMORY,
    JAOS_ERR_IO,             /* the underlying read failed, not the content */
    JAOS_ERR_NUMERICAL,      /* computation abandoned for numerical reasons */
} jaos_status;

/* Outcome of a solve. Distinct from jaos_status on purpose: hitting a work or
 * time budget is not a failure — it is an honest report of where the solver
 * stopped (see DECISIONS.md, D8). */
typedef enum jaos_solve_status {
    JAOS_SOLVE_NOT_RUN = 0,
    JAOS_SOLVE_OPTIMAL,
    JAOS_SOLVE_INFEASIBLE,
    JAOS_SOLVE_UNBOUNDED,
    JAOS_SOLVE_WORK_LIMIT,
    JAOS_SOLVE_TIME_LIMIT,
    JAOS_SOLVE_NUMERICAL_ERROR,
    /* Appended, and appended on purpose: an enumerator inserted above this
     * line would renumber every one below it for anyone who did not
     * recompile. */
    JAOS_SOLVE_INTERRUPTED,
} jaos_solve_status;

/* Human-readable name for a status. Static storage; never NULL, including for
 * values outside the enum. */
JAOS_NODISCARD const char *jaos_status_str(jaos_status s);
JAOS_NODISCARD const char *jaos_solve_status_str(jaos_solve_status s);

/* How much the solver says about what it is doing. Silent by default: a
 * library that writes to stdout because nobody asked it not to is a library
 * that cannot be embedded. */
typedef enum jaos_log_level {
    JAOS_LOG_OFF = 0,
    JAOS_LOG_SUMMARY,   /* one line when a solve starts, one when it ends */
    JAOS_LOG_PROGRESS,  /* and the objective every so many iterations */
    JAOS_LOG_DETAIL,    /* and the events that change how a solve behaves */
} jaos_log_level;

/* ------------------------------------------------------------------------- */
/* Problem data                                                              */
/* ------------------------------------------------------------------------- */

/* The problem JAOS works on is the bounded form
 *
 *     optimize   c'x + c0
 *     subject to rl <=  A x  <= ru        (row bounds)
 *                xl <=   x   <= xu        (column bounds)
 *
 * which subsumes equalities (rl == ru), ranged rows, fixed and free
 * variables. An absent bound is IEEE infinity of the right sign: use
 * jaos_infinity(), or any value v with isinf(v). */

/* Positive IEEE infinity, for absent bounds. */
JAOS_NODISCARD double jaos_infinity(void);

typedef enum jaos_obj_sense {
    JAOS_MINIMIZE = 0,
    JAOS_MAXIMIZE = 1,
} jaos_obj_sense;

/* Opaque. One model is used by one thread at a time; distinct models are
 * fully independent. */
typedef struct jaos_model jaos_model;

/* Allocates an empty model into *out. Frees with jaos_model_free. */
JAOS_NODISCARD jaos_status jaos_model_new(jaos_model **out);

/* Frees a model and everything it owns. NULL is fine. */
void jaos_model_free(jaos_model *m);

/* Loads a complete problem, replacing whatever the model held. All data is
 * copied; the caller's arrays are never retained.
 *
 * The matrix arrives in compressed sparse column form: a_start[num_col + 1]
 * with a_start[0] == 0 and a_start[num_col] == num_nz; a_index holds row
 * indices, a_value the coefficients. Column entries need not be sorted;
 * JAOS sorts its copy. Explicit zeros are dropped. a_start may be NULL only
 * when num_nz == 0, meaning an all-zero matrix.
 *
 * Rejected as JAOS_ERR_INVALID_INPUT: NaN anywhere; non-finite costs,
 * offset or matrix values; row indices out of range; duplicate row indices
 * within a column; inconsistent a_start. Inconsistent bounds (xl > xu) are
 * NOT rejected — that is a legitimate, trivially infeasible model, and
 * deciding feasibility is the solver's job, not the loader's. */
JAOS_NODISCARD jaos_status jaos_load_lp(jaos_model *m,
    int64_t num_col, int64_t num_row,
    jaos_obj_sense sense, double obj_offset,
    const double *col_cost,
    const double *col_lower, const double *col_upper,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);

/* Dimension queries. NULL model reads as empty. */
JAOS_NODISCARD int64_t jaos_num_col(const jaos_model *m);
JAOS_NODISCARD int64_t jaos_num_row(const jaos_model *m);
JAOS_NODISCARD int64_t jaos_num_nz(const jaos_model *m);

/* ------------------------------------------------------------------------- */
/* Reading and changing a loaded problem                                     */
/* ------------------------------------------------------------------------- */

/* Read back one cost or one pair of bounds. Either bound pointer may be NULL.
 *
 * These exist because the setters below do. A caller who built the model with
 * jaos_load_lp already knows what is in it, but one who read it from a file
 * does not — and telling that caller they may change a bound while giving them
 * no way to see the bound they are changing is not an API, it is a trap. The
 * first program to need them was JAOS's own: measuring what warm re-solve buys
 * means applying a branch-and-bound branching step to a Netlib instance, and a
 * branch is `x_j <= floor(x_j*)` only when floor(x_j*) is still above the
 * column's own lower bound. That could not be asked.
 *
 * The values are the model's own, exactly as loaded or last set: no scaling,
 * no substituted default, and an absent bound reads as an infinity. */
JAOS_NODISCARD jaos_status jaos_col_cost(const jaos_model *m, int64_t col,
                                         double *cost);
JAOS_NODISCARD jaos_status jaos_col_bounds(const jaos_model *m, int64_t col,
                                           double *lower, double *upper);
JAOS_NODISCARD jaos_status jaos_row_bounds(const jaos_model *m, int64_t row,
                                           double *lower, double *upper);

/* Change one cost or one pair of bounds in place, leaving the rest of the
 * model as it stands. Costs must be finite. Bounds may be infinite but never
 * NaN, and `lower > upper` is accepted: that is a model with no feasible
 * point, which the solve reports as infeasible, not a call to refuse — the
 * same rule `jaos_load_lp` applies, so a model is buildable the same way by
 * either route.
 *
 * **Any of these discards the answer the model is holding.** That answer was
 * computed for the problem as it stood, and once a bound moves it describes a
 * different problem; leaving it readable would let a caller change one number
 * and read back the previous optimum with nothing to say it was stale.
 * `jaos_status_of` reads JAOS_SOLVE_NOT_RUN afterwards and `jaos_solution`
 * refuses, so the mistake surfaces at the call rather than as a number.
 *
 * Tolerances, budgets and logging settings are configuration and survive.
 *
 * **The basis survives too, and re-solving starts from it.** The answer stops
 * being true when a bound moves; the basis it was read off does not stop
 * being a basis, and for a small change it is usually near the new problem's.
 * Starting there is what the dual simplex is for — moving a bound leaves
 * every reduced cost where it was, so the basis is still dual feasible and
 * the method resumes from a point it can use, rather than from the slack
 * basis it would otherwise walk back to. See jaos_set_basis. */
JAOS_NODISCARD jaos_status jaos_set_col_cost(jaos_model *m, int64_t col,
                                             double cost);
JAOS_NODISCARD jaos_status jaos_set_col_bounds(jaos_model *m, int64_t col,
                                               double lower, double upper);
JAOS_NODISCARD jaos_status jaos_set_row_bounds(jaos_model *m, int64_t row,
                                               double lower, double upper);

/* Change one matrix entry, which may create it or remove it. Setting a
 * coefficient to exactly 0.0 deletes it, because the loaded model holds no
 * explicit zeros — writing one in would leave a model that no longer matches
 * what loading the same data would produce. Setting one where there was no
 * entry inserts it. The value must be finite.
 *
 * Costs more than changing a bound: the row-wise mirror and the scaling are
 * both computed from the matrix, so both are discarded and rebuilt on the
 * next solve. Changing many entries before solving once pays that once. */
JAOS_NODISCARD jaos_status jaos_set_coefficient(jaos_model *m, int64_t row,
                                                int64_t col, double value);

/* Grow or shrink the problem itself.
 *
 * New columns and rows are appended, so existing indices never move: the
 * columns added by a call occupy jaos_num_col() .. jaos_num_col() + num_new - 1
 * as it stood before it, and likewise for rows. The matrix argument follows
 * jaos_load_lp exactly — the same layout, the same validation, the same
 * dropping of explicit zeros — except that jaos_add_cols describes the new
 * columns down (row indices into the *existing* rows) and jaos_add_rows
 * describes the new rows across (column indices into the existing columns).
 * Either may be empty: num_nz == 0 adds columns with no coefficients, which is
 * what a column generation scheme wants before it knows them.
 *
 * Deletion takes a set of indices, not one index, and that is not a
 * convenience. Deleting renumbers everything after what was deleted, so a
 * caller removing three rows one at a time has to track the shift itself and
 * will eventually get it wrong. Given the whole set, JAOS renumbers once. What
 * survives keeps its relative order and is renumbered densely from zero.
 * Repeated or out-of-range indices are refused rather than tolerated: a
 * repeated index is a caller who has already lost track.
 *
 * **All four discard the answer**, for the reason the setters above do, and
 * all four discard the row-wise mirror and the scaling, because all four
 * change the matrix.
 *
 * **The starting basis survives exactly when what is left is still a basis.**
 * There is one rule and it is the same one jaos_set_basis enforces: a model
 * with n rows needs n basic variables. New rows arrive basic, which is where
 * a slack basis puts them and which keeps the count right by construction, so
 * adding rows keeps the basis — that is the case that matters, since a basis
 * made primal infeasible by a new constraint is precisely what the dual
 * simplex is best at resuming from. New columns arrive nonbasic at a bound,
 * which also keeps the count. Deleting normally does not: remove a row whose
 * activity was not basic, or a column that was, and the count no longer works
 * out, so the basis is dropped and the next solve runs cold.
 *
 * A new column with no finite bound drops the basis too. A nonbasic variable
 * with no bounds rests pinned at zero and this solver cannot always price it
 * back off — the same refusal jaos_set_basis's consumer already makes, and
 * keeping a basis known to be unusable would only move the cost to the solve.
 */
JAOS_NODISCARD jaos_status jaos_add_cols(jaos_model *m, int64_t num_new,
    const double *col_cost, const double *col_lower, const double *col_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);

JAOS_NODISCARD jaos_status jaos_add_rows(jaos_model *m, int64_t num_new,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *ar_start, const int64_t *ar_index,
    const double *ar_value);

JAOS_NODISCARD jaos_status jaos_delete_cols(jaos_model *m, int64_t num_del,
                                            const int64_t *cols);
JAOS_NODISCARD jaos_status jaos_delete_rows(jaos_model *m, int64_t num_del,
                                            const int64_t *rows);

/* ------------------------------------------------------------------------- */
/* File readers                                                              */
/* ------------------------------------------------------------------------- */

/* Both readers accept a gzip-compressed file wherever they accept a plain
 * one. The choice is made on the first two bytes, so the name does not
 * matter and no separate call is needed. A gzip file whose checksum or
 * length does not match its contents is refused, never parsed.
 * docs/format-support.md, "Compressed input". */

/* Reads an MPS file (fixed or free layout) into the model, replacing its
 * contents. On failure the model's problem data is left as it was and
 * jaos_model_error() carries a message with the offending line number.
 * Dialect decisions are documented in docs/format-support.md. Integer
 * markers and integer bound types are recognized and rejected until MILP
 * support lands (PLAN.md, M3). */
JAOS_NODISCARD jaos_status jaos_read_mps(jaos_model *m, const char *path);

/* Reads a CPLEX-style LP-format file into the model, replacing its
 * contents. Same error contract as jaos_read_mps. Ranged constraints,
 * constants inside constraints and integer sections are recognized and
 * rejected; see docs/format-support.md for the dialect. */
JAOS_NODISCARD jaos_status jaos_read_lp(jaos_model *m, const char *path);

/* ------------------------------------------------------------------------- */
/* File writers                                                              */
/* ------------------------------------------------------------------------- */

/* Writes the model to `path`, replacing whatever was there.
 *
 * **What JAOS writes, JAOS reads back as the same model.** Values are
 * written to enough digits to be exact, and the one construction the MPS
 * reader rebuilds by arithmetic — a ranged row — is checked against what
 * the reader will make of it before it goes out. Where the format cannot
 * express what the model holds, the call fails with
 * JAOS_ERR_INVALID_INPUT, jaos_model_error() names the row or column, and
 * no file is left behind. A failed write removes the partial file.
 *
 * The model carries no names: it is indices from the moment it is loaded.
 * So the writer generates them, `C1..Cn` for columns and `R1..Rm` for rows,
 * with `COST` for the objective row. Reading the file back gives the same
 * indices, because both formats list rows and columns in index order.
 *
 * MPS is written in free layout, which jaos_read_mps autodetects. It has
 * three refusals, all of them about bounds.
 *
 * Two are shapes MPS has no form for: a row whose lower bound is above its
 * upper one, and a bound sitting at an infinity of the wrong sign.
 *
 * The third is the ranged row that neither RANGES form reconstructs. The
 * reader rebuilds a ranged row by arithmetic — its `G` form gives
 * [b, b + |r|] and its `L` form [b - |r|, b] — so the writer computes what
 * the reader will make of both and refuses the row when neither returns the
 * pair exactly. It is rare and it is real: the refusal only ever fires on
 * rows whose two bounds are unrelated, never on the shapes real data has,
 * and no form the writer accepted has ever reconstructed wrong
 * (bench/measurements/02-138/ranges.txt owns the counts).
 *
 * The number written for each value is the shortest of 15, 16 or 17
 * significant digits that reads back as the same double, so files stay
 * readable without becoming approximate. Numbers are written under an
 * explicit "C" locale: the host application's locale cannot corrupt the
 * file, which is the rule jaos_read_mps already applies to parsing. */
JAOS_NODISCARD jaos_status jaos_write_mps(jaos_model *m, const char *path);

/* Writes the model as a CPLEX-style LP file, in the dialect jaos_read_lp
 * accepts. Same contract as jaos_write_mps, and three more refusals, because
 * the dialect is narrower than MPS: a ranged row, a free row, and a row with
 * no coefficients. Each is reported with the row named and a pointer to
 * jaos_write_mps, which has none of these limits.
 *
 * The objective names every column, including the ones costing nothing. LP
 * format has no COLUMNS section, so the reader numbers a column where its
 * name first appears; listing only the costed columns would renumber the
 * rest by wherever their first coefficient sits, and the file would read
 * back as a different model without failing. docs/format-support.md has the
 * dialect and what share of the gate survives it (D226). */
JAOS_NODISCARD jaos_status jaos_write_lp(jaos_model *m, const char *path);

/* Writes the last solve's answer to `path`: the objective, then every
 * column with its value, reduced cost and basis status, then every row with
 * its activity, dual and basis status. The format is JAOS's own, one record
 * per line, documented in docs/format-support.md.
 *
 * Available only when the last solve found an optimum, under the same rule
 * as jaos_solution and jaos_basis and for their reason: a solve that
 * found no optimum has no solution to write down, and a file of zeros does
 * not read as missing. Otherwise JAOS_ERR_INVALID_INPUT, with
 * jaos_model_error() naming the status the solve actually reached.
 *
 * Refused as well when the answer holds a value no file can carry. An
 * objective is a sum and can overflow, and a model whose bounds reach 1e300
 * solves to an optimum with an infinity or a NaN in it. Writing one would
 * print a word whose spelling belongs to the host libc, so the file would
 * not be reproducible; the call fails instead and jaos_model_error() names
 * the row or the column.
 *
 * Names match what jaos_write_mps and jaos_write_lp generate, so a solution
 * file and a model file written from the same model refer to the same
 * rows and columns. */
JAOS_NODISCARD jaos_status jaos_write_solution(jaos_model *m,
                                               const char *path);

/* Detail message for the model's last failed operation, or "" when the last
 * operation succeeded. Static storage inside the model; never NULL. */
JAOS_NODISCARD const char *jaos_model_error(const jaos_model *m);

/* ------------------------------------------------------------------------- */
/* Solving                                                                   */
/* ------------------------------------------------------------------------- */

/* Budgets. Both default to unlimited.
 *
 * The work limit is counted in deterministic work units and is reproducible
 * across machines; the time limit is wall-clock and is not — where it cuts
 * depends on the machine. That is why they are separate settings rather than
 * one "limit" (DECISIONS.md, D8). The clock never influences which pivot is
 * chosen; it only decides whether to stop at a checkpoint.
 *
 * Both are resumable. A solve that stops at either leaves the basis it stopped
 * on where the next solve will find it, so raising the limit and calling
 * jaos_solve again continues from there instead of starting over. There is no
 * answer to read in between — the run did not produce one — and jaos_basis
 * says so, because a stopping point is not a solution. See jaos_set_basis. */
JAOS_NODISCARD jaos_status jaos_set_work_limit(jaos_model *m, int64_t units);
JAOS_NODISCARD jaos_status jaos_set_time_limit(jaos_model *m, double seconds);

/* The two tolerances a caller owns. Both default to 1e-7; passing 0.0
 * restores that default.
 *
 * The primal tolerance is how far a variable may sit outside its bounds and
 * still count as feasible. The dual tolerance is how far a reduced cost may
 * sit on the wrong side of zero. Together they say how much precision the
 * data deserves — measured inputs and exact ones do not want the same answer,
 * and the solver has no way to know which it was given.
 *
 * **These two, and nothing about how the problem is solved.** Which pricing
 * rule, when a carried weight stops being worth keeping, when to refactorize,
 * whether a sparse or a dense path is cheaper: those are the solver's to
 * decide and are not settings. Nobody linking this library can be expected to
 * know whether their model wants one or the other, and an option that asks
 * hands back a problem that belongs here.
 *
 * Both act in the scaled space the solver works in, not in the units of the
 * model as written; docs/tolerances.md says what that means for a coefficient
 * range. A value that is not finite and non-negative is rejected rather than
 * clamped, because a solver that quietly substitutes its own number reports
 * success for a run its caller cannot reason about.
 *
 * D8 still holds: a tolerance changes the answer identically on every
 * machine. */
JAOS_NODISCARD jaos_status jaos_set_primal_tolerance(jaos_model *m, double tol);
JAOS_NODISCARD jaos_status jaos_set_dual_tolerance(jaos_model *m, double tol);

/* Where the solver's output goes, and how much of it there is.
 *
 * There is no default destination. A library that writes to stdout because
 * nobody told it not to cannot be embedded in a server, a GUI or another
 * library, so JAOS says nothing at all until a callback is installed —
 * setting a level without one changes nothing.
 *
 * `line` is a complete message with no trailing newline, valid only for the
 * duration of the call: copy it if it must outlive that. `user` is handed
 * back untouched. The callback must not call into JAOS on the same model.
 *
 * **Logging never changes an answer.** No message is produced by computing
 * anything the solve did not already compute, no output is emitted from a
 * decision point, and the level is not readable by the solver's arithmetic.
 * A model solved at JAOS_LOG_DETAIL returns the same bits as the same model
 * solved silently, which is D8 and is checked over all 139 reference
 * instances rather than assumed.
 *
 * Passing NULL for `cb` turns output off again. */
typedef void (*jaos_log_fn)(void *user, jaos_log_level level, const char *line);

JAOS_NODISCARD jaos_status jaos_set_log_callback(jaos_model *m,
                                                 jaos_log_fn cb, void *user);
JAOS_NODISCARD jaos_status jaos_set_log_level(jaos_model *m,
                                              jaos_log_level level);

/* ------------------------------------------------------------------------- */
/* Watching a solve, and stopping one                                        */
/* ------------------------------------------------------------------------- */

typedef enum jaos_callback_action {
    JAOS_CALLBACK_CONTINUE = 0,
    JAOS_CALLBACK_STOP,
} jaos_callback_action;

/* What the solve can say about itself while it is still running.
 *
 * There is no objective here, and its absence is the design. A dual simplex
 * carries a point that is not feasible until it finishes, so any objective it
 * could report mid-solve is a number about a point the solver does not vouch
 * for — and this library does not hand back numbers it will not stand behind
 * (D20). What it does watch is the total primal infeasibility, which is the
 * measure its own progress and stall detection are written in, so that is
 * what a watcher gets: the real one, not a plausible one. */
typedef struct jaos_progress {
    int64_t iterations;
    int64_t work_units;
    /* The best total reached so far. The first call comes before anything has
     * been priced, so it reports the infinity this starts at — that is not a
     * placeholder, it is the true answer to "how close is it" before the
     * question has been asked once. */
    double primal_infeasibility;
} jaos_progress;

/* Called during a solve, and its return value decides whether the solve goes
 * on. `user` is handed back untouched. Like the log callback it must not call
 * into JAOS on the same model — the solver is holding a factorization, a
 * scaling and a basis derived from the model as it stood, and changing the
 * model underneath them leaves the two disagreeing with nothing to notice it.
 *
 * **A callback may look, and it may ask the solve to stop. It may not steer
 * one.** Which column prices, when to refactorize, whether a weight is worth
 * carrying: those are the method, and D64's line puts the method on this side
 * of the wall. A caller cannot know the answers and being asked for them is a
 * problem handed back.
 *
 * **What this does to determinism, exactly.** Asking is paced by iteration
 * count and never by a clock, so *when* the question is put is itself
 * reproducible; and given the same sequence of answers the solve is
 * bit-identical, because nothing else about it depends on the callback
 * existing. What the caller decides is the caller's, and if they decide on a
 * clock then their stopping point moves — which is already true of
 * jaos_set_time_limit, so this generalises a precedent rather than breaking a
 * rule. A callback that always returns CONTINUE returns the same bits as no
 * callback at all, over all 139 reference instances.
 *
 * **Stopping is not answering.** The solve ends as JAOS_SOLVE_INTERRUPTED,
 * jaos_solution refuses, and there is nothing to read in between. It keeps
 * the basis it stopped on, exactly as a budget stop does, so calling
 * jaos_solve again continues from there rather than starting over.
 *
 * Passing NULL for `cb` means nobody is asked and the solve runs to its own
 * end. */
typedef jaos_callback_action (*jaos_progress_fn)(const jaos_progress *p,
                                                 void *user);

JAOS_NODISCARD jaos_status jaos_set_progress_callback(jaos_model *m,
                                                      jaos_progress_fn cb,
                                                      void *user);

/* Solves the model. The outcome is reported by jaos_solve_status, which the
 * return value does not duplicate: JAOS_OK means the solve ran, not that it
 * found an optimum. */
JAOS_NODISCARD jaos_status jaos_solve(jaos_model *m);

JAOS_NODISCARD jaos_solve_status jaos_status_of(const jaos_model *m);

/* Objective value of the solution held by the model, including the constant
 * term, into *out. Returns JAOS_ERR_INVALID_INPUT when no optimum is
 * available, rather than a number that cannot be told apart from a genuine
 * objective of zero. */
JAOS_NODISCARD jaos_status jaos_objective(const jaos_model *m, double *out);

/* Copies the solution into caller-provided buffers; any of them may be NULL.
 * col_value and col_dual hold num_col entries, row_activity and row_dual
 * num_row each. The library never hands out pointers into its own storage,
 * so there are no lifetimes to track.
 *
 * Available only when the last solve found an optimum, under the same rule
 * as jaos_objective: any other outcome returns JAOS_ERR_INVALID_INPUT,
 * because a buffer of zeros cannot be told apart from an answer that is
 * genuinely zero. */
JAOS_NODISCARD jaos_status jaos_solution(const jaos_model *m,
    double *col_value, double *row_activity, double *row_dual,
    double *col_dual);

/* Where each variable rests in the basis behind the reported solution.
 *
 * A basic variable's value comes out of the factorization and may sit
 * anywhere between its bounds; a nonbasic one is pinned to a bound, and that
 * pinning is what makes a basis determine a point at all. The difference is
 * not recoverable from the values: a basic variable that happens to land
 * exactly on a bound reads identically to a nonbasic one resting there, and
 * only one of the two is a constraint the optimum is actually held by. That
 * is why this is reported rather than left to be inferred.
 *
 * A row is described by its activity, so JAOS_BASIS_AT_LOWER on row i means
 * A_i x rests on rl_i. */
typedef enum jaos_basis_status {
    JAOS_BASIS_BASIC = 0,
    JAOS_BASIS_AT_LOWER,
    JAOS_BASIS_AT_UPPER,
    JAOS_BASIS_FREE,   /* nonbasic at zero, both bounds infinite */
} jaos_basis_status;

/* Copies the basis into caller-provided buffers; either may be NULL.
 * col_status holds num_col entries, row_status num_row.
 *
 * Available only when the last solve found an optimum, under the same rule
 * as jaos_solution and for a sharper version of the same reason: a buffer of
 * zeros does not read as missing, it reads as a solution in which everything
 * is basic. Exactly num_row of the num_col + num_row statuses are basic, a
 * nonbasic status names a bound the variable has, and a column whose two
 * bounds are equal is named at the one its reduced cost points into, so the
 * statuses are a basis of this model as loaded and the ranging calls below
 * read them as one (D257, D258). */
JAOS_NODISCARD jaos_status jaos_basis(const jaos_model *m,
    jaos_basis_status *col_status, jaos_basis_status *row_status);

/* Where the next solve starts. Both arrays are required, sized as above —
 * half a basis does not say which variables are basic, so there is nothing
 * useful to do with one.
 *
 * **A solve sets this for itself.** Nothing needs to be called for a re-solve
 * to be warm: change a bound and solve again, and the previous basis is where
 * the second solve begins. This function is for the cases that route cannot
 * reach — a basis carried over from another model, one saved to a file and
 * read back, or the one a branch-and-bound node hands to its children.
 *
 * A solve that stopped at a work or time limit sets it too, which is what
 * makes those budgets resumable: solve, raise the limit, solve again, and the
 * second call continues rather than starting over. So does one that ended
 * INFEASIBLE or UNBOUNDED — the model is answered, but the next model that
 * differs from it by one bound has no closer place to begin. A solve
 * abandoned for numerical reasons is the exception and leaves the previous
 * basis untouched: it cannot corrupt an answer, but it is the one state this
 * solver does not vouch for, and offering it would be recommending it.
 *
 * Refused as JAOS_ERR_INVALID_INPUT: a value that is not one of the four
 * statuses, and any count of basic variables other than num_row. Those are
 * structural, and nothing that happens later can make a wrong count right.
 *
 * Two things are deliberately *not* refused, because a basis stored across a
 * modification meets both and must keep working. A nonbasic status may name a
 * bound the variable no longer has — jaos_set_col_bounds can retire it — and
 * the basic columns may be linearly dependent. The solve repairs each: a
 * status with no bound behind it is moved to the other bound, or to free; a
 * singular basis has logicals put back into it until it factors. Both cost
 * iterations and neither costs correctness.
 *
 * A warm start is a starting point and never a claim about the answer. The
 * solve that follows proves optimality from scratch, so a basis that is
 * wrong, stale or hostile costs time and cannot produce a wrong verdict.
 * That sentence was measured false once (D146: 26 wrong optima from 80
 * hostile bases) and is enforced now rather than assumed (D148): the solve
 * reads its own settled dual violation before publishing, an uncertified
 * warm start is retried once from the slack basis, and an uncertified cold
 * start reports JAOS_SOLVE_NUMERICAL_ERROR instead of an answer.
 *
 * What it does change is which optimum is reported when a model has more than
 * one: two runs from different starting bases can stop at different vertices,
 * both optimal and both with the same objective. Determinism is unaffected —
 * the same starting basis gives the same answer on every machine and every run
 * (D8) — but "solve twice and compare the bits" is a statement about a
 * sequence of calls, not about the model.
 *
 * Dropped by anything that loads a problem, since the indices then refer to a
 * different model. Kept by every modification, which is the point of it. */
JAOS_NODISCARD jaos_status jaos_set_basis(jaos_model *m,
    const jaos_basis_status *col_status, const jaos_basis_status *row_status);

/* Forgets the starting basis, so the next solve begins where a first solve
 * would. NULL is fine, and so is a model that never had one.
 *
 * Without this, warm starting would be a one-way door: a model that has been
 * solved once could never be solved from scratch again, and "what would this
 * model do cold" would stop being a question its own library could answer.
 * Three callers want it — one reproducing a result exactly, one escaping a
 * warm start that turned out to be a bad one, and one timing the two against
 * each other. JAOS's own acceptance gate is the third: it solves every
 * reference instance twice and requires the two runs to agree bit for bit,
 * which is a statement about the solver only if both runs start in the same
 * place.
 *
 * Clearing is not `jaos_set_basis` with nulls. Passing one null there is half
 * a basis and is refused, and a call that means "none" should not have to be
 * spelled as a special case of a call that means "this one". */
void jaos_clear_basis(jaos_model *m);

/* Work units consumed by the last solve. */
JAOS_NODISCARD int64_t jaos_work_units(const jaos_model *m);

/* Simplex iterations performed by the last solve. */
JAOS_NODISCARD int64_t jaos_iterations(const jaos_model *m);

/* Seconds the last solve took, 0 if none has run. Wall clock, monotonic, and
 * excluding the read that loaded the model — the same span the work counter
 * covers, so the two describe one thing.
 *
 * **This is the only number JAOS reports that is not reproducible**, and it is
 * offered anyway because it answers a question the other two cannot. Work
 * units are identical on every machine and in every run, which is what makes
 * them a regression test; they are also biased, by a factor that is not
 * constant, against exactly the work that costs the most real time. A change
 * that halves the units and doubles the seconds has happened here. So the two
 * are read together, and neither on its own.
 *
 * The consequence for a caller who records results: **do not put this in a
 * file you diff against later.** JAOS keeps its own acceptance records free of
 * wall-clock numbers for that reason — a baseline that changes on every run
 * cannot detect a regression — and keeps its competitive timings in a
 * different place that says which machine produced them. */
JAOS_NODISCARD double jaos_solve_time(const jaos_model *m);

/* ------------------------------------------------------------------------- */
/* Independent solution checker                                              */
/* ------------------------------------------------------------------------- */

/* Verdict of jaos_check_solution. Violations are raw magnitudes, not
 * pass/fail: the booleans compare them against the tolerance given. */
typedef struct jaos_check_report {
    double max_col_violation;   /* worst breach of a column bound          */
    double max_row_violation;   /* worst breach of a row (activity) bound  */
    double max_row_violation_relative;
                                /* the same breach as a fraction of what
                                   the row carries — sum of |a_ij x_j|.
                                   Reported only; no verdict reads it     */
    double max_dual_violation;  /* worst breach of a dual sign condition,
                                   including complementary slackness       */
    double primal_objective;    /* c'x + c0, in the model's own sense      */
    double dual_objective;      /* meaningful only when duals were given   */
    double objective_gap;       /* |primal - dual|
                                   / (1 + |primal| + |dual|)              */
    /* The two halves the gap is the difference of. P - D is a sum over
       every row and column of w_v (v - bound_v); these accumulate the
       non-negative terms and the magnitudes of the negative ones
       separately, in the objective's own units. A negative term needs a
       primal violation, so on an exactly feasible point gap_negative is
       zero and gap_positive alone bounds the suboptimality:
       P - P* <= gap_positive — but only when gap_certified below says the
       sum it came from was complete. When gap_negative is not zero the two
       cancel, and a small objective_gap no longer says the point is nearly
       optimal, which the gap on its own cannot show. Both decide nothing. */
    double gap_positive;
    double gap_negative;

    /* The largest multiplier whose term that sum could not take, and how
       many there were.

       A multiplier whose sign points at an infinite bound has no w * bound
       to contribute: the term is minus infinity, because the dual objective
       of a variable free in the improving direction is unbounded below.
       Dropping it silently leaves dual_objective describing a *different*
       problem — one where that variable had a finite bound — so the bound
       above stops holding for the problem that was asked about.

       That is not hypothetical. Two variables and one constraint are enough
       to build a point that is arbitrarily suboptimal and on which every
       number in this report reads zero, including gap_positive (D47). This
       pair is what makes that case visible from outside.

       Neither decides anything, and that is deliberate: deciding would need
       a threshold on the multiplier, and what makes a dropped term cost
       anything is the distance the variable would travel, which is a
       property of the whole polytope and not of the column. No test on the
       multiplier alone separates the harmful case from the harmless one —
       measured, in D47. So the caller is given the fact and its size. */
    double max_dropped_multiplier;
    int64_t dropped_terms;

    /* How much better the objective provably gets, from the dropped terms
       alone. A lower bound on P - P*, in the objective's own units, and a
       certificate rather than an estimate: it is |w| times a distance the
       point can actually travel, along a direction every point of which is
       feasible. Zero means nothing was certified, which is emphatically not
       the same as nothing being wrong.

       The direction is one column moving on its own with every other
       variable held where it is. That is why no factorization is needed and
       why the checker keeps owing the solver nothing: the simplex direction
       lets the basic variables absorb the move and travels further, but
       computing it needs B^-1 a_j. Travelling less far certifies less
       suboptimality, and a smaller lower bound is still a lower bound.

       Infinity means the objective improves without limit along a feasible
       ray, which is a proof that the model is unbounded rather than that this
       point is suboptimal.

       **A zero here says almost nothing, and the reason is structural.** A
       column moving alone is stopped by the first row that is tight, and a
       vertex is what having tight rows means — so at any vertex, which is
       where every simplex answer sits, this is essentially zero however
       wrong the point is. Measured: on four answers this solver is known to
       get wrong by 1.04e-3, it reads between 4e-20 and 3e-31, the same as on
       the correct ones (D73). A positive value is worth acting on; a small
       one is not evidence of anything.

       Rows contribute nothing here. A row's activity cannot be moved on its
       own, so there is no single-entity direction to measure along, and what
       a dropped row multiplier is worth is the part that would need the
       factorization.

       The library decides nothing on it. The gate does: `bench/run` holds
       every solve to an absolute bar on this figure (D185). D73 had found
       an earlier reading that could not tell a wrong answer from a right
       one; the bar is what settled that this one can.                       */
    double certified_suboptimality;

    /* Columns that could move without limit — no row ever stops them — but
       whose rate of improvement this checker calls zero.

       Counted rather than folded into the value above, because the two are
       different kinds of statement. With a finite step the product is
       self-limiting: a multiplier that is really roundoff certifies a
       roundoff-sized suboptimality, which is why the number above needs no
       threshold to be safe. With an infinite step the product is infinite for
       any nonzero multiplier at all, so it stops certifying anything and
       becomes the question that has no local answer — is this multiplier
       real? Five instances of JAOS's own reference set land here, every one
       of them matching a published finite optimum.

       A nonzero count means: there is a direction along which this model may
       be unbounded, and this report cannot tell you whether it is.          */
    int64_t unquantified_rays;

    /* gap_positive as a fraction of the objective it bounds: an absolute
       bound means nothing without the magnitude beside it, since 1e-4 on an
       objective of 3e2 and 1e-4 on one of 3e-4 are different claims.

       This is the number that catches a point far from optimal while every
       sign condition holds, which is the case D47 built and D87 could not
       reach. It is only as strong as gap_certified says: where a term was
       dropped it bounds nothing. */
    double relative_suboptimality;

    bool primal_feasible;       /* violations within tolerance             */
    /* Sign conditions, and the gap over bounds THE MODEL DECLARED.

       That restriction is deliberate. The checker bounds variables the model
       left unbounded by what the rows imply, which is sound — every feasible
       point satisfies an implied bound, so it moves neither the feasible
       region nor the optimum — but slack: nothing puts the variable on such
       a bound, so its term survives at an optimum and measures the bound's
       looseness rather than the point's error. Feeding that into the verdict
       rejects correct answers, measured (D87, D91). The suboptimality bound
       above uses every term; this uses the ones that must vanish.          */
    bool dual_feasible;
    bool checked_duals;         /* false when row_dual was NULL            */
    /* No term was dropped, so gap_positive really is a bound on P - P*.
       False does not mean the answer is wrong — most dropped multipliers
       are roundoff — it means this report does not prove it right.        */
    bool gap_certified;
} jaos_check_report;

/* Judges a claimed solution against the model as loaded — original space,
 * no scaling, independent of any solver bookkeeping.
 *
 * col_value[num_col] is required. row_dual[num_row] is optional; without it
 * only primal feasibility is checked.
 *
 * Dual convention: reduced costs are d = c - A'y. For minimization, at an
 * optimum: y_i >= 0 where the row is at its lower bound, y_i <= 0 at its
 * upper, y_i == 0 strictly inside; d_j likewise for columns. For
 * maximization every sign flips. Fixed rows and columns (equal bounds)
 * accept any multiplier sign.
 *
 * tol is absolute where it measures a residual — the primal violations
 * above are magnitudes in the model's own units, and tol is compared
 * against them directly. It is scaled where it decides whether a value
 * rests on a bound: a row activity is a sum, and how precisely a sum can be
 * placed is set by the terms that went into it, not by the total they came
 * to. That window is tol times the sum of the magnitudes of the row's terms
 * (times max(1, |x_j|) for a column). The objective gap is compared as a
 * relative quantity. docs/tolerances.md carries every formula; DECISIONS.md
 * D23 carries why the two are not the same kind of test. */
JAOS_NODISCARD jaos_status jaos_check_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, double tol,
    jaos_check_report *out);

/* The infeasibility certificate behind the last solve's
 * JAOS_SOLVE_INFEASIBLE: a vector y of num_row values such that, for
 * every x inside the column bounds whose row activities lie inside the
 * row bounds, the identity y'(Ax) = (A'y)'x cannot hold — the smallest
 * the left side can be still exceeds the largest the right side can be.
 * jaos_check_certificate verifies exactly that from the model alone.
 *
 * Available whether the simplex refused a row or presolve did: a ray
 * proved on a presolve-reduced model is lifted back through the
 * reductions into this model's own rows, and a reduction that proves
 * infeasibility by itself seeds one from the bound it refused. The one
 * refusal left is a model whose own bounds are inverted, which has no
 * ray to offer: the bounds are the proof. row_ray receives num_row
 * values. */
JAOS_NODISCARD jaos_status jaos_certificate(const jaos_model *m,
                                            double *row_ray);

/* Verdict of jaos_check_certificate. The proof is a difference of two
 * sums, so both halves are published beside it: from the difference
 * alone, a small gap between two large halves and a genuinely small
 * quantity read the same. */
typedef struct jaos_certificate_report {
    double sup_columns;  /* sum over columns j of the largest (A'y)_j x_j
                            the column's own bounds allow; +infinity when
                            a needed bound side is infinite, and the
                            proof dies with it                           */
    double inf_rows;     /* sum over rows i of the smallest y_i (Ax)_i
                            the row's own bounds allow; -infinity when a
                            needed side is infinite                      */
    double gap;          /* inf_rows - sup_columns; > 0 is the proof     */
    bool certified;      /* gap > tol * (1 + |sup_columns| + |inf_rows|) */
} jaos_certificate_report;

/* Judges a claimed infeasibility certificate against the model as it was
 * loaded — original space, model bounds only, no solver bookkeeping. A
 * ray that leans on a bound the model does not have makes one of the two
 * sums infinite and is rejected, whatever produced it. tol plays the
 * same role as the objective gap's in jaos_check_solution: the gap is
 * judged relative to the size of the two halves it is a difference of.
 * Each column's (A'y)_j is itself a sum, placeable no more finely than
 * its terms allow, so below tol times the sum of their magnitudes it
 * counts as zero — the same scaled rule jaos_check_solution documents
 * above. */
JAOS_NODISCARD jaos_status jaos_check_certificate(const jaos_model *m,
    const double *row_ray, double tol, jaos_certificate_report *out);

/* The ray behind the last solve's JAOS_SOLVE_UNBOUNDED: a direction d
 * over the structural columns such that moving any feasible point along
 * it stays feasible forever and improves the objective at a fixed rate.
 * jaos_check_ray verifies exactly that from the model alone.
 *
 * Available whether the solve proved the ray or presolve did: a ray on a
 * presolve-reduced model is lifted back into this model's own columns,
 * and the one reduction that proves unboundedness by itself, a column
 * with no live entry whose cost runs off an open side, seeds one.
 * col_ray receives num_col values. */
JAOS_NODISCARD jaos_status jaos_unbounded_ray(const jaos_model *m,
                                              double *col_ray);

/* Verdict of jaos_check_ray. */
typedef struct jaos_ray_report {
    double rate;            /* c'd, in the model's own sense: certifying
                               needs it improving — negative when
                               minimizing, positive when maximizing     */
    double max_col_escape;  /* the largest |d_j| that pushes past a
                               finite column bound side; 0 when clean   */
    double max_row_escape;  /* the largest |(Ad)_i| past its own traffic
                               floor that pushes past a finite row
                               side; 0 when clean                       */
    bool certified;         /* both escapes zero, and the rate clears
                               tol against the cost terms' own size     */
} jaos_ray_report;

/* Judges a claimed unbounded ray against the model as it was loaded —
 * original space, model bounds only, no solver bookkeeping. d_j may
 * only point past an infinite bound side; each row's movement (Ad)_i is
 * a sum and counts only above tol times the magnitudes that formed it,
 * the same scaled rule as everywhere in this header. */
JAOS_NODISCARD jaos_status jaos_check_ray(const jaos_model *m,
    const double *col_ray, double tol, jaos_ray_report *out);

/* --- Diagnosing an infeasible model ----------------------------------- */

/* Which sides of a bound belong to an irreducible infeasible subsystem.
 * A row's two bounds are two constraints, and so are a column's; an IIS
 * may hold either one without the other. The values combine as bits. */
typedef enum jaos_iis_side {
    JAOS_IIS_NONE  = 0,
    JAOS_IIS_LOWER = 1,
    JAOS_IIS_UPPER = 2,
    JAOS_IIS_BOTH  = 3,
} jaos_iis_side;

/* What jaos_iis did, beside the answer. */
typedef struct jaos_iis_report {
    int64_t members;         /* bound sides in the IIS, rows and columns
                                together                                 */
    int64_t candidates;      /* sides the deletion filter started from   */
    int64_t solves;          /* re-solves the filters ran, on a private
                                copy of the model                        */
    int64_t work_units;      /* their total, in jaos_work_units' unit;
                                not billed to the model                  */
    bool from_certificate;   /* the candidates were the sides the
                                certificate leans on; false when the
                                filter had to start from every finite
                                side of the model                        */
} jaos_iis_report;

/* An irreducible infeasible subsystem of the last solve's INFEASIBLE
 * model: a set of bound sides, rows' and columns', that is infeasible on
 * its own and becomes feasible when any one of them is dropped. Every
 * other side of the model may be relaxed to its infinity and the
 * subsystem stays infeasible, so the sides named are where to look.
 * row_side receives num_row values and col_side num_col; either may be
 * NULL. A model may have several IISs; this call finds one, the same
 * one on every machine and every run.
 *
 * Chinneck and Dravnieks's sensitivity filter followed by their deletion
 * filter (ORSA Journal on Computing 3(2), 1991): the certificate's
 * support first, which is an infeasible subsystem already, then one
 * warm re-solve per candidate side to ask whether the rest is still
 * infeasible without it. The re-solves run on a private copy with the
 * objective removed, so the caller's model, answer, certificate and
 * basis are untouched; the copy carries the caller's limits and
 * tolerances and the progress callback, so a watcher can stop it, and
 * not the log callback. A re-solve that stops on a budget, an
 * interruption or a numerical failure cannot decide its side and the
 * call returns JAOS_ERR_NUMERICAL with the model's error text saying
 * which. The cost is stated, not billed: one solve to confirm the
 * candidates and one per candidate, and the report says how many and
 * what they cost. */
JAOS_NODISCARD jaos_status jaos_iis(jaos_model *m, jaos_iis_side *row_side,
                                    jaos_iis_side *col_side,
                                    jaos_iis_report *out);

/* --- Sensitivity and ranging ------------------------------------------ */

/* How far one number in the model may move, everything else held, before
 * the basis behind the last optimum stops being optimal. The three calls
 * below answer that for every cost, every row bound and every column
 * bound at once, each interval containing the number's current value. An
 * end that is not limited reads +-jaos_infinity(); a degenerate basis
 * reports intervals of zero width on the side a tie closes, which is the
 * truthful answer and not a failure.
 *
 * All three factor the published basis on the model as loaded, so they
 * need what jaos_basis needs: the last solve found an optimum. The
 * intervals are about the basis and not about the answer: at a model with
 * more than one optimal basis, another basis can carry the same optimum
 * further, and the answer to "how far can this cost move before the
 * optimal VALUES change" is the union over those bases, which is not
 * computed here. Reproducible bit for bit, like everything else (D8).
 *
 * The cost is stated rather than billed, since jaos_work_units belongs to
 * the solve: one factorization of the basis per call, then for cost
 * ranging one BTRAN and one row price per basic structural column, and
 * for either bound ranging one FTRAN per nonbasic variable. On a model of
 * a hundred thousand rows a full cost ranging is comparable to the solve. */

/* Cost ranging: for every column j, the interval col_cost[j] may take.
 * Both arrays take num_col values; either may be NULL. A fixed column's
 * cost decides nothing and reads unlimited both ways. */
JAOS_NODISCARD jaos_status jaos_cost_ranging(jaos_model *m,
                                             double *lower, double *upper);

/* Right-hand-side ranging: for every row, the interval each of its two
 * bounds may take -- [lower_lo, lower_hi] for row_lower and
 * [upper_lo, upper_hi] for row_upper. All four arrays take num_row values;
 * any may be NULL. A bound the row's activity does not rest on may close
 * in on the activity and no further; the bound it rests on moves the
 * basic variables with it and is limited by their own bounds. */
JAOS_NODISCARD jaos_status jaos_rhs_ranging(jaos_model *m,
    double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi);

/* The same for every column's own bounds, num_col values each. */
JAOS_NODISCARD jaos_status jaos_bound_ranging(jaos_model *m,
    double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi);

/* --- Exact verification of a final basis ------------------------------ */

/* What jaos_verify concluded. REFUSED is not a failure: it is the honest
 * answer when the numbers the proof needs do not fit, and the report says
 * how far outside they were. */
typedef enum jaos_proof {
    JAOS_PROOF_OPTIMAL = 0,  /* proved, with no tolerance anywhere    */
    JAOS_PROOF_BROKEN,       /* the basis does not certify the answer */
    JAOS_PROOF_REFUSED,      /* the arithmetic does not fit           */
} jaos_proof;

/* Which of the checks a BROKEN verdict came from. They run in this order and
 * the first to fail is the one reported, so a basis can fail more than one. */
typedef enum jaos_proof_stage {
    JAOS_PROOF_STAGE_NONE = 0,
    JAOS_PROOF_STAGE_RANK,    /* the basis has no transversal          */
    JAOS_PROOF_STAGE_PRIMAL,  /* a basic value is outside its bounds   */
    JAOS_PROOF_STAGE_DUAL,    /* a reduced cost points out of the model */
} jaos_proof_stage;

/* What jaos_verify did, beside the verdict. */
typedef struct jaos_verify_report {
    jaos_proof status;
    jaos_proof_stage stage;  /* on BROKEN, which check said so             */
    double  bound_bits;      /* what the proof needs, read before any of it
                                is attempted: the Hadamard bound of the
                                scaled basis plus that of its largest block */
    double  capacity_bits;   /* what the arithmetic holds                  */
    int64_t blocks;          /* strongly connected components of the basis */
    int64_t largest_block;   /* rows in the biggest one                    */
    int64_t at_row;          /* on BROKEN, the row that breaks it, or -1   */
    int64_t at_col;          /* and the column, or -1                      */
    double  violation;       /* on BROKEN, how far out it is. The only
                                rounded number in the report: what decided
                                the verdict was an exact comparison, and
                                this is what the caller is told afterwards */
    int64_t bytes_held;      /* the largest single block table allocated   */
    int64_t terms;           /* integer products formed; the cost scales
                                with this                                  */
} jaos_verify_report;

/* Proves, or refuses to prove, that the last solve's published basis
 * certifies its answer. Nothing here compares against a tolerance.
 *
 * The basis is rebuilt over the integers -- each row scaled by the power of
 * two that clears its mantissas, which is exact -- permuted to block
 * triangular form by a maximum transversal and the strongly connected
 * components, and each block eliminated by Bareiss's fraction-free method.
 * The basic values and the duals come out as exact rationals, and the
 * verdict is OPTIMAL when every basic value lies inside its bounds and every
 * nonbasic reduced cost points into the model.
 *
 * REFUSED comes before the work, not during it: one pass over the basis
 * bounds every number the proof would hold, and `bound_bits` against
 * `capacity_bits` is the whole test. A basis past it is refused with nothing
 * allocated. It can also come during, when the bound was loose enough to
 * admit a basis the arithmetic then could not hold; `terms` says how far it
 * got. Measured over the gate: the bound admits 36 of the 110 instances that
 * have a basis to read, and it is an upper bound and a loose one, so more
 * than 36 may prove (D273, D274).
 *
 * BROKEN names the first row or column that fails, in the order the checks
 * run: the basic values against their bounds, then the reduced costs.
 *
 * Reproducible bit for bit, like everything else (D8): every loop runs in
 * index order and no tie is broken by an address. The cost is stated rather
 * than billed to jaos_work_units, and it is not small -- an elimination on a
 * block of k rows holds k*(k+1) numbers and forms about k**3 products of
 * them. */
JAOS_NODISCARD jaos_status jaos_verify(jaos_model *m,
                                       jaos_verify_report *out);

#ifdef __cplusplus
}
#endif

#endif /* JAOS_H */
