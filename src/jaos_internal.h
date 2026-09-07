/* SPDX-License-Identifier: Apache-2.0 */
#ifndef JAOS_INTERNAL_H
#define JAOS_INTERNAL_H

#include "jaos.h"

#include <float.h>
#include <stddef.h>

typedef struct {
    int64_t work_limit;
    double time_limit;

    double primal_tol;
    double dual_tol;

    jaos_log_fn log_cb;
    void *log_user;
    jaos_log_level log_level;

    jaos_progress_fn progress_cb;
    void *progress_user;

    bool force_primal;
    bool primal_dantzig;

    double mip_gap;

    bool mip_dive;
    bool mip_cut_rounds_set;
    int64_t mip_cut_rounds;

    bool mip_cut_depth_set;
    int64_t mip_cut_depth;
    bool mip_no_cut_drop;

    bool mip_node_cut_cap_set;
    int64_t mip_node_cut_cap;

    bool mip_cover_rounds_set;
    int64_t mip_cover_rounds;

    bool mip_cut_stall_set;
    double mip_cut_stall;

    bool mip_node_cut_stall_set;
    double mip_node_cut_stall;

    bool mip_root_cut_drop_set;
    bool mip_root_cut_drop;

    bool mip_cover_lift_set;
    bool mip_cover_lift;

    bool mip_mir_rounds_set;
    int64_t mip_mir_rounds;

    bool mip_dive_backtrack_set;
    int64_t mip_dive_backtrack;

    bool mip_dive_gap_set;
    double mip_dive_gap;

    bool mip_node_mir_set;
    bool mip_node_mir;

    bool mip_mir_aggregate_set;
    int64_t mip_mir_aggregate;

    bool mip_dive_heuristic_set;
    int64_t mip_dive_heuristic;
    bool mip_dive_heuristic_depth_set;
    int64_t mip_dive_heuristic_depth;
    bool mip_rins_set;
    int64_t mip_rins;
    bool mip_feaspump_set;
    int64_t mip_feaspump;
    bool mip_pump_general_set;
    bool mip_pump_general;
    bool mip_pump_obj_set;
    double mip_pump_obj;

    bool mip_pump_always_set;
    bool mip_pump_always;

    bool mip_rcfix_set;
    bool mip_rcfix;

    bool mip_propagate_set;
    int64_t mip_propagate;

    bool mip_cutoff_set;
    double mip_cutoff;

    bool mip_propagate_depth_set;
    int64_t mip_propagate_depth;
    bool mip_dive_degrade_set;
    double mip_dive_degrade;

    bool mip_no_heuristics;

    int64_t mip_node_limit;
    int mip_branching;

    bool mip_reliability_set;
    int64_t mip_reliability;

    bool mip_probe_cap_set;
    double mip_probe_cap;

    bool mip_probe_depth_set;
    int64_t mip_probe_depth;
    int64_t mip_pool_size;
    int mip_dive_child;
    jaos_incumbent_fn incumbent_cb;
    void *incumbent_user;
} jm_config;

typedef struct {
    char *pool;
    int64_t pool_len, pool_cap;
    int64_t *off, *val;
    int64_t n, cap;
    int64_t *slot;
    int64_t nslot;
} jm_nmap;

void jm_nmap_free(jm_nmap *m);
bool jm_nmap_get(const jm_nmap *m, const char *name, int64_t *val);

bool jm_nmap_insert(jm_nmap *m, const char *name, int64_t value);

char *jm_name_copy(const char *name);

char **jm_nmap_to_names(const jm_nmap *m, int64_t n);

typedef struct {
    int64_t fixed_col;
    int64_t empty_row;
    int64_t empty_col;
    int64_t singleton_row;
    int64_t singleton_col;
    int64_t free_col_singleton;
    int64_t forcing_row;
    int64_t redundant_row;
    int64_t implied_free_col;
    int64_t tightened_bound;
    int64_t duplicate_row;
    int64_t duplicate_col;
    int64_t dominated_col;
    int64_t rounds;
} jm_presolve_counts;

struct jaos_model {
    int64_t num_col;
    int64_t num_row;
    int64_t num_nz;

    jaos_obj_sense sense;
    double obj_offset;

    double *col_cost;
    double *col_lower, *col_upper;
    double *row_lower, *row_upper;

    char **col_name;
    char **row_name;
    char *obj_name;

    char *model_name;
    bool name_map_valid;
    jm_nmap col_map, row_map;

    bool *col_integer;
    bool *col_semi;

    int64_t num_sos;
    int *sos_type;
    int64_t *sos_start;
    int64_t *sos_col;
    double *sos_weight;

    int64_t mip_nodes, mip_solves, mip_cuts, mip_heur, mip_first_inc;
    int64_t mip_rcfix_n, mip_prop_n;
    double mip_bound;
    bool mip_has_incumbent;
    double mip_inc_obj;

    double *mip_start;
    double *mip_inc_x;

    double *mip_pool_x;
    double *mip_pool_obj;
    int64_t mip_pool_n;

    int64_t *a_start;
    int64_t *a_index;
    double  *a_value;

    bool rowwise_valid;
    int64_t *ar_start;
    int64_t *ar_index;
    double  *ar_value;

    bool scale_valid;
    double *row_scale;
    double *col_scale;

    bool scale_clamped;

    jm_config cfg;

    jaos_solve_status solve_status;
    double objective;
    double *sol_col;
    double *sol_row;
    double *sol_dual;
    double *sol_redcost;
    jaos_basis_status *sol_col_status;
    jaos_basis_status *sol_row_status;

    bool sol_basis_ok;

    double *sol_farkas;
    bool farkas_ok;

    double *sol_ray;
    bool ray_ok;

    char **exact_col;
    char **exact_dual;
    char *exact_obj;

    char **exact_farkas;

    char **exact_uray;
    int64_t solve_work;
    int64_t solve_iters;

    int64_t solve_primal_iters, solve_phase1_iters;

    double solve_time;

    int64_t presolve_num_row, presolve_num_col, presolve_num_nz;

    jm_presolve_counts presolve_counts;

    jaos_basis_status *start_col_status;
    jaos_basis_status *start_row_status;

    char err[256];
};

typedef enum {
    JM_BASIC = 0,
    JM_AT_LOWER,
    JM_AT_UPPER,
    JM_FREE,
} jm_var_status;

JAOS_NODISCARD jaos_status jm_dual_simplex(jaos_model *m);

double jm_primal_tolerance(const jaos_model *m);

typedef struct jm_tableau jm_tableau;
JAOS_NODISCARD jaos_status jm_tableau_build(jaos_model *m, jm_tableau **out);
void jm_tableau_free(jm_tableau *t);
int64_t jm_tableau_position(const jm_tableau *t, int64_t v);
int64_t jm_tableau_variable(const jm_tableau *t, int64_t p);
double jm_tableau_value(const jm_tableau *t, int64_t p);
int64_t jm_tableau_work(const jm_tableau *t);
JAOS_NODISCARD jaos_status jm_tableau_row(jm_tableau *t, int64_t p, double *row);

bool jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor,
                   const int64_t *pat, int64_t npat);

int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol);

int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den);

bool jm_primal_row_wins(double step, int64_t var,
                        double best_step, int64_t best_var, bool bland);

int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words);

int64_t jm_nonbasic_build(int64_t nvar, const jm_var_status *status,
                          uint64_t *mark);
void jm_nonbasic_insert(uint64_t *mark, int64_t v);
void jm_nonbasic_remove(uint64_t *mark, int64_t v);
int64_t jm_nonbasic_expand(int64_t nvar, const uint64_t *mark, int64_t *out);

void *jm_alloc_array(int64_t n, size_t elsize);
void *jm_calloc_array(int64_t n, size_t elsize);

bool jm_grow(void **arr, int64_t *cap, int64_t need, size_t elsize);
#define JM_GROW(a, cap, need) \
    ((need) <= (cap) ? true : jm_grow((void **)&(a), &(cap), (need), sizeof *(a)))

constexpr int JM_NAME_BUF = 24;
const char *jm_col_name(const jaos_model *m, int64_t j, char *buf);
const char *jm_row_name(const jaos_model *m, int64_t i, char *buf);
const char *jm_obj_name(const jaos_model *m);

void jm_model_take_names(jaos_model *m, char **col, char **row, char *obj);

bool jm_name_ok(const char *name);

bool jm_lp_name_ok(const char *s);

void jm_model_drop_exact(jaos_model *m);

JAOS_NODISCARD jaos_status jm_branch_and_bound(jaos_model *m);
bool jm_model_has_integer(const jaos_model *m);

JAOS_NODISCARD jaos_status jm_slurp(jaos_model *m, const char *path,
                                    char **out, int64_t *out_len);

JAOS_NODISCARD bool jm_gzip(const char *data, int64_t n, char **out,
                            int64_t *out_n);

bool jm_box_inverted(double lower, double upper);

JAOS_NODISCARD jaos_status jm_model_ensure_rowwise(jaos_model *m);

JAOS_NODISCARD jaos_status jm_model_ensure_solution_arrays(jaos_model *m);

JAOS_NODISCARD jaos_status jm_model_remember_basis(jaos_model *m);

JAOS_NODISCARD bool jm_model_basis_count_ok(const jaos_model *m);

void jm_model_publish_objective(jaos_model *m);

static_assert(FLT_EVAL_METHOD == 0,
              "Dekker's split needs double arithmetic evaluated at double");

#ifdef __FAST_MATH__
#error "JAOS cannot be built with -ffast-math or -Ofast: associative maths deletes the compensated-summation residue (D34, D175, D219)"
#endif

void jm_obj_add(double *sum, double *comp, double t);
JAOS_NODISCARD double jm_two_product_residue(double a, double b, double p);

[[gnu::format(printf, 2, 3)]]
void jm_set_err(jaos_model *m, const char *fmt, ...);

static inline bool jm_logging_at(const jaos_model *m, jaos_log_level level)
{
    return m != nullptr && m->cfg.log_cb != nullptr && m->cfg.log_level >= level;
}

[[gnu::format(printf, 3, 4)]]
void jm_log(const jaos_model *m, jaos_log_level level, const char *fmt, ...);

typedef enum {
    JM_SCALE_NONE = 0,
    JM_SCALE_CURTIS_REID,
    JM_SCALE_GEOMETRIC,
} jm_scale_mode;

JAOS_NODISCARD jaos_status jm_model_scale(jaos_model *m, jm_scale_mode mode);

double jm_scaled_abs(const jaos_model *m, int64_t j, int64_t k);

typedef struct { int64_t units; } jm_work;

constexpr int64_t JM_WORK_NONZERO    = 1;
constexpr int64_t JM_WORK_ELIMINATED = 2;
constexpr int64_t JM_WORK_FACTOR     = 4096;
constexpr int64_t JM_WORK_UPDATE     = 64;

static inline void jm_work_add(jm_work *w, int64_t n)
{
    if (w != nullptr)
        w->units += n;
}

typedef enum {
    JM_PS_FIXED_COL,
    JM_PS_EMPTY_ROW,
    JM_PS_EMPTY_COL,
    JM_PS_SINGLETON_ROW,
    JM_PS_SINGLETON_COL,
    JM_PS_FREE_COL_SINGLETON,
    JM_PS_REDUNDANT_ROW,
    JM_PS_FORCING_ROW,
    JM_PS_IMPLIED_FREE_COL,
} jm_presolve_tag;

typedef struct {
    jm_presolve_tag tag;
    int64_t index;
    int64_t index2;
    double value;
    double cost;
    double coef;
    double lo, hi;
    double row_lo, row_hi;
    bool row_tightens_lo, row_tightens_hi;
} jm_presolve_rec;

typedef enum {
    JM_PRESOLVE_NONE,
    JM_PRESOLVE_REDUCED,
    JM_PRESOLVE_INFEASIBLE,
    JM_PRESOLVE_UNBOUNDED,
    JM_PRESOLVE_SOLVED,
} jm_presolve_outcome;

typedef struct {
    jaos_model reduced;
    int64_t *orig_col, *orig_row;
    int64_t *col_map, *row_map;
    jm_presolve_rec *arena;
    int64_t arena_len, arena_cap;
    jm_presolve_counts counts;
    jm_presolve_outcome outcome;

    int64_t proof_index;
    double proof_sign;
    jaos_model *orig;
} jm_presolve;

void jm_presolve_init(jm_presolve *p);
void jm_presolve_free(jm_presolve *p);

JAOS_NODISCARD jaos_status jm_presolve_run(const jaos_model *m, jm_presolve *p,
                                           jm_work *w);

JAOS_NODISCARD jaos_status jm_postsolve_expand(jm_presolve *p);

JAOS_NODISCARD jaos_status jm_postsolve_solved(jm_presolve *p);

JAOS_NODISCARD jaos_status jm_postsolve_infeasible_or_unbounded(
    jm_presolve *p, jaos_solve_status status);

typedef struct {
    int64_t *idx;
    double  *val;
    int64_t n, cap;
} jm_svec;

void jm_svec_free(jm_svec *v);
bool jm_svec_push(jm_svec *v, int64_t i, double x);
void jm_svec_erase(jm_svec *v, int64_t i);

typedef struct {
    int64_t dim;
    int64_t rank;

    int64_t *l_start;
    int64_t *l_index;
    double  *l_value;

    jm_svec *urow;
    jm_svec *ucol;
    double  *u_diag;

    jm_svec ft;
    int64_t *ft_source;
    int64_t ft_source_cap;
    int64_t n_updates;

    int64_t *slot_at;
    int64_t *pos_of;

    int64_t *perm_row;
    int64_t *perm_col;
    int64_t *inv_col;

    double drop;

    double *tmp;
    double *spike;

    int64_t *mark;
    int64_t stamp;
    int64_t *dfs_node;
    int64_t *dfs_next;
    int64_t *pattern;

    int64_t *lrow_start;
    int64_t *lrow_index;
} jm_lu;

constexpr double LU_PIVOT_TOL = 0.1;

void jm_lu_init(jm_lu *lu);
void jm_lu_free(jm_lu *lu);

JAOS_NODISCARD jaos_status jm_lu_factor(jm_lu *lu, int64_t dim,
    const int64_t *start, const int64_t *index, const double *value,
    double pivot_tol, jm_work *w);

void jm_lu_ftran(jm_lu *lu, double *x, jm_work *w);
void jm_lu_btran(jm_lu *lu, double *x, jm_work *w);

void jm_lu_ftran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

void jm_lu_btran_sparse(jm_lu *lu, double *x, jm_work *w,
                        int64_t *pat, int64_t *npat);

JAOS_NODISCARD jaos_status jm_lu_update(jm_lu *lu, int64_t col_out,
    const double *new_col, double min_pivot_ratio, jm_work *w);

#ifndef JM_EXACT_LIMBS
#define JM_EXACT_LIMBS 128
#endif

typedef struct {
    uint32_t w[JM_EXACT_LIMBS];
    int64_t  n;
} jm_nat;

typedef struct {
    jm_nat  mag;
    int32_t sign;
} jm_bigint;

typedef struct {
    jm_bigint num;
    jm_nat    den;
} jm_rational;

void    jm_nat_set_zero(jm_nat *a);
void    jm_nat_set_u64(jm_nat *a, uint64_t v);
bool    jm_nat_is_zero(const jm_nat *a);
int64_t jm_nat_bits(const jm_nat *a);
int     jm_nat_cmp(const jm_nat *a, const jm_nat *b);

void    jm_nat_sub(jm_nat *r, const jm_nat *a, const jm_nat *b);
void    jm_nat_shr(jm_nat *r, const jm_nat *a, int64_t bits);

JAOS_NODISCARD bool jm_nat_add(jm_nat *r, const jm_nat *a, const jm_nat *b);
JAOS_NODISCARD bool jm_nat_mul(jm_nat *r, const jm_nat *a, const jm_nat *b);
JAOS_NODISCARD bool jm_nat_shl(jm_nat *r, const jm_nat *a, int64_t bits);
JAOS_NODISCARD bool jm_nat_divmod(jm_nat *q, jm_nat *rem, const jm_nat *a,
                                  const jm_nat *b);
JAOS_NODISCARD bool jm_nat_gcd(jm_nat *r, const jm_nat *a, const jm_nat *b);

void    jm_bigint_set_zero(jm_bigint *a);
void    jm_bigint_set_i64(jm_bigint *a, int64_t v);
bool    jm_bigint_is_zero(const jm_bigint *a);
int32_t jm_bigint_sign(const jm_bigint *a);
void    jm_bigint_neg(jm_bigint *a);
int     jm_bigint_cmp(const jm_bigint *a, const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_add(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_sub(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);
JAOS_NODISCARD bool jm_bigint_mul(jm_bigint *r, const jm_bigint *a,
                                  const jm_bigint *b);

JAOS_NODISCARD bool jm_bigint_shl(jm_bigint *r, const jm_bigint *a,
                                  int64_t bits);

JAOS_NODISCARD bool jm_bigint_divexact(jm_bigint *q, const jm_bigint *a,
                                       const jm_bigint *b);

void    jm_rational_set_zero(jm_rational *r);
void    jm_rational_set_i64(jm_rational *r, int64_t v);
bool    jm_rational_is_zero(const jm_rational *r);
int32_t jm_rational_sign(const jm_rational *r);
void    jm_rational_neg(jm_rational *r);

int     jm_rational_cmp(const jm_rational *a, const jm_rational *c);

JAOS_NODISCARD bool jm_rational_cmp_checked(const jm_rational *a,
                                            const jm_rational *c, int *out);

double  jm_rational_to_double(const jm_rational *r);

char   *jm_rational_decimal(const jm_rational *r);
JAOS_NODISCARD bool jm_rational_from_decimal(jm_rational *r, const char *s);

JAOS_NODISCARD bool jm_rational_from_double(jm_rational *r, double d);

JAOS_NODISCARD bool jm_rational_add(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_sub(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_mul(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);
JAOS_NODISCARD bool jm_rational_div(jm_rational *r, const jm_rational *a,
                                    const jm_rational *c);

typedef struct {
    jm_bigint m;
    int64_t   e;
} jm_dyadic;

void    jm_dyadic_set_zero(jm_dyadic *d);
bool    jm_dyadic_is_zero(const jm_dyadic *d);
int32_t jm_dyadic_sign(const jm_dyadic *d);

double  jm_dyadic_to_double(const jm_dyadic *d);

JAOS_NODISCARD bool jm_dyadic_from_double(jm_dyadic *d, double v);

JAOS_NODISCARD bool jm_dyadic_add(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_sub(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_mul(jm_dyadic *r, const jm_dyadic *a,
                                  const jm_dyadic *b);
JAOS_NODISCARD bool jm_dyadic_cmp(const jm_dyadic *a, const jm_dyadic *b,
                                  int *out);

typedef struct {
    double  objective;
    double  row_violation;
    double  col_violation;
    int64_t row_at;
    int64_t col_at;
    int64_t terms;
} jm_exact_point;

JAOS_NODISCARD bool jm_exact_evaluate(jaos_model *m, const double *x,
                                      jm_exact_point *out);
#endif
