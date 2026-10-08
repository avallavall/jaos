#include "jaos_internal.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

constexpr double RELAX_BOX_FLOOR = 1.0;
constexpr double RELAX_BOX_START = 2.0;
constexpr double RELAX_BOX_GROWTH = 2.0;
constexpr int64_t RELAX_BOX_ROUNDS = 16;
constexpr int64_t RELAX_ROUND_WORK = 64;
constexpr int64_t RELAX_LATTICE_CELLS = 4096;

typedef struct {
    bool lo, hi;
} rx_sides;

static rx_sides rx_sides_of(double lower, double upper, bool in_scope)
{
    rx_sides s = {false, false};
    if (!in_scope)
        return s;
    s.lo = lower > -INFINITY;
    s.hi = upper < INFINITY;
    return s;
}

static rx_sides rx_col_sides(const jaos_model *m, int64_t j, bool in_scope)
{
    rx_sides s = rx_sides_of(m->col_lower[j], m->col_upper[j], in_scope);
    if (m->col_semi != nullptr && m->col_semi[j])
        s.lo = false;
    return s;
}

typedef struct {
    jaos_model *c;
    int64_t nc, nr;
    int64_t ecol;
    int64_t erow;

    int64_t *row_s, *row_t;
    int64_t *col_s, *col_t;

    int64_t *box;
    int64_t nbox;
} rx;

static void rx_free(rx *g)
{
    if (g->c != nullptr)
        jaos_model_free(g->c);
    g->c = nullptr;
    free(g->row_s); free(g->row_t);
    free(g->col_s); free(g->col_t);
    free(g->box);
    g->row_s = g->row_t = g->col_s = g->col_t = g->box = nullptr;
}

static jaos_status rx_build(rx *g, jaos_model *m, jaos_relax_scope scope)
{
    const int64_t nc = g->nc, nr = g->nr;
    const bool do_rows = (scope & JAOS_RELAX_ROWS) != 0;
    const bool do_cols = (scope & JAOS_RELAX_COLS) != 0;

    jaos_status rc = JAOS_ERR_OUT_OF_MEMORY;
    double *cost = nullptr, *cl = nullptr, *cu = nullptr;
    double *rl = nullptr, *ru = nullptr;
    int64_t *ap = nullptr, *ai = nullptr;
    double *av = nullptr;

    g->row_s = jm_alloc_array(nr > 0 ? nr : 1, sizeof *g->row_s);
    g->row_t = jm_alloc_array(nr > 0 ? nr : 1, sizeof *g->row_t);
    g->col_s = jm_alloc_array(nc > 0 ? nc : 1, sizeof *g->col_s);
    g->col_t = jm_alloc_array(nc > 0 ? nc : 1, sizeof *g->col_t);
    g->box   = jm_alloc_array(nc > 0 ? nc : 1, sizeof *g->box);
    if (g->row_s == nullptr || g->row_t == nullptr ||
        g->col_s == nullptr || g->col_t == nullptr || g->box == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++) g->row_s[i] = g->row_t[i] = -1;
    for (int64_t j = 0; j < nc; j++) g->col_s[j] = g->col_t[j] = -1;
    g->nbox = 0;
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if ((s.lo || s.hi) && m->col_integer != nullptr && m->col_integer[j])
            g->box[g->nbox++] = j;
    }

    int64_t ecol = 0, erow = 0;
    for (int64_t i = 0; i < nr; i++) {
        const rx_sides s = rx_sides_of(m->row_lower[i], m->row_upper[i],
                                       do_rows);
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if (!s.lo && !s.hi)
            continue;
        erow++;
        ecol += (s.lo ? 1 : 0) + (s.hi ? 1 : 0);
    }
    g->ecol = ecol;
    g->erow = erow;

    const int64_t tc = nc + ecol, tr = nr + erow;

    const int64_t tnz = m->num_nz + ecol + erow;

    cost = calloc((size_t)(tc > 0 ? tc : 1), sizeof *cost);
    cl   = jm_alloc_array(tc > 0 ? tc : 1, sizeof *cl);
    cu   = jm_alloc_array(tc > 0 ? tc : 1, sizeof *cu);
    rl   = jm_alloc_array(tr > 0 ? tr : 1, sizeof *rl);
    ru   = jm_alloc_array(tr > 0 ? tr : 1, sizeof *ru);
    ap   = jm_alloc_array(tc + 1, sizeof *ap);
    ai   = jm_alloc_array(tnz > 0 ? tnz : 1, sizeof *ai);
    av   = jm_alloc_array(tnz > 0 ? tnz : 1, sizeof *av);
    if (cost == nullptr || cl == nullptr || cu == nullptr || rl == nullptr ||
        ru == nullptr || ap == nullptr || ai == nullptr || av == nullptr)
        goto out;

    for (int64_t i = 0; i < nr; i++) {
        rl[i] = m->row_lower[i];
        ru[i] = m->row_upper[i];
    }

    int64_t nz = 0, r = nr;
    for (int64_t j = 0; j < nc; j++) {
        ap[j] = nz;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            ai[nz] = m->a_index[k];
            av[nz] = m->a_value[k];
            nz++;
        }
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if (s.lo || s.hi) {
            rl[r] = s.lo ? m->col_lower[j] : -INFINITY;
            ru[r] = s.hi ? m->col_upper[j] : INFINITY;
            ai[nz] = r;
            av[nz] = 1.0;
            nz++;

            cl[j] = s.lo ? -INFINITY : m->col_lower[j];
            cu[j] = s.hi ?  INFINITY : m->col_upper[j];
            r++;
        } else {
            cl[j] = m->col_lower[j];
            cu[j] = m->col_upper[j];
        }
    }
    assert(r == tr);

    int64_t e = nc;
    for (int64_t i = 0; i < nr; i++) {
        const rx_sides s = rx_sides_of(m->row_lower[i], m->row_upper[i],
                                       do_rows);
        if (s.lo) {
            ap[e] = nz; ai[nz] = i; av[nz] = 1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->row_s[i] = e; e++;
        }
        if (s.hi) {
            ap[e] = nz; ai[nz] = i; av[nz] = -1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->row_t[i] = e; e++;
        }
    }
    r = nr;
    for (int64_t j = 0; j < nc; j++) {
        const rx_sides s = rx_col_sides(m, j, do_cols);
        if (!s.lo && !s.hi)
            continue;
        if (s.lo) {
            ap[e] = nz; ai[nz] = r; av[nz] = 1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->col_s[j] = e; e++;
        }
        if (s.hi) {
            ap[e] = nz; ai[nz] = r; av[nz] = -1.0; nz++;
            cost[e] = 1.0; cl[e] = 0.0; cu[e] = INFINITY;
            g->col_t[j] = e; e++;
        }
        r++;
    }
    assert(e == tc && nz == tnz);
    ap[tc] = nz;

    rc = jaos_model_new(&g->c);
    if (rc != JAOS_OK)
        goto out;

    rc = jaos_load_lp(g->c, tc, tr, JAOS_MINIMIZE, 0.0, cost, cl, cu, rl, ru,
                      tnz, ap, ai, av);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the relaxation's copy refused the model: %s",
                   jaos_model_error(g->c));
        goto out;
    }

    g->c->cfg.work_limit = m->cfg.work_limit;
    g->c->cfg.time_limit = m->cfg.time_limit;
    g->c->cfg.primal_tol = m->cfg.primal_tol;
    g->c->cfg.dual_tol = m->cfg.dual_tol;
    g->c->cfg.progress_cb = m->cfg.progress_cb;
    g->c->cfg.progress_user = m->cfg.progress_user;
    g->c->cfg.log_cb = m->cfg.log_cb;
    g->c->cfg.log_user = m->cfg.log_user;
    g->c->cfg.log_level = m->cfg.log_level;
    g->c->cfg.force_primal = m->cfg.force_primal;
    rc = JAOS_OK;
out:
    free(cost); free(cl); free(cu); free(rl); free(ru);
    free(ap); free(ai); free(av);
    return rc;
}

static jaos_status rx_discrete(rx *g, const jaos_model *m)
{
    jaos_status rc;
    for (int64_t j = 0; j < g->nc; j++) {
        if (m->col_integer != nullptr && m->col_integer[j]) {
            rc = jaos_set_col_integer(g->c, j, true);
            if (rc != JAOS_OK)
                return rc;
        }
        if (m->col_semi != nullptr && m->col_semi[j]) {
            rc = jaos_set_col_semicontinuous(g->c, j, true);
            if (rc != JAOS_OK)
                return rc;
        }
    }
    for (int64_t i = 0; i < g->nr; i++) {
        if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0) {
            rc = jaos_set_row_indicator(g->c, i, m->row_ind_col[i],
                                        m->row_ind_val[i]);
            if (rc != JAOS_OK)
                return rc;
        }
    }
    for (int64_t k = 0; k < m->num_sos; k++) {
        const int64_t n = m->sos_start[k + 1] - m->sos_start[k];
        rc = jaos_add_sos(g->c, m->sos_type[k], n,
                          m->sos_col + m->sos_start[k],
                          m->sos_weight + m->sos_start[k]);
        if (rc != JAOS_OK)
            return rc;
    }
    return JAOS_OK;
}

static jaos_status rx_box(rx *g, const jaos_model *m, double width)
{
    for (int64_t k = 0; k < g->nbox; k++) {
        const int64_t j = g->box[k];
        const rx_sides s = rx_col_sides(m, j, true);
        const double lo = s.lo ? m->col_lower[j] - width : m->col_lower[j];
        const double hi = s.hi ? m->col_upper[j] + width : m->col_upper[j];
        const jaos_status rc = jaos_set_col_bounds(g->c, j, lo, hi);
        if (rc != JAOS_OK)
            return rc;
    }
    return JAOS_OK;
}

static jaos_status rx_solve(rx *g, jaos_model *m, int64_t *used, int64_t cap)
{
    const int64_t limit = m->cfg.work_limit;
    if (limit > 0)
        g->c->cfg.work_limit = limit > *used ? limit - *used : 1;
    if (cap > 0 && (limit <= 0 || cap < g->c->cfg.work_limit))
        g->c->cfg.work_limit = cap;
    const jaos_status rc = jaos_solve(g->c);
    if (rc != JAOS_OK) {
        jm_set_err(m, "the relaxation's solve failed: %s (%s)",
                   jaos_status_str(rc), jaos_model_error(g->c));
        return rc;
    }
    *used += jaos_work_units(g->c);
    return JAOS_OK;
}

static double rx_at(const double *x, int64_t e)
{
    return e < 0 ? 0.0 : x[e];
}

static bool rx_eq_row(const jaos_model *m, int64_t i)
{
    return m->row_lower[i] == m->row_upper[i] && isfinite(m->row_lower[i]) &&
           (m->row_ind_col == nullptr || m->row_ind_col[i] < 0);
}

static bool rx_is_int(const jaos_model *m, int64_t j)
{
    return m->col_integer != nullptr && m->col_integer[j];
}

static int64_t rx_find(int64_t *up, int64_t i)
{
    while (up[i] != i) {
        up[i] = up[up[i]];
        i = up[i];
    }
    return i;
}

static bool rx_quot(jm_bigint *q, const jm_bigint *a, const jm_bigint *b)
{
    jm_nat rem;
    if (!jm_nat_divmod(&q->mag, &rem, &a->mag, &b->mag))
        return false;
    q->sign = q->mag.n == 0 ? 0 : a->sign * b->sign;
    return true;
}

static bool rx_axpy(jm_bigint *r, const jm_bigint *f, const jm_bigint *g,
                    const jm_bigint *p)
{
    jm_bigint s, t;
    return jm_bigint_mul(&s, f, r) && jm_bigint_mul(&t, g, p) &&
           jm_bigint_sub(r, &s, &t);
}

static bool rx_content(jm_bigint *row, int64_t w)
{
    jm_nat g, one, rem;
    jm_nat_set_zero(&g);
    for (int64_t u = 0; u < w; u++)
        if (row[u].sign != 0 && !jm_nat_gcd(&g, &g, &row[u].mag))
            return false;
    jm_nat_set_u64(&one, 1);
    if (g.n == 0 || jm_nat_cmp(&g, &one) == 0)
        return true;
    for (int64_t u = 0; u < w; u++)
        if (row[u].sign != 0 &&
            !jm_nat_divmod(&row[u].mag, &rem, &row[u].mag, &g))
            return false;
    return true;
}

static bool rx_smaller(const jm_bigint *a, const jm_bigint *b)
{
    return jm_nat_cmp(&a->mag, &b->mag) < 0;
}

static bool rx_block_fill(const jaos_model *m, jm_bigint *a, int64_t w,
                          const int64_t *rows, int64_t nb, const int64_t *rs,
                          const int64_t *rj, const double *rv,
                          const int64_t *cpos)
{
    jm_dyadic d;
    for (int64_t p = 0; p < nb; p++) {
        const int64_t r = rows[p];
        int64_t low = INT64_MAX;
        for (int64_t e = rs[r]; e <= rs[r + 1]; e++) {
            const double v = e < rs[r + 1] ? rv[e] : m->row_lower[r];
            if (!jm_dyadic_from_double(&d, v))
                return false;
            if (d.m.sign != 0 && d.e < low)
                low = d.e;
        }
        for (int64_t e = rs[r]; e <= rs[r + 1]; e++) {
            const double v = e < rs[r + 1] ? rv[e] : m->row_lower[r];
            const int64_t u = e < rs[r + 1] ? cpos[rj[e]] : w - 1;
            if (!jm_dyadic_from_double(&d, v))
                return false;
            if (d.m.sign != 0 && !jm_bigint_shl(&a[p * w + u], &d.m, d.e - low))
                return false;
        }
        if (!rx_content(a + p * w, w))
            return false;
    }
    return true;
}

static bool rx_eliminate(const jaos_model *m, jm_bigint *a, int64_t w,
                         int64_t nb, const int64_t *cols, bool *alive,
                         int64_t *work)
{
    for (int64_t t = 0; t + 1 < w; t++) {
        if (rx_is_int(m, cols[t]))
            continue;
        int64_t p = -1;
        for (int64_t r = 0; r < nb; r++)
            if (alive[r] && a[r * w + t].sign != 0 &&
                (p < 0 || rx_smaller(&a[r * w + t], &a[p * w + t])))
                p = r;
        if (p < 0)
            continue;
        alive[p] = false;
        const jm_bigint f = a[p * w + t];
        for (int64_t r = 0; r < nb; r++) {
            if (!alive[r] || a[r * w + t].sign == 0)
                continue;
            const jm_bigint g = a[r * w + t];
            for (int64_t u = 0; u < w; u++)
                if (!rx_axpy(&a[r * w + u], &f, &g, &a[p * w + u]))
                    return false;
            *work += w;
            if (!rx_content(a + r * w, w))
                return false;
        }
    }
    return true;
}

static int rx_hermite(const jaos_model *m, jm_bigint *a, int64_t w,
                      int64_t nb, const int64_t *cols, const bool *alive,
                      bool *open, int64_t *work)
{
    const int64_t nc = w - 1;
    for (int64_t t = 0; t < nc; t++)
        open[t] = rx_is_int(m, cols[t]);
    for (int64_t p = 0; p < nb; p++) {
        if (!alive[p])
            continue;
        jm_bigint *row = a + p * w;
        int64_t k;
        for (;;) {
            k = -1;
            for (int64_t t = 0; t < nc; t++)
                if (open[t] && row[t].sign != 0 &&
                    (k < 0 || rx_smaller(&row[t], &row[k])))
                    k = t;
            if (k < 0)
                break;
            bool more = false;
            for (int64_t t = 0; t < nc; t++) {
                if (t == k || !open[t] || row[t].sign == 0)
                    continue;
                more = true;
                jm_bigint q, one;
                jm_bigint_set_i64(&one, 1);
                if (!rx_quot(&q, &row[t], &row[k]))
                    return -1;
                for (int64_t r = p; r < nb; r++) {
                    if (!alive[r])
                        continue;
                    if (!rx_axpy(&a[r * w + t], &one, &q, &a[r * w + k]))
                        return -1;
                }
                *work += nb - p;
            }
            if (!more)
                break;
        }
        if (k < 0) {
            if (row[nc].sign != 0)
                return 1;
            continue;
        }
        jm_bigint y, one;
        jm_nat rem;
        if (!jm_nat_divmod(&y.mag, &rem, &row[nc].mag, &row[k].mag))
            return -1;
        if (rem.n != 0)
            return 1;
        y.sign = y.mag.n == 0 ? 0 : row[nc].sign * row[k].sign;
        jm_bigint_set_i64(&one, 1);
        open[k] = false;
        for (int64_t r = p + 1; r < nb; r++) {
            if (!alive[r] || a[r * w + k].sign == 0)
                continue;
            if (!rx_axpy(&a[r * w + nc], &one, &a[r * w + k], &y))
                return -1;
            jm_bigint_set_zero(&a[r * w + k]);
        }
        *work += nb - p;
    }
    return 0;
}

static bool rx_lattice_block(const jaos_model *m, const int64_t *rows,
                             int64_t nb, const int64_t *rs, const int64_t *rj,
                             const double *rv, int64_t *cpos, int64_t *cols,
                             int64_t *work)
{
    int64_t nc = 0;
    bool any_int = false;
    for (int64_t p = 0; p < nb; p++)
        for (int64_t e = rs[rows[p]]; e < rs[rows[p] + 1]; e++)
            if (cpos[rj[e]] < 0) {
                cpos[rj[e]] = nc;
                cols[nc++] = rj[e];
                any_int = any_int || rx_is_int(m, rj[e]);
            }
    const int64_t w = nc + 1;
    bool proved = false;
    jm_bigint *a = nullptr;
    bool *alive = nullptr, *open = nullptr;
    if (!any_int || nb > RELAX_LATTICE_CELLS / w)
        goto out;
    a = calloc((size_t)(nb * w), sizeof *a);
    alive = jm_alloc_array(nb, sizeof *alive);
    open = jm_alloc_array(nc, sizeof *open);
    if (a == nullptr || alive == nullptr || open == nullptr)
        goto out;
    for (int64_t p = 0; p < nb; p++)
        alive[p] = true;
    if (!rx_block_fill(m, a, w, rows, nb, rs, rj, rv, cpos) ||
        !rx_eliminate(m, a, w, nb, cols, alive, work))
        goto out;
    proved = rx_hermite(m, a, w, nb, cols, alive, open, work) == 1;
out:
    for (int64_t t = 0; t < nc; t++)
        cpos[cols[t]] = -1;
    free(a);
    free(alive);
    free(open);
    return proved;
}

static bool rx_range_row_empty(const jaos_model *m, int64_t i)
{
    const int64_t p0 = m->ar_start[i], p1 = m->ar_start[i + 1];
    const double lo = m->row_lower[i], hi = m->row_upper[i];
    jm_dyadic d;
    int64_t low = INT64_MAX;
    bool any = false;
    for (int64_t k = p0; k <= p1 + 1; k++) {
        const double v = k < p1 ? m->ar_value[k] : (k == p1 ? lo : hi);
        if (k < p1 && !rx_is_int(m, m->ar_index[k]))
            return false;
        if (!jm_dyadic_from_double(&d, v))
            return false;
        if (d.m.sign != 0 && d.e < low)
            low = d.e;
        any = any || (k < p1 && d.m.sign != 0);
    }
    if (!any)
        return false;
    jm_nat g, rem, one;
    jm_bigint bl, bh, span, t;
    jm_nat_set_zero(&g);
    jm_bigint_set_zero(&bl);
    jm_bigint_set_zero(&bh);
    for (int64_t k = p0; k <= p1 + 1; k++) {
        const double v = k < p1 ? m->ar_value[k] : (k == p1 ? lo : hi);
        if (!jm_dyadic_from_double(&d, v))
            return false;
        if (d.m.sign == 0)
            continue;
        if (!jm_bigint_shl(&t, &d.m, d.e - low))
            return false;
        if (k < p1) {
            if (!jm_nat_gcd(&g, &g, &t.mag))
                return false;
        } else if (k == p1) {
            bl = t;
        } else {
            bh = t;
        }
    }
    jm_nat_set_u64(&one, 1);
    if (jm_nat_cmp(&g, &one) <= 0 || !jm_bigint_sub(&span, &bh, &bl))
        return false;
    if (!jm_nat_divmod(nullptr, &rem, &bl.mag, &g))
        return false;
    if (rem.n == 0)
        return false;
    jm_nat gap = g;
    if (bl.sign > 0)
        jm_nat_sub(&gap, &g, &rem);
    else
        gap = rem;
    return span.sign >= 0 && jm_nat_cmp(&span.mag, &gap) < 0;
}

static int64_t rx_range_empty(jaos_model *m, int64_t *work)
{
    if (jm_model_ensure_rowwise(m) != JAOS_OK)
        return -1;
    for (int64_t i = 0; i < m->num_row; i++) {
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        if (!isfinite(lo) || !isfinite(hi) || !(lo < hi) ||
            (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0))
            continue;
        *work += m->ar_start[i + 1] - m->ar_start[i] + 2;
        if (rx_range_row_empty(m, i))
            return i;
    }
    return -1;
}

static bool rx_lattice_empty(const jaos_model *m, int64_t *work)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    if (nr == 0 || nc == 0)
        return false;
    bool proved = false;
    int64_t *up = jm_alloc_array(nr, sizeof *up);
    int64_t *rs = calloc((size_t)nr + 1, sizeof *rs);
    int64_t *at = jm_alloc_array(nr + 1, sizeof *at);
    int64_t *order = jm_alloc_array(nr, sizeof *order);
    int64_t *cpos = jm_alloc_array(nc, sizeof *cpos);
    int64_t *cols = jm_alloc_array(nc, sizeof *cols);
    int64_t *rj = nullptr;
    double *rv = nullptr;
    if (up == nullptr || rs == nullptr || at == nullptr || order == nullptr ||
        cpos == nullptr || cols == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++)
        up[i] = i;
    for (int64_t j = 0; j < nc; j++) {
        cpos[j] = -1;
        int64_t first = -1;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (m->a_value[k] == 0.0 || !rx_eq_row(m, i))
                continue;
            rs[i + 1]++;
            const int64_t ri = rx_find(up, i);
            if (first < 0) {
                first = ri;
            } else if (ri != first) {
                const int64_t lo = ri < first ? ri : first;
                up[ri + first - lo] = lo;
                first = lo;
            }
        }
    }
    for (int64_t i = 0; i < nr; i++)
        rs[i + 1] += rs[i];
    rj = jm_alloc_array(rs[nr] > 0 ? rs[nr] : 1, sizeof *rj);
    rv = jm_alloc_array(rs[nr] > 0 ? rs[nr] : 1, sizeof *rv);
    if (rj == nullptr || rv == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++)
        at[i] = rs[i];
    for (int64_t j = 0; j < nc; j++)
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (m->a_value[k] == 0.0 || !rx_eq_row(m, i))
                continue;
            rj[at[i]] = j;
            rv[at[i]++] = m->a_value[k];
        }
    for (int64_t i = 0; i <= nr; i++)
        at[i] = 0;
    for (int64_t i = 0; i < nr; i++)
        if (rx_eq_row(m, i)) {
            up[i] = rx_find(up, i);
            at[up[i] + 1]++;
        }
    for (int64_t i = 0; i < nr; i++)
        at[i + 1] += at[i];
    for (int64_t i = 0; i < nr; i++)
        if (rx_eq_row(m, i))
            order[at[up[i]]++] = i;
    for (int64_t i = 0, b = 0; i < nr && !proved; i++) {
        if (!rx_eq_row(m, i) || up[i] != i)
            continue;
        const int64_t e = at[i];
        proved = rx_lattice_block(m, order + b, e - b, rs, rj, rv, cpos, cols,
                                  work);
        b = e;
    }
out:
    free(up); free(rs); free(at); free(order);
    free(cpos); free(cols); free(rj); free(rv);
    return proved;
}

jaos_status jaos_feasrelax(jaos_model *m, jaos_relax_scope scope,
                           double *row_move, double *col_move,
                           jaos_relax_report *out)
{
    if (m == nullptr || out == nullptr)
        return JAOS_ERR_INVALID_INPUT;
    if (scope != JAOS_RELAX_ROWS && scope != JAOS_RELAX_COLS &&
        scope != JAOS_RELAX_BOTH)
        return JAOS_ERR_INVALID_INPUT;
    memset(out, 0, sizeof *out);
    out->at_row = -1;
    out->at_col = -1;
    out->status = JAOS_SOLVE_NOT_RUN;
    m->err[0] = '\0';
    if (jm_model_has_conic(m)) {
        jm_set_err(m, "the feasibility relaxation is a linear program, and "
                      "this model has cones or quadratic rows");
        return JAOS_ERR_INVALID_INPUT;
    }

    if (row_move != nullptr && m->num_row > 0)
        memset(row_move, 0, (size_t)m->num_row * sizeof *row_move);
    if (col_move != nullptr && m->num_col > 0)
        memset(col_move, 0, (size_t)m->num_col * sizeof *col_move);

    rx g = {.nc = m->num_col, .nr = m->num_row};
    double *x = nullptr;
    jaos_status rc = rx_build(&g, m, scope);
    if (rc != JAOS_OK)
        goto out;

    int64_t used = 0, unit = 0, rounds = 0;
    bool too_wide = false;
    double width = 0.0;
    if (g.nbox > 0) {
        rc = rx_solve(&g, m, &used, 0);
        if (rc != JAOS_OK)
            goto out;
        out->work_units = used;
        out->status = jaos_status_of(g.c);
        if (out->status == JAOS_SOLVE_OPTIMAL) {
            double v_lp;
            rc = jaos_objective(g.c, &v_lp);
            if (rc != JAOS_OK)
                goto out;
            width = ceil(fmax(RELAX_BOX_FLOOR, RELAX_BOX_START * v_lp));
        }
    }
    if (g.nbox == 0 || out->status == JAOS_SOLVE_OPTIMAL) {
        rc = rx_discrete(&g, m);
        if (rc != JAOS_OK)
            goto out;
        for (;;) {
            if (g.nbox > 0) {
                rc = rx_box(&g, m, width);
                if (rc != JAOS_OK)
                    goto out;
            }
            const int64_t left = m->cfg.work_limit > 0
                                     ? m->cfg.work_limit - used : INT64_MAX;
            const int64_t cap = unit > 0 ? unit * RELAX_ROUND_WORK : 0;
            const bool ours = cap > 0 && cap < left;
            rc = rx_solve(&g, m, &used, cap);
            if (rc != JAOS_OK)
                goto out;
            out->work_units = used;
            out->status = jaos_status_of(g.c);
            if (g.nbox == 0)
                break;
            if (unit == 0)
                unit = used > 0 ? used : 1;
            if (ours && out->status == JAOS_SOLVE_WORK_LIMIT) {
                too_wide = true;
                break;
            }
            if (out->status == JAOS_SOLVE_INFEASIBLE) {
                if (++rounds >= RELAX_BOX_ROUNDS) {
                    too_wide = true;
                    break;
                }
                width *= RELAX_BOX_GROWTH;
                continue;
            }
            if (out->status == JAOS_SOLVE_OPTIMAL) {
                double v;
                rc = jaos_objective(g.c, &v);
                if (rc != JAOS_OK)
                    goto out;
                if (v > width) {
                    if (++rounds >= RELAX_BOX_ROUNDS) {
                        too_wide = true;
                        break;
                    }
                    width *= RELAX_BOX_GROWTH;
                    continue;
                }
            }
            break;
        }
    }
    if (out->status != JAOS_SOLVE_OPTIMAL) {
        const bool cols_only = too_wide && scope == JAOS_RELAX_COLS;
        const bool lattice = cols_only && rx_lattice_empty(m, &used);
        const int64_t range_row = cols_only && !lattice
                                      ? rx_range_empty(m, &used) : -1;
        if (lattice || range_row >= 0) {
            out->work_units = used;
            out->status = JAOS_SOLVE_INFEASIBLE;
        }
        if (range_row >= 0) {
            out->at_row = range_row;
            jm_set_err(m, "no point in the columns' box widened by %.6g, "
                          "after %lld rounds and %lld work units, and none "
                          "in any box: row %lld, over integer columns only, "
                          "asks for a value between its sides that no "
                          "multiple of its coefficients' greatest common "
                          "divisor reaches",
                       width, (long long)(rounds + 1), (long long)used,
                       (long long)range_row);
        } else if (lattice) {
            jm_set_err(m, "no point in the columns' box widened by %.6g, "
                          "after %lld rounds and %lld work units, and none "
                          "in any box: the equality rows, with the "
                          "continuous columns eliminated, ask for an "
                          "integer combination that no integer point gives",
                       width, (long long)(rounds + 1), (long long)used);
        } else if (too_wide)
            jm_set_err(m, "no point in the columns' box widened by %.6g, "
                          "after %lld rounds and %lld work units, so there "
                          "is no smallest violation to report; the rows and "
                          "the integrality may admit no point at all",
                       width, (long long)(rounds + 1), (long long)used);
        else
            jm_set_err(m, "the relaxation's solve answered %s, so there is no "
                          "smallest violation to report",
                       jaos_solve_status_str(out->status));
        rc = JAOS_ERR_NUMERICAL;
        goto out;
    }

    const int64_t tc = g.nc + g.ecol;
    x = jm_alloc_array(tc > 0 ? tc : 1, sizeof *x);
    if (x == nullptr) {
        rc = JAOS_ERR_OUT_OF_MEMORY;
        goto out;
    }
    rc = jaos_solution(g.c, x, nullptr, nullptr, nullptr);
    if (rc != JAOS_OK)
        goto out;

    rc = jaos_objective(g.c, &out->total);
    if (rc != JAOS_OK)
        goto out;

    for (int64_t i = 0; i < g.nr; i++) {
        const double v = rx_at(x, g.row_t[i]) - rx_at(x, g.row_s[i]);
        if (row_move != nullptr)
            row_move[i] = v;
        if (v == 0.0)
            continue;
        out->rows_moved++;
        if (fabs(v) > out->largest) {
            out->largest = fabs(v);
            out->at_row = i;
            out->at_col = -1;
        }
    }
    double sum = 0.0;
    bool integer_moved = false;
    for (int64_t i = 0; i < g.nr; i++)
        sum += fabs(rx_at(x, g.row_t[i]) - rx_at(x, g.row_s[i]));
    for (int64_t j = 0; j < g.nc; j++) {
        double v = rx_at(x, g.col_t[j]) - rx_at(x, g.col_s[j]);
        if (v < 0.0) {
            const double lo = m->col_lower[j];
            v = x[j] < lo ? x[j] - lo : 0.0;
            while (v < 0.0 && lo + v > x[j])
                v = nextafter(v, -INFINITY);
        } else if (v > 0.0) {
            const double hi = m->col_upper[j];
            v = x[j] > hi ? x[j] - hi : 0.0;
            while (v > 0.0 && hi + v < x[j])
                v = nextafter(v, INFINITY);
        }
        if (col_move != nullptr)
            col_move[j] = v;
        if (v == 0.0)
            continue;
        sum += fabs(v);
        if (m->col_integer != nullptr && m->col_integer[j])
            integer_moved = true;
        out->cols_moved++;
        if (fabs(v) > out->largest) {
            out->largest = fabs(v);
            out->at_row = -1;
            out->at_col = j;
        }
    }
    if (integer_moved)
        out->total = sum;
    rc = JAOS_OK;
out:
    free(x);
    rx_free(&g);
    return rc;
}
