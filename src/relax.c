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
constexpr int64_t RELAX_LATTICE_BITS = 4096;
constexpr int64_t RELAX_SHADOW_CELLS = 16384;
constexpr int64_t RELAX_SHADOW_WORK = 4000000;

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

static bool rx_fits(const jm_bigint *a)
{
    return jm_nat_bits(&a->mag) <= RELAX_LATTICE_BITS;
}

static bool rx_quot(jm_bigint *q, const jm_bigint *a, const jm_bigint *b)
{
    const int32_t s = a->sign * b->sign;
    jm_nat rem = {0};
    const bool ok = jm_nat_divmod(&q->mag, &rem, &a->mag, &b->mag);
    jm_nat_free(&rem);
    if (ok)
        q->sign = q->mag.n == 0 ? 0 : s;
    return ok;
}

static bool rx_axpy(jm_bigint *r, const jm_bigint *f, const jm_bigint *g,
                    const jm_bigint *p)
{
    jm_bigint s = {0}, t = {0};
    const bool ok = jm_bigint_mul(&s, f, r) && rx_fits(&s) &&
                    jm_bigint_mul(&t, g, p) && rx_fits(&t) &&
                    jm_bigint_sub(r, &s, &t) && rx_fits(r);
    jm_bigint_free(&s);
    jm_bigint_free(&t);
    return ok;
}

static bool rx_content(jm_bigint *row, int64_t w)
{
    jm_nat g = {0};
    bool ok = true;
    for (int64_t u = 0; ok && u < w; u++)
        if (row[u].sign != 0)
            ok = jm_nat_gcd(&g, &g, &row[u].mag);
    if (ok && g.n != 0 && !jm_nat_is_one(&g))
        for (int64_t u = 0; ok && u < w; u++)
            if (row[u].sign != 0)
                ok = jm_nat_divmod(&row[u].mag, nullptr, &row[u].mag, &g);
    jm_nat_free(&g);
    return ok;
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
    jm_dyadic d = {0};
    bool ok = true;
    for (int64_t p = 0; ok && p < nb; p++) {
        const int64_t r = rows[p];
        int64_t low = INT64_MAX;
        for (int64_t e = rs[r]; ok && e <= rs[r + 1]; e++) {
            const double v = e < rs[r + 1] ? rv[e] : m->row_lower[r];
            ok = jm_dyadic_from_double(&d, v);
            if (ok && d.m.sign != 0 && d.e < low)
                low = d.e;
        }
        for (int64_t e = rs[r]; ok && e <= rs[r + 1]; e++) {
            const double v = e < rs[r + 1] ? rv[e] : m->row_lower[r];
            const int64_t u = e < rs[r + 1] ? cpos[rj[e]] : w - 1;
            ok = jm_dyadic_from_double(&d, v);
            if (ok && d.m.sign != 0)
                ok = jm_bigint_shl(&a[p * w + u], &d.m, d.e - low) &&
                     rx_fits(&a[p * w + u]);
        }
        ok = ok && rx_content(a + p * w, w);
    }
    jm_dyadic_free(&d);
    return ok;
}

static bool rx_eliminate(const jaos_model *m, jm_bigint *a, int64_t w,
                         int64_t nb, const int64_t *cols, bool *alive,
                         int64_t *work)
{
    jm_bigint g = {0};
    bool ok = true;
    for (int64_t t = 0; ok && t + 1 < w; t++) {
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
        const jm_bigint *f = &a[p * w + t];
        for (int64_t r = 0; ok && r < nb; r++) {
            if (!alive[r] || a[r * w + t].sign == 0)
                continue;
            ok = jm_bigint_copy(&g, &a[r * w + t]);
            for (int64_t u = 0; ok && u < w; u++)
                ok = rx_axpy(&a[r * w + u], f, &g, &a[p * w + u]);
            if (ok)
                *work += w;
            ok = ok && rx_content(a + r * w, w);
        }
    }
    jm_bigint_free(&g);
    return ok;
}

static int rx_hermite(const jaos_model *m, jm_bigint *a, int64_t w,
                      int64_t nb, const int64_t *cols, const bool *alive,
                      bool *open, int64_t *work)
{
    const int64_t nc = w - 1;
    jm_bigint q = {0}, one = {0}, y = {0};
    jm_nat rem = {0};
    jm_bigint_set_i64(&one, 1);
    int res = 0;
    for (int64_t t = 0; t < nc; t++)
        open[t] = rx_is_int(m, cols[t]);
    for (int64_t p = 0; res == 0 && p < nb; p++) {
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
            for (int64_t t = 0; res == 0 && t < nc; t++) {
                if (t == k || !open[t] || row[t].sign == 0)
                    continue;
                more = true;
                if (!rx_quot(&q, &row[t], &row[k]))
                    res = -1;
                for (int64_t r = p; res == 0 && r < nb; r++) {
                    if (!alive[r])
                        continue;
                    if (!rx_axpy(&a[r * w + t], &one, &q, &a[r * w + k]))
                        res = -1;
                }
                if (res == 0)
                    *work += nb - p;
            }
            if (!more || res != 0)
                break;
        }
        if (res != 0)
            break;
        if (k < 0) {
            if (row[nc].sign != 0)
                res = 1;
            continue;
        }
        const int32_t s = row[nc].sign * row[k].sign;
        if (!jm_nat_divmod(&y.mag, &rem, &row[nc].mag, &row[k].mag)) {
            res = -1;
            break;
        }
        if (rem.n != 0) {
            res = 1;
            break;
        }
        y.sign = y.mag.n == 0 ? 0 : s;
        open[k] = false;
        for (int64_t r = p + 1; res == 0 && r < nb; r++) {
            if (!alive[r] || a[r * w + k].sign == 0)
                continue;
            if (!rx_axpy(&a[r * w + nc], &one, &a[r * w + k], &y))
                res = -1;
            jm_bigint_set_zero(&a[r * w + k]);
        }
        if (res == 0)
            *work += nb - p;
    }
    jm_bigint_free(&q);
    jm_bigint_free(&one);
    jm_bigint_free(&y);
    jm_nat_free(&rem);
    return res;
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
    if (a != nullptr)
        for (int64_t k = 0; k < nb * w; k++)
            jm_bigint_free(&a[k]);
    free(a);
    free(alive);
    free(open);
    return proved;
}

static bool rx_range_row_empty(const jaos_model *m, int64_t i)
{
    const int64_t p0 = m->ar_start[i], p1 = m->ar_start[i + 1];
    const double lo = m->row_lower[i], hi = m->row_upper[i];
    jm_dyadic d = {0};
    int64_t low = INT64_MAX;
    bool any = false, ok = true;
    for (int64_t k = p0; ok && k <= p1 + 1; k++) {
        const double v = k < p1 ? m->ar_value[k] : (k == p1 ? lo : hi);
        ok = (k >= p1 || rx_is_int(m, m->ar_index[k])) &&
             jm_dyadic_from_double(&d, v);
        if (ok && d.m.sign != 0 && d.e < low)
            low = d.e;
        any = any || (ok && k < p1 && d.m.sign != 0);
    }
    ok = ok && any;
    jm_nat g = {0}, rem = {0}, one = {0}, gap = {0};
    jm_bigint bl = {0}, bh = {0}, span = {0}, t = {0};
    for (int64_t k = p0; ok && k <= p1 + 1; k++) {
        const double v = k < p1 ? m->ar_value[k] : (k == p1 ? lo : hi);
        ok = jm_dyadic_from_double(&d, v);
        if (!ok || d.m.sign == 0)
            continue;
        ok = jm_bigint_shl(&t, &d.m, d.e - low) && rx_fits(&t);
        if (!ok)
            break;
        if (k < p1)
            ok = jm_nat_gcd(&g, &g, &t.mag);
        else
            jm_bigint_swap(k == p1 ? &bl : &bh, &t);
    }
    jm_nat_set_u64(&one, 1);
    ok = ok && jm_nat_cmp(&g, &one) > 0 && jm_bigint_sub(&span, &bh, &bl) &&
         rx_fits(&span) && jm_nat_divmod(nullptr, &rem, &bl.mag, &g) &&
         rem.n != 0;
    if (ok)
        ok = bl.sign > 0 ? jm_nat_sub(&gap, &g, &rem) : jm_nat_copy(&gap, &rem);
    const bool empty = ok && span.sign >= 0 && jm_nat_cmp(&span.mag, &gap) < 0;
    jm_dyadic_free(&d);
    jm_nat_free(&g);
    jm_nat_free(&rem);
    jm_nat_free(&one);
    jm_nat_free(&gap);
    jm_bigint_free(&bl);
    jm_bigint_free(&bh);
    jm_bigint_free(&span);
    jm_bigint_free(&t);
    return empty;
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

enum { RX_OPEN = -1, RX_POINT = 0, RX_NONE = 1, RX_GO = 2 };
enum { RX_KEEP, RX_DROP, RX_EMPTY, RX_FAIL };

typedef struct {
    int64_t work, budget, cells;
    const bool *isint;
} rx_meter;

typedef struct {
    int64_t n, m, size;
    jm_bigint *a;
    int8_t *kind;
} rx_sys;

static jm_bigint *rx_row(const rx_sys *s, int64_t r)
{
    return s->a + r * (s->n + 1);
}

static bool rx_sys_new(rx_sys *s, rx_meter *mt, int64_t n, int64_t rows)
{
    const int64_t w = n + 1, r = rows > 0 ? rows : 1;
    *s = (rx_sys){.n = n};
    if (r > (RELAX_SHADOW_CELLS - mt->cells) / w)
        return false;
    s->a = calloc((size_t)(r * w), sizeof *s->a);
    s->kind = jm_alloc_array(r, sizeof *s->kind);
    if (s->a == nullptr || s->kind == nullptr) {
        free(s->a);
        free(s->kind);
        *s = (rx_sys){.n = n};
        return false;
    }
    s->size = r * w;
    mt->cells += s->size;
    return true;
}

static void rx_sys_free(rx_sys *s, rx_meter *mt)
{
    for (int64_t k = 0; k < s->size; k++)
        jm_bigint_free(&s->a[k]);
    mt->cells -= s->size;
    free(s->a);
    free(s->kind);
    *s = (rx_sys){.n = s->n};
}

static bool rx_sys_copy(rx_sys *c, const rx_sys *s, rx_meter *mt)
{
    const int64_t cells = s->m * (s->n + 1);
    if (!rx_sys_new(c, mt, s->n, s->m))
        return false;
    bool ok = true;
    for (int64_t k = 0; ok && k < cells; k++)
        ok = jm_bigint_copy(&c->a[k], &s->a[k]);
    if (!ok) {
        rx_sys_free(c, mt);
        return false;
    }
    memcpy(c->kind, s->kind, (size_t)s->m * sizeof *s->kind);
    c->m = s->m;
    mt->work += cells;
    return true;
}

static void rx_compact(rx_sys *s, rx_meter *mt)
{
    const int64_t w = s->n + 1;
    int64_t d = 0;
    for (int64_t r = 0; r < s->m; r++) {
        if (s->kind[r] < 0)
            continue;
        if (d != r) {
            for (int64_t u = 0; u < w; u++)
                jm_bigint_swap(&rx_row(s, d)[u], &rx_row(s, r)[u]);
            s->kind[d] = s->kind[r];
            mt->work += w;
        }
        d++;
    }
    s->m = d;
}

static int rx_tidy(jm_bigint *row, int64_t n, bool eq, const bool *isint)
{
    jm_nat g = {0}, q = {0}, rem = {0}, one = {0};
    jm_bigint *b = &row[n];
    bool integral = true, ok = true;
    for (int64_t t = 0; ok && t < n; t++)
        if (row[t].sign != 0) {
            integral = integral && isint[t];
            ok = jm_nat_gcd(&g, &g, &row[t].mag);
        }
    int v = RX_KEEP;
    if (!ok || (g.n != 0 && !integral && !jm_nat_gcd(&g, &g, &b->mag)))
        v = RX_FAIL;
    else if (g.n == 0)
        v = (eq ? b->sign != 0 : b->sign > 0) ? RX_EMPTY : RX_DROP;
    else if (!jm_nat_is_one(&g)) {
        jm_nat_set_u64(&one, 1);
        if (!jm_nat_divmod(&q, &rem, &b->mag, &g))
            v = RX_FAIL;
        else if (rem.n != 0 && eq)
            v = RX_EMPTY;
        else if (rem.n != 0 && b->sign > 0 && !jm_nat_add(&q, &q, &one))
            v = RX_FAIL;
        if (v == RX_KEEP) {
            jm_nat_swap(&b->mag, &q);
            if (b->mag.n == 0)
                b->sign = 0;
            for (int64_t t = 0; v == RX_KEEP && t < n; t++)
                if (row[t].sign != 0 &&
                    !jm_nat_divmod(&row[t].mag, nullptr, &row[t].mag, &g))
                    v = RX_FAIL;
        }
    }
    jm_nat_free(&g);
    jm_nat_free(&q);
    jm_nat_free(&rem);
    jm_nat_free(&one);
    return v;
}

static int rx_tidy_all(rx_sys *s, rx_meter *mt)
{
    for (int64_t r = 0; r < s->m; r++) {
        mt->work += s->n + 1;
        const int v = rx_tidy(rx_row(s, r), s->n, s->kind[r] == 1,
                              mt->isint);
        if (v == RX_EMPTY)
            return RX_NONE;
        if (v == RX_FAIL)
            return RX_OPEN;
        if (v == RX_DROP)
            s->kind[r] = -1;
    }
    rx_compact(s, mt);
    return RX_GO;
}

static int rx_order(const rx_sys *s, const int32_t *lead, int64_t i,
                    int64_t j)
{
    const jm_bigint *x = rx_row(s, i), *y = rx_row(s, j);
    for (int64_t t = 0; t < s->n; t++) {
        const int32_t a = x[t].sign * lead[i], b = y[t].sign * lead[j];
        if (a != b)
            return a < b ? -1 : 1;
        if (a == 0)
            continue;
        const int c = jm_nat_cmp(&x[t].mag, &y[t].mag);
        if (c != 0)
            return a > 0 ? c : -c;
    }
    return 0;
}

static void rx_sort(const rx_sys *s, const int32_t *lead, int64_t *idx,
                    int64_t *tmp, int64_t k, rx_meter *mt)
{
    for (int64_t width = 1; width < k; width *= 2) {
        for (int64_t lo = 0; lo < k; lo += 2 * width) {
            const int64_t mid = lo + width < k ? lo + width : k;
            const int64_t hi = lo + 2 * width < k ? lo + 2 * width : k;
            int64_t a = lo, b = mid, o = lo;
            while (a < mid && b < hi)
                tmp[o++] = rx_order(s, lead, idx[b], idx[a]) < 0 ? idx[b++]
                                                                 : idx[a++];
            while (a < mid)
                tmp[o++] = idx[a++];
            while (b < hi)
                tmp[o++] = idx[b++];
        }
        memcpy(idx, tmp, (size_t)k * sizeof *idx);
        mt->work += k * s->n;
    }
}

static int rx_merge(rx_sys *s, int64_t h, int64_t up)
{
    const int64_t n = s->n;
    jm_bigint sum = {0};
    int v = RX_GO;
    if (!jm_bigint_add(&sum, &rx_row(s, h)[n], &rx_row(s, up)[n]))
        v = RX_OPEN;
    else if (sum.sign > 0)
        v = RX_NONE;
    else if (sum.sign == 0) {
        s->kind[h] = 1;
        s->kind[up] = -1;
    }
    jm_bigint_free(&sum);
    return v;
}

static int rx_pairs(rx_sys *s, rx_meter *mt)
{
    const int64_t m = s->m, n = s->n, cap = m > 0 ? m : 1;
    int64_t *idx = jm_alloc_array(cap, sizeof *idx);
    int64_t *tmp = jm_alloc_array(cap, sizeof *tmp);
    int32_t *lead = jm_alloc_array(cap, sizeof *lead);
    int v = RX_OPEN;
    if (idx == nullptr || tmp == nullptr || lead == nullptr)
        goto out;
    int64_t k = 0;
    for (int64_t r = 0; r < m; r++) {
        lead[r] = 0;
        if (s->kind[r] != 0)
            continue;
        const jm_bigint *row = rx_row(s, r);
        for (int64_t t = 0; t < n && lead[r] == 0; t++)
            lead[r] = row[t].sign;
        idx[k++] = r;
    }
    rx_sort(s, lead, idx, tmp, k, mt);
    v = RX_GO;
    for (int64_t g = 0; g < k && v == RX_GO;) {
        const int64_t h = idx[g];
        int64_t e = g + 1, up = -1;
        for (; e < k && rx_order(s, lead, h, idx[e]) == 0; e++) {
            const int64_t r = idx[e];
            if (lead[r] != lead[h] && up < 0) {
                up = r;
                continue;
            }
            jm_bigint *kb = &rx_row(s, lead[r] == lead[h] ? h : up)[n];
            if (jm_bigint_cmp(&rx_row(s, r)[n], kb) > 0)
                jm_bigint_swap(&rx_row(s, r)[n], kb);
            s->kind[r] = -1;
        }
        mt->work += (e - g) * n;
        if (up >= 0)
            v = rx_merge(s, h, up);
        g = e;
    }
    if (v == RX_GO)
        rx_compact(s, mt);
out:
    free(idx);
    free(tmp);
    free(lead);
    return v;
}

static int rx_pivot(rx_sys *s, rx_meter *mt, int64_t p, int64_t t)
{
    const int64_t w = s->n + 1;
    jm_bigint f = {0}, g = {0};
    jm_bigint *pr = rx_row(s, p);
    bool ok = jm_bigint_copy(&f, &pr[t]);
    const int32_t fs = f.sign;
    f.sign = 1;
    for (int64_t r = 0; ok && r < s->m; r++) {
        jm_bigint *row = rx_row(s, r);
        if (r == p || row[t].sign == 0)
            continue;
        ok = jm_bigint_copy(&g, &row[t]);
        g.sign *= fs;
        for (int64_t u = 0; ok && u < w; u++)
            ok = rx_axpy(&row[u], &f, &g, &pr[u]);
        mt->work += w;
    }
    jm_bigint_free(&f);
    jm_bigint_free(&g);
    if (!ok)
        return RX_OPEN;
    s->kind[p] = -1;
    rx_compact(s, mt);
    return RX_GO;
}

static int rx_unimodular(rx_sys *s, rx_meter *mt, int64_t p)
{
    const int64_t n = s->n, m = s->m;
    jm_bigint *pr = rx_row(s, p);
    jm_bigint one = {0}, q = {0}, y = {0};
    jm_nat rem = {0};
    jm_bigint_set_i64(&one, 1);
    int v = RX_GO;
    int64_t k = -1;
    for (bool more = true; more && v == RX_GO;) {
        k = -1;
        for (int64_t u = 0; u < n; u++)
            if (pr[u].sign != 0 && (k < 0 || rx_smaller(&pr[u], &pr[k])))
                k = u;
        more = false;
        for (int64_t u = 0; k >= 0 && u < n && v == RX_GO; u++) {
            if (u == k || pr[u].sign == 0)
                continue;
            more = true;
            if (!rx_quot(&q, &pr[u], &pr[k]))
                v = RX_OPEN;
            for (int64_t r = 0; r < m && v == RX_GO; r++) {
                jm_bigint *row = rx_row(s, r);
                if (row[k].sign != 0 && !rx_axpy(&row[u], &one, &q, &row[k]))
                    v = RX_OPEN;
            }
            mt->work += m;
        }
    }
    if (v == RX_GO && k < 0)
        v = RX_OPEN;
    if (v == RX_GO) {
        const int32_t sy = pr[n].sign * pr[k].sign;
        if (!jm_nat_divmod(&y.mag, &rem, &pr[n].mag, &pr[k].mag))
            v = RX_OPEN;
        else if (rem.n != 0)
            v = RX_NONE;
        y.sign = y.mag.n == 0 ? 0 : sy;
    }
    for (int64_t r = 0; r < m && v == RX_GO; r++) {
        jm_bigint *row = rx_row(s, r);
        if (r == p || row[k].sign == 0)
            continue;
        if (!rx_axpy(&row[n], &one, &row[k], &y))
            v = RX_OPEN;
        jm_bigint_set_zero(&row[k]);
    }
    mt->work += m;
    jm_bigint_free(&one);
    jm_bigint_free(&q);
    jm_bigint_free(&y);
    jm_nat_free(&rem);
    if (v == RX_GO) {
        s->kind[p] = -1;
        rx_compact(s, mt);
    }
    return v;
}

static int rx_equality(rx_sys *s, rx_meter *mt)
{
    for (int64_t t = 0; t < s->n; t++) {
        if (mt->isint[t])
            continue;
        int64_t p = -1;
        for (int64_t r = 0; r < s->m; r++)
            if (s->kind[r] == 1 && rx_row(s, r)[t].sign != 0 &&
                (p < 0 || rx_smaller(&rx_row(s, r)[t], &rx_row(s, p)[t])))
                p = r;
        mt->work += s->m;
        if (p >= 0)
            return rx_pivot(s, mt, p, t);
    }
    int64_t p = 0;
    while (s->kind[p] != 1)
        p++;
    return rx_unimodular(s, mt, p);
}

static bool rx_combine(jm_bigint *out, const jm_bigint *lo,
                       const jm_bigint *hi, int64_t t, int64_t w, bool dark)
{
    jm_bigint a = {0}, one = {0}, x = {0}, y = {0};
    jm_bigint_set_i64(&one, 1);
    bool ok = jm_bigint_copy(&a, &hi[t]);
    a.sign = 1;
    for (int64_t k = 0; ok && k < w; k++)
        ok = jm_bigint_mul(&x, &a, &lo[k]) && rx_fits(&x) &&
             jm_bigint_mul(&y, &lo[t], &hi[k]) && rx_fits(&y) &&
             jm_bigint_add(&out[k], &x, &y) && rx_fits(&out[k]);
    if (ok && dark)
        ok = jm_bigint_sub(&a, &a, &one) && jm_bigint_sub(&x, &lo[t], &one) &&
             jm_bigint_mul(&y, &a, &x) &&
             jm_bigint_add(&out[w - 1], &out[w - 1], &y) &&
             rx_fits(&out[w - 1]);
    jm_bigint_free(&a);
    jm_bigint_free(&one);
    jm_bigint_free(&x);
    jm_bigint_free(&y);
    return ok;
}

static int rx_shadow(const rx_sys *s, rx_meter *mt, int64_t t, bool dark,
                     rx_sys *o)
{
    const int64_t n = s->n, w = n + 1, m = s->m;
    int64_t p = 0, q = 0;
    for (int64_t r = 0; r < m; r++) {
        const int32_t c = rx_row(s, r)[t].sign;
        p += c > 0;
        q += c < 0;
    }
    if (!rx_sys_new(o, mt, n, m - p - q + p * q))
        return RX_OPEN;
    int64_t d = 0;
    int v = RX_GO;
    for (int64_t r = 0; r < m && v == RX_GO; r++)
        if (rx_row(s, r)[t].sign == 0) {
            for (int64_t u = 0; u < w && v == RX_GO; u++)
                if (!jm_bigint_copy(&rx_row(o, d)[u], &rx_row(s, r)[u]))
                    v = RX_OPEN;
            o->kind[d++] = 0;
            mt->work += w;
        }
    for (int64_t l = 0; l < m && v == RX_GO; l++) {
        const jm_bigint *lo = rx_row(s, l);
        if (lo[t].sign <= 0)
            continue;
        for (int64_t u = 0; u < m && v == RX_GO; u++) {
            const jm_bigint *hi = rx_row(s, u);
            if (hi[t].sign >= 0)
                continue;
            jm_bigint *out = rx_row(o, d);
            mt->work += w;
            if (!rx_combine(out, lo, hi, t, w, dark)) {
                v = RX_OPEN;
                break;
            }
            const int c = rx_tidy(out, n, false, mt->isint);
            if (c == RX_EMPTY)
                v = RX_NONE;
            else if (c == RX_FAIL)
                v = RX_OPEN;
            else if (c == RX_KEEP)
                o->kind[d++] = 0;
        }
    }
    o->m = d;
    if (v != RX_GO)
        rx_sys_free(o, mt);
    return v;
}

static int64_t rx_reach(int64_t big, int64_t c)
{
    const int64_t num = big * c - big - c;
    return num < 0 ? -1 : num / big;
}

static int64_t rx_small(const jm_bigint *c)
{
    const jm_dyadic d = {.m = {.mag = c->mag, .sign = c->sign != 0}};
    return (int64_t)jm_dyadic_to_double(&d);
}

static int rx_prove(rx_sys *s, rx_meter *mt);

static int rx_splinters(const rx_sys *s, rx_meter *mt, int64_t t)
{
    const int64_t n = s->n, m = s->m;
    int64_t big[2] = {0, 0}, count[2] = {0, 0};
    for (int64_t r = 0; r < m; r++) {
        const jm_bigint *c = &rx_row(s, r)[t];
        if (c->sign == 0)
            continue;
        if (jm_nat_bits(&c->mag) > 31)
            return RX_OPEN;
        const int side = c->sign > 0 ? 0 : 1;
        if (rx_small(c) > big[side])
            big[side] = rx_small(c);
    }
    for (int64_t r = 0; r < m; r++) {
        const jm_bigint *c = &rx_row(s, r)[t];
        if (c->sign != 0) {
            const int side = c->sign > 0 ? 0 : 1;
            count[side] += 1 + rx_reach(big[1 - side], rx_small(c));
        }
    }
    const int side = count[1] < count[0] ? 1 : 0;
    if (count[side] > (mt->budget - mt->work) / (m * (n + 1)))
        return RX_OPEN;
    jm_bigint add = {0};
    int v = RX_NONE;
    for (int64_t r = 0; r < m && v == RX_NONE; r++) {
        const jm_bigint *c = &rx_row(s, r)[t];
        if (c->sign != (side == 0 ? 1 : -1))
            continue;
        const int64_t reach = rx_reach(big[1 - side], rx_small(c));
        for (int64_t k = 0; k <= reach && v == RX_NONE; k++) {
            rx_sys sp;
            if (mt->work >= mt->budget || !rx_sys_copy(&sp, s, mt)) {
                v = RX_OPEN;
                break;
            }
            jm_bigint_set_i64(&add, k);
            sp.kind[r] = 1;
            jm_bigint *b = &rx_row(&sp, r)[n];
            if (!jm_bigint_add(b, b, &add)) {
                rx_sys_free(&sp, mt);
                v = RX_OPEN;
                break;
            }
            v = rx_prove(&sp, mt);
        }
    }
    jm_bigint_free(&add);
    return v;
}

static bool rx_unit(const jm_bigint *c)
{
    return jm_nat_is_one(&c->mag);
}

static int rx_step(rx_sys *s, rx_meter *mt)
{
    const int64_t n = s->n, m = s->m;
    if (m == 0)
        return RX_POINT;
    int64_t best = -1, bp = 0, bq = 0;
    int brank = 4;
    for (int64_t t = 0; t < n; t++) {
        int64_t p = 0, q = 0;
        bool lone = true, uone = true;
        for (int64_t r = 0; r < m; r++) {
            const jm_bigint *c = &rx_row(s, r)[t];
            if (c->sign > 0) {
                p++;
                lone = lone && rx_unit(c);
            } else if (c->sign < 0) {
                q++;
                uone = uone && rx_unit(c);
            }
        }
        if (p + q == 0)
            continue;
        const int rank = p == 0 || q == 0 ? 0
                       : !mt->isint[t]    ? 1
                       : lone || uone     ? 2 : 3;
        if (best < 0 || rank < brank || (rank == brank && p * q < bp * bq)) {
            best = t;
            brank = rank;
            bp = p;
            bq = q;
        }
    }
    mt->work += n * m;
    if (best < 0)
        return RX_POINT;
    if (brank == 0) {
        for (int64_t r = 0; r < m; r++)
            if (rx_row(s, r)[best].sign != 0)
                s->kind[r] = -1;
        rx_compact(s, mt);
        return RX_GO;
    }
    rx_sys o;
    int v = rx_shadow(s, mt, best, false, &o);
    if (brank < 3) {
        if (v == RX_GO) {
            rx_sys_free(s, mt);
            *s = o;
        }
        return v;
    }
    if (v == RX_NONE || (v == RX_GO && rx_prove(&o, mt) == RX_NONE))
        return RX_NONE;
    v = rx_shadow(s, mt, best, true, &o);
    if (v == RX_OPEN)
        return RX_OPEN;
    if (v == RX_GO && (v = rx_prove(&o, mt)) != RX_NONE)
        return v;
    return rx_splinters(s, mt, best);
}

static int rx_prove(rx_sys *s, rx_meter *mt)
{
    int v = RX_GO;
    while (v == RX_GO) {
        if (mt->work >= mt->budget) {
            v = RX_OPEN;
            break;
        }
        v = rx_tidy_all(s, mt);
        if (v == RX_GO)
            v = rx_pairs(s, mt);
        if (v != RX_GO)
            break;
        bool eq = false;
        for (int64_t r = 0; r < s->m && !eq; r++)
            eq = s->kind[r] == 1;
        v = eq ? rx_equality(s, mt) : rx_step(s, mt);
    }
    rx_sys_free(s, mt);
    return v;
}

static bool rx_rows_fill(const jaos_model *m, rx_sys *s, const int64_t *rows,
                         int64_t nb, const int8_t *side, const int64_t *cpos)
{
    const int64_t n = s->n;
    jm_dyadic d = {0};
    jm_bigint t = {0}, bl = {0}, bh = {0};
    bool ok = true;
    int64_t g = 0;
    for (int64_t p = 0; ok && p < nb; p++) {
        const int64_t i = rows[p], k0 = m->ar_start[i], k1 = m->ar_start[i + 1];
        const double lo = m->row_lower[i], hi = m->row_upper[i];
        int64_t low = INT64_MAX;
        for (int64_t k = k0; ok && k <= k1 + 1; k++) {
            const double v = k < k1 ? m->ar_value[k] : k == k1 ? lo : hi;
            if (!isfinite(v))
                continue;
            ok = jm_dyadic_from_double(&d, v);
            if (ok && d.m.sign != 0 && d.e < low)
                low = d.e;
        }
        jm_bigint *ra = rx_row(s, g);
        jm_bigint_set_zero(&bl);
        jm_bigint_set_zero(&bh);
        for (int64_t k = k0; ok && k <= k1 + 1; k++) {
            const double v = k < k1 ? m->ar_value[k] : k == k1 ? lo : hi;
            if (!isfinite(v))
                continue;
            ok = jm_dyadic_from_double(&d, v);
            if (!ok || d.m.sign == 0)
                continue;
            jm_bigint *to = k < k1 ? &ra[cpos[m->ar_index[k]]]
                          : k == k1 ? &bl : &bh;
            ok = jm_bigint_shl(&t, &d.m, d.e - low) && rx_fits(&t) &&
                 jm_bigint_add(to, to, &t) && rx_fits(to);
        }
        if (!ok)
            break;
        if (side[i] == 2) {
            for (int64_t u = 0; u < n; u++)
                jm_bigint_neg(&ra[u]);
            jm_bigint_neg(&bh);
            jm_bigint_swap(&ra[n], &bh);
            s->kind[g++] = 0;
        } else if (side[i] == 1 || lo == hi) {
            jm_bigint_swap(&ra[n], &bl);
            s->kind[g++] = side[i] == 1 ? 0 : 1;
        } else {
            jm_bigint_swap(&ra[n], &bl);
            s->kind[g++] = 0;
            jm_bigint *rb = rx_row(s, g);
            for (int64_t u = 0; ok && u < n; u++) {
                ok = jm_bigint_copy(&rb[u], &ra[u]);
                jm_bigint_neg(&rb[u]);
            }
            jm_bigint_neg(&bh);
            jm_bigint_swap(&rb[n], &bh);
            s->kind[g++] = 0;
        }
    }
    s->m = g;
    jm_dyadic_free(&d);
    jm_bigint_free(&t);
    jm_bigint_free(&bl);
    jm_bigint_free(&bh);
    return ok;
}

static int rx_rows_block(const jaos_model *m, const int64_t *rows, int64_t nb,
                         const int8_t *side, int64_t *cpos, int64_t *cols,
                         bool *isint, rx_meter *mt)
{
    int64_t n = 0, ng = 0;
    bool any_int = false;
    for (int64_t p = 0; p < nb; p++) {
        const int64_t i = rows[p];
        ng += side[i] == 3 && m->row_lower[i] != m->row_upper[i] ? 2 : 1;
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
            const int64_t j = m->ar_index[k];
            if (m->ar_value[k] == 0.0 || cpos[j] >= 0)
                continue;
            cpos[j] = n;
            cols[n] = j;
            isint[n] = rx_is_int(m, j);
            any_int = any_int || isint[n];
            n++;
        }
    }
    int v = RX_OPEN;
    rx_sys s;
    if (any_int && ng <= RELAX_LATTICE_CELLS / (n + 1) &&
        rx_sys_new(&s, mt, n, ng)) {
        mt->isint = isint;
        if (rx_rows_fill(m, &s, rows, nb, side, cpos))
            v = rx_prove(&s, mt);
        else
            rx_sys_free(&s, mt);
    }
    for (int64_t t = 0; t < n; t++)
        cpos[cols[t]] = -1;
    return v;
}

static bool rx_loose(const int64_t *cnt, bool cont)
{
    const int64_t tot = cnt[0] + cnt[1] + cnt[2];
    return tot > 0 && ((cnt[2] == 0 && (cnt[0] == 0 || cnt[1] == 0)) ||
                       (cont && tot == 1));
}

static void rx_count(const jaos_model *m, int64_t i, const int8_t *side,
                     int64_t *cnt, int64_t by)
{
    for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1]; k++) {
        const double v = m->ar_value[k];
        if (v == 0.0)
            continue;
        const int64_t j = m->ar_index[k];
        const int slot = side[i] == 3 ? 2 : (side[i] == 1) == (v > 0.0) ? 0 : 1;
        cnt[3 * j + slot] += by;
    }
}

static int rx_rows_empty(jaos_model *m, int64_t *work, int64_t budget)
{
    const int64_t nr = m->num_row, nc = m->num_col;
    if (nr == 0 || nc == 0 || budget <= 0 ||
        jm_model_ensure_rowwise(m) != JAOS_OK)
        return RX_OPEN;
    rx_meter mt = {.budget = budget};
    int v = RX_OPEN;
    int8_t *side = jm_alloc_array(nr, sizeof *side);
    int64_t *cnt = calloc((size_t)(3 * nc), sizeof *cnt);
    int64_t *queue = jm_alloc_array(nc, sizeof *queue);
    bool *queued = calloc((size_t)nc, sizeof *queued);
    int64_t *up = jm_alloc_array(nr, sizeof *up);
    int64_t *at = jm_alloc_array(nr + 1, sizeof *at);
    int64_t *order = jm_alloc_array(nr, sizeof *order);
    int64_t *cpos = jm_alloc_array(nc, sizeof *cpos);
    int64_t *cols = jm_alloc_array(nc, sizeof *cols);
    bool *isint = jm_alloc_array(nc, sizeof *isint);
    if (side == nullptr || cnt == nullptr || queue == nullptr ||
        queued == nullptr || up == nullptr || at == nullptr ||
        order == nullptr || cpos == nullptr || cols == nullptr ||
        isint == nullptr)
        goto out;
    for (int64_t i = 0; i < nr; i++) {
        side[i] = 0;
        if (m->row_ind_col != nullptr && m->row_ind_col[i] >= 0)
            continue;
        bool any = false;
        for (int64_t k = m->ar_start[i]; k < m->ar_start[i + 1] && !any; k++)
            any = m->ar_value[k] != 0.0;
        if (any)
            side[i] = (int8_t)((isfinite(m->row_lower[i]) ? 1 : 0) |
                               (isfinite(m->row_upper[i]) ? 2 : 0));
        if (side[i] != 0)
            rx_count(m, i, side, cnt, 1);
    }
    int64_t qh = 0, qt = 0;
    for (int64_t j = 0; j < nc; j++) {
        cpos[j] = -1;
        if (rx_loose(cnt + 3 * j, !rx_is_int(m, j))) {
            queue[qt++] = j;
            queued[j] = true;
        }
    }
    while (qh < qt) {
        const int64_t j = queue[qh++];
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (m->a_value[k] == 0.0 || side[i] == 0)
                continue;
            rx_count(m, i, side, cnt, -1);
            side[i] = 0;
            for (int64_t e = m->ar_start[i]; e < m->ar_start[i + 1]; e++) {
                const int64_t c = m->ar_index[e];
                if (!queued[c] && rx_loose(cnt + 3 * c, !rx_is_int(m, c))) {
                    queue[qt++] = c;
                    queued[c] = true;
                }
            }
            mt.work += m->ar_start[i + 1] - m->ar_start[i];
        }
    }
    for (int64_t i = 0; i < nr; i++)
        up[i] = i;
    for (int64_t j = 0; j < nc; j++) {
        int64_t first = -1;
        for (int64_t k = m->a_start[j]; k < m->a_start[j + 1]; k++) {
            const int64_t i = m->a_index[k];
            if (m->a_value[k] == 0.0 || side[i] == 0)
                continue;
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
    for (int64_t i = 0; i <= nr; i++)
        at[i] = 0;
    for (int64_t i = 0; i < nr; i++)
        if (side[i] != 0) {
            up[i] = rx_find(up, i);
            at[up[i] + 1]++;
        }
    for (int64_t i = 0; i < nr; i++)
        at[i + 1] += at[i];
    for (int64_t i = 0; i < nr; i++)
        if (side[i] != 0)
            order[at[up[i]]++] = i;
    v = RX_POINT;
    for (int64_t i = 0, b = 0; i < nr && v != RX_NONE; i++) {
        if (side[i] == 0 || up[i] != i)
            continue;
        const int64_t e = at[i];
        const int r = rx_rows_block(m, order + b, e - b, side, cpos, cols,
                                    isint, &mt);
        if (r != RX_POINT)
            v = r;
        b = e;
    }
out:
    free(side); free(cnt); free(queue); free(queued); free(up); free(at);
    free(order); free(cpos); free(cols); free(isint);
    *work += mt.work;
    return v;
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
        int64_t budget = RELAX_SHADOW_WORK;
        if (m->cfg.work_limit > 0 && m->cfg.work_limit - used < budget)
            budget = m->cfg.work_limit - used;
        const bool rows = cols_only && !lattice && range_row < 0 &&
                          rx_rows_empty(m, &used, budget) == RX_NONE;
        out->work_units = used;
        if (lattice || range_row >= 0 || rows)
            out->status = JAOS_SOLVE_INFEASIBLE;
        if (rows) {
            jm_set_err(m, "no point in the columns' box widened by %.6g, "
                          "after %lld rounds and %lld work units, and none "
                          "in any box: the rows together, with the "
                          "continuous columns projected out, leave no "
                          "integer point",
                       width, (long long)(rounds + 1), (long long)used);
        } else if (range_row >= 0) {
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
