/* SPDX-License-Identifier: Apache-2.0 */

#include "jaos_internal.h"
#include "jaos_sys.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double PRIMAL_TOL    = 1e-7;
constexpr double PIVOT_MIN     = 1e-9;

constexpr double PIVOT_MARGIN  = 1.0;

constexpr double PRIMAL_HARRIS_DELTA = 0.5;

constexpr double DUAL_TOL      = 1e-9;

constexpr double LU_UPDATE_TOL = 1e-9;

constexpr double DSE_MIN = 1e-12;

constexpr double DSE_DRIFT = 10.0;

#ifndef JAOS_DSE_GUESS_RESTARTS
#define JAOS_DSE_GUESS_RESTARTS 64
#endif
constexpr int64_t DSE_GUESS_RESTARTS = JAOS_DSE_GUESS_RESTARTS;

constexpr double DEVEX_RESET = 3.0;

constexpr int64_t SPARSE_ALPHA_DEN = 4;

constexpr int64_t SPARSE_RHO_DEN = 4;

constexpr int64_t SPARSE_COL_DEN = 8;

constexpr int64_t REFACTOR_EVERY = 64;

constexpr double LU_AGREE_TOL = 1e-5;

constexpr int64_t TIME_CHECK_EVERY = 64;

constexpr int64_t LOG_EVERY = 1000;

constexpr int64_t PROGRESS_EVERY = 64;

constexpr double ARTIFICIAL_BOUND = 1e10;

constexpr int64_t ITER_SANITY_FACTOR = 200;

static_assert(ITER_SANITY_FACTOR >= 60,
              "the iteration cap is shared across phases (D196)");

constexpr int64_t STALL_FACTOR = 10;

#ifndef JAOS_DUAL_PERTURB
#define JAOS_DUAL_PERTURB 1e-6
#endif
constexpr double DUAL_PERTURB = JAOS_DUAL_PERTURB;

constexpr double PHASE1_RISE_MAX = 1.0;

constexpr double NOISE_MARGIN = 1e5;

constexpr int64_t SETTLE_ROUNDS = 32;
constexpr int64_t POLISH_ROUNDS = 4;
constexpr int64_t SETTLE_ROUNDS_PRIMAL = 256;

constexpr int64_t WARM_REPAIR_MAX_SHORT = 4;

typedef enum { NOT_FAKE = 0, FAKE_LO, FAKE_UP } jm_fake;

typedef struct {
    jaos_model *m;
    int64_t nrow, ncol, nvar;

    double *av;

    double *arv;

    double *lo, *up, *cost;
    double *cost0;
    double *shift;

    jm_var_status *status;
    int64_t *basis;
    int64_t *where;

    jm_fake *fake;

    double *xb;
    double *d;

    double *dse;

    double *devex;
    uint64_t *devref;
    bool devex_on;
    bool devex_stale;
    bool pse_on;
    double *sigma;
    double *pse_tau;

    jm_lu lu;
    jm_work work;

    double *col;
    double *raw;

    double *rhsc;
    double *resc;

    double *y;
    double *rho;
    double *tau;
    double *alpha;

    double farkas_sign;
    int64_t farkas_basic;

    double *uray;
    bool uray_ok;

    int64_t *apat;
    int64_t anpat;
    uint64_t *amark;

    uint64_t *nbmark;

    int64_t *rpat;
    int64_t nrpat;
    uint64_t *rmark;

    int64_t *cpat;
    int64_t ncpat;

    int64_t *cand;
    double *rnum, *rden;

    int64_t *prow;
    double *pnum, *pden;
    double *rrange;

#ifndef NDEBUG

    int64_t *dbg_cand;
    double *dbg_rnum, *dbg_rden;
    double *dbg_rrange;

    double *dbg_col;

    int64_t dbg_piv_since_verify;
#endif

    int64_t *bs, *bi;
    double *bv;
    int64_t bi_cap, bv_cap;

    jm_var_status *sav_status;
    int64_t *sav_basis;
    double *sav_lo, *sav_up;
    jm_fake *sav_fake;

    jm_var_status *bst_status;
    int64_t *bst_basis;
    double *bst_lo, *bst_up;
    jm_fake *bst_fake;
    double bst_obj;

    double bst_dviol;
    bool bst_valid;

    double started;
    int64_t iters;
    bool needs_refactor;

    bool verified;

    bool shift_pending;

    bool costs_perturbed;
    int64_t n_perturb;

    bool dse_guess;
    int64_t n_guess_restart;
    bool dse_exact_pending;

    bool duals_dirty;

    double infeas_best;
    int64_t last_gain;
    bool bland;

    double dinfeas_best;

    double primal_tol;
    double dual_tol;

    int64_t n_refactor;
    int64_t n_weight_restart;
    int64_t n_pse_exact;
    int64_t n_pse_cheap;
    int64_t n_bland;

    int64_t n_stability;

    int64_t n_primal_iters;

    int64_t n_phase1_iters;

    double *c1;

    int64_t *c1_at;
    int64_t n_c1_at;

    bool in_phase1;
} sx;

static inline void set_verified(sx *s, bool v)
{
    s->verified = v;
#ifndef NDEBUG
    s->dbg_piv_since_verify = 0;
#endif
}

static inline bool verified_fresh(const sx *s)
{
    assert(!s->verified || s->dbg_piv_since_verify == 0);
    return s->verified;
}

static void sx_free(sx *s)
{
    free(s->av); free(s->arv);
    free(s->lo); free(s->up); free(s->cost); free(s->cost0); free(s->shift);
    free(s->status); free(s->basis); free(s->where);
    free(s->xb); free(s->d); free(s->dse);
    free(s->devex); free(s->devref); free(s->sigma); free(s->pse_tau);
    free(s->col); free(s->raw); free(s->rhsc); free(s->resc);
    free(s->y); free(s->rho);
    free(s->tau); free(s->alpha); free(s->apat); free(s->amark);
    free(s->nbmark);
    free(s->rpat); free(s->rmark); free(s->cpat);
    free(s->cand); free(s->rnum); free(s->rden); free(s->rrange);
    free(s->prow); free(s->pnum); free(s->pden);
#ifndef NDEBUG
    free(s->dbg_cand); free(s->dbg_rnum); free(s->dbg_rden);
    free(s->dbg_rrange); free(s->dbg_col);
#endif
    free(s->bs); free(s->bi); free(s->bv);
    free(s->fake); free(s->c1); free(s->c1_at);
    free(s->uray);
    free(s->sav_status); free(s->sav_basis);
    free(s->sav_lo); free(s->sav_up); free(s->sav_fake);
    free(s->bst_status); free(s->bst_basis);
    free(s->bst_lo); free(s->bst_up); free(s->bst_fake);
    jm_lu_free(&s->lu);
    memset(s, 0, sizeof *s);
}

static jaos_status sx_init(sx *s, jaos_model *m)
{
    memset(s, 0, sizeof *s);
    jm_lu_init(&s->lu);
    s->m = m;
    s->nrow = m->num_row;
    s->ncol = m->num_col;
    s->nvar = m->num_col + m->num_row;

    s->primal_tol = m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol : PRIMAL_TOL;
    s->dual_tol   = m->cfg.dual_tol   > 0.0 ? m->cfg.dual_tol   : DUAL_TOL;

    if (!m->scale_valid) {
        jaos_status st = jm_model_scale(m, JM_SCALE_CURTIS_REID);
        if (st != JAOS_OK)
            return st;
    }

    {
        jaos_status st = jm_model_ensure_rowwise(m);
        if (st != JAOS_OK)
            return st;
    }

    s->av     = jm_alloc_array(m->num_nz, sizeof(double));
    s->arv    = jm_alloc_array(m->num_nz, sizeof(double));
    s->lo     = jm_alloc_array(s->nvar, sizeof(double));
    s->up     = jm_alloc_array(s->nvar, sizeof(double));
    s->cost   = jm_calloc_array(s->nvar, sizeof(double));
    s->cost0  = jm_calloc_array(s->nvar, sizeof(double));
    s->shift  = jm_calloc_array(s->nvar, sizeof(double));
    s->status = jm_alloc_array(s->nvar, sizeof(jm_var_status));
    s->basis  = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->where  = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->xb     = jm_calloc_array(s->nrow, sizeof(double));
    s->d      = jm_calloc_array(s->nvar, sizeof(double));
    s->dse    = jm_alloc_array(s->nrow, sizeof(double));
    s->devex  = jm_alloc_array(s->nvar, sizeof(double));
    s->devref = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
    s->devex_on = false;
    s->devex_stale = false;
    s->sigma   = jm_alloc_array(s->nvar, sizeof(double));
    s->pse_tau = jm_alloc_array(s->nrow > 0 ? s->nrow : 1, sizeof(double));
    s->col    = jm_calloc_array(s->nrow, sizeof(double));
    s->raw    = jm_calloc_array(s->nrow, sizeof(double));
    s->rhsc   = jm_calloc_array(s->nrow, sizeof(double));
    s->resc   = jm_calloc_array(s->nrow, sizeof(double));
    s->y      = jm_calloc_array(s->nrow, sizeof(double));
    s->rho    = jm_calloc_array(s->nrow, sizeof(double));
    s->tau    = jm_calloc_array(s->nrow, sizeof(double));
    s->alpha  = jm_calloc_array(s->nvar, sizeof(double));
    s->apat   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->amark  = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
    s->nbmark = jm_calloc_array((s->nvar + 63) / 64, sizeof(uint64_t));
    s->rpat   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->rmark  = jm_calloc_array((s->nrow + 63) / 64, sizeof(uint64_t));
    s->cpat   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->ncpat  = -1;
    s->anpat  = -1;
    s->nrpat  = -1;
    s->duals_dirty = true;
    s->cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->rrange = jm_alloc_array(s->nvar, sizeof(double));
    s->prow   = jm_alloc_array(s->nrow, sizeof(int64_t));
    s->pnum   = jm_alloc_array(s->nrow, sizeof(double));
    s->pden   = jm_alloc_array(s->nrow, sizeof(double));
    s->bs     = jm_alloc_array(s->nrow + 1, sizeof(int64_t));
    s->fake   = jm_calloc_array(s->nvar, sizeof *s->fake);
    s->uray   = jm_calloc_array(s->ncol, sizeof(double));

    if (!s->av || !s->arv || !s->lo || !s->up || !s->cost || !s->cost0 ||
        !s->shift ||
        !s->status || !s->basis ||
        !s->where || !s->xb || !s->d || !s->dse || !s->devex || !s->devref ||
        !s->sigma || !s->pse_tau ||
        !s->col || !s->raw || !s->rhsc || !s->resc ||
        !s->y || !s->rho || !s->tau || !s->alpha || !s->apat || !s->amark ||
        !s->nbmark ||
        !s->rpat || !s->rmark || !s->cpat || !s->cand || !s->rnum ||
        !s->rden || !s->rrange || !s->prow || !s->pnum || !s->pden ||
        !s->bs || !s->fake || !s->uray) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }

#ifndef NDEBUG

    s->dbg_cand   = jm_alloc_array(s->nvar, sizeof(int64_t));
    s->dbg_rnum   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rden   = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_rrange = jm_alloc_array(s->nvar, sizeof(double));
    s->dbg_col    = jm_alloc_array(s->nrow, sizeof(double));
    if (!s->dbg_cand || !s->dbg_rnum || !s->dbg_rden || !s->dbg_rrange ||
        !s->dbg_col) {
        sx_free(s);
        return JAOS_ERR_OUT_OF_MEMORY;
    }
#endif

    const double *rho = m->row_scale, *gamma = m->col_scale;

    for (int64_t j = 0; j < s->ncol; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->av[k] = rho[m->a_index[k]] * m->a_value[k] * gamma[j];

    for (int64_t i = 0; i < s->nrow; i++)
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            s->arv[p] = rho[i] * m->ar_value[p] * gamma[m->ar_index[p]];

    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    for (int64_t j = 0; j < s->ncol; j++) {
        s->lo[j] = m->col_lower[j] / gamma[j];
        s->up[j] = m->col_upper[j] / gamma[j];
        s->cost[j] = sigma * m->col_cost[j] * gamma[j];
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        s->lo[s->ncol + i] = m->row_lower[i] * rho[i];
        s->up[s->ncol + i] = m->row_upper[i] * rho[i];
        s->cost[s->ncol + i] = 0.0;
    }

    memcpy(s->cost0, s->cost, (size_t)s->nvar * sizeof *s->cost0);

#ifndef NDEBUG

    for (int64_t j = 0; j < s->ncol; j++) {
        assert(isfinite(s->lo[j]) == isfinite(m->col_lower[j]));
        assert(isfinite(s->up[j]) == isfinite(m->col_upper[j]));
    }
    for (int64_t i = 0; i < s->nrow; i++) {
        assert(isfinite(s->lo[s->ncol + i]) == isfinite(m->row_lower[i]));
        assert(isfinite(s->up[s->ncol + i]) == isfinite(m->row_upper[i]));
    }
#endif
    return JAOS_OK;
}

static double nonbasic_value(const sx *s, int64_t v)
{
    switch (s->status[v]) {
    case JM_AT_LOWER: return s->lo[v];
    case JM_AT_UPPER: return s->up[v];
    case JM_FREE:     return 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;
}

static double var_value(const sx *s, int64_t v)
{
    return s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                    : nonbasic_value(s, v);
}

static double real_lower(const sx *s, int64_t v)
{
    return s->fake[v] == FAKE_LO ? -HUGE_VAL : s->lo[v];
}

static double real_upper(const sx *s, int64_t v)
{
    return s->fake[v] == FAKE_UP ? HUGE_VAL : s->up[v];
}

static void var_column(const sx *s, int64_t v, double *out)
{
    memset(out, 0, (size_t)s->nrow * sizeof *out);
    if (v < s->ncol) {
        const jaos_model *m = s->m;
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
            out[m->a_index[k]] = s->av[k];
    } else {
        out[v - s->ncol] = -1.0;
    }
}

static double price_entry(sx *s, const double *w, int64_t v)
{
    if (v >= s->ncol) {
        jm_work_add(&s->work, JM_WORK_NONZERO);
        return -w[v - s->ncol];
    }
    const jaos_model *m = s->m;
    double a = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        a += w[m->a_index[k]] * s->av[k];
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return a;
}

static void build_initial_basis(sx *s)
{
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->ncol + i;
        s->basis[i] = v;
        s->status[v] = JM_BASIC;
        s->where[v] = i;

        s->dse[i] = 1.0;
    }
    for (int64_t j = 0; j < s->ncol; j++) {
        s->where[j] = -1;
        bool has_lo = isfinite(s->lo[j]);
        bool has_up = isfinite(s->up[j]);

        if (s->cost[j] > 0.0) {
            if (!has_lo) {
                s->lo[j] = -ARTIFICIAL_BOUND;
                s->fake[j] = FAKE_LO;
            }
            s->status[j] = JM_AT_LOWER;
        } else if (s->cost[j] < 0.0) {
            if (!has_up) {
                s->up[j] = ARTIFICIAL_BOUND;
                s->fake[j] = FAKE_UP;
            }
            s->status[j] = JM_AT_UPPER;
        } else if (has_lo) {
            s->status[j] = JM_AT_LOWER;
        } else if (has_up) {
            s->status[j] = JM_AT_UPPER;
        } else {
            s->status[j] = JM_FREE;
        }
    }

    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

#ifndef NDEBUG

    for (int64_t v = 0; v < s->nvar; v++) {
        assert(s->fake[v] != FAKE_LO || s->lo[v] == -ARTIFICIAL_BOUND);
        assert(s->fake[v] != FAKE_UP || s->up[v] == ARTIFICIAL_BOUND);

        assert(s->fake[v] == NOT_FAKE || s->fake[v] == FAKE_LO ||
               s->fake[v] == FAKE_UP);
    }
#endif
}

static bool build_warm_basis(sx *s)
{
    const jaos_model *m = s->m;
    if (m->start_col_status == nullptr || m->start_row_status == nullptr)
        return false;

    jaos_basis_status *want_arr =
        jm_alloc_array(s->nvar > 0 ? s->nvar : 1, sizeof *want_arr);
    if (want_arr == nullptr)
        return false;
    for (int64_t v = 0; v < s->nvar; v++)
        want_arr[v] = v < s->ncol ? m->start_col_status[v]
                                  : m->start_row_status[v - s->ncol];

    int64_t nbasic = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        nbasic += want_arr[v] == JAOS_BASIS_BASIC;

    if (nbasic < s->nrow && s->nrow - nbasic <= WARM_REPAIR_MAX_SHORT) {
        unsigned char *cov = jm_calloc_array(s->nrow > 0 ? s->nrow : 1,
                                             sizeof *cov);
        if (cov == nullptr) {
            free(want_arr);
            return false;
        }
        int64_t covnz = 0;
        for (int64_t v = 0; v < s->nvar; v++) {
            if (want_arr[v] != JAOS_BASIS_BASIC)
                continue;
            if (v < s->ncol) {
                for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                    cov[m->a_index[k]] = 1;
                covnz += m->a_start[v + 1] - m->a_start[v];
            } else {
                cov[v - s->ncol] = 1;
                covnz++;
            }
        }
        jm_work_add(&s->work, covnz * JM_WORK_NONZERO);
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (!cov[i] && want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        for (int64_t i = 0; i < s->nrow && nbasic < s->nrow; i++)
            if (want_arr[s->ncol + i] != JAOS_BASIS_BASIC) {
                want_arr[s->ncol + i] = JAOS_BASIS_BASIC;
                nbasic++;
            }
        free(cov);
        jm_log(s->m, JAOS_LOG_DETAIL,
               "the mapped starting basis arrived short and was repaired "
               "by promoting logicals");
    }
    if (nbasic != s->nrow) {
        free(want_arr);
        return false;
    }

    int64_t p = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        jaos_basis_status want = want_arr[v];
        if (want == JAOS_BASIS_BASIC) {
            s->basis[p] = v;
            s->status[v] = JM_BASIC;
            s->where[v] = p;
            p++;
            continue;
        }

        s->where[v] = -1;
        if (want == JAOS_BASIS_AT_UPPER && isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else if (isfinite(s->lo[v]))
            s->status[v] = JM_AT_LOWER;
        else if (isfinite(s->up[v]))
            s->status[v] = JM_AT_UPPER;
        else
            s->status[v] = JM_FREE;
    }
    free(want_arr);

    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    for (int64_t i = 0; i < s->nrow; i++)
        s->dse[i] = 1.0;
    s->dse_guess = !m->cfg.node_solve && !m->cfg.force_primal;
    s->n_guess_restart = 0;
    s->dse_exact_pending = false;

    s->shift_pending = true;
    return true;
}

static jaos_status refactorize(sx *s)
{
    int64_t nz = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        nz += v < s->ncol ? s->m->a_start[v + 1] - s->m->a_start[v] : 1;
    }

    int64_t room = nz > 0 ? nz : 1;
    if (!JM_GROW(s->bi, s->bi_cap, room) || !JM_GROW(s->bv, s->bv_cap, room))
        return JAOS_ERR_OUT_OF_MEMORY;

    int64_t p = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        s->bs[i] = p;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            for (int64_t k = s->m->a_start[v]; k < s->m->a_start[v + 1]; k++) {
                s->bi[p] = s->m->a_index[k];
                s->bv[p] = s->av[k];
                p++;
            }
        } else {
            s->bi[p] = v - s->ncol;
            s->bv[p] = -1.0;
            p++;
        }
    }
    s->bs[s->nrow] = p;

    jaos_status st = jm_lu_factor(&s->lu, s->nrow, s->bs, s->bi, s->bv,
                                  LU_PIVOT_TOL, &s->work);
    if (st != JAOS_OK)
        return st;
    s->needs_refactor = false;
    return JAOS_OK;
}

static void subtract_basis_times(sx *s, double *r, const double *z)
{
    int64_t nz = 0;
    double *comp = s->resc;
    memset(comp, 0, (size_t)s->nrow * sizeof *comp);
    for (int64_t i = 0; i < s->nrow; i++) {
        double zi = z[i];
        if (zi == 0.0)
            continue;
        int64_t v = s->basis[i];
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                const int64_t ii = m->a_index[k];
                const double t = -(s->av[k] * zi);
                const double a = r[ii], u = a + t;
                comp[ii] += (fabs(a) >= fabs(t)) ? ((a - u) + t)
                                                 : ((t - u) + a);
                r[ii] = u;
            }
            nz += m->a_start[v + 1] - m->a_start[v];
        } else {
            const int64_t ii = v - s->ncol;
            const double a = r[ii], u = a + zi;
            comp[ii] += (fabs(a) >= fabs(zi)) ? ((a - u) + zi)
                                              : ((zi - u) + a);
            r[ii] = u;
            nz++;
        }
    }

    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(r[i]) && isfinite(comp[i]))
            r[i] += comp[i];
    jm_work_add(&s->work, nz * JM_WORK_NONZERO);
}

static void compute_primal(sx *s, bool refine)
{
    double *rhs = s->col;
    double *comp = s->rhsc;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);
    memset(comp, 0, (size_t)s->nrow * sizeof *comp);

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        double val = nonbasic_value(s, v);
        if (val == 0.0)
            continue;
        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++) {
                const int64_t i = m->a_index[k];
                const double t = -(s->av[k] * val);
                const double a = rhs[i], u = a + t;
                comp[i] += (fabs(a) >= fabs(t)) ? ((a - u) + t)
                                                : ((t - u) + a);
                rhs[i] = u;
            }
            jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                                  JM_WORK_NONZERO);
        } else {
            const int64_t i = v - s->ncol;
            const double a = rhs[i], u = a + val;
            comp[i] += (fabs(a) >= fabs(val)) ? ((a - u) + val)
                                              : ((val - u) + a);
            rhs[i] = u;
            jm_work_add(&s->work, JM_WORK_NONZERO);
        }
    }

    for (int64_t i = 0; i < s->nrow; i++)
        if (isfinite(rhs[i]) && isfinite(comp[i]))
            rhs[i] += comp[i];

    if (refine)
        memcpy(s->raw, rhs, (size_t)s->nrow * sizeof *s->raw);

    jm_lu_ftran(&s->lu, rhs, &s->work);
    memcpy(s->xb, rhs, (size_t)s->nrow * sizeof *rhs);

    if (!refine)
        return;

    double *r = s->raw;
    subtract_basis_times(s, r, s->xb);
    jm_lu_ftran(&s->lu, r, &s->work);
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] += r[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
}

static void compute_duals(sx *s, bool refine)
{
    double *y = s->y;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);

    if (refine) {

        double *r = s->tau;
        int64_t nz = 0;
        for (int64_t i = 0; i < s->nrow; i++) {
            int64_t v = s->basis[i];
            double dot;
            if (v < s->ncol) {
                const jaos_model *m = s->m;
                dot = 0.0;
                for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
                    dot += s->av[k] * y[m->a_index[k]];
                nz += m->a_start[v + 1] - m->a_start[v];
            } else {
                dot = -y[v - s->ncol];
                nz++;
            }
            r[i] = s->cost[v] - dot;
        }
        jm_work_add(&s->work, nz * JM_WORK_NONZERO);
        jm_lu_btran(&s->lu, r, &s->work);
        for (int64_t i = 0; i < s->nrow; i++)
            y[i] += r[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC) {
            s->d[v] = 0.0;
            continue;
        }
        s->d[v] = s->cost[v] - price_entry(s, s->y, v);
    }

    s->duals_dirty = true;
}

static void shift_to_feasible(sx *s, int64_t v);
static bool shifts_costs(const sx *s);

constexpr int REPAIR_ATTEMPTS = 4;

static bool repair_singular_basis(sx *s)
{
    const int64_t n = s->nrow;
    const int64_t rank = s->lu.rank;

    if (rank < 0 || rank >= n)
        return false;

    bool *row_covered = jm_calloc_array(n, sizeof(bool));
    bool *pos_used    = jm_calloc_array(n, sizeof(bool));
    if (row_covered == nullptr || pos_used == nullptr) {
        free(row_covered);
        free(pos_used);
        return false;
    }

    for (int64_t k = 0; k < rank; k++) {
        row_covered[s->lu.perm_row[k]] = true;
        pos_used[s->lu.perm_col[k]] = true;
    }

    bool done = true;
    int64_t i = 0;
    for (int64_t p = 0; p < n; p++) {
        if (pos_used[p])
            continue;
        while (i < n && row_covered[i])
            i++;
        if (i >= n) {

            done = false;
            break;
        }

        int64_t leaving  = s->basis[p];
        int64_t entering = s->ncol + i;
        if (s->status[entering] == JM_BASIC) {
            done = false;
            break;
        }

        if (isfinite(s->lo[leaving]))
            s->status[leaving] = JM_AT_LOWER;
        else if (isfinite(s->up[leaving]))
            s->status[leaving] = JM_AT_UPPER;
        else
            s->status[leaving] = JM_FREE;
        jm_nonbasic_insert(s->nbmark, leaving);
        s->where[leaving] = -1;

        s->basis[p] = entering;
        s->status[entering] = JM_BASIC;
        jm_nonbasic_remove(s->nbmark, entering);
        s->where[entering] = p;
        i++;
    }

    free(row_covered);
    free(pos_used);
    if (!done)
        return false;

    for (int64_t k = 0; k < n; k++)
        s->dse[k] = 1.0;
    return true;
}

#ifndef NDEBUG

static bool nbmark_consistent(const sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        const bool in_map = (s->nbmark[v >> 6] >> (v & 63)) & 1;
        if (in_map != (s->status[v] != JM_BASIC))
            return false;
    }
    return true;
}
#endif

static jaos_status refresh(sx *s, bool *ok, bool refine)
{
    bool repaired = false;
    s->n_refactor++;

    for (int attempt = 0;; attempt++) {
        jaos_status st = refactorize(s);
        if (st != JAOS_OK)
            return st;
        if (s->lu.rank == s->nrow)
            break;
        if (attempt + 1 >= REPAIR_ATTEMPTS || !repair_singular_basis(s)) {
            jm_set_err(s->m, "the basis went singular at iteration %lld and "
                             "could not be repaired: rank %lld of %lld",
                       (long long)s->iters, (long long)s->lu.rank,
                       (long long)s->nrow);
            *ok = false;
            return JAOS_OK;
        }
        repaired = true;
    }

    compute_primal(s, refine);
    compute_duals(s, refine);

    if (shifts_costs(s)) {
        bool sweep = repaired || s->shift_pending;
        s->shift_pending = false;
        if (sweep)
            for (int64_t v = 0; v < s->nvar; v++)
                shift_to_feasible(s, v);
    }

    assert(nbmark_consistent(s));

    *ok = true;
    return JAOS_OK;
}

static uint64_t perturb_hash(uint64_t x)
{
    x += UINT64_C(0x9E3779B97F4A7C15);
    x = (x ^ (x >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94D049BB133111EB);
    return x ^ (x >> 31);
}

static void perturb_costs(sx *s)
{
    int64_t moved = 0;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (!(s->lo[v] < s->up[v]))
            continue;
        double sign;
        if (s->status[v] == JM_AT_LOWER)
            sign = 1.0;
        else if (s->status[v] == JM_AT_UPPER)
            sign = -1.0;
        else
            continue;
        const double u = 0.5 + 0.5 * (double)(perturb_hash((uint64_t)v) >> 11) *
                                   (1.0 / 9007199254740992.0);
        const double delta = sign * DUAL_PERTURB * (1.0 + fabs(s->cost0[v])) * u;
        s->cost[v] += delta;
        s->shift[v] += delta;
        s->d[v] += delta;
        moved++;
    }
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    s->costs_perturbed = true;
    s->n_perturb++;
    jm_log(s->m, JAOS_LOG_DETAIL,
           "iter %lld: no progress for %lld iterations, perturbing %lld "
           "reduced costs", (long long)s->iters,
           (long long)(s->iters - s->last_gain), (long long)moved);
    s->last_gain = s->iters;
}

static int64_t price_row(sx *s, bool *below, double *violation)
{

    if (!s->bland &&
        s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
        if (DUAL_PERTURB > 0.0 && !s->costs_perturbed && shifts_costs(s)) {
            perturb_costs(s);
        } else {
        s->bland = true;
        s->n_bland++;
        jm_log(s->m, JAOS_LOG_DETAIL,
               "iter %lld: no progress for %lld iterations, switching to "
               "Bland's rule", (long long)s->iters,
               (long long)(s->iters - s->last_gain));
        }
    }

    int64_t best = -1;
    double best_score = 0.0;
    double total = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        int64_t v = s->basis[i];
        double viol_lo = isfinite(s->lo[v]) ? s->lo[v] - s->xb[i] : 0.0;
        double viol_up = isfinite(s->up[v]) ? s->xb[i] - s->up[v] : 0.0;

        bool under = viol_lo >= viol_up;
        double viol = under ? viol_lo : viol_up;
        if (viol <= s->primal_tol)
            continue;

        total += viol;

        if (s->bland) {
            if (best < 0 || v < s->basis[best]) {
                best = i;
                *below = under;
                *violation = viol;
            }
            continue;
        }

        double score = viol * viol / s->dse[i];
        if (score > best_score) {
            best_score = score;
            best = i;
            *below = under;
            *violation = viol;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    if (best >= 0 && total < s->infeas_best) {
        s->infeas_best = total;
        s->last_gain = s->iters;
        s->bland = false;
    }
    return best;
}

static bool weight_drifted(double carried, double exact, double factor)
{
    if (!isfinite(carried) || carried <= 0.0)
        return true;
    if (!isfinite(exact) || exact <= 0.0)
        return true;
    return carried > exact * factor || carried * factor < exact;
}

bool jm_dse_update(int64_t n, double *w, int64_t r,
                   const double *alpha, const double *tau,
                   double exact_r, double drift_factor,
                   const int64_t *pat, int64_t npat)
{
    if (weight_drifted(w[r], exact_r, drift_factor)) {
        for (int64_t i = 0; i < n; i++)
            w[i] = 1.0;
        return true;
    }
    w[r] = exact_r;

    double pivot = alpha[r];
    if (pivot == 0.0)
        return false;

    double wr = w[r];

    const int64_t nvisit = pat != nullptr ? npat : n;
    for (int64_t k = 0; k < nvisit; k++) {
        int64_t i = pat != nullptr ? pat[k] : k;
        if (i == r || alpha[i] == 0.0)
            continue;
        double kk = alpha[i] / pivot;
        double wi = w[i] - 2.0 * kk * tau[i] + kk * kk * wr;
        w[i] = wi > DSE_MIN ? wi : DSE_MIN;
    }
    double wnew = wr / (pivot * pivot);
    w[r] = wnew > DSE_MIN ? wnew : DSE_MIN;
    return false;
}

static int64_t bfrt_walk(sx *s, int64_t n, double remaining)
{
    int64_t live = n;

    while (live > 0) {
        int64_t k = 0;
        double least = HUGE_VAL;
        for (int64_t j = 0; j < live; j++) {
            double t = s->rnum[j] / s->rden[j];
            if (t < least) {
                least = t;
                k = j;
            }
        }
        jm_work_add(&s->work, live * JM_WORK_NONZERO);

        double width = s->rrange[k];
        if (!isfinite(width))
            break;
        if (!(remaining - s->rden[k] * width > 0.0))
            break;
        remaining -= s->rden[k] * width;

        live--;
        int64_t ci = s->cand[k];
        double a = s->rnum[k], b = s->rden[k], c = s->rrange[k];
        s->cand[k]   = s->cand[live];   s->cand[live]   = ci;
        s->rnum[k]   = s->rnum[live];   s->rnum[live]   = a;
        s->rden[k]   = s->rden[live];   s->rden[live]   = b;
        s->rrange[k] = s->rrange[live]; s->rrange[live] = c;
    }

    if (live == 0 && remaining <= s->primal_tol) {

        live = 1;
    }
    return live;
}

static void apply_flips(sx *s, int64_t at, int64_t n)
{

    double *rhs = s->col;
    memset(rhs, 0, (size_t)s->nrow * sizeof *rhs);

    for (int64_t k = at; k < n; k++) {

        assert(isfinite(s->rrange[k]));
        int64_t v = s->cand[k];
        double from = nonbasic_value(s, v);
        s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                   : JM_AT_LOWER;
        double delta = nonbasic_value(s, v) - from;
        if (delta == 0.0)
            continue;

        if (v < s->ncol) {
            const jaos_model *m = s->m;
            for (int64_t p = m->a_start[v]; p < m->a_start[v + 1]; p++)
                rhs[m->a_index[p]] += s->av[p] * delta;
            jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                                  JM_WORK_NONZERO);
        } else {
            rhs[v - s->ncol] -= delta;
            jm_work_add(&s->work, JM_WORK_NONZERO);
        }
    }

    int64_t nc = 0;
    jm_lu_ftran_sparse(&s->lu, rhs, &s->work, s->cpat, &nc);
    if (nc * SPARSE_COL_DEN <= s->nrow) {
        for (int64_t k = 0; k < nc; k++)
            s->xb[s->cpat[k]] -= rhs[s->cpat[k]];
        jm_work_add(&s->work, nc * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= rhs[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }

    s->ncpat = -1;
}

static void admit_candidate(sx *s, int64_t v, bool below, int64_t *n)
{
    if (s->status[v] == JM_BASIC)
        return;
    if (s->lo[v] == s->up[v])
        return;
    double a = s->alpha[v];
    if (fabs(a) < PIVOT_MIN)
        return;

    bool ok;
    double dist;
    if (s->status[v] == JM_AT_LOWER) {
        ok = below ? (a < 0.0) : (a > 0.0);
        dist = s->d[v];
    } else if (s->status[v] == JM_AT_UPPER) {
        ok = below ? (a > 0.0) : (a < 0.0);
        dist = -s->d[v];
    } else {
        ok = true;
        dist = 0.0;
    }
    if (!ok)
        return;

    int64_t k = (*n)++;
    s->cand[k] = v;
    s->rnum[k] = dist > 0.0 ? dist : 0.0;
    s->rden[k] = fabs(a);
    s->rrange[k] = s->up[v] - s->lo[v];
}

static int64_t dual_ratio_test(sx *s, bool below, double violation,
                               double *theta_out)
{
    int64_t n = 0;

    if (s->anpat >= 0) {
        for (int64_t t = 0; t < s->anpat; t++)
            admit_candidate(s, s->apat[t], below, &n);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    } else {

        assert(nbmark_consistent(s));

        int64_t visited = 0;
        int64_t nwords = (s->nvar + 63) / 64;
        for (int64_t w = 0; w < nwords; w++) {
            uint64_t bits = s->nbmark[w];
            while (bits != 0) {
                admit_candidate(s, (w << 6) + __builtin_ctzll(bits), below,
                                &n);
                bits &= bits - 1;
                visited++;
            }
        }
        jm_work_add(&s->work, visited * JM_WORK_NONZERO);

        assert(visited == s->nvar - s->nrow);
    }

#ifndef NDEBUG

    {
        for (int64_t k = 0; k < n; k++) {
            s->dbg_cand[k]   = s->cand[k];
            s->dbg_rnum[k]   = s->rnum[k];
            s->dbg_rden[k]   = s->rden[k];
            s->dbg_rrange[k] = s->rrange[k];
        }
        int64_t dn = 0;
        for (int64_t v = 0; v < s->nvar; v++)
            admit_candidate(s, v, below, &dn);
        assert(dn == n);
        for (int64_t k = 0; k < n; k++) {
            assert(s->cand[k] == s->dbg_cand[k]);
            assert(s->rnum[k] == s->dbg_rnum[k]);
            assert(s->rden[k] == s->dbg_rden[k]);
            assert(s->rrange[k] == s->dbg_rrange[k]);
        }
    }
#endif

    if (n == 0)
        return -1;

    if (s->bland) {
        int64_t b = jm_bland_pick(n, s->cand, s->rnum, s->rden);
        jm_work_add(&s->work, 2 * n * JM_WORK_NONZERO);
        int64_t bv = s->cand[b];
        *theta_out = s->d[bv] / s->alpha[bv];
        return bv;
    }

    int64_t live = bfrt_walk(s, n, violation);
    if (live == 0)
        return -1;

    int64_t k = jm_harris_pick(live, s->rnum, s->rden, s->dual_tol);
    jm_work_add(&s->work, 2 * live * JM_WORK_NONZERO);
    int64_t best = s->cand[k];

    if (live < n)
        apply_flips(s, live, n);

    *theta_out = s->d[best] / s->alpha[best];
    return best;
}

int64_t jm_bland_pick(int64_t n, const int64_t *var, const double *num,
                      const double *den)
{
    if (n <= 0)
        return -1;

    double least = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        double t = num[k] / den[k];
        if (t < least)
            least = t;
    }

    int64_t best = -1;
    for (int64_t k = 0; k < n; k++)
        if (num[k] / den[k] <= least && (best < 0 || var[k] < var[best]))
            best = k;
    return best;
}

bool jm_primal_row_wins(double step, int64_t var,
                        double best_step, int64_t best_var, bool bland)
{
    if (step < best_step)
        return true;
    return bland && best_var >= 0 && step == best_step && var < best_var;
}

int64_t jm_pattern_order(int64_t n, int64_t *pos, uint64_t *mark,
                         int64_t limit, int64_t *words)
{
    *words = 0;
    if (n <= 0 || limit <= 0)
        return 0;

#ifndef NDEBUG
    const int64_t nwords_dbg = (limit + 63) / 64;
    for (int64_t w = 0; w < nwords_dbg; w++)
        assert(mark[w] == 0);
#endif

    int64_t lo = (limit + 63) / 64, hi = -1;
    for (int64_t t = 0; t < n; t++) {
        int64_t p = pos[t];
        if (p < 0 || p >= limit)
            continue;
        int64_t w = p >> 6;
        mark[w] |= UINT64_C(1) << (p & 63);
        if (w < lo) lo = w;
        if (w > hi) hi = w;
    }

    int64_t k = 0;
    for (int64_t w = lo; w <= hi; w++) {
        uint64_t bits = mark[w];
        if (bits == 0)
            continue;
        mark[w] = 0;
        while (bits != 0) {
            pos[k++] = (w << 6) + __builtin_ctzll(bits);
            bits &= bits - 1;
        }
    }
    *words = hi >= lo ? hi - lo + 1 : 0;
#ifndef NDEBUG

    assert(k <= n);
    for (int64_t w = lo; w <= hi; w++)
        assert(mark[w] == 0);

    for (int64_t t = 1; t < k; t++)
        assert(pos[t] > pos[t - 1]);
#endif
    return k;
}

int64_t jm_nonbasic_build(int64_t nvar, const jm_var_status *status,
                          uint64_t *mark)
{
    int64_t nwords = (nvar + 63) / 64;
    for (int64_t w = 0; w < nwords; w++)
        mark[w] = 0;

    int64_t k = 0;
    for (int64_t v = 0; v < nvar; v++) {
        if (status[v] == JM_BASIC)
            continue;
        mark[v >> 6] |= UINT64_C(1) << (v & 63);
        k++;
    }
    return k;
}

void jm_nonbasic_insert(uint64_t *mark, int64_t v)
{
    mark[v >> 6] |= UINT64_C(1) << (v & 63);
}

void jm_nonbasic_remove(uint64_t *mark, int64_t v)
{
    mark[v >> 6] &= ~(UINT64_C(1) << (v & 63));
}

int64_t jm_nonbasic_expand(int64_t nvar, const uint64_t *mark, int64_t *out)
{
    int64_t nwords = (nvar + 63) / 64, k = 0;
    for (int64_t w = 0; w < nwords; w++) {
        uint64_t bits = mark[w];
        while (bits != 0) {
            out[k++] = (w << 6) + __builtin_ctzll(bits);
            bits &= bits - 1;
        }
    }
    return k;
}

int64_t jm_harris_pick(int64_t n, const double *num, const double *den,
                       double dual_tol)
{
    if (n <= 0)
        return -1;

#ifndef NDEBUG
    for (int64_t k = 0; k < n; k++) {
        assert(num[k] >= 0.0);
        assert(den[k] > 0.0);
    }
#endif

    double window = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        double t = (num[k] + dual_tol) / den[k];
        if (t < window)
            window = t;
    }

    int64_t best = 0;
    double best_den = 0.0;
    for (int64_t k = 0; k < n; k++) {
        if (num[k] / den[k] <= window && den[k] > best_den) {
            best_den = den[k];
            best = k;
        }
    }

    assert(best >= 0 && best < n);
    return best;
}

static void price_all(sx *s)
{
    const jaos_model *m = s->m;

    if (s->anpat < 0)
        memset(s->alpha, 0, (size_t)s->nvar * sizeof *s->alpha);
    else
        for (int64_t k = 0; k < s->anpat; k++)
            s->alpha[s->apat[k]] = 0.0;

    const int64_t cap = s->nvar / SPARSE_ALPHA_DEN;
    int64_t np = 0, touched = 0;

    const bool sparse_rows = s->nrpat >= 0;
    const int64_t nvisit = sparse_rows ? s->nrpat : s->nrow;
    int64_t nfound = 0;

    for (int64_t k = 0; k < nvisit; k++) {
        int64_t i = sparse_rows ? s->rpat[k] : k;
        double w = s->rho[i];
        if (w == 0.0)
            continue;

        if (!sparse_rows)
            s->rpat[nfound++] = i;

        int64_t lg = s->ncol + i;
        if (np < cap)
            s->apat[np] = lg;
        np++;
        s->alpha[lg] = -w;
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++) {
            int64_t c = m->ar_index[p];
            double prev = s->alpha[c];
            if (prev == 0.0) {
                if (np < cap)
                    s->apat[np] = c;
                np++;
            }
            s->alpha[c] = prev + w * s->arv[p];
        }
        touched += m->ar_start[i + 1] - m->ar_start[i];
    }
    if (!sparse_rows)
        s->nrpat = nfound;

    const bool sparse_zero = np <= cap && np < s->nrow;
    if (sparse_zero) {
        for (int64_t k = 0; k < np; k++) {
            int64_t v = s->apat[k];
            if (s->status[v] == JM_BASIC)
                s->alpha[v] = 0.0;
        }
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->alpha[s->basis[i]] = 0.0;
    }

    jm_work_add(&s->work, (touched + nvisit) * JM_WORK_NONZERO);

    if (np > cap) {
        s->anpat = -1;
        return;
    }
    int64_t words = 0;
    s->anpat = jm_pattern_order(np, s->apat, s->amark, s->nvar, &words);
    jm_work_add(&s->work, (np + words + s->anpat) * JM_WORK_NONZERO);

#ifndef NDEBUG

    int64_t nz_total = 0, nz_pat = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        if (s->alpha[v] != 0.0)
            nz_total++;
    for (int64_t k = 0; k < s->anpat; k++)
        if (s->alpha[s->apat[k]] != 0.0)
            nz_pat++;
    assert(nz_total == nz_pat);
#endif
}

static void exact_weights(sx *s)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    for (int64_t i = 0; i < s->nrow; i++) {
        s->rho[i] = 1.0;
        int64_t nr = 0, words = 0;
        jm_lu_btran_sparse(&s->lu, s->rho, &s->work, s->rpat, &nr);
        double w = 0.0;
        if (nr * SPARSE_RHO_DEN <= s->nrow) {
            const int64_t np = jm_pattern_order(nr, s->rpat, s->rmark,
                                                s->nrow, &words);
            jm_work_add(&s->work, (nr + words + np) * JM_WORK_NONZERO);
            for (int64_t k = 0; k < np; k++) {
                const double v = s->rho[s->rpat[k]];
                w += v * v;
                s->rho[s->rpat[k]] = 0.0;
            }
        } else {
            for (int64_t k = 0; k < s->nrow; k++) {
                w += s->rho[k] * s->rho[k];
                s->rho[k] = 0.0;
            }
            jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
        }
        s->dse[i] = w > DSE_MIN ? w : DSE_MIN;
    }
    s->nrpat = -1;
}

static void build_pricing_row(sx *s, int64_t r)
{
    memset(s->rho, 0, (size_t)s->nrow * sizeof *s->rho);
    s->rho[r] = 1.0;

    int64_t nr = 0, words = 0;
    jm_lu_btran_sparse(&s->lu, s->rho, &s->work, s->rpat, &nr);
    if (nr * SPARSE_RHO_DEN <= s->nrow) {
        s->nrpat = jm_pattern_order(nr, s->rpat, s->rmark, s->nrow, &words);
        jm_work_add(&s->work, (nr + words + s->nrpat) * JM_WORK_NONZERO);
    } else {
        s->nrpat = -1;
    }

    price_all(s);

#ifndef NDEBUG

    if (s->nrpat >= 0) {
        int64_t nz_total = 0, nz_pat = 0;
        for (int64_t i = 0; i < s->nrow; i++)
            if (s->rho[i] != 0.0)
                nz_total++;
        for (int64_t k = 0; k < s->nrpat; k++)
            if (s->rho[s->rpat[k]] != 0.0)
                nz_pat++;
        assert(nz_total == nz_pat);
    }
#endif
}

static int64_t price_and_select(sx *s, int64_t r, bool below,
                                double violation, double *theta_dual)
{
    build_pricing_row(s, r);
    return dual_ratio_test(s, below, violation, theta_dual);
}

static void shift_to_feasible(sx *s, int64_t v)
{
    double need = 0.0;
    if (s->status[v] == JM_AT_LOWER) {
        if (s->d[v] < 0.0)
            need = -s->d[v];
    } else if (s->status[v] == JM_AT_UPPER) {
        if (s->d[v] > 0.0)
            need = -s->d[v];
    } else if (s->status[v] == JM_FREE) {
        need = -s->d[v];
    } else {
        return;
    }
    if (need == 0.0)
        return;

    const double before = s->cost[v];
    s->cost[v] += need;
    s->shift[v] += s->cost[v] - before;
    s->d[v] = 0.0;
}

static bool shifts_costs(const sx *s)
{
    return !s->in_phase1 && !s->m->cfg.force_primal;
}

static void update_dual(sx *s, int64_t v, int64_t q, double theta_dual)
{
    if (s->status[v] == JM_BASIC || v == q)
        return;
    s->d[v] -= theta_dual * s->alpha[v];
    if (shifts_costs(s))
        shift_to_feasible(s, v);
}

static inline bool devref_has(const sx *s, int64_t v)
{
    return (s->devref[v >> 6] >> (v & 63)) & 1u;
}

static void devex_reset(sx *s)
{
    memset(s->devref, 0, (size_t)((s->nvar + 63) / 64) * sizeof *s->devref);
    for (int64_t v = 0; v < s->nvar; v++) {
        s->devex[v] = 1.0;
        if (s->status[v] != JM_BASIC)
            s->devref[v >> 6] |= (uint64_t)1 << (v & 63);
    }
    s->devex_stale = false;
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
}

static void devex_update(sx *s, int64_t q, int64_t leaving, double alpha_q)
{
    const double wq = s->devex[q];
    double truew = devref_has(s, q) ? 1.0 : 0.0;
    if (s->ncpat >= 0) {
        for (int64_t k = 0; k < s->ncpat; k++) {
            const int64_t i = s->cpat[k];
            if (devref_has(s, s->basis[i]))
                truew += s->col[i] * s->col[i];
        }
        jm_work_add(&s->work, s->ncpat * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            if (devref_has(s, s->basis[i]))
                truew += s->col[i] * s->col[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }
    if (truew > DEVEX_RESET * wq || wq > DEVEX_RESET * truew) {
        s->devex_stale = true;
        s->n_weight_restart++;
        return;
    }
    const double inv = 1.0 / alpha_q;
    if (s->anpat < 0) {
        for (int64_t v = 0; v < s->nvar; v++) {
            if (s->status[v] == JM_BASIC || v == q)
                continue;
            const double ratio = s->alpha[v] * inv;
            const double w = ratio * ratio * wq;
            if (w > s->devex[v])
                s->devex[v] = w;
        }
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    } else {
        for (int64_t t = 0; t < s->anpat; t++) {
            const int64_t v = s->apat[t];
            if (s->status[v] == JM_BASIC || v == q)
                continue;
            const double ratio = s->alpha[v] * inv;
            const double w = ratio * ratio * wq;
            if (w > s->devex[v])
                s->devex[v] = w;
        }
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    }
    const double wl = wq * inv * inv;
    s->devex[leaving] = wl > 1.0 ? wl : 1.0;
}

static void pse_reset(sx *s)
{
    const jaos_model *m = s->m;
    for (int64_t v = 0; v < s->ncol; v++) {
        double n = 1.0;
        for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
            n += s->av[k] * s->av[k];
        s->devex[v] = n;
    }
    for (int64_t i = 0; i < s->nrow; i++)
        s->devex[s->ncol + i] = 2.0;
    jm_work_add(&s->work, (m->num_nz + s->nrow) * JM_WORK_NONZERO);
    s->devex_stale = false;
}

static void pse_exact(sx *s)
{
    double *col = jm_alloc_array(s->nrow > 0 ? s->nrow : 1, sizeof *col);
    if (col == nullptr)
        return;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC) {
            s->devex[v] = 1.0;
            continue;
        }
        var_column(s, v, col);
        jm_lu_ftran(&s->lu, col, &s->work);
        double n = 1.0;
        for (int64_t i = 0; i < s->nrow; i++)
            n += col[i] * col[i];
        s->devex[v] = n;
    }
    free(col);
    jm_work_add(&s->work, (int64_t)s->nvar * (s->nrow + 1) * JM_WORK_NONZERO);
    s->devex_stale = false;
    s->n_pse_exact++;
}

constexpr int64_t PSE_CHEAP_RESTARTS = 64;

static void pse_refresh(sx *s)
{
    if (s->in_phase1 || s->n_pse_cheap < PSE_CHEAP_RESTARTS) {
        s->n_pse_cheap += !s->in_phase1;
        pse_reset(s);
    } else {
        pse_exact(s);
    }
}

static void primal_weights_reset(sx *s)
{
    if (s->pse_on)
        pse_refresh(s);
    else
        devex_reset(s);
}

static void pse_update(sx *s, int64_t q, int64_t leaving, double alpha_q)
{
    double exact = 1.0;
    if (s->ncpat >= 0) {
        for (int64_t k = 0; k < s->ncpat; k++) {
            const double v = s->col[s->cpat[k]];
            exact += v * v;
        }
        jm_work_add(&s->work, s->ncpat * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            exact += s->col[i] * s->col[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }
    if (weight_drifted(s->devex[q], exact, DSE_DRIFT)) {
        s->devex_stale = true;
        s->n_weight_restart++;
        return;
    }

    double *tau = s->pse_tau;
    memcpy(tau, s->col, (size_t)s->nrow * sizeof *tau);
    jm_lu_btran(&s->lu, tau, &s->work);

    const jaos_model *m = s->m;
    double *sigma = s->sigma;
    memset(sigma, 0, (size_t)s->nvar * sizeof *sigma);
    int64_t touched = 0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double w = tau[i];
        if (w == 0.0)
            continue;
        sigma[s->ncol + i] = -w;
        for (int64_t p = m->ar_start[i]; p < m->ar_start[i + 1]; p++)
            sigma[m->ar_index[p]] += w * s->arv[p];
        touched += m->ar_start[i + 1] - m->ar_start[i] + 1;
    }
    jm_work_add(&s->work, touched * JM_WORK_NONZERO);

    const double inv = 1.0 / alpha_q;
    if (s->anpat < 0) {
        for (int64_t v = 0; v < s->nvar; v++) {
            if (s->status[v] == JM_BASIC || v == q || s->alpha[v] == 0.0)
                continue;
            const double ratio = s->alpha[v] * inv;
            const double w = s->devex[v] - 2.0 * ratio * sigma[v] +
                             ratio * ratio * exact;
            const double floor = 1.0 + ratio * ratio;
            s->devex[v] = w > floor ? w : floor;
        }
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    } else {
        for (int64_t t = 0; t < s->anpat; t++) {
            const int64_t v = s->apat[t];
            if (s->status[v] == JM_BASIC || v == q || s->alpha[v] == 0.0)
                continue;
            const double ratio = s->alpha[v] * inv;
            const double w = s->devex[v] - 2.0 * ratio * sigma[v] +
                             ratio * ratio * exact;
            const double floor = 1.0 + ratio * ratio;
            s->devex[v] = w > floor ? w : floor;
        }
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    }
    const double wl = exact * inv * inv;
    const double floor_l = 1.0 + inv * inv;
    s->devex[leaving] = wl > floor_l ? wl : floor_l;
}

static jaos_status pivot(sx *s, int64_t r, int64_t q, bool below,
                         double theta_dual, bool *took)
{
#ifndef NDEBUG

    s->dbg_piv_since_verify++;
#endif
    int64_t leaving = s->basis[r];
    double bound = below ? s->lo[leaving] : s->up[leaving];
    double alpha_q = s->alpha[q];

    var_column(s, q, s->raw);
    memcpy(s->col, s->raw, (size_t)s->nrow * sizeof *s->col);
    {
        int64_t nc = 0;
        jm_lu_ftran_sparse(&s->lu, s->col, &s->work, s->cpat, &nc);
        s->ncpat = nc * SPARSE_COL_DEN <= s->nrow ? nc : -1;
    }

    {
        double a = fabs(alpha_q), c = fabs(s->col[r]);
        double big = a > c ? a : c;
        if (big > 0.0 && fabs(alpha_q - s->col[r]) > LU_AGREE_TOL * big &&
            s->lu.n_updates > 0) {
            s->needs_refactor = true;
            s->n_stability++;
            *took = false;
            return JAOS_OK;
        }
    }
    *took = true;

    double theta_primal = (s->xb[r] - bound) / alpha_q;

    if (s->duals_dirty || s->anpat < 0) {
        for (int64_t v = 0; v < s->nvar; v++)
            update_dual(s, v, q, theta_dual);
        s->duals_dirty = false;
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    } else {
        for (int64_t t = 0; t < s->anpat; t++)
            update_dual(s, s->apat[t], q, theta_dual);
        jm_work_add(&s->work, s->anpat * JM_WORK_NONZERO);
    }
    s->d[leaving] = -theta_dual;
    s->d[q] = 0.0;

    if (s->devex_on) {
        if (s->pse_on)
            pse_update(s, q, leaving, alpha_q);
        else
            devex_update(s, q, leaving, alpha_q);
    }

    memcpy(s->tau, s->rho, (size_t)s->nrow * sizeof *s->tau);
    jm_lu_ftran(&s->lu, s->tau, &s->work);

    double exact = 0.0;
    if (s->nrpat >= 0) {
        for (int64_t k = 0; k < s->nrpat; k++) {
            double v = s->rho[s->rpat[k]];
            exact += v * v;
        }
        jm_work_add(&s->work, s->nrpat * JM_WORK_NONZERO);
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            exact += s->rho[i] * s->rho[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }

    const bool sparse_col = s->ncpat >= 0;
    if (jm_dse_update(s->nrow, s->dse, r, s->col, s->tau, exact, DSE_DRIFT,
                      sparse_col ? s->cpat : nullptr, s->ncpat)) {
        s->n_weight_restart++;
        if (s->dse_guess && ++s->n_guess_restart >= DSE_GUESS_RESTARTS) {
            s->dse_guess = false;
            s->dse_exact_pending = true;
        }
    }
    jm_work_add(&s->work,
                (sparse_col ? s->ncpat : s->nrow) * JM_WORK_NONZERO);

    double q_value = nonbasic_value(s, q);
    if (sparse_col) {
        for (int64_t k = 0; k < s->ncpat; k++) {
            int64_t i = s->cpat[k];
            s->xb[i] -= theta_primal * s->col[i];
        }
    } else {
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= theta_primal * s->col[i];
    }
    s->xb[r] = q_value + theta_primal;

    s->status[leaving] = below ? JM_AT_LOWER : JM_AT_UPPER;
    jm_nonbasic_insert(s->nbmark, leaving);
    s->where[leaving] = -1;
    s->basis[r] = q;
    s->status[q] = JM_BASIC;
    jm_nonbasic_remove(s->nbmark, q);
    s->where[q] = r;

    if (s->devex_on && s->devex_stale && !s->pse_on)
        primal_weights_reset(s);

    if (shifts_costs(s))
        shift_to_feasible(s, leaving);

    if (s->lu.n_updates >= REFACTOR_EVERY) {
        s->needs_refactor = true;
        return JAOS_OK;
    }
    jaos_status ust = jm_lu_update(&s->lu, r, s->raw, LU_UPDATE_TOL,
                                   &s->work);
    if (ust == JAOS_ERR_NUMERICAL || ust == JAOS_ERR_OUT_OF_MEMORY) {
        s->needs_refactor = true;
        return JAOS_OK;
    }
    return ust;
}

static void repair_dual_infeasibility(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC || s->fake[v] != NOT_FAKE)
            continue;

        double to;
        if (s->status[v] == JM_AT_LOWER && s->d[v] < 0.0)
            to = s->up[v];
        else if (s->status[v] == JM_AT_UPPER && s->d[v] > 0.0)
            to = s->lo[v];
        else
            continue;
        if (!isfinite(to))
            continue;

        double delta = to - nonbasic_value(s, v);
        var_column(s, v, s->col);
        for (int64_t i = 0; i < s->nrow; i++)
            s->col[i] *= delta;
        jm_lu_ftran(&s->lu, s->col, &s->work);

        bool safe = true;
        for (int64_t i = 0; i < s->nrow; i++) {
            double x = s->xb[i] - s->col[i];
            int64_t b = s->basis[i];
            if (x < s->lo[b] - s->primal_tol ||
                x > s->up[b] + s->primal_tol) {
                safe = false;
                break;
            }

            if ((s->fake[b] == FAKE_LO && x <= s->lo[b] + s->primal_tol) ||
                (s->fake[b] == FAKE_UP && x >= s->up[b] - s->primal_tol)) {
                safe = false;
                break;
            }
        }
        jm_work_add(&s->work, 2 * s->nrow * JM_WORK_NONZERO);
        if (!safe)
            continue;

        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= s->col[i];
        s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                   : JM_AT_LOWER;
    }
}

static bool repay_shifts(sx *s)
{
    bool any = false;
    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->cost[v] == s->cost0[v] && s->shift[v] == 0.0)
            continue;
        s->cost[v] = s->cost0[v];
        s->shift[v] = 0.0;
        any = true;
    }
    return any;
}

static bool shifts_outstanding(const sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++)
        if (s->cost[v] != s->cost0[v] || s->shift[v] != 0.0)
            return true;
    return false;
}

static void settle_shifts(sx *s)
{
    if (!repay_shifts(s))
        return;

    compute_duals(s, false);
    repair_dual_infeasibility(s);
}

static jaos_status run(sx *s, jaos_solve_status *out);

static double dual_breach(const sx *s, int64_t v)
{
    switch (s->status[v]) {
    case JM_AT_LOWER: return s->d[v] < -s->dual_tol ? -s->d[v] : 0.0;
    case JM_AT_UPPER: return s->d[v] > s->dual_tol ? s->d[v] : 0.0;
    case JM_FREE:     return fabs(s->d[v]) > s->dual_tol ? fabs(s->d[v]) : 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;
}

static double published_breach(const sx *s, int64_t v)
{
    const jaos_model *m = s->m;
    const double d = v < s->ncol ? s->d[v] / m->col_scale[v]
                                 : s->d[v] * m->row_scale[v - s->ncol];
    switch (s->status[v]) {
    case JM_AT_LOWER: return d < -s->dual_tol ? -d : 0.0;
    case JM_AT_UPPER: return d > s->dual_tol ? d : 0.0;
    case JM_FREE:     return fabs(d) > s->dual_tol ? fabs(d) : 0.0;
    case JM_BASIC:    break;
    }
    return 0.0;
}

static bool breached(const sx *s, int64_t v)
{
    return dual_breach(s, v) != 0.0 || published_breach(s, v) != 0.0;
}

static bool save_settled(sx *s)
{
    if (s->sav_status == nullptr) {
        s->sav_status = jm_alloc_array(s->nvar, sizeof *s->sav_status);
        s->sav_basis  = jm_alloc_array(s->nrow, sizeof *s->sav_basis);
        s->sav_lo     = jm_alloc_array(s->nvar, sizeof *s->sav_lo);
        s->sav_up     = jm_alloc_array(s->nvar, sizeof *s->sav_up);
        s->sav_fake   = jm_alloc_array(s->nvar, sizeof *s->sav_fake);
        if (!s->sav_status || !s->sav_basis || !s->sav_lo || !s->sav_up ||
            !s->sav_fake)
            return false;
    }
    memcpy(s->sav_status, s->status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->sav_basis, s->basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->sav_lo, s->lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->sav_up, s->up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->sav_fake, s->fake, (size_t)s->nvar * sizeof *s->fake);
    return true;
}

static double settled_objective(const sx *s)
{
#ifndef NDEBUG

    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif

    double sum = 0.0, comp = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double x = s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                                  : nonbasic_value(s, v);
        const double c = s->cost0[v];
        const double t = c * x;
        jm_obj_add(&sum, &comp, t);
        const double e = jm_two_product_residue(c, x, t);
        if (e != 0.0)
            jm_obj_add(&sum, &comp, e);
    }

    return (isfinite(sum) && isfinite(comp)) ? sum + comp : sum;
}

static double settled_dual_violation(const sx *s)
{
    double worst = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        double br = published_breach(s, v);
        if (br > worst)
            worst = br;
    }
    return worst;
}

static bool better_point(double tol, double dviol_a, double obj_a,
                         double dviol_b, double obj_b)
{
    const bool a_ok = dviol_a <= tol, b_ok = dviol_b <= tol;
    if (a_ok != b_ok)
        return a_ok;
    return a_ok ? obj_a < obj_b : dviol_a < dviol_b;
}

static bool save_best(sx *s)
{
    if (s->bst_status == nullptr) {
        s->bst_status = jm_alloc_array(s->nvar, sizeof *s->bst_status);
        s->bst_basis  = jm_alloc_array(s->nrow, sizeof *s->bst_basis);
        s->bst_lo     = jm_alloc_array(s->nvar, sizeof *s->bst_lo);
        s->bst_up     = jm_alloc_array(s->nvar, sizeof *s->bst_up);
        s->bst_fake   = jm_alloc_array(s->nvar, sizeof *s->bst_fake);
        if (!s->bst_status || !s->bst_basis || !s->bst_lo || !s->bst_up ||
            !s->bst_fake)
            return false;
    }
    memcpy(s->bst_status, s->status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->bst_basis, s->basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->bst_lo, s->lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->bst_up, s->up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->bst_fake, s->fake, (size_t)s->nvar * sizeof *s->fake);
    s->bst_obj = settled_objective(s);
    s->bst_dviol = settled_dual_violation(s);
    s->bst_valid = true;
    return true;
}

static jaos_status take_best_if_better(sx *s, bool *ok)
{
    *ok = true;
    if (!s->bst_valid ||
        !better_point(s->dual_tol, s->bst_dviol, s->bst_obj,
                      settled_dual_violation(s), settled_objective(s)))
        return JAOS_OK;

    repay_shifts(s);
    memcpy(s->status, s->bst_status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->basis, s->bst_basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->lo, s->bst_lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->up, s->bst_up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->fake, s->bst_fake, (size_t)s->nvar * sizeof *s->fake);

    for (int64_t v = 0; v < s->nvar; v++)
        s->where[v] = -1;
    for (int64_t i = 0; i < s->nrow; i++)
        s->where[s->basis[i]] = i;

    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    s->needs_refactor = true;
    set_verified(s, false);
    return refresh(s, ok, true);
}

static jaos_status restore_settled(sx *s, bool *ok)
{
    repay_shifts(s);
    memcpy(s->status, s->sav_status, (size_t)s->nvar * sizeof *s->status);
    memcpy(s->basis, s->sav_basis, (size_t)s->nrow * sizeof *s->basis);
    memcpy(s->lo, s->sav_lo, (size_t)s->nvar * sizeof *s->lo);
    memcpy(s->up, s->sav_up, (size_t)s->nvar * sizeof *s->up);
    memcpy(s->fake, s->sav_fake, (size_t)s->nvar * sizeof *s->fake);

    for (int64_t v = 0; v < s->nvar; v++)
        s->where[v] = -1;
    for (int64_t i = 0; i < s->nrow; i++)
        s->where[s->basis[i]] = i;
    jm_nonbasic_build(s->nvar, s->status, s->nbmark);

    s->needs_refactor = true;
    return refresh(s, ok, true);
}

static double column_traffic(const sx *s, int64_t v)
{
    double t = fabs(s->cost[v]);
    if (v >= s->ncol)
        return t + fabs(s->y[v - s->ncol]);

    const jaos_model *m = s->m;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        t += fabs(s->y[m->a_index[k]] * s->av[k]);
    return t;
}

static double alpha_traffic(sx *s, int64_t v)
{
    if (v >= s->ncol) {
        jm_work_add(&s->work, JM_WORK_NONZERO);
        return fabs(s->rho[v - s->ncol]);
    }
    const jaos_model *m = s->m;
    double t = 0.0;
    for (int64_t k = m->a_start[v]; k < m->a_start[v + 1]; k++)
        t += fabs(s->rho[m->a_index[k]] * s->av[k]);
    jm_work_add(&s->work, (m->a_start[v + 1] - m->a_start[v]) *
                          JM_WORK_NONZERO);
    return t;
}

static bool alpha_unusable(sx *s, int64_t q, double *min_alpha)
{
    const double a = fabs(s->alpha[q]);
    *min_alpha = PIVOT_MIN;
    if (a < PIVOT_MIN)
        return true;
    const double rel = PIVOT_MARGIN * DBL_EPSILON * alpha_traffic(s, q);
    if (a < rel) {
        *min_alpha = rel;
        return true;
    }
    return false;
}

static bool can_move(const sx *s, int64_t v)
{
    double wrong_way;
    switch (s->status[v]) {
    case JM_AT_LOWER: wrong_way = s->d[v] < 0.0 ? -s->d[v] : 0.0; break;
    case JM_AT_UPPER: wrong_way = s->d[v] > 0.0 ? s->d[v] : 0.0; break;
    default:          return false;
    }
    if (wrong_way == 0.0)
        return false;
    if (wrong_way <= NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v))
        return false;

    double other = s->status[v] == JM_AT_LOWER ? real_upper(s, v)
                                               : real_lower(s, v);
    if (!isfinite(other))
        return false;

    return breached(s, v);
}

static bool anything_to_move(const sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++)
        if (can_move(s, v))
            return true;
    return false;
}

static void arm_reentry(sx *s)
{
    for (int64_t v = 0; v < s->nvar; v++) {
        if (can_move(s, v)) {
            s->status[v] = s->status[v] == JM_AT_LOWER ? JM_AT_UPPER
                                                       : JM_AT_LOWER;
        } else if (dual_breach(s, v) != 0.0) {

            shift_to_feasible(s, v);
        }
    }
}

static bool wants_a_pivot(const sx *s, int64_t v)
{
    if (!breached(s, v))
        return false;
    double wrong_way = fabs(s->d[v]);
    if (wrong_way <= NOISE_MARGIN * DBL_EPSILON * column_traffic(s, v))
        return false;
    if (s->status[v] == JM_FREE)
        return true;
    return !isfinite(s->status[v] == JM_AT_LOWER ? real_upper(s, v)
                                                : real_lower(s, v));
}

static int64_t primal_apply_floor(sx *s, int64_t n, double cmax)
{
    const double rel = PIVOT_MARGIN * DBL_EPSILON * cmax;
    if (!(rel > PIVOT_MIN) || n == 0)
        return n;
    int64_t m = 0;
    for (int64_t k = 0; k < n; k++) {
        if (s->pden[k] < rel)
            continue;
        s->prow[m] = s->prow[k];
        s->pnum[m] = s->pnum[k];
        s->pden[m] = s->pden[k];
        m++;
    }
    jm_work_add(&s->work, n * JM_WORK_NONZERO);
    return m > 0 ? m : n;
}

static int64_t primal_pick(sx *s, int64_t n, bool bland)
{
    if (n <= 0)
        return -1;
#ifndef NDEBUG
    for (int64_t k = 0; k < n; k++)
        assert(s->pden[k] > 0.0 && s->pnum[k] >= 0.0);
#endif
    if (!bland) {
        const double width = PRIMAL_HARRIS_DELTA * s->primal_tol;

        assert(PRIMAL_HARRIS_DELTA > 0.0 && PRIMAL_HARRIS_DELTA <= 1.0);
        assert(width >= 0.0 && width <= s->primal_tol);
        const int64_t k = jm_harris_pick(n, s->pnum, s->pden, width);
        jm_work_add(&s->work, 2 * n * JM_WORK_NONZERO);
        return k;
    }
    int64_t best = -1;
    double best_step = HUGE_VAL;
    for (int64_t k = 0; k < n; k++) {
        const double t = s->pnum[k] / s->pden[k];
        if (jm_primal_row_wins(t, s->basis[s->prow[k]], best_step,
                               best >= 0 ? s->basis[s->prow[best]] : -1,
                               true)) {
            best_step = t;
            best = k;
        }
    }
    jm_work_add(&s->work, n * JM_WORK_NONZERO);
    return best;
}

static void snap_if_past(sx *s, int64_t r, bool below)
{
    const int64_t v = s->basis[r];
    const double bound = below ? real_lower(s, v) : real_upper(s, v);
    if (below ? s->xb[r] < bound : s->xb[r] > bound)
        s->xb[r] = bound;
}

static double primal_dir(const sx *s, int64_t q)
{
    return s->d[q] < 0.0 ? 1.0 : -1.0;
}

static int64_t primal_ratio_test(sx *s, int64_t q, double dir, bool bland,
                                 bool *below, double *step)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    int64_t n = 0;
    double cmax = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];
        const double amove = fabs(move);
        if (amove > cmax)
            cmax = amove;
        if (!(amove >= PIVOT_MIN))
            continue;

        const int64_t b = s->basis[i];
        const double limit = move < 0.0 ? real_lower(s, b) : real_upper(s, b);
        if (!isfinite(limit))
            continue;

        double dist = move > 0.0 ? limit - s->xb[i] : s->xb[i] - limit;
        if (dist < 0.0)
            dist = 0.0;
        s->prow[n] = i;
        s->pnum[n] = dist;
        s->pden[n] = amove;
        n++;
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    n = primal_apply_floor(s, n, cmax);
    const int64_t k = primal_pick(s, n, bland);
    if (k < 0) {
        *step = HUGE_VAL;
        return -1;
    }
    const int64_t r = s->prow[k];
    *below = -dir * s->col[r] < 0.0;
    *step = s->pnum[k] / s->pden[k];
    if (*step == 0.0)
        snap_if_past(s, r, *below);
    return r;
}

static jaos_status primal_cleanup(sx *s, int64_t *pivots)
{
    *pivots = 0;

    int64_t n = 0;
    for (int64_t v = 0; v < s->nvar; v++)
        if (wants_a_pivot(s, v))
            s->cand[n++] = v;

    for (int64_t k = 0; k < n; k++) {
        int64_t q = s->cand[k];
        if (s->status[q] == JM_BASIC)
            continue;

        if (s->cost[q] != s->cost0[q] || s->shift[q] != 0.0) {
            const double give_back = s->cost[q] - s->cost0[q];
            s->cost[q] = s->cost0[q];
            s->d[q] -= give_back;
            s->shift[q] = 0.0;
            s->duals_dirty = true;
        }
        if (!breached(s, q))
            continue;

        bool below = false;
        double step = 0.0;

        int64_t r = primal_ratio_test(s, q, primal_dir(s, q), false, &below,
                                      &step);
        if (r < 0)
            continue;

        build_pricing_row(s, r);

        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha))
            continue;

        bool took = false;
        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took) {

            break;
        }

        s->iters++;
        (*pivots)++;

        if (s->needs_refactor)
            break;
    }
    return JAOS_OK;
}

static jaos_status reenter_after_settling(sx *s, jaos_solve_status *stopped)
{

    s->bst_valid = false;
    if (!save_best(s))
        return JAOS_ERR_OUT_OF_MEMORY;

    const int64_t rounds = s->m->cfg.force_primal ? SETTLE_ROUNDS_PRIMAL
                                                  : SETTLE_ROUNDS;

    for (int64_t round = 0; round < rounds; round++) {

        if (!anything_to_move(s)) {

            if (!save_settled(s))
                return JAOS_ERR_OUT_OF_MEMORY;

            int64_t pivots = 0;
            jaos_status st = primal_cleanup(s, &pivots);
            if (st != JAOS_OK)
                return st;
            if (pivots == 0) {

                bool ok = false;
                st = take_best_if_better(s, &ok);
                if (st != JAOS_OK)
                    return st;
                return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
            }

            bool ok = false;
            set_verified(s, false);
            s->needs_refactor = true;
            st = refresh(s, &ok, true);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                st = restore_settled(s, &ok);
                if (st != JAOS_OK)
                    return st;
                if (!ok)
                    return JAOS_ERR_NUMERICAL;

                s->m->err[0] = '\0';
                return JAOS_OK;
            }
            settle_shifts(s);
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }
        if (!save_settled(s))
            return JAOS_ERR_OUT_OF_MEMORY;
        arm_reentry(s);

        set_verified(s, false);
        s->needs_refactor = true;

        jaos_solve_status again = JAOS_SOLVE_NOT_RUN;
        jaos_status st = run(s, &again);
        if (st != JAOS_OK)
            return st;

        if (again == JAOS_SOLVE_OPTIMAL) {
            settle_shifts(s);
            if (better_point(s->dual_tol, settled_dual_violation(s),
                             settled_objective(s), s->bst_dviol, s->bst_obj) &&
                !save_best(s))
                return JAOS_ERR_OUT_OF_MEMORY;
            continue;
        }

        bool ok = false;
        st = restore_settled(s, &ok);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return JAOS_ERR_NUMERICAL;
        settle_shifts(s);
        if (again == JAOS_SOLVE_WORK_LIMIT ||
            again == JAOS_SOLVE_TIME_LIMIT ||
            again == JAOS_SOLVE_INTERRUPTED) {

            *stopped = again;
            return JAOS_OK;
        }
        st = take_best_if_better(s, &ok);
        if (st != JAOS_OK)
            return st;
        return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
    }

    bool ok = false;
    jaos_status st = take_best_if_better(s, &ok);
    if (st != JAOS_OK)
        return st;
    return ok ? JAOS_OK : JAOS_ERR_NUMERICAL;
}

static bool held_by_an_invented_bound(const sx *s, int64_t j)
{
    if (s->fake[j] == FAKE_LO)
        return s->status[j] == JM_AT_LOWER && s->d[j] > s->dual_tol;
    if (s->fake[j] == FAKE_UP)
        return s->status[j] == JM_AT_UPPER && s->d[j] < -s->dual_tol;
    return false;
}

static bool improves_without_limit(sx *s, int64_t j)
{
    var_column(s, j, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;

    bool unlimited = true;
    for (int64_t i = 0; i < s->nrow; i++) {
        double step = sgn * s->col[i];
        if (fabs(step) < PIVOT_MIN)
            continue;
        int64_t b = s->basis[i];
        double limit = step > 0.0 ? real_upper(s, b) : real_lower(s, b);
        if (isfinite(limit)) {
            unlimited = false;
            break;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return unlimited;
}

static bool combined_improves_without_limit(sx *s)
{
    const jaos_model *m = s->m;

    memset(s->col, 0, (size_t)s->nrow * sizeof *s->col);
    for (int64_t j = 0; j < s->ncol; j++) {
        if (!held_by_an_invented_bound(s, j))
            continue;
        const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++)
            s->col[m->a_index[k]] += sgn * s->av[k];
        jm_work_add(&s->work,
                    (m->a_start[j + 1] - m->a_start[j]) * JM_WORK_NONZERO);
    }
    jm_lu_ftran(&s->lu, s->col, &s->work);

    bool unlimited = true;
    for (int64_t i = 0; i < s->nrow; i++) {
        const double step = s->col[i];
        if (fabs(step) < PIVOT_MIN)
            continue;
        const int64_t b = s->basis[i];
        const double limit = step > 0.0 ? real_upper(s, b) : real_lower(s, b);
        if (isfinite(limit)) {
            unlimited = false;
            break;
        }
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    return unlimited;
}

static void ray_basics(sx *s, double bsign)
{
    memset(s->uray, 0, (size_t)s->ncol * sizeof *s->uray);
    for (int64_t i = 0; i < s->nrow; i++) {
        if (fabs(s->col[i]) < PIVOT_MIN)
            continue;
        const int64_t b = s->basis[i];
        if (b < s->ncol)
            s->uray[b] = bsign * s->col[i];
    }
}

static jaos_solve_status classify_optimum(sx *s)
{
    int64_t blocked = -1;

    for (int64_t j = 0; j < s->ncol; j++) {
        if (!held_by_an_invented_bound(s, j))
            continue;
        if (improves_without_limit(s, j)) {

            const double sgn = (s->fake[j] == FAKE_LO) ? 1.0 : -1.0;
            ray_basics(s, sgn);
            s->uray[j] = -sgn;
            s->uray_ok = true;
            return JAOS_SOLVE_UNBOUNDED;
        }
        if (blocked < 0)
            blocked = j;
    }

    if (blocked < 0)
        return JAOS_SOLVE_OPTIMAL;

    if (combined_improves_without_limit(s)) {

        ray_basics(s, 1.0);
        for (int64_t j = 0; j < s->ncol; j++)
            if (held_by_an_invented_bound(s, j))
                s->uray[j] = (s->fake[j] == FAKE_LO) ? -1.0 : 1.0;
        s->uray_ok = true;
        return JAOS_SOLVE_UNBOUNDED;
    }

    jm_set_err(s->m, "column %lld improves past the bound dual phase 1 lent "
                     "it; moving that column alone runs into a constraint, "
                     "and so does the one direction that moves several "
                     "columns together at unit rate; the model is therefore "
                     "either bounded at an optimum beyond the reach of this "
                     "phase 1, or unbounded along a direction neither test "
                     "tries, and this verdict decides neither",
                     (long long)blocked);
    return JAOS_SOLVE_NUMERICAL_ERROR;
}

static double elapsed_seconds(const sx *s)
{
    return jm_monotonic_seconds() - s->started;
}

static bool out_of_time(const sx *s)
{
    if (s->m->cfg.time_limit <= 0.0)
        return false;
    return elapsed_seconds(s) >= s->m->cfg.time_limit;
}

static double primal_worst_violation(const sx *s)
{
    double worst = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        double viol = 0.0;
        if (isfinite(lo) && lo - s->xb[i] > viol)
            viol = lo - s->xb[i];
        if (isfinite(up) && s->xb[i] - up > viol)
            viol = s->xb[i] - up;
        if (viol > worst)
            worst = viol;
    }
    return worst;
}

static int64_t primal_price(sx *s, double *total)
{

    if (!s->bland &&
        s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
        s->bland = true;
        s->n_bland++;
        jm_log(s->m, JAOS_LOG_DETAIL,
               "iter %lld: no progress for %lld iterations, switching to "
               "Bland's rule", (long long)s->iters,
               (long long)(s->iters - s->last_gain));
    }

    int64_t best = -1;
    double best_breach = 0.0;
    double sum = 0.0;

    for (int64_t v = 0; v < s->nvar; v++) {
        if (s->status[v] == JM_BASIC)
            continue;
        if (s->lo[v] == s->up[v])
            continue;
        const double breach = dual_breach(s, v);
        if (breach == 0.0)
            continue;
        sum += breach;

        if (s->bland) {
            if (best < 0)
                best = v;
            continue;
        }
        const double score = s->devex_on ? breach * breach / s->devex[v]
                                         : breach;
        if (score > best_breach) {
            best_breach = score;
            best = v;
        }
    }
    jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);
    *total = sum;
    return best;
}

static void primal_move_to(sx *s, int64_t q, double delta, jm_var_status to)
{
#ifndef NDEBUG

    {
        double *chk = s->dbg_col;
        const jm_work saved = s->work;
        var_column(s, q, chk);
        jm_lu_ftran(&s->lu, chk, &s->work);
        s->work = saved;
        assert(memcmp(chk, s->col, (size_t)s->nrow * sizeof *chk) == 0);
    }
#endif
    for (int64_t i = 0; i < s->nrow; i++)
        s->xb[i] -= delta * s->col[i];
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    s->status[q] = to;
}

static void primal_bound_flip(sx *s, int64_t q, double delta)
{
    primal_move_to(s, q, delta,
                   s->status[q] == JM_AT_LOWER ? JM_AT_UPPER : JM_AT_LOWER);
}

static jaos_status retire_one_loan(sx *s, int64_t j, bool *off)
{
    *off = false;

    const bool lent_low = s->fake[j] == FAKE_LO;
    const double dir = lent_low ? 1.0 : -1.0;
    const double target = lent_low ? s->up[j] : s->lo[j];
    const double from = nonbasic_value(s, j);

    bool below = false;
    double step = 0.0;
    const int64_t r = primal_ratio_test(s, j, dir, false, &below, &step);
    const double reach = isfinite(target) ? fabs(target - from) : HUGE_VAL;

    if (r >= 0 && step < reach) {
        build_pricing_row(s, r);
        double min_alpha = 0.0;
        if (alpha_unusable(s, j, &min_alpha))
            return JAOS_OK;
        bool took = false;
        const jaos_status st = pivot(s, r, j, below, s->d[j] / s->alpha[j],
                                     &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            return JAOS_OK;
        s->iters++;
    } else if (isfinite(target)) {
        primal_bound_flip(s, j, target - from);
    } else {
        primal_move_to(s, j, -from, JM_FREE);
    }

    assert(s->status[j] != (lent_low ? JM_AT_LOWER : JM_AT_UPPER));
    if (lent_low)
        s->lo[j] = -HUGE_VAL;
    else
        s->up[j] = HUGE_VAL;
    s->fake[j] = NOT_FAKE;
    set_verified(s, false);
    *off = true;
    return JAOS_OK;
}

static bool rests_on_a_loan(const sx *s, int64_t j)
{
    return (s->fake[j] == FAKE_LO && s->status[j] == JM_AT_LOWER) ||
           (s->fake[j] == FAKE_UP && s->status[j] == JM_AT_UPPER);
}

static double loan_reach(const sx *s, int64_t j)
{
    const double from = nonbasic_value(s, j);
    const double target = s->fake[j] == FAKE_LO ? s->up[j] : s->lo[j];
    return isfinite(target) ? fabs(target - from) : fabs(from);
}

static double objective_traffic(const sx *s)
{
    double t = 0.0;
    for (int64_t v = 0; v < s->nvar; v++) {
        const double x = s->status[v] == JM_BASIC ? s->xb[s->where[v]]
                                                  : nonbasic_value(s, v);
        t += fabs(s->cost0[v] * x);
    }
    return isfinite(t) ? t : 0.0;
}

static bool loan_moves_the_objective(const sx *s, int64_t j, double bar)
{
    return fabs(s->d[j]) * loan_reach(s, j) > bar;
}

static jaos_status retirement_undone(sx *s, int64_t retired, double dviol,
                                     double pviol)
{
    bool ok = false;
    const jaos_status st = restore_settled(s, &ok);
    if (st != JAOS_OK)
        return st;
    if (!ok)
        return JAOS_ERR_NUMERICAL;
    s->m->err[0] = '\0';
    settle_shifts(s);
    jm_log(s->m, JAOS_LOG_DETAIL,
           "%lld lent bounds retired and put back: the point that came out "
           "breaches its dual signs by %.6g and its own bounds by %.6g",
           (long long)retired, dviol, pviol);
    return JAOS_OK;
}

static jaos_status retire_lent_bounds(sx *s)
{
    bool any = false;
    for (int64_t j = 0; j < s->ncol && !any; j++)
        any = rests_on_a_loan(s, j);
    if (!any)
        return JAOS_OK;

    if (!save_settled(s))
        return JAOS_ERR_OUT_OF_MEMORY;

    int64_t retired = 0, declined = 0, priced = 0;

    for (int pass = 0; pass < 2; pass++) {
        bool rebuilt = false;
        for (int64_t j = 0; j < s->ncol; j++) {
            if (!rests_on_a_loan(s, j))
                continue;

            const double bar = DBL_EPSILON * objective_traffic(s);
            if (loan_moves_the_objective(s, j, bar)) {
                if (pass == 0)
                    priced++;
                continue;
            }
            bool off = false;
            const jaos_status st = retire_one_loan(s, j, &off);
            if (st != JAOS_OK)
                return st;
            if (off)
                retired++;
            else
                declined++;

            if (s->needs_refactor) {
                bool ok = false;
                const jaos_status rst = refresh(s, &ok, true);
                if (rst != JAOS_OK)
                    return rst;
                rebuilt = true;
                if (!ok)
                    return retirement_undone(s, retired, HUGE_VAL, HUGE_VAL);
            }
        }
        if (!rebuilt)
            break;
    }

    {
        bool ok = false;
        s->needs_refactor = true;
        const jaos_status st = refresh(s, &ok, true);
        if (st != JAOS_OK)
            return st;
        if (!ok)
            return retirement_undone(s, retired, HUGE_VAL, HUGE_VAL);
    }

    settle_shifts(s);

    const double dviol = settled_dual_violation(s);
    const double pviol = primal_worst_violation(s);
    if (dviol != 0.0 || pviol > s->primal_tol)
        return retirement_undone(s, retired, dviol, pviol);

    jm_log(s->m, JAOS_LOG_DETAIL,
           "%lld lent bounds retired, %lld declined by the pricing row, "
           "%lld left because moving them would move the objective; worst "
           "bound breach %.6g", (long long)retired, (long long)declined,
           (long long)priced, pviol);
    return JAOS_OK;
}

static double primal_phase1_costs(sx *s)
{

    const int64_t cleared = s->n_c1_at;
    for (int64_t k = 0; k < cleared; k++)
        s->c1[s->c1_at[k]] = 0.0;
    s->n_c1_at = 0;
#ifndef NDEBUG

    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->c1[v] == 0.0);
#endif

    double total = 0.0;
    for (int64_t i = 0; i < s->nrow; i++) {
        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        if (isfinite(lo) && s->xb[i] < lo - s->primal_tol) {
            s->c1[v] = -1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += lo - s->xb[i];
        } else if (isfinite(up) && s->xb[i] > up + s->primal_tol) {
            s->c1[v] = 1.0;
            s->c1_at[s->n_c1_at++] = v;
            total += s->xb[i] - up;
        }
    }

    assert(s->n_c1_at <= s->nrow);

    jm_work_add(&s->work, (cleared + s->nrow) * JM_WORK_NONZERO);
    return total;
}

static void primal_phase1_duals(sx *s)
{

    assert(s->cost != s->c1);
    double *real_cost = s->cost;
    s->cost = s->c1;
    compute_duals(s, false);
    s->cost = real_cost;
}

static bool phase1_lands_low(const sx *s, int64_t i, double move)
{
    const int64_t v = s->basis[i];
    const double lo = real_lower(s, v), up = real_upper(s, v);
    if (isfinite(lo) && s->xb[i] < lo - s->primal_tol)
        return true;
    if (isfinite(up) && s->xb[i] > up + s->primal_tol)
        return false;
    return move < 0.0;
}

static int64_t primal_phase1_ratio(sx *s, int64_t q, bool bland, bool *below,
                                   double *step)
{
    var_column(s, q, s->col);
    jm_lu_ftran(&s->lu, s->col, &s->work);

    const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
    int64_t n = 0;
    double cmax = 0.0;

    for (int64_t i = 0; i < s->nrow; i++) {
        const double move = -dir * s->col[i];
        const double amove = fabs(move);
        if (amove > cmax)
            cmax = amove;
        if (!(amove >= PIVOT_MIN))
            continue;

        const int64_t v = s->basis[i];
        const double lo = real_lower(s, v), up = real_upper(s, v);
        const bool under = isfinite(lo) && s->xb[i] < lo - s->primal_tol;
        const bool over  = isfinite(up) && s->xb[i] > up + s->primal_tol;

        double limit;
        assert(phase1_lands_low(s, i, move) == (under || (!over && move < 0.0)));
        if (under) {
            if (move < 0.0)
                continue;
            limit = lo;
        } else if (over) {
            if (move > 0.0)
                continue;
            limit = up;
        } else {
            limit = move < 0.0 ? lo : up;
            if (!isfinite(limit))
                continue;
        }

        double dist = move > 0.0 ? limit - s->xb[i] : s->xb[i] - limit;
        if (dist < 0.0)
            dist = 0.0;
        s->prow[n] = i;
        s->pnum[n] = dist;
        s->pden[n] = amove;
        n++;
    }
    jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);

    n = primal_apply_floor(s, n, cmax);
    const int64_t k = primal_pick(s, n, bland);
    if (k < 0) {
        *step = HUGE_VAL;
        return -1;
    }
    const int64_t r = s->prow[k];
    *step = s->pnum[k] / s->pden[k];
    *below = phase1_lands_low(s, r, -dir * s->col[r]);
    if (*step == 0.0)
        snap_if_past(s, r, *below);
    return r;
}

static jaos_status run_primal_phase1(sx *s, jaos_solve_status *out,
                                     bool *feasible)
{
    *feasible = false;

    if (s->c1 == nullptr || s->c1_at == nullptr) {
        free(s->c1);    s->c1 = nullptr;
        free(s->c1_at); s->c1_at = nullptr;
        s->c1 = jm_calloc_array(s->nvar, sizeof *s->c1);
        s->c1_at = jm_alloc_array(s->nrow, sizeof *s->c1_at);
        s->n_c1_at = 0;
        if (s->c1 == nullptr || s->c1_at == nullptr)
            return JAOS_ERR_OUT_OF_MEMORY;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);
    const int64_t entered = s->iters;
    double best_total = HUGE_VAL;

    s->last_gain = s->iters;
    s->bland = false;

    for (;;) {

        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }

        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations in the primal phase 1 (%lld into the "
                             "solve, against a shared cap of %lld), the last "
                             "%lld without the total infeasibility improving; "
                             "this is a JAOS defect",
                       (long long)(s->iters - entered),
                       (long long)s->iters, (long long)iter_cap,
                       (long long)(s->iters - s->last_gain));
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            bool ok = false;
            jaos_status st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok) {

                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }
        if (s->devex_on && s->pse_on && s->devex_stale) {
            pse_refresh(s);
            if (s->devex_stale)
                return JAOS_ERR_OUT_OF_MEMORY;
        }

        const double total = primal_phase1_costs(s);

        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "phase 1, iter %lld: infeasibility %.6g, work %lld",
                   (long long)s->iters, total, (long long)s->work.units);
        if (total == 0.0) {
            *feasible = true;
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "phase 1 reached a feasible point in %lld iterations",
                   (long long)(s->iters - entered));
            return JAOS_OK;
        }
        if (total < best_total) {
            best_total = total;
            s->infeas_best = total;
            s->last_gain = s->iters;
            s->bland = false;
        }

        if (!isfinite(total) ||
            (total > best_total &&
             total / best_total > 1.0 + PHASE1_RISE_MAX)) {

            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "the primal phase 1's total infeasibility stands at "
                       "%.6g at iteration %lld, %.6g times its own best of "
                       "%.6g, on a freshly computed point; the basis it is "
                       "pivoting on is too ill-conditioned for another pivot "
                       "to repair the start",
                       total, (long long)s->iters, total / best_total,
                       best_total);
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        if (!s->bland &&
            s->iters - s->last_gain > STALL_FACTOR * (s->nrow + s->ncol + 1)) {
            s->bland = true;
            s->n_bland++;
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "iter %lld: the primal phase 1 has not reduced its "
                   "infeasibility for %lld iterations, switching to Bland's "
                   "rule", (long long)s->iters,
                   (long long)(s->iters - s->last_gain));
        }

        primal_phase1_duals(s);

        int64_t q = -1;
        double best_score = 0.0;
        for (int64_t v = 0; v < s->nvar; v++) {
            if (s->status[v] == JM_BASIC || s->lo[v] == s->up[v])
                continue;
            double gain;
            switch (s->status[v]) {
            case JM_AT_LOWER: gain = -s->d[v]; break;
            case JM_AT_UPPER: gain =  s->d[v]; break;
            case JM_FREE:     gain = fabs(s->d[v]); break;
            default:          continue;
            }
            if (gain <= s->dual_tol)
                continue;

            if (s->bland) {
                if (q < 0)
                    q = v;
                continue;
            }
            const double score = s->devex_on ? gain * gain / s->devex[v]
                                             : gain;
            if (score > best_score) {
                best_score = score;
                q = v;
            }
        }
        jm_work_add(&s->work, s->nvar * JM_WORK_NONZERO);

        if (q < 0) {

            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {

                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "the primal phase 1 cannot reduce a total bound "
                       "violation of %.6g any further, on a freshly computed "
                       "point; reading that as an infeasible model needs the "
                       "proof D19 requires, and the columns may be held by "
                       "bounds dual phase 1 invented rather than by the "
                       "model", total);
            return JAOS_ERR_NUMERICAL;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_phase1_ratio(s, q, s->bland, &below, &step);

        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);
                    set_verified(s, false);
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {

            if (!verified_fresh(s)) {
                bool okv = false;
                const jaos_status stv = refresh(s, &okv, true);
                if (stv != JAOS_OK)
                    return stv;
                if (!okv) {

                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            jm_set_err(s->m,
                       "column %lld reduces the primal phase 1's objective "
                       "and no declared bound stops it, on a freshly computed "
                       "point, which cannot happen on an objective bounded "
                       "below by zero; this is a JAOS defect", (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha)) {
            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }

            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld of the primal "
                       "phase 1 on a freshly built factorization, against a "
                       "floor of %.6g; this is a JAOS defect",
                       (long long)q, s->alpha[q], (long long)r, min_alpha);
            return JAOS_ERR_NUMERICAL;
        }

        set_verified(s, false);
        bool took = false;

        jaos_status st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            continue;

        s->iters++;
        s->n_primal_iters++;
    }
}

static jaos_status run_primal(sx *s, jaos_solve_status *out)
{
    s->dinfeas_best = HUGE_VAL;
    s->last_gain = s->iters;
    s->bland = false;

    s->infeas_best = HUGE_VAL;

    s->shift_pending = false;

    s->devex_on = !s->m->cfg.primal_dantzig;
    s->pse_on = s->devex_on && !s->m->cfg.primal_devex;
    if (s->pse_on)
        s->devex_stale = true;
    else if (s->devex_on)
        primal_weights_reset(s);

    bool ok = false;
    jaos_status st = refresh(s, &ok, false);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    if (primal_worst_violation(s) > s->primal_tol) {
        bool feasible = false;
        const int64_t phase1_entered = s->iters;
        s->in_phase1 = true;
        st = run_primal_phase1(s, out, &feasible);
        s->in_phase1 = false;

        s->n_phase1_iters = s->iters - phase1_entered;
        if (st != JAOS_OK)
            return st;
        if (!feasible)
            return JAOS_OK;

        s->needs_refactor = true;
        bool ok2 = false;
        st = refresh(s, &ok2, false);
        if (st != JAOS_OK)
            return st;
        if (!ok2) {
            *out = JAOS_SOLVE_NUMERICAL_ERROR;
            return JAOS_OK;
        }

        const double left = primal_worst_violation(s);
        if (left > s->primal_tol) {
            jm_set_err(s->m,
                       "the primal phase 1 returned with a declared bound "
                       "still violated by %.6g; this is a JAOS defect", left);
            return JAOS_ERR_NUMERICAL;
        }

        s->last_gain = s->iters;
        s->bland = false;
        s->dinfeas_best = HUGE_VAL;
        if (s->pse_on)
            s->devex_stale = true;
        else if (s->devex_on)
            primal_weights_reset(s);
    }

    s->infeas_best = 0.0;

    const int64_t phase2_entered = s->iters;
    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }
        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {
            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "primal phase-2 iterations (%lld into the solve, "
                             "%lld of them phase 1, against a shared cap of "
                             "%lld), the last %lld without the total dual "
                             "infeasibility improving%s, %lld pivots declined "
                             "on factorization disagreement; this is a JAOS "
                             "defect",
                       (long long)(s->iters - phase2_entered),
                       (long long)s->iters, (long long)phase2_entered,
                       (long long)iter_cap,
                       (long long)(s->iters - s->last_gain),
                       s->bland ? ", under Bland's rule" : "",
                       (long long)s->n_stability);
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }
        if (s->devex_on && s->pse_on && s->devex_stale) {
            pse_refresh(s);
            if (s->devex_stale)
                return JAOS_ERR_OUT_OF_MEMORY;
        }

        double total = 0.0;
        int64_t q = primal_price(s, &total);

        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best dual infeasibility %.6g, work %lld",
                   (long long)s->iters, s->dinfeas_best,
                   (long long)s->work.units);

        if (q >= 0 && total < s->dinfeas_best) {
            s->dinfeas_best = total;
            s->last_gain = s->iters;
            s->bland = false;
        }

        if (q < 0) {

            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }
            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        bool below = false;
        double step = 0.0;
        int64_t r = primal_ratio_test(s, q, primal_dir(s, q), s->bland, &below,
                                      &step);

        {
            const double other = s->d[q] < 0.0 ? real_upper(s, q)
                                               : real_lower(s, q);
            if (isfinite(other)) {
                const double delta = other - nonbasic_value(s, q);
                if (fabs(delta) <= step) {
                    primal_bound_flip(s, q, delta);

                    set_verified(s, false);
                    s->iters++;
                    s->n_primal_iters++;
                    continue;
                }
            }
        }

        if (r < 0) {

            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }

            if (!shifts_outstanding(s)) {

                const double dir = s->d[q] < 0.0 ? 1.0 : -1.0;
                ray_basics(s, -dir);
                if (q < s->ncol)
                    s->uray[q] = dir;
                s->uray_ok = true;
                *out = JAOS_SOLVE_UNBOUNDED;
                return JAOS_OK;
            }
            jm_set_err(s->m,
                       "column %lld improves and no declared bound stops it, "
                       "on a freshly computed point, but a cost this solve "
                       "borrowed is still outstanding: the reduced cost that "
                       "says the column improves belongs to a shifted "
                       "objective, and a ray of that is not the ray D19 "
                       "requires of the model's own",
                       (long long)q);
            return JAOS_ERR_NUMERICAL;
        }

        build_pricing_row(s, r);
        double min_alpha = 0.0;
        if (alpha_unusable(s, q, &min_alpha)) {

            if (s->lu.n_updates > 0) {
                s->needs_refactor = true;
                s->n_stability++;
                continue;
            }
            jm_set_err(s->m,
                       "column %lld prices at %.6g in row %lld on a freshly "
                       "built factorization, against a floor of %.6g, which "
                       "no pivot can use; this is a JAOS defect",
                       (long long)q, s->alpha[q], (long long)r, min_alpha);
            return JAOS_ERR_NUMERICAL;
        }

        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, s->d[q] / s->alpha[q], &took);
        if (st != JAOS_OK)
            return st;
        if (!took)
            continue;

        s->iters++;
        s->n_primal_iters++;
    }
}

static jaos_status run(sx *s, jaos_solve_status *out)
{
    s->devex_on = false;
    s->infeas_best = HUGE_VAL;
    s->last_gain = s->iters;
    s->bland = false;

    bool ok = false;
    jaos_status st = refresh(s, &ok, false);
    if (st != JAOS_OK)
        return st;
    if (!ok) {
        *out = JAOS_SOLVE_NUMERICAL_ERROR;
        return JAOS_OK;
    }

    const int64_t iter_cap = ITER_SANITY_FACTOR * (s->nrow + s->ncol + 1);

    for (;;) {
        if (s->m->cfg.work_limit > 0 && s->work.units >= s->m->cfg.work_limit) {
            *out = JAOS_SOLVE_WORK_LIMIT;
            return JAOS_OK;
        }
        if (s->iters % TIME_CHECK_EVERY == 0 && out_of_time(s)) {
            *out = JAOS_SOLVE_TIME_LIMIT;
            return JAOS_OK;
        }

        if (s->m->cfg.progress_cb != nullptr &&
            s->iters % PROGRESS_EVERY == 0) {
            const jaos_progress p = {
                .iterations = s->iters,
                .work_units = s->work.units,
                .primal_infeasibility = s->infeas_best,
            };
            if (s->m->cfg.progress_cb(&p, s->m->cfg.progress_user) ==
                JAOS_CALLBACK_STOP) {
                *out = JAOS_SOLVE_INTERRUPTED;
                return JAOS_OK;
            }
        }
        if (s->iters > iter_cap) {

            jm_set_err(s->m, "internal iteration guard tripped after %lld "
                             "iterations, the last %lld without the total "
                             "infeasibility improving%s, %lld pivots declined "
                             "on factorization disagreement; this is a JAOS "
                             "defect",
                       (long long)s->iters,
                       (long long)(s->iters - s->last_gain),
                       s->bland ? ", under Bland's rule" : "",
                       (long long)s->n_stability);
            return JAOS_ERR_NUMERICAL;
        }

        if (s->needs_refactor) {
            st = refresh(s, &ok, false);
            if (st != JAOS_OK)
                return st;
            if (!ok) {
                *out = JAOS_SOLVE_NUMERICAL_ERROR;
                return JAOS_OK;
            }
        }

        if (s->dse_exact_pending) {
            exact_weights(s);
            s->dse_exact_pending = false;
            jm_log(s->m, JAOS_LOG_DETAIL,
                   "iter %lld: the starting weights drifted %lld times; "
                   "replaced by exact ones", (long long)s->iters,
                   (long long)s->n_guess_restart);
        }

        bool below = false;
        double violation = 0.0;
        int64_t r = price_row(s, &below, &violation);

        if (s->iters % LOG_EVERY == 0)
            jm_log(s->m, JAOS_LOG_PROGRESS,
                   "iter %lld: best infeasibility %.6g, work %lld",
                   (long long)s->iters, s->infeas_best,
                   (long long)s->work.units);
        if (r < 0) {

            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }

            *out = JAOS_SOLVE_OPTIMAL;
            return JAOS_OK;
        }

        double theta_dual = 0.0;
        int64_t q = price_and_select(s, r, below, violation, &theta_dual);
        if (q < 0) {

            if (!verified_fresh(s)) {
                st = refresh(s, &ok, true);
                if (st != JAOS_OK)
                    return st;
                if (!ok) {
                    *out = JAOS_SOLVE_NUMERICAL_ERROR;
                    return JAOS_OK;
                }
                set_verified(s, true);
                continue;
            }

            s->farkas_sign = below ? -1.0 : 1.0;
            s->farkas_basic = s->basis[r];
            *out = JAOS_SOLVE_INFEASIBLE;
            return JAOS_OK;
        }

        set_verified(s, false);
        bool took = false;
        st = pivot(s, r, q, below, theta_dual, &took);
        if (st != JAOS_OK)
            return st;

        if (!took)
            continue;

        s->iters++;
    }
}

jaos_status jm_model_ensure_solution_arrays(jaos_model *m)
{
    if (m->sol_col != nullptr && m->sol_row != nullptr &&
        m->sol_dual != nullptr && m->sol_redcost != nullptr &&
        m->sol_col_status != nullptr && m->sol_row_status != nullptr &&
        m->sol_farkas != nullptr && m->sol_ray != nullptr)
        return JAOS_OK;

    free(m->sol_col);        m->sol_col = nullptr;
    free(m->sol_row);        m->sol_row = nullptr;
    free(m->sol_dual);       m->sol_dual = nullptr;
    free(m->sol_redcost);    m->sol_redcost = nullptr;
    free(m->sol_col_status); m->sol_col_status = nullptr;
    free(m->sol_row_status); m->sol_row_status = nullptr;
    free(m->sol_farkas);     m->sol_farkas = nullptr;
    free(m->sol_ray);        m->sol_ray = nullptr;

    m->sol_col     = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_row     = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_dual    = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_redcost = jm_alloc_array(m->num_col, sizeof(double));
    m->sol_col_status = jm_alloc_array(m->num_col, sizeof(jaos_basis_status));
    m->sol_row_status = jm_alloc_array(m->num_row, sizeof(jaos_basis_status));
    m->sol_farkas  = jm_alloc_array(m->num_row, sizeof(double));
    m->sol_ray     = jm_alloc_array(m->num_col, sizeof(double));
    if (!m->sol_col || !m->sol_row || !m->sol_dual || !m->sol_redcost ||
        !m->sol_col_status || !m->sol_row_status || !m->sol_farkas ||
        !m->sol_ray) {
        free(m->sol_col);        m->sol_col = nullptr;
        free(m->sol_row);        m->sol_row = nullptr;
        free(m->sol_dual);       m->sol_dual = nullptr;
        free(m->sol_redcost);    m->sol_redcost = nullptr;
        free(m->sol_col_status); m->sol_col_status = nullptr;
        free(m->sol_row_status); m->sol_row_status = nullptr;
        free(m->sol_farkas);     m->sol_farkas = nullptr;
        free(m->sol_ray);        m->sol_ray = nullptr;
        return JAOS_ERR_OUT_OF_MEMORY;
    }
    return JAOS_OK;
}

static double published(double v)
{
    return v == 0.0 ? 0.0 : v;
}

static jaos_basis_status published_status(jm_var_status st)
{
    switch (st) {
    case JM_BASIC:    return JAOS_BASIS_BASIC;
    case JM_AT_LOWER: return JAOS_BASIS_AT_LOWER;
    case JM_AT_UPPER: return JAOS_BASIS_AT_UPPER;
    case JM_FREE:     return JAOS_BASIS_FREE;
    }
    return JAOS_BASIS_BASIC;
}

static void polish_unscaled(sx *s)
{
    const jaos_model *m = s->m;
    const double *rho = m->row_scale, *gamma = m->col_scale;
    double *r = s->raw, *comp = s->resc;

    for (int64_t round = 0; round < POLISH_ROUNDS; round++) {
        memset(comp, 0, (size_t)s->nrow * sizeof *comp);
        for (int64_t i = 0; i < s->nrow; i++)
            r[i] = -var_value(s, s->ncol + i) / rho[i];
        int64_t nz = 0;
        for (int64_t j = 0; j < s->ncol; j++) {
            const double xj = gamma[j] * var_value(s, j);
            if (xj == 0.0)
                continue;
            for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
                const int64_t i = m->a_index[k];
                const double t = m->a_value[k] * xj;
                const double a = r[i], u = a + t;
                comp[i] += (fabs(a) >= fabs(t)) ? ((a - u) + t)
                                                : ((t - u) + a);
                r[i] = u;
            }
            nz += m->a_start[j + 1] - m->a_start[j];
        }
        jm_work_add(&s->work, (nz + 2 * s->nrow) * JM_WORK_NONZERO);

        double worst = 0.0;
        for (int64_t i = 0; i < s->nrow; i++) {
            if (isfinite(comp[i]))
                r[i] += comp[i];
            if (fabs(r[i]) > worst)
                worst = fabs(r[i]);
        }
        if (!(worst > s->primal_tol))
            return;
        jm_log(m, JAOS_LOG_DETAIL,
               "the point leaves a row by %.6g in the model's units; "
               "polishing", worst);
        for (int64_t i = 0; i < s->nrow; i++)
            r[i] *= rho[i];
        jm_lu_ftran(&s->lu, r, &s->work);
        for (int64_t i = 0; i < s->nrow; i++)
            s->xb[i] -= r[i];
        jm_work_add(&s->work, s->nrow * JM_WORK_NONZERO);
    }
}

static jaos_status publish(sx *s, jaos_solve_status status, jm_presolve *p)
{
    jaos_model *m = s->m;
    const double sigma = (m->sense == JAOS_MAXIMIZE) ? -1.0 : 1.0;
    (void)p;

    m->solve_status = status;
    m->solve_iters = s->iters;

    jaos_status st = jm_model_ensure_solution_arrays(m);
    if (st != JAOS_OK)
        return st;

    if (status != JAOS_SOLVE_OPTIMAL) {

        m->objective = 0.0;
        memset(m->sol_col, 0, (size_t)m->num_col * sizeof(double));
        memset(m->sol_row, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_dual, 0, (size_t)m->num_row * sizeof(double));
        memset(m->sol_redcost, 0, (size_t)m->num_col * sizeof(double));

        memset(m->sol_farkas, 0, (size_t)m->num_row * sizeof(double));
        if (status == JAOS_SOLVE_INFEASIBLE && s->farkas_sign != 0.0) {
            for (int64_t i = 0; i < m->num_row; i++) {

                const int64_t sl = m->num_col + i;
                if ((s->status[sl] == JM_BASIC ||
                     fabs(s->rho[i]) < PIVOT_MIN) && sl != s->farkas_basic) {
                    m->sol_farkas[i] = 0.0;
                    continue;
                }
                m->sol_farkas[i] = published(s->farkas_sign * s->rho[i] *
                                             m->row_scale[i]);
            }
            m->farkas_ok = true;
        }

        memset(m->sol_ray, 0, (size_t)m->num_col * sizeof(double));
        if (status == JAOS_SOLVE_UNBOUNDED && s->uray_ok) {
            for (int64_t j = 0; j < m->num_col; j++)
                m->sol_ray[j] = published(m->col_scale[j] * s->uray[j]);
            m->ray_ok = true;
        }

        if (status == JAOS_SOLVE_WORK_LIMIT ||
            status == JAOS_SOLVE_TIME_LIMIT ||
            status == JAOS_SOLVE_INTERRUPTED ||
            status == JAOS_SOLVE_INFEASIBLE ||
            status == JAOS_SOLVE_UNBOUNDED) {
            for (int64_t j = 0; j < m->num_col; j++)
                m->sol_col_status[j] = published_status(s->status[j]);
            for (int64_t i = 0; i < m->num_row; i++)
                m->sol_row_status[i] =
                    published_status(s->status[m->num_col + i]);
            (void)jm_model_remember_basis(m);
            m->sol_basis_ok = true;
        } else {
            memset(m->sol_col_status, 0,
                   (size_t)m->num_col * sizeof *m->sol_col_status);
            memset(m->sol_row_status, 0,
                   (size_t)m->num_row * sizeof *m->sol_row_status);
        }
        m->solve_work = s->work.units;

        m->solve_time = elapsed_seconds(s);
#if !defined(JAOS_NO_PRESOLVE)
        if (p->outcome == JM_PRESOLVE_REDUCED) {
            jaos_status pst = jm_postsolve_expand(p);
            if (pst != JAOS_OK)
                return pst;
        }
#endif
        return JAOS_OK;
    }

#ifndef NDEBUG

    for (int64_t v = 0; v < s->nvar; v++)
        assert(s->shift[v] == 0.0 && s->cost[v] == s->cost0[v]);
#endif

    const double *rho = m->row_scale, *gamma = m->col_scale;

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col[j] = published(gamma[j] * var_value(s, j));
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row[i] = published(var_value(s, m->num_col + i) / rho[i]);

    double *y = s->y;
    for (int64_t i = 0; i < s->nrow; i++)
        y[i] = s->cost[s->basis[i]];
    jm_lu_btran(&s->lu, y, &s->work);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_dual[i] = published(sigma * y[i] * rho[i]);
    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_redcost[j] = published(sigma * s->d[j] / gamma[j]);

    for (int64_t j = 0; j < m->num_col; j++)
        m->sol_col_status[j] = published_status(s->status[j]);
    for (int64_t i = 0; i < m->num_row; i++)
        m->sol_row_status[i] = published_status(s->status[m->num_col + i]);

    for (int64_t j = 0; j < m->num_col; j++) {
        if (m->col_lower[j] != m->col_upper[j] ||
            m->sol_col_status[j] == JAOS_BASIS_BASIC)
            continue;
        const double dc = sigma * m->sol_redcost[j];
        if (dc < 0.0)
            m->sol_col_status[j] = JAOS_BASIS_AT_UPPER;
        else if (dc > 0.0)
            m->sol_col_status[j] = JAOS_BASIS_AT_LOWER;
    }

    jm_model_publish_objective(m);
    m->solve_work = s->work.units;
    m->solve_time = elapsed_seconds(s);

    (void)jm_model_remember_basis(m);
    m->sol_basis_ok = true;

#if !defined(JAOS_NO_PRESOLVE)

    if (p->outcome == JM_PRESOLVE_REDUCED) {
        jaos_status pst = jm_postsolve_expand(p);
        if (pst != JAOS_OK)
            return pst;
    }
#endif
    return JAOS_OK;
}

static jaos_status publish_inverted_box(jaos_model *m)
{
    jaos_status est = jm_model_ensure_solution_arrays(m);
    if (est != JAOS_OK)
        return est;
    m->solve_status = JAOS_SOLVE_INFEASIBLE;
    m->solve_work = 0;
    m->solve_time = 0.0;
    m->objective = 0.0;
    m->presolve_num_row = m->num_row;
    m->presolve_num_col = m->num_col;
    m->presolve_num_nz  = m->num_nz;
    memset(m->sol_col, 0, (size_t)m->num_col * sizeof(double));
    memset(m->sol_row, 0, (size_t)m->num_row * sizeof(double));
    memset(m->sol_dual, 0, (size_t)m->num_row * sizeof(double));
    memset(m->sol_redcost, 0, (size_t)m->num_col * sizeof(double));
    memset(m->sol_col_status, 0,
           (size_t)m->num_col * sizeof *m->sol_col_status);
    memset(m->sol_row_status, 0,
           (size_t)m->num_row * sizeof *m->sol_row_status);
    memset(m->sol_farkas, 0, (size_t)m->num_row * sizeof(double));
    memset(m->sol_ray, 0, (size_t)m->num_col * sizeof(double));
    return JAOS_OK;
}

static bool has_inverted_box(const jaos_model *m, bool *is_row, int64_t *at)
{
    for (int64_t j = 0; j < m->num_col; j++)
        if (jm_box_inverted(m->col_lower[j], m->col_upper[j])) {
            *is_row = false;
            *at = j;
            return true;
        }
    for (int64_t i = 0; i < m->num_row; i++)
        if (jm_box_inverted(m->row_lower[i], m->row_upper[i])) {
            *is_row = true;
            *at = i;
            return true;
        }
    return false;
}

jaos_status jm_dual_simplex(jaos_model *m)
{
    jm_presolve p;
    jm_presolve_init(&p);
    p.orig = m;

    m->solve_iters = 0;
    m->solve_primal_iters = 0;
    m->solve_phase1_iters = 0;
    m->solve_barrier_iters = 0;

    m->farkas_ok = false;
    m->ray_ok = false;

    m->sol_basis_ok = false;

    {
        bool is_row = false;
        int64_t at = -1;
        if (has_inverted_box(m, &is_row, &at)) {
            jm_log(m, JAOS_LOG_SUMMARY,
                   "%s %lld has its lower bound %.17g above its upper %.17g: "
                   "infeasible, no simplex run",
                   is_row ? "row" : "column", (long long)at,
                   is_row ? m->row_lower[at] : m->col_lower[at],
                   is_row ? m->row_upper[at] : m->col_upper[at]);
            jm_presolve_free(&p);
            return publish_inverted_box(m);
        }
    }

    jm_work pre_work = {0};

#if !defined(JAOS_NO_PRESOLVE)

    const bool quadratic = jm_model_has_quadratic(m);
    jaos_status pst = quadratic ? JAOS_OK : jm_presolve_run(m, &p, &pre_work);
    if (pst != JAOS_OK) {
        jm_presolve_free(&p);
        return pst;
    }
    if (quadratic) {
        p.outcome = JM_PRESOLVE_NONE;
        jm_log(m, JAOS_LOG_SUMMARY,
               "presolve: skipped, the objective has a quadratic term");
    }

    m->presolve_counts = p.counts;

    if (p.outcome == JM_PRESOLVE_NONE) {
        if (!quadratic)
            jm_log(m, JAOS_LOG_SUMMARY, "presolve: nothing fired");
    } else if (p.outcome == JM_PRESOLVE_INFEASIBLE ||
              p.outcome == JM_PRESOLVE_UNBOUNDED) {
        jm_log(m, JAOS_LOG_SUMMARY,
               "presolve: %s, no simplex run; "
               "empty_row=%lld empty_col=%lld singleton_row=%lld "
               "singleton_col=%lld free_col_singleton=%lld rounds=%lld",
               p.outcome == JM_PRESOLVE_INFEASIBLE ? "infeasible"
                                                    : "unbounded",
               (long long)p.counts.empty_row, (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row,
               (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton,
               (long long)p.counts.rounds);
    } else {
        jm_log(m, JAOS_LOG_SUMMARY,
               "presolve: %lld rows, %lld columns, %lld nonzeros -> "
               "%lld rows, %lld columns, %lld nonzeros; "
               "fixed_col=%lld empty_row=%lld empty_col=%lld "
               "singleton_row=%lld singleton_col=%lld "
               "free_col_singleton=%lld rounds=%lld",
               (long long)m->num_row, (long long)m->num_col,
               (long long)m->num_nz, (long long)p.reduced.num_row,
               (long long)p.reduced.num_col, (long long)p.reduced.num_nz,
               (long long)p.counts.fixed_col, (long long)p.counts.empty_row,
               (long long)p.counts.empty_col,
               (long long)p.counts.singleton_row,
               (long long)p.counts.singleton_col,
               (long long)p.counts.free_col_singleton,
               (long long)p.counts.rounds);
    }
#endif

    if (p.outcome == JM_PRESOLVE_SOLVED) {

        m->presolve_num_row = p.reduced.num_row;
        m->presolve_num_col = p.reduced.num_col;
        m->presolve_num_nz  = p.reduced.num_nz;
        jaos_status st = jm_postsolve_solved(&p);
        jm_presolve_free(&p);
        return st;
    }

    if (p.outcome == JM_PRESOLVE_INFEASIBLE ||
        p.outcome == JM_PRESOLVE_UNBOUNDED) {

        m->presolve_num_row = p.reduced.num_row;
        m->presolve_num_col = p.reduced.num_col;
        m->presolve_num_nz  = p.reduced.num_nz;
        const jaos_solve_status status = (p.outcome == JM_PRESOLVE_INFEASIBLE)
            ? JAOS_SOLVE_INFEASIBLE : JAOS_SOLVE_UNBOUNDED;
        jaos_status st = jm_postsolve_infeasible_or_unbounded(&p, status);
        jm_presolve_free(&p);
        return st;
    }

    jaos_model *target = (p.outcome == JM_PRESOLVE_REDUCED) ? &p.reduced : m;
    m->presolve_num_row = target->num_row;
    m->presolve_num_col = target->num_col;
    m->presolve_num_nz  = target->num_nz;

    int64_t barrier_iters = 0;
    if (jm_model_has_quadratic(m) ||
        ((m->cfg.barrier || m->cfg.pdlp) && !m->cfg.node_solve)) {
        bool crossover = false, handoff = false;
        jaos_status bst = m->cfg.pdlp
            ? jm_pdlp(m, target, &p, &pre_work, &crossover, &handoff,
                      &barrier_iters)
            : jm_barrier(m, target, &p, &pre_work, &crossover, &handoff,
                         &barrier_iters);
        m->solve_barrier_iters = barrier_iters;
        if (bst != JAOS_OK || (!crossover && !handoff)) {
            jm_presolve_free(&p);
            return bst;
        }
    }

    sx s;
    jaos_status st = sx_init(&s, target);
    if (st != JAOS_OK) {
        jm_presolve_free(&p);
        return st;
    }

    s.work = pre_work;
    s.started = jm_monotonic_seconds();

    jm_log(m, JAOS_LOG_SUMMARY,
           "%s simplex: %lld rows, %lld columns, %lld nonzeros, "
           "primal tol %.3g, dual tol %.3g",
           m->cfg.force_primal ? "primal" : "dual",
           (long long)m->num_row, (long long)m->num_col,
           (long long)m->num_nz, s.primal_tol, s.dual_tol);

    jaos_solve_status outcome;
    bool allow_warm = true;
    for (;;) {
        const bool warm = allow_warm && build_warm_basis(&s);
        if (!warm)
            build_initial_basis(&s);
        jm_log(m, JAOS_LOG_DETAIL, "starting from %s",
               warm ? "the basis on the model" : "the slack basis");

        st = m->cfg.force_primal ? run_primal(&s, &outcome)
                                 : run(&s, &outcome);

        if (st == JAOS_ERR_NUMERICAL && warm) {
            jm_log(m, JAOS_LOG_SUMMARY,
                   "the supplied basis reached no answer (%s); restarting "
                   "cold from the slack basis after %lld iterations, %lld "
                   "refactorizations, %lld stalls, %lld stability rebuilds",
                   target->err, (long long)s.iters, (long long)s.n_refactor,
                   (long long)s.n_bland, (long long)s.n_stability);
            const jm_work carried = s.work;
            const double t0 = s.started;
            sx_free(&s);
            st = sx_init(&s, target);
            if (st != JAOS_OK) {
                jm_presolve_free(&p);
                return st;
            }
            s.work = carried;
            s.started = t0;
            target->err[0] = '\0';
            allow_warm = false;
            continue;
        }
        if (st != JAOS_OK || outcome != JAOS_SOLVE_OPTIMAL)
            break;

        settle_shifts(&s);
        jaos_solve_status stopped = JAOS_SOLVE_NOT_RUN;
        st = reenter_after_settling(&s, &stopped);
        if (st != JAOS_OK)
            break;
        if (stopped != JAOS_SOLVE_NOT_RUN) {

            outcome = stopped;
            break;
        }

        settle_shifts(&s);
        const double breach = settled_dual_violation(&s);
        if (breach != 0.0) {
            if (warm) {
                jm_log(m, JAOS_LOG_SUMMARY,
                       "the settled point from the supplied basis is not "
                       "dual feasible; restarting cold from the slack "
                       "basis after %lld iterations, %lld refactorizations, "
                       "%lld weight restarts, %lld stalls, %lld stability "
                       "rebuilds",
                       (long long)s.iters, (long long)s.n_refactor,
                       (long long)s.n_weight_restart, (long long)s.n_bland,
                       (long long)s.n_stability);
                const jm_work carried = s.work;
                const double t0 = s.started;
                sx_free(&s);
                st = sx_init(&s, target);
                if (st != JAOS_OK) {
                    jm_presolve_free(&p);
                    return st;
                }
                s.work = carried;
                s.started = t0;

                target->err[0] = '\0';
                allow_warm = false;
                continue;
            }
            jm_set_err(m, "the settled point is not dual feasible: a "
                          "reduced cost breaches its bound by %.6g after "
                          "settling, from a %s start; publishing that as "
                          "OPTIMAL would certify a point the reduced costs "
                          "do not support", breach, warm ? "warm" : "cold");
            outcome = JAOS_SOLVE_NUMERICAL_ERROR;
            break;
        }
        outcome = classify_optimum(&s);

        if (outcome == JAOS_SOLVE_OPTIMAL)
            st = retire_lent_bounds(&s);
        break;
    }

    if (target != m && target->err[0] != '\0' &&
        (st != JAOS_OK || outcome == JAOS_SOLVE_NUMERICAL_ERROR))
        memcpy(m->err, target->err, sizeof m->err);

    if (st == JAOS_OK && outcome == JAOS_SOLVE_OPTIMAL)
        polish_unscaled(&s);
    if (st == JAOS_OK)
        st = publish(&s, outcome, &p);

    if (st != JAOS_OK)
        m->solve_iters = s.iters;
    m->solve_iters += barrier_iters;
    m->solve_primal_iters = s.n_primal_iters;
    m->solve_phase1_iters = s.n_phase1_iters;

    if (st == JAOS_OK)
        jm_log(m, JAOS_LOG_SUMMARY,
               "%s after %lld iterations, %lld work units; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds, %lld primal iterations, %lld of them "
               "phase 1",
               jaos_solve_status_str(outcome), (long long)s.iters,
               (long long)s.work.units, (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters,
               (long long)s.n_phase1_iters);
    else
        jm_log(m, JAOS_LOG_SUMMARY,
               "abandoned after %lld iterations, %lld work units: %s; "
               "%lld refactorizations, %lld weight restarts, %lld stalls, "
               "%lld stability rebuilds, %lld primal iterations, %lld of them "
               "phase 1",
               (long long)s.iters, (long long)s.work.units,
               jaos_status_str(st), (long long)s.n_refactor,
               (long long)s.n_weight_restart, (long long)s.n_bland,
               (long long)s.n_stability, (long long)s.n_primal_iters,
               (long long)s.n_phase1_iters);

    sx_free(&s);
    jm_presolve_free(&p);
    return st;
}

double jm_primal_tolerance(const jaos_model *m)
{
    return m->cfg.primal_tol > 0.0 ? m->cfg.primal_tol : PRIMAL_TOL;
}

double jm_dual_tolerance(const jaos_model *m)
{
    return m->cfg.dual_tol > 0.0 ? m->cfg.dual_tol : DUAL_TOL;
}
