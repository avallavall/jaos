/* JAOS — Just Another Optimization Solver.
 *
 * Public API. This is the only public header; everything not declared here is
 * internal and carries no stability promise.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef JAOS_H
#define JAOS_H

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

#ifdef __cplusplus
}
#endif

#endif /* JAOS_H */
