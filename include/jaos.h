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
#define JAOS_VERSION_MINOR 1
#define JAOS_VERSION_PATCH 0
#define JAOS_VERSION_STRING "0.1.0-dev"

/* Runtime library version, e.g. "0.1.0-dev". Static storage, never NULL. */
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
} jaos_solve_status;

/* Human-readable name for a status. Static storage; never NULL, including for
 * values outside the enum. */
JAOS_NODISCARD const char *jaos_status_str(jaos_status s);
JAOS_NODISCARD const char *jaos_solve_status_str(jaos_solve_status s);

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
/* File readers                                                              */
/* ------------------------------------------------------------------------- */

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
 * chosen; it only decides whether to stop at a checkpoint. */
JAOS_NODISCARD jaos_status jaos_set_work_limit(jaos_model *m, int64_t units);
JAOS_NODISCARD jaos_status jaos_set_time_limit(jaos_model *m, double seconds);

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

/* Work units consumed by the last solve. */
JAOS_NODISCARD int64_t jaos_work_units(const jaos_model *m);

/* Simplex iterations performed by the last solve. */
JAOS_NODISCARD int64_t jaos_iterations(const jaos_model *m);

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
       P - P* <= gap_positive. When it is not zero the two cancel, and a
       small objective_gap no longer says the point is nearly optimal —
       which the gap on its own cannot show. Both decide nothing.        */
    double gap_positive;
    double gap_negative;
    bool primal_feasible;       /* violations within tolerance             */
    bool dual_feasible;         /* sign conditions and gap within tol      */
    bool checked_duals;         /* false when row_dual was NULL            */
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

#ifdef __cplusplus
}
#endif

#endif /* JAOS_H */
