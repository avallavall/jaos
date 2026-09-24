/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JAOS_H
#define JAOS_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__cplusplus) && __cplusplus >= 201703L
  #define JAOS_NODISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
  #define JAOS_NODISCARD [[nodiscard]]
#else
  #define JAOS_NODISCARD
#endif

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(JAOS_BUILD)
    #define JAOS_API __declspec(dllexport)
  #else
    #define JAOS_API
  #endif
#elif defined(__GNUC__) || defined(__clang__)
  #define JAOS_API __attribute__((visibility("default")))
#else
  #define JAOS_API
#endif

#define JAOS_VERSION_MAJOR 0
#define JAOS_VERSION_MINOR 4
#define JAOS_VERSION_PATCH 0
#define JAOS_VERSION_STRING "0.4.0"

JAOS_NODISCARD JAOS_API const char *jaos_version(void);
JAOS_NODISCARD JAOS_API const char *jaos_build_commit(void);

typedef enum jaos_status {
    JAOS_OK = 0,
    JAOS_ERR_INVALID_INPUT = 1,
    JAOS_ERR_OUT_OF_MEMORY = 2,
    JAOS_ERR_IO = 3,
    JAOS_ERR_NUMERICAL = 4,
} jaos_status;

typedef enum jaos_solve_status {
    JAOS_SOLVE_NOT_RUN = 0,
    JAOS_SOLVE_OPTIMAL = 1,
    JAOS_SOLVE_INFEASIBLE = 2,
    JAOS_SOLVE_UNBOUNDED = 3,
    JAOS_SOLVE_WORK_LIMIT = 4,
    JAOS_SOLVE_TIME_LIMIT = 5,
    JAOS_SOLVE_NUMERICAL_ERROR = 6,
    JAOS_SOLVE_INTERRUPTED = 7,
    JAOS_SOLVE_NODE_LIMIT = 8,
} jaos_solve_status;

JAOS_NODISCARD JAOS_API const char *jaos_status_str(jaos_status s);
JAOS_NODISCARD JAOS_API const char *jaos_solve_status_str(jaos_solve_status s);

typedef enum jaos_log_level {
    JAOS_LOG_OFF = 0,
    JAOS_LOG_SUMMARY = 1,
    JAOS_LOG_PROGRESS = 2,
    JAOS_LOG_DETAIL = 3,
} jaos_log_level;

JAOS_NODISCARD JAOS_API double jaos_infinity(void);

typedef enum jaos_obj_sense {
    JAOS_MINIMIZE = 0,
    JAOS_MAXIMIZE = 1,
} jaos_obj_sense;

typedef struct jaos_model jaos_model;

JAOS_NODISCARD JAOS_API jaos_status jaos_model_new(jaos_model **out);

JAOS_API void jaos_model_free(jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_load_lp(jaos_model *m,
    int64_t num_col, int64_t num_row,
    jaos_obj_sense sense, double obj_offset,
    const double *col_cost,
    const double *col_lower, const double *col_upper,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);

JAOS_NODISCARD JAOS_API int64_t jaos_num_col(const jaos_model *m);
JAOS_NODISCARD JAOS_API int64_t jaos_num_row(const jaos_model *m);
JAOS_NODISCARD JAOS_API int64_t jaos_num_nz(const jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_col_cost(const jaos_model *m, int64_t col,
                                         double *cost);
JAOS_NODISCARD JAOS_API jaos_status jaos_col_bounds(const jaos_model *m, int64_t col,
                                           double *lower, double *upper);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_bounds(const jaos_model *m, int64_t row,
                                           double *lower, double *upper);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_cost(jaos_model *m, int64_t col,
                                             double cost);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_bounds(jaos_model *m, int64_t col,
                                               double lower, double upper);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_row_bounds(jaos_model *m, int64_t row,
                                               double lower, double upper);

JAOS_NODISCARD JAOS_API jaos_status jaos_objective_sense(const jaos_model *m,
                                                jaos_obj_sense *sense);
JAOS_NODISCARD JAOS_API jaos_status jaos_objective_offset(const jaos_model *m,
                                                 double *offset);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_objective_sense(jaos_model *m,
                                                    jaos_obj_sense sense);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_objective_offset(jaos_model *m,
                                                     double offset);

#define JAOS_NAME_MAX 255

JAOS_NODISCARD JAOS_API jaos_status jaos_col_name(const jaos_model *m, int64_t col,
                                         char *buf, int64_t cap);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_name(const jaos_model *m, int64_t row,
                                         char *buf, int64_t cap);
JAOS_NODISCARD JAOS_API jaos_status jaos_objective_name(const jaos_model *m,
                                               char *buf, int64_t cap);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_name(jaos_model *m, int64_t col,
                                             const char *name);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_row_name(jaos_model *m, int64_t row,
                                             const char *name);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_objective_name(jaos_model *m,
                                                   const char *name);
JAOS_NODISCARD JAOS_API jaos_status jaos_col_index(jaos_model *m, const char *name,
                                          int64_t *col);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_index(jaos_model *m, const char *name,
                                          int64_t *row);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_integer(jaos_model *m, int64_t col,
                                                bool is_integer);
JAOS_NODISCARD JAOS_API jaos_status jaos_col_integer(const jaos_model *m, int64_t col,
                                            bool *is_integer);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_semicontinuous(jaos_model *m,
                                                       int64_t col,
                                                       bool is_semi);
JAOS_NODISCARD JAOS_API jaos_status jaos_col_semicontinuous(const jaos_model *m,
                                                   int64_t col, bool *is_semi);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_col_quadratic(jaos_model *m, int64_t col,
                                                  double q);
JAOS_NODISCARD JAOS_API jaos_status jaos_col_quadratic(const jaos_model *m,
                                              int64_t col, double *q);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_quadratic(jaos_model *m, int64_t num_nz,
                                              const int64_t *rows,
                                              const int64_t *cols,
                                              const double *values);
JAOS_NODISCARD JAOS_API int64_t jaos_quadratic_nz(const jaos_model *m);
JAOS_NODISCARD JAOS_API jaos_status jaos_quadratic(const jaos_model *m, int64_t *rows,
                                          int64_t *cols, double *values);

JAOS_NODISCARD JAOS_API jaos_status jaos_add_sos(jaos_model *m, int type, int64_t n,
                                        const int64_t *cols,
                                        const double *weights);
JAOS_NODISCARD JAOS_API int64_t jaos_num_sos(const jaos_model *m);

typedef enum jaos_cone_type {
    JAOS_CONE_QUADRATIC = 1,
    JAOS_CONE_ROTATED = 2,
} jaos_cone_type;

JAOS_NODISCARD JAOS_API jaos_status jaos_add_cone(jaos_model *m, jaos_cone_type type,
                                         int64_t n, const int64_t *cols);
JAOS_NODISCARD JAOS_API int64_t jaos_num_cones(const jaos_model *m);
JAOS_NODISCARD JAOS_API jaos_status jaos_cone(const jaos_model *m, int64_t k,
                                     jaos_cone_type *type, int64_t *n,
                                     int64_t *cols);
JAOS_NODISCARD JAOS_API jaos_status jaos_delete_cones(jaos_model *m, int64_t num_del,
                                             const int64_t *cones);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_row_quadratic(jaos_model *m, int64_t row,
                                                  int64_t num_nz,
                                                  const int64_t *rows,
                                                  const int64_t *cols,
                                                  const double *values);
JAOS_NODISCARD JAOS_API int64_t jaos_row_quadratic_nz(const jaos_model *m, int64_t row);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_quadratic(const jaos_model *m, int64_t row,
                                              int64_t *rows, int64_t *cols,
                                              double *values);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_row_indicator(jaos_model *m, int64_t row,
                                                  int64_t col, int value);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_indicator(const jaos_model *m, int64_t row,
                                              int64_t *col, int *value);
JAOS_NODISCARD JAOS_API jaos_status jaos_sos(const jaos_model *m, int64_t k, int *type,
                                    int64_t *n, int64_t *cols, double *weights);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_gap(jaos_model *m, double gap);

typedef enum jaos_gap_rule {
    JAOS_GAP_SHIFTED = 0,
    JAOS_GAP_RELATIVE = 1,
} jaos_gap_rule;

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_gap_rule(jaos_model *m,
                                                 jaos_gap_rule rule);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive(jaos_model *m, bool on);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cut_rounds(jaos_model *m,
                                                   int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cut_depth(jaos_model *m,
                                                  int64_t depth);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cut_drop(jaos_model *m, bool on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_node_cut_cap(jaos_model *m,
                                                     int64_t cap);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cover_rounds(jaos_model *m,
                                                     int64_t rounds);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_clique_rounds(jaos_model *m,
                                                      int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_zero_half_rounds(jaos_model *m,
                                                         int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_flow_cover_rounds(jaos_model *m,
                                                          int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cut_stall(jaos_model *m,
                                                  double fraction);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_node_cut_stall(jaos_model *m,
                                                       double fraction);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_root_cut_drop(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cover_lift(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_mir_rounds(jaos_model *m,
                                                   int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_backtrack(jaos_model *m,
                                                       int64_t times);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_gap(jaos_model *m,
                                                 double fraction);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_node_mir(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_mir_aggregate(jaos_model *m,
                                                      int64_t rows);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_heuristic(jaos_model *m,
                                                       int64_t solves);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_heuristic_depth(jaos_model *m,
                                                             int64_t depth);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_rins(jaos_model *m, int64_t solves);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_local_branching(jaos_model *m,
                                                          int64_t size);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_node_select(jaos_model *m,
                                                      int64_t rule);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_restart(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_feaspump(jaos_model *m,
                                                 int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_pump_general(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_pump_obj(jaos_model *m, double decay);

typedef struct jaos_model_stats {
    int64_t num_row, num_col, num_nz;
    int64_t integer_col;
    int64_t binary_col;
    int64_t equality_row, ranged_row, one_sided_row, free_row;
    int64_t fixed_col, ranged_col, one_sided_col, free_col;
    int64_t empty_row, empty_col;
    int64_t obj_nz;
    double  min_abs, max_abs;
    double  obj_min_abs, obj_max_abs;
    int64_t semicontinuous_col, sos_set, indicator_row;
    int64_t quadratic_col;
    int64_t cone_set, quadratic_row;
} jaos_model_stats;

typedef struct jaos_presolve_report {
    int64_t num_row, num_col, num_nz;
    int64_t rounds;
    int64_t fixed_col;
    int64_t empty_row, empty_col;
    int64_t singleton_row, singleton_col;
    int64_t free_col_singleton;
    int64_t forcing_row, redundant_row;
    int64_t implied_free_col;
    int64_t tightened_bound;
    int64_t duplicate_row, duplicate_col, dominated_col;
    int64_t aggregated_col;
} jaos_presolve_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_presolve_result(const jaos_model *m,
                                                jaos_presolve_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_model_statistics(const jaos_model *m,
                                                 jaos_model_stats *out);

JAOS_NODISCARD JAOS_API bool jaos_model_has_integer(const jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_start(jaos_model *m,
                                              const double *col_value);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_cutoff(jaos_model *m, double cutoff);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_pump_always(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_rcfix(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_tighten(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_probing(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_probing_cap(jaos_model *m,
                                                    double multiple);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_clique_fix(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_conflicts(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_symmetry(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_orbital(jaos_model *m, int on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_propagate(jaos_model *m,
                                                  int64_t rounds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_propagate_depth(jaos_model *m,
                                                        int64_t depth);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_degrade(jaos_model *m,
                                                     double frac);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_heuristics(jaos_model *m, bool on);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_node_limit(jaos_model *m,
                                                   int64_t nodes);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_tree_batch(jaos_model *m,
                                                   int64_t nodes);

typedef enum jaos_branching {
    JAOS_BRANCH_PSEUDOCOST = 0,
    JAOS_BRANCH_MOST_FRACTIONAL = 1,
} jaos_branching;

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_branching(jaos_model *m,
                                                  jaos_branching rule);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_reliability(jaos_model *m,
                                                    int64_t branches);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_probe_cap(jaos_model *m,
                                                  double multiple);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_probe_depth(jaos_model *m,
                                                    int64_t depth);

typedef enum jaos_dive_child {
    JAOS_DIVE_NEARER = 0,
    JAOS_DIVE_UP = 1,
    JAOS_DIVE_DOWN = 2,
    JAOS_DIVE_PSEUDOCOST = 3,
} jaos_dive_child;

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_dive_child(jaos_model *m,
                                                   jaos_dive_child rule);

typedef struct jaos_mip_report {
    int64_t nodes;
    int64_t lp_solves;
    bool    has_incumbent;
    double  incumbent;
    double  bound;
    int64_t cuts;
    int64_t heuristic_points;
    int64_t first_incumbent_node;
    int64_t fixed_cols;
    int64_t tightened;
    int64_t symmetry_generators;
    int64_t symmetry_orbits;
    bool    start_accepted;
} jaos_mip_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_mip_result(const jaos_model *m,
                                           jaos_mip_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_mip_incumbent(const jaos_model *m,
                                              double *col_value,
                                              double *objective);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_mip_pool_size(jaos_model *m, int64_t size);
JAOS_NODISCARD JAOS_API jaos_status jaos_mip_pool_count(const jaos_model *m,
                                               int64_t *count);
JAOS_NODISCARD JAOS_API jaos_status jaos_mip_pool_solution(const jaos_model *m,
                                                  int64_t k, double *col_value,
                                                  double *objective);

JAOS_NODISCARD JAOS_API jaos_status jaos_model_name(const jaos_model *m,
                                           char *buf, int64_t cap);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_model_name(jaos_model *m,
                                               const char *name);

JAOS_NODISCARD JAOS_API jaos_status jaos_model_copy(const jaos_model *src,
                                           jaos_model **out);

JAOS_NODISCARD JAOS_API jaos_status jaos_col_entries(const jaos_model *m, int64_t col,
                                            int64_t *count, int64_t *index,
                                            double *value);
JAOS_NODISCARD JAOS_API jaos_status jaos_row_entries(jaos_model *m, int64_t row,
                                            int64_t *count, int64_t *index,
                                            double *value);
JAOS_NODISCARD JAOS_API jaos_status jaos_coefficient(const jaos_model *m, int64_t row,
                                            int64_t col, double *value);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_coefficient(jaos_model *m, int64_t row,
                                                int64_t col, double value);

JAOS_NODISCARD JAOS_API jaos_status jaos_add_cols(jaos_model *m, int64_t num_new,
    const double *col_cost, const double *col_lower, const double *col_upper,
    int64_t num_nz, const int64_t *a_start, const int64_t *a_index,
    const double *a_value);

JAOS_NODISCARD JAOS_API jaos_status jaos_add_rows(jaos_model *m, int64_t num_new,
    const double *row_lower, const double *row_upper,
    int64_t num_nz, const int64_t *ar_start, const int64_t *ar_index,
    const double *ar_value);

JAOS_NODISCARD JAOS_API jaos_status jaos_delete_cols(jaos_model *m, int64_t num_del,
                                            const int64_t *cols);
JAOS_NODISCARD JAOS_API jaos_status jaos_delete_rows(jaos_model *m, int64_t num_del,
                                            const int64_t *rows);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_mps(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_lp(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_nl(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_mps(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_lp(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_nl(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_qplib(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_qplib(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_cbf(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_cbf(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_osil(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_osil(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_solution(jaos_model *m,
                                               const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_sol_ampl(jaos_model *m,
                                               const char *path,
                                               const char *message);

JAOS_NODISCARD JAOS_API const char *jaos_model_error(const jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_work_limit(jaos_model *m, int64_t units);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_threads(jaos_model *m, int64_t threads);
JAOS_NODISCARD JAOS_API int64_t jaos_threads_of(const jaos_model *m);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_time_limit(jaos_model *m, double seconds);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_primal_tolerance(jaos_model *m, double tol);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_dual_tolerance(jaos_model *m, double tol);

typedef enum jaos_algorithm {
    JAOS_ALGORITHM_DUAL = 0,
    JAOS_ALGORITHM_PRIMAL = 1,
    JAOS_ALGORITHM_BARRIER = 2,
    JAOS_ALGORITHM_PDLP = 3,
    JAOS_ALGORITHM_CONCURRENT = 4,
} jaos_algorithm;

JAOS_NODISCARD JAOS_API jaos_status jaos_set_algorithm(jaos_model *m, jaos_algorithm alg);
JAOS_NODISCARD JAOS_API jaos_algorithm jaos_algorithm_of(const jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_option(jaos_model *m, const char *name,
                                           const char *value);
JAOS_NODISCARD JAOS_API jaos_status jaos_get_option(const jaos_model *m,
                                           const char *name, char *buf,
                                           int64_t cap);
JAOS_NODISCARD JAOS_API jaos_status jaos_read_options(jaos_model *m, const char *path);
JAOS_NODISCARD JAOS_API int64_t jaos_num_options(void);
JAOS_NODISCARD JAOS_API const char *jaos_option_name(int64_t k);

typedef void (*jaos_log_fn)(void *user, jaos_log_level level, const char *line);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_log_callback(jaos_model *m,
                                                 jaos_log_fn cb, void *user);
JAOS_NODISCARD JAOS_API jaos_status jaos_set_log_level(jaos_model *m,
                                              jaos_log_level level);

typedef enum jaos_callback_action {
    JAOS_CALLBACK_CONTINUE = 0,
    JAOS_CALLBACK_STOP = 1,
} jaos_callback_action;

typedef struct jaos_progress {
    int64_t iterations;
    int64_t work_units;

    double primal_infeasibility;

    int64_t nodes;
    double  bound;
    bool    has_incumbent;
    double  incumbent;
} jaos_progress;

typedef jaos_callback_action (*jaos_progress_fn)(const jaos_progress *p,
                                                 void *user);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_progress_callback(jaos_model *m,
                                                      jaos_progress_fn cb,
                                                      void *user);

typedef struct jaos_incumbent {
    int64_t node;
    double  objective;
    double  bound;
    const double *col_value;
    int64_t num_col;
    bool    by_rounding;
} jaos_incumbent;

typedef jaos_callback_action (*jaos_incumbent_fn)(const jaos_incumbent *inc,
                                                  void *user);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_incumbent_callback(jaos_model *m,
                                                       jaos_incumbent_fn cb,
                                                       void *user);

typedef struct jaos_node {
    int64_t node;
    int64_t depth;
    double  objective;
    double  bound;
    const double *col_value;
    int64_t num_col;
    bool    integral;
    int64_t branch_col;
    void   *internal;
} jaos_node;

typedef jaos_callback_action (*jaos_node_fn)(jaos_node *ev, void *user);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_node_callback(jaos_model *m,
                                                  jaos_node_fn cb, void *user);

JAOS_NODISCARD JAOS_API jaos_status jaos_node_add_row(jaos_node *ev, int64_t nnz,
                                             const int64_t *index,
                                             const double *value,
                                             double lower, double upper);

JAOS_NODISCARD JAOS_API jaos_status jaos_node_add_solution(jaos_node *ev,
                                                  int64_t num_col,
                                                  const double *col_value);

JAOS_NODISCARD JAOS_API jaos_status jaos_solve(jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_solve_status jaos_status_of(const jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_objective(const jaos_model *m, double *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_solution(const jaos_model *m,
    double *col_value, double *row_activity, double *row_dual,
    double *col_dual);

typedef enum jaos_basis_status {
    JAOS_BASIS_BASIC = 0,
    JAOS_BASIS_AT_LOWER = 1,
    JAOS_BASIS_AT_UPPER = 2,
    JAOS_BASIS_FREE = 3,
} jaos_basis_status;

JAOS_NODISCARD JAOS_API jaos_status jaos_basis(const jaos_model *m,
    jaos_basis_status *col_status, jaos_basis_status *row_status);

JAOS_NODISCARD JAOS_API jaos_status jaos_set_basis(jaos_model *m,
    const jaos_basis_status *col_status, const jaos_basis_status *row_status);

JAOS_API void jaos_clear_basis(jaos_model *m);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_solution(jaos_model *m,
    const char *path, double *objective,
    double *col_value, double *col_dual, jaos_basis_status *col_status,
    double *row_activity, double *row_dual, jaos_basis_status *row_status);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_certificate(jaos_model *m,
    const char *path, jaos_solve_status *status,
    double *row_ray, double *col_ray);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_cone_duals(jaos_model *m,
    const char *path, double *cone_dual);

JAOS_NODISCARD JAOS_API jaos_status jaos_read_basis(jaos_model *m, const char *path,
    jaos_basis_status *col_status, jaos_basis_status *row_status);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_mps_basis(jaos_model *m,
    const char *path);
JAOS_NODISCARD JAOS_API jaos_status jaos_read_mps_basis(jaos_model *m,
    const char *path, jaos_basis_status *col_status,
    jaos_basis_status *row_status);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_point(jaos_model *m, const char *path);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_point_values(jaos_model *m,
    const char *path, const double *col_value);

JAOS_NODISCARD JAOS_API jaos_status jaos_write_duals(jaos_model *m, const char *path);
JAOS_NODISCARD JAOS_API jaos_status jaos_write_dual_values(jaos_model *m,
    const char *path, const double *row_dual);
JAOS_NODISCARD JAOS_API jaos_status jaos_read_point(jaos_model *m, const char *path,
                                           double *col_value);
JAOS_NODISCARD JAOS_API jaos_status jaos_read_duals(jaos_model *m, const char *path,
                                           double *row_dual);

JAOS_NODISCARD JAOS_API jaos_status jaos_solution_file_status(jaos_model *m,
    const char *path, jaos_solve_status *status);

JAOS_NODISCARD JAOS_API int64_t jaos_work_units(const jaos_model *m);

JAOS_NODISCARD JAOS_API int64_t jaos_iterations(const jaos_model *m);

JAOS_NODISCARD JAOS_API double jaos_solve_time(const jaos_model *m);

typedef struct jaos_check_report {
    double max_col_violation;
    double max_row_violation;
    double max_row_violation_relative;

    double max_dual_violation;
    double primal_objective;
    double dual_objective;
    double objective_gap;

    double gap_positive;
    double gap_negative;

    double max_dropped_multiplier;
    int64_t dropped_terms;

    double certified_suboptimality;

    int64_t unquantified_rays;

    double relative_suboptimality;

    bool primal_feasible;

    bool dual_feasible;
    bool checked_duals;

    bool gap_certified;

    double max_integrality_violation;

    double max_cone_violation;
} jaos_check_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_check_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, double tol,
    jaos_check_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_check_conic_solution(const jaos_model *m,
    const double *col_value, const double *row_dual, const double *cone_dual,
    double tol, jaos_check_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_cone_dual(const jaos_model *m, int64_t k,
                                          double *z);

JAOS_NODISCARD JAOS_API jaos_status jaos_certificate(const jaos_model *m,
                                            double *row_ray);

typedef struct jaos_certificate_report {
    double sup_columns;
    double inf_rows;
    double gap;
    bool certified;
} jaos_certificate_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_check_certificate(const jaos_model *m,
    const double *row_ray, double tol, jaos_certificate_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_check_conic_certificate(const jaos_model *m,
    const double *row_ray, const double *cone_ray, double tol,
    jaos_certificate_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_unbounded_ray(const jaos_model *m,
                                              double *col_ray);

typedef struct jaos_ray_report {
    double rate;
    double max_col_escape;
    double max_row_escape;
    double curvature;
    bool certified;
} jaos_ray_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_check_ray(const jaos_model *m,
    const double *col_ray, double tol, jaos_ray_report *out);

typedef enum jaos_iis_side {
    JAOS_IIS_NONE  = 0,
    JAOS_IIS_LOWER = 1,
    JAOS_IIS_UPPER = 2,
    JAOS_IIS_BOTH  = 3,
} jaos_iis_side;

typedef struct jaos_iis_report {
    int64_t members;
    int64_t candidates;
    int64_t solves;
    int64_t work_units;
    bool from_certificate;
} jaos_iis_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_iis(jaos_model *m, jaos_iis_side *row_side,
                                    jaos_iis_side *col_side,
                                    jaos_iis_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_iis_model(const jaos_model *m,
    const jaos_iis_side *row_side, const jaos_iis_side *col_side,
    jaos_model **out);

typedef enum jaos_relax_scope {
    JAOS_RELAX_ROWS = 1,
    JAOS_RELAX_COLS = 2,
    JAOS_RELAX_BOTH = 3,
} jaos_relax_scope;

typedef struct jaos_relax_report {
    double  total;
    int64_t rows_moved;
    int64_t cols_moved;
    int64_t at_row;
    int64_t at_col;
    double  largest;
    int64_t work_units;
    jaos_solve_status status;
} jaos_relax_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_feasrelax(jaos_model *m,
                                          jaos_relax_scope scope,
                                          double *row_move, double *col_move,
                                          jaos_relax_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_cost_ranging(jaos_model *m,
                                             double *lower, double *upper);

JAOS_NODISCARD JAOS_API jaos_status jaos_rhs_ranging(jaos_model *m,
    double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi);

JAOS_NODISCARD JAOS_API jaos_status jaos_bound_ranging(jaos_model *m,
    double *lower_lo, double *lower_hi, double *upper_lo, double *upper_hi);

typedef enum jaos_proof {
    JAOS_PROOF_OPTIMAL = 0,
    JAOS_PROOF_BROKEN = 1,
    JAOS_PROOF_REFUSED = 2,
} jaos_proof;

typedef enum jaos_proof_stage {
    JAOS_PROOF_STAGE_NONE = 0,
    JAOS_PROOF_STAGE_RANK = 1,
    JAOS_PROOF_STAGE_PRIMAL = 2,
    JAOS_PROOF_STAGE_DUAL = 3,
} jaos_proof_stage;

typedef struct jaos_verify_report {
    jaos_proof status;
    jaos_proof_stage stage;
    double  bound_bits;
    double  capacity_bits;
    int64_t blocks;
    int64_t largest_block;
    int64_t at_row;
    int64_t at_col;
    double  violation;
    int64_t bytes_held;
    int64_t terms;
} jaos_verify_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_verify(jaos_model *m,
                                       jaos_verify_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_verify_basis(jaos_model *m,
    const jaos_basis_status *col_status,
    const jaos_basis_status *row_status, jaos_verify_report *out);

JAOS_NODISCARD JAOS_API jaos_status jaos_exact_col_value(const jaos_model *m,
                                                int64_t col,
                                                const char **out);
JAOS_NODISCARD JAOS_API jaos_status jaos_exact_row_dual(const jaos_model *m,
                                               int64_t row,
                                               const char **out);
JAOS_NODISCARD JAOS_API jaos_status jaos_exact_objective(const jaos_model *m,
                                                const char **out);

typedef struct jaos_exact_ray_report {
    bool    derived;
    double  bound_bits;
    double  capacity_bits;
    int64_t blocks;
    int64_t largest_block;
    int64_t at_row;
    int64_t bytes_held;
    int64_t terms;
} jaos_exact_ray_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_exact_certificate(jaos_model *m,
                                                  jaos_exact_ray_report *out);
JAOS_NODISCARD JAOS_API jaos_status jaos_exact_row_multiplier(const jaos_model *m,
                                                     int64_t row,
                                                     const char **out);

JAOS_NODISCARD JAOS_API jaos_status jaos_exact_unbounded_ray(
    jaos_model *m, jaos_exact_ray_report *out);
JAOS_NODISCARD JAOS_API jaos_status jaos_exact_col_direction(const jaos_model *m,
                                                    int64_t col,
                                                    const char **out);

typedef enum jaos_proof_kind {
    JAOS_PROOF_FILE_OPTIMAL = 0,
    JAOS_PROOF_FILE_INFEASIBLE = 1,
    JAOS_PROOF_FILE_UNBOUNDED = 2,
} jaos_proof_kind;

typedef struct jaos_proof_report {
    bool    primal;
    bool    dual;
    bool    objective;
    int64_t bad_row;
    int64_t bad_col;
    int64_t terms;

    jaos_proof_kind kind;
    bool    certified;
} jaos_proof_report;

JAOS_NODISCARD JAOS_API jaos_status jaos_write_proof(jaos_model *m, const char *path);
JAOS_NODISCARD JAOS_API jaos_status jaos_check_proof(jaos_model *m, const char *path,
                                            jaos_proof_report *out);

#ifdef __cplusplus
}
#endif

#endif
