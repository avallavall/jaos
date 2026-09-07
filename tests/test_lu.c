/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

constexpr int    MAXN      = 40;
constexpr double PIVOT_TOL = 0.1;

static uint64_t rng;

static void rng_seed(uint64_t s) { rng = s ? s : 0x9e3779b97f4a7c15u; }

static uint64_t rng_next(void)
{
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    return rng * 0x2545f4914f6cdd1du;
}

static double rng_val(void)
{
    double u = (double)(rng_next() >> 11) / 9007199254740992.0;
    double v = 2.0 * u - 1.0;
    return fabs(v) < 0.05 ? (v < 0 ? -0.05 : 0.05) : v;
}

typedef struct {
    int64_t n;
    double a[MAXN][MAXN];
    int64_t start[MAXN + 1];
    int64_t index[MAXN * MAXN];
    double value[MAXN * MAXN];
    int64_t nnz;
} mat;

static void mat_pack(mat *m)
{
    m->nnz = 0;
    for (int64_t j = 0; j < m->n; j++) {
        m->start[j] = m->nnz;
        for (int64_t i = 0; i < m->n; i++)
            if (m->a[i][j] != 0.0) {
                m->index[m->nnz] = i;
                m->value[m->nnz] = m->a[i][j];
                m->nnz++;
            }
    }
    m->start[m->n] = m->nnz;
}

static void mat_mul(const mat *m, const double *x, double *out)
{
    for (int64_t i = 0; i < m->n; i++) {
        double s = 0.0;
        for (int64_t j = 0; j < m->n; j++)
            s += m->a[i][j] * x[j];
        out[i] = s;
    }
}

static void mat_mul_t(const mat *m, const double *x, double *out)
{
    for (int64_t j = 0; j < m->n; j++) {
        double s = 0.0;
        for (int64_t i = 0; i < m->n; i++)
            s += m->a[i][j] * x[i];
        out[j] = s;
    }
}

static void must_factor(const mat *m, jm_lu *lu, jm_work *w)
{
    jm_lu_init(lu);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(lu, m->n, m->start, m->index,
                                                m->value, PIVOT_TOL, w));
}

static double max_abs_diff(const double *a, const double *b, int64_t n)
{
    double d = 0.0;
    for (int64_t i = 0; i < n; i++) {
        double t = fabs(a[i] - b[i]);
        if (t > d)
            d = t;
    }
    return d;
}

static double solve_residual(const mat *m, jm_lu *lu, jm_work *w)
{
    double b[MAXN], x[MAXN], back[MAXN];
    double worst = 0.0;

    for (int64_t i = 0; i < m->n; i++)
        b[i] = rng_val() * 4.0;

    memcpy(x, b, sizeof(double) * (size_t)m->n);
    jm_lu_ftran(lu, x, w);
    mat_mul(m, x, back);
    worst = max_abs_diff(back, b, m->n);

    memcpy(x, b, sizeof(double) * (size_t)m->n);
    jm_lu_btran(lu, x, w);
    mat_mul_t(m, x, back);
    double t = max_abs_diff(back, b, m->n);
    return t > worst ? t : worst;
}

static void make_random(mat *m, int64_t n, double density)
{
    m->n = n;
    memset(m->a, 0, sizeof m->a);
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++)
            if (i == j)
                m->a[i][j] = 4.0 + rng_val();
            else if ((double)(rng_next() >> 11) / 9007199254740992.0 < density)
                m->a[i][j] = rng_val();
    mat_pack(m);
}

static void test_identity_factors_and_solves_exactly(void)
{
    mat m;
    m.n = 6;
    memset(m.a, 0, sizeof m.a);
    for (int64_t i = 0; i < m.n; i++)
        m.a[i][i] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

    double x[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        x[i] = (double)(i + 1);
    jm_lu_ftran(&lu, x, &w);
    for (int64_t i = 0; i < m.n; i++)
        TEST_ASSERT_EQUAL_DOUBLE((double)(i + 1), x[i]);

    jm_lu_free(&lu);
}

static void test_permutation_matrix(void)
{
    const int64_t perm[] = {3, 0, 4, 1, 2};
    mat m;
    m.n = 5;
    memset(m.a, 0, sizeof m.a);
    for (int64_t j = 0; j < m.n; j++)
        m.a[perm[j]][j] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

    rng_seed(11);
    TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-12);
    jm_lu_free(&lu);
}

static void test_triangular_matrices(void)
{
    for (int upper = 0; upper <= 1; upper++) {
        mat m;
        m.n = 8;
        memset(m.a, 0, sizeof m.a);
        rng_seed(20 + (uint64_t)upper);
        for (int64_t i = 0; i < m.n; i++)
            for (int64_t j = 0; j < m.n; j++)
                if (i == j)
                    m.a[i][j] = 2.0 + rng_val();
                else if (upper ? (j > i) : (i > j))
                    m.a[i][j] = rng_val();
        mat_pack(&m);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
        TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-9);
        jm_lu_free(&lu);
    }
}

static void test_random_matrices_solve_correctly(void)
{
    const double densities[] = {0.05, 0.2, 0.6};
    int cases = 0;

    for (int64_t n = 1; n <= 25; n++) {
        for (int d = 0; d < 3; d++) {
            rng_seed((uint64_t)(n * 100 + d + 1));
            mat m;
            make_random(&m, n, densities[d]);

            jm_lu lu;
            jm_work w = {0};
            must_factor(&m, &lu, &w);
            TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

            for (int rep = 0; rep < 3; rep++)
                TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);

            jm_lu_free(&lu);
            cases++;
        }
    }
    TEST_ASSERT_EQUAL_INT(75, cases);
}

static void test_btran_unit_vectors_solve_exactly(void)
{
    const double densities[] = {0.05, 0.2, 0.6};

    for (int64_t n = 2; n <= 25; n += 7) {
        for (int d = 0; d < 3; d++) {
            rng_seed((uint64_t)(n * 1000 + d + 7));
            mat m;
            make_random(&m, n, densities[d]);

            jm_lu lu;
            jm_work w = {0};
            must_factor(&m, &lu, &w);
            TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

            for (int64_t k = 0; k < m.n; k++) {
                double x[MAXN] = {0}, back[MAXN];
                x[k] = 1.0;
                jm_lu_btran(&lu, x, &w);
                mat_mul_t(&m, x, back);
                for (int64_t i = 0; i < m.n; i++)
                    TEST_ASSERT_DOUBLE_WITHIN(1e-8, i == k ? 1.0 : 0.0,
                                              back[i]);
            }
            jm_lu_free(&lu);
        }
    }
}

static void test_dense_matrices(void)
{
    for (int64_t n = 2; n <= 12; n++) {
        rng_seed((uint64_t)(500 + n));
        mat m;
        make_random(&m, n, 1.0);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
        TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);
        jm_lu_free(&lu);
    }
}

static void test_singular_matrices_are_reported_not_hidden(void)
{

    {
        mat m;
        m.n = 4;
        memset(m.a, 0, sizeof m.a);
        for (int64_t i = 0; i < m.n; i++)
            m.a[i][i] = 1.0;
        m.a[2][2] = 0.0;
        mat_pack(&m);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        TEST_ASSERT_EQUAL_INT64(3, lu.rank);
        jm_lu_free(&lu);
    }

    {
        mat m;
        m.n = 3;
        memset(m.a, 0, sizeof m.a);
        m.a[0][0] = 1.0; m.a[1][0] = 2.0;
        m.a[0][1] = 1.0; m.a[1][1] = 2.0;
        m.a[2][2] = 5.0;
        mat_pack(&m);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        TEST_ASSERT_TRUE(lu.rank < m.n);
        jm_lu_free(&lu);
    }
}

static void test_markowitz_prefers_singletons(void)
{

    mat m;
    m.n = 4;
    rng_seed(77);
    for (int64_t i = 0; i < m.n; i++)
        for (int64_t j = 0; j < m.n; j++)
            m.a[i][j] = 1.0 + rng_val();
    for (int64_t i = 0; i < m.n; i++)
        m.a[i][0] = 0.0;
    m.a[2][0] = 3.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
    TEST_ASSERT_EQUAL_INT64(0, lu.perm_col[0]);
    TEST_ASSERT_EQUAL_INT64(2, lu.perm_row[0]);
    jm_lu_free(&lu);
}

static void test_work_counter_moves_and_repeats(void)
{
    rng_seed(999);
    mat m;
    make_random(&m, 12, 0.4);

    jm_lu lu;
    jm_lu_init(&lu);
    jm_work w1 = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w1));

    TEST_ASSERT_TRUE(w1.units >= JM_WORK_FACTOR);

    double x[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        x[i] = 1.0;
    int64_t before = w1.units;
    jm_lu_ftran(&lu, x, &w1);
    TEST_ASSERT_TRUE(w1.units > before);
    jm_lu_free(&lu);

    jm_lu lu2;
    jm_lu_init(&lu2);
    jm_work w2 = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu2, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w2));
    for (int64_t i = 0; i < m.n; i++)
        x[i] = 1.0;
    jm_lu_ftran(&lu2, x, &w2);
    TEST_ASSERT_EQUAL_INT64(w1.units, w2.units);
    jm_lu_free(&lu2);
}

static void test_factor_rejects_bad_arguments(void)
{
    mat m;
    m.n = 2;
    memset(m.a, 0, sizeof m.a);
    m.a[0][0] = 1.0;
    m.a[1][1] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_lu_init(&lu);
    jm_work w = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(nullptr, m.n, m.start, m.index, m.value, PIVOT_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(&lu, -1, m.start, m.index, m.value, PIVOT_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(&lu, m.n, m.start, m.index, m.value, 0.0, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(&lu, m.n, m.start, m.index, m.value, 1.5, &w));

    int64_t bad_index[] = {0, 9};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(&lu, m.n, m.start, bad_index, m.value, PIVOT_TOL, &w));

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jm_lu_factor(&lu, m.n, m.start, m.index, m.value, PIVOT_TOL, nullptr));
    jm_lu_free(&lu);
}

static void fill_pm1(mat *m, int64_t n, const int *vals)
{
    m->n = n;
    memset(m->a, 0, sizeof m->a);
    for (int64_t i = 0; i < n; i++)
        for (int64_t j = 0; j < n; j++)
            m->a[i][j] = (double)vals[i * n + j];
    mat_pack(m);
}

static void test_exact_cancellation_does_not_corrupt_state(void)
{
    static const int v[36] = {
         1,  1, -1,  1,  0,  0,
        -1,  1,  0,  0,  1, -1,
         1,  0,  1,  0,  1, -1,
         0, -1, -1,  1,  0,  0,
         0, -1, -1,  0,  1, -1,
         0, -1,  1,  1,  1, -1,
    };
    mat m;
    fill_pm1(&m, 6, v);

    jm_lu lu;
    jm_lu_init(&lu);
    jm_work w = {0};

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w));
    TEST_ASSERT_EQUAL_INT64(5, lu.rank);
    jm_lu_free(&lu);
}

static void test_nonsingular_pm1_matrix_reaches_full_rank(void)
{
    static const int v[64] = {
         1,  0, -1,  0,  0,  0,  1,  1,
         0, -1, -1, -1,  0,  1,  0,  1,
         0,  0,  1,  0,  0, -1,  0, -1,
         0,  0,  0,  1,  0,  0,  0,  0,
         0,  0,  0,  0,  1,  0,  0,  0,
        -1,  0,  0,  1,  0,  1,  0, -1,
         0,  0, -1,  0,  0,  0,  1,  0,
         0, -1,  0,  0,  0,  0,  0, -1,
    };
    mat m;
    fill_pm1(&m, 8, v);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
    rng_seed(6);
    TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-9);
    jm_lu_free(&lu);
}

static void test_random_pm1_matrices(void)
{
    int checked = 0;
    for (int seed = 1; seed <= 400; seed++) {
        rng_seed((uint64_t)seed * 7919u);
        int64_t n = 4 + (int64_t)(rng_next() % 7);

        mat m;
        m.n = n;
        memset(m.a, 0, sizeof m.a);
        for (int64_t i = 0; i < n; i++)
            for (int64_t j = 0; j < n; j++) {
                uint64_t r = rng_next() % 3;
                m.a[i][j] = r == 0 ? -1.0 : (r == 1 ? 0.0 : 1.0);
            }
        mat_pack(&m);

        double d[MAXN][MAXN];
        memcpy(d, m.a, sizeof d);
        bool nonsingular = true;
        for (int64_t k = 0; k < n && nonsingular; k++) {
            int64_t best = k;
            for (int64_t i = k + 1; i < n; i++)
                if (fabs(d[i][k]) > fabs(d[best][k]))
                    best = i;
            if (fabs(d[best][k]) < 1e-9) {
                nonsingular = false;
                break;
            }
            for (int64_t j = 0; j < n; j++) {
                double t = d[k][j];
                d[k][j] = d[best][j];
                d[best][j] = t;
            }
            for (int64_t i = k + 1; i < n; i++) {
                double f = d[i][k] / d[k][k];
                for (int64_t j = k; j < n; j++)
                    d[i][j] -= f * d[k][j];
            }
        }

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        if (nonsingular) {
            TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
            TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);
            checked++;
        }
        jm_lu_free(&lu);
    }

    TEST_ASSERT_TRUE(checked > 100);
}

constexpr double UPDATE_TOL = 1e-9;

static void mat_set_col(mat *m, int64_t c, const double *col)
{
    for (int64_t i = 0; i < m->n; i++)
        m->a[i][c] = col[i];
    mat_pack(m);
}

static void test_update_matches_the_new_basis(void)
{
    rng_seed(4242);
    mat m;
    make_random(&m, 10, 0.35);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);

    double col[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        col[i] = (i % 3 == 0) ? rng_val() * 2.0 : 0.0;
    col[4] = 3.0;

    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jm_lu_update(&lu, 2, col, UPDATE_TOL, &w));
    TEST_ASSERT_EQUAL_INT64(1, lu.n_updates);

    mat_set_col(&m, 2, col);
    TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);
    jm_lu_free(&lu);
}

static void test_update_chain_agrees_with_refactorization(void)
{
    int applied = 0;
    for (int64_t n = 4; n <= 20; n += 4) {
        rng_seed((uint64_t)(n * 31 + 7));
        mat m;
        make_random(&m, n, 0.3);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);

        for (int step = 0; step < 12; step++) {
            int64_t c = (int64_t)(rng_next() % (uint64_t)n);

            double col[MAXN];
            for (int64_t i = 0; i < n; i++)
                col[i] = ((rng_next() >> 11) % 4 == 0) ? rng_val() : 0.0;
            col[c] = 3.0 + rng_val();

            jaos_status st = jm_lu_update(&lu, c, col, UPDATE_TOL, &w);
            if (st == JAOS_ERR_NUMERICAL) {

                TEST_ASSERT_TRUE(lu.rank < 0);
                break;
            }
            TEST_ASSERT_EQUAL_INT(JAOS_OK, st);
            applied++;
            mat_set_col(&m, c, col);

            TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-7);

            jm_lu fresh;
            jm_lu_init(&fresh);
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&fresh, m.n, m.start,
                                            m.index, m.value, PIVOT_TOL, &w));
            TEST_ASSERT_EQUAL_INT64(m.n, fresh.rank);

            double b[MAXN], x1[MAXN], x2[MAXN];
            for (int64_t i = 0; i < n; i++)
                b[i] = rng_val() * 3.0;
            memcpy(x1, b, sizeof(double) * (size_t)n);
            memcpy(x2, b, sizeof(double) * (size_t)n);
            jm_lu_ftran(&lu, x1, &w);
            jm_lu_ftran(&fresh, x2, &w);
            TEST_ASSERT_TRUE(max_abs_diff(x1, x2, n) < 1e-7);

            memcpy(x1, b, sizeof(double) * (size_t)n);
            memcpy(x2, b, sizeof(double) * (size_t)n);
            jm_lu_btran(&lu, x1, &w);
            jm_lu_btran(&fresh, x2, &w);
            TEST_ASSERT_TRUE(max_abs_diff(x1, x2, n) < 1e-7);

            jm_lu_free(&fresh);
        }
        jm_lu_free(&lu);
    }

    TEST_ASSERT_TRUE(applied >= 40);
}

static void test_update_refuses_a_singular_replacement(void)
{
    mat m;
    m.n = 3;
    memset(m.a, 0, sizeof m.a);
    m.a[0][0] = 1.0;
    m.a[1][1] = 1.0;
    m.a[2][2] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);

    double col[3] = {1.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL,
        jm_lu_update(&lu, 1, col, UPDATE_TOL, &w));
    TEST_ASSERT_TRUE(lu.rank < 0);

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(&lu, 0, col, UPDATE_TOL, &w));
    jm_lu_free(&lu);
}

static void test_a_wrecked_factorization_writes_nothing(void)
{
    mat m;
    m.n = 3;
    memset(m.a, 0, sizeof m.a);
    m.a[0][0] = 2.0;
    m.a[1][1] = 4.0;
    m.a[2][2] = 8.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);

    double good[3] = {1.0, 2.0, 3.0};
    jm_lu_ftran(&lu, good, &w);
    TEST_ASSERT_TRUE(good[0] == 0.5 && good[1] == 0.5 && good[2] == 0.375);

    double col[3] = {2.0, 0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL,
        jm_lu_update(&lu, 1, col, UPDATE_TOL, &w));
    TEST_ASSERT_TRUE(lu.rank < 0);

    static const double SENTINEL[3] = {-7.5, 11.25, -0.125};
    double x[3];
    int64_t pat[3];
    int64_t npat;

    memcpy(x, SENTINEL, sizeof x);
    jm_lu_ftran(&lu, x, &w);
    TEST_ASSERT_TRUE(x[0] == SENTINEL[0] && x[1] == SENTINEL[1] &&
                     x[2] == SENTINEL[2]);

    memcpy(x, SENTINEL, sizeof x);
    jm_lu_btran(&lu, x, &w);
    TEST_ASSERT_TRUE(x[0] == SENTINEL[0] && x[1] == SENTINEL[1] &&
                     x[2] == SENTINEL[2]);

    memcpy(x, SENTINEL, sizeof x);
    npat = 999;
    jm_lu_ftran_sparse(&lu, x, &w, pat, &npat);
    TEST_ASSERT_EQUAL_INT64(0, npat);
    TEST_ASSERT_TRUE(x[0] == SENTINEL[0] && x[1] == SENTINEL[1] &&
                     x[2] == SENTINEL[2]);

    memcpy(x, SENTINEL, sizeof x);
    npat = 999;
    jm_lu_btran_sparse(&lu, x, &w, pat, &npat);
    TEST_ASSERT_EQUAL_INT64(0, npat);
    TEST_ASSERT_TRUE(x[0] == SENTINEL[0] && x[1] == SENTINEL[1] &&
                     x[2] == SENTINEL[2]);

    jm_lu_free(&lu);
}

static void test_update_with_the_same_column_is_stable(void)
{
    rng_seed(31337);
    mat m;
    make_random(&m, 8, 0.4);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);

    for (int rep = 0; rep < 5; rep++) {
        double col[MAXN];
        for (int64_t i = 0; i < m.n; i++)
            col[i] = m.a[i][3];
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jm_lu_update(&lu, 3, col, UPDATE_TOL, &w));
        TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-9);
    }
    jm_lu_free(&lu);
}

static void test_update_rejects_bad_arguments(void)
{
    mat m;
    m.n = 2;
    memset(m.a, 0, sizeof m.a);
    m.a[0][0] = 1.0;
    m.a[1][1] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_work w = {0};
    must_factor(&m, &lu, &w);
    double col[2] = {1.0, 1.0};

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(nullptr, 0, col, UPDATE_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(&lu, 0, nullptr, UPDATE_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(&lu, -1, col, UPDATE_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(&lu, 2, col, UPDATE_TOL, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_update(&lu, 0, col, 0.0, &w));
    jm_lu_free(&lu);
}

static void test_updates_are_bit_identical_across_runs(void)
{
    double first[MAXN];
    for (int run = 0; run < 2; run++) {
        rng_seed(24680);
        mat m;
        make_random(&m, 9, 0.35);

        jm_lu lu;
        jm_work w = {0};
        must_factor(&m, &lu, &w);
        for (int step = 0; step < 4; step++) {
            double col[MAXN];
            for (int64_t i = 0; i < m.n; i++)
                col[i] = (i % 2 == 0) ? (double)(i + step + 1) : 0.0;
            col[step] = 2.5 + (double)step;
            TEST_ASSERT_EQUAL_INT(JAOS_OK,
                jm_lu_update(&lu, step, col, UPDATE_TOL, &w));
        }
        double x[MAXN];
        for (int64_t i = 0; i < m.n; i++)
            x[i] = 1.0 + (double)i;
        jm_lu_ftran(&lu, x, &w);

        if (run == 0)
            memcpy(first, x, sizeof(double) * (size_t)m.n);
        else
            for (int64_t i = 0; i < m.n; i++)
                TEST_ASSERT_EQUAL_MEMORY(&first[i], &x[i], sizeof(double));
        jm_lu_free(&lu);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_identity_factors_and_solves_exactly);
    RUN_TEST(test_permutation_matrix);
    RUN_TEST(test_triangular_matrices);
    RUN_TEST(test_random_matrices_solve_correctly);
    RUN_TEST(test_btran_unit_vectors_solve_exactly);
    RUN_TEST(test_dense_matrices);
    RUN_TEST(test_singular_matrices_are_reported_not_hidden);
    RUN_TEST(test_markowitz_prefers_singletons);
    RUN_TEST(test_work_counter_moves_and_repeats);
    RUN_TEST(test_factor_rejects_bad_arguments);
    RUN_TEST(test_exact_cancellation_does_not_corrupt_state);
    RUN_TEST(test_nonsingular_pm1_matrix_reaches_full_rank);
    RUN_TEST(test_random_pm1_matrices);
    RUN_TEST(test_update_matches_the_new_basis);
    RUN_TEST(test_update_chain_agrees_with_refactorization);
    RUN_TEST(test_update_refuses_a_singular_replacement);
    RUN_TEST(test_a_wrecked_factorization_writes_nothing);
    RUN_TEST(test_update_with_the_same_column_is_stable);
    RUN_TEST(test_update_rejects_bad_arguments);
    RUN_TEST(test_updates_are_bit_identical_across_runs);
    return UNITY_END();
}
