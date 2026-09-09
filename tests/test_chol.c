/* SPDX-License-Identifier: Apache-2.0 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

constexpr int MAXN = 64;

typedef struct {
    int64_t n;
    double a[MAXN][MAXN];
    int64_t start[MAXN + 1];
    int64_t index[MAXN * MAXN];
    double value[MAXN * MAXN];
} sym;

static void sym_pack(sym *m)
{
    int64_t nnz = 0;
    for (int64_t j = 0; j < m->n; j++) {
        m->start[j] = nnz;
        for (int64_t i = 0; i < m->n; i++)
            if (m->a[i][j] != 0.0) {
                m->index[nnz] = i;
                m->value[nnz] = m->a[i][j];
                nnz++;
            }
    }
    m->start[m->n] = nnz;
}

static void sym_clear(sym *m, int64_t n)
{
    m->n = n;
    memset(m->a, 0, sizeof m->a);
}

static void sym_mul(const sym *m, const double *x, double *out)
{
    for (int64_t i = 0; i < m->n; i++) {
        double s = 0.0;
        for (int64_t j = 0; j < m->n; j++)
            s += m->a[i][j] * x[j];
        out[i] = s;
    }
}

static void must_factor(const sym *m, jm_chol *c, jm_work *w)
{
    jm_chol_init(c);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_symbolic(c, m->n, m->start,
                                                    m->index, w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_numeric(c, m->value, w));
}

static double residual(const sym *m, const jm_chol *c, const double *b)
{
    double x[MAXN], back[MAXN];
    memcpy(x, b, sizeof(double) * (size_t)m->n);
    jm_chol_solve(c, x, nullptr);
    sym_mul(m, x, back);
    double worst = 0.0;
    for (int64_t i = 0; i < m->n; i++) {
        double t = fabs(back[i] - b[i]);
        if (t > worst)
            worst = t;
    }
    return worst;
}

static uint64_t rng;

static uint64_t rng_next(void)
{
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    return rng * 0x2545f4914f6cdd1du;
}

static double rng_unit(void)
{
    return (double)(rng_next() >> 11) / 9007199254740992.0;
}

static void make_normal_equations(sym *m, int64_t nrow, int64_t ncol,
                                  double density)
{
    static double a[MAXN][2 * MAXN];
    static double d[2 * MAXN];
    memset(a, 0, sizeof a);
    for (int64_t j = 0; j < ncol; j++) {
        d[j] = 0.5 + rng_unit();
        int64_t placed = 0;
        for (int64_t i = 0; i < nrow; i++)
            if (rng_unit() < density) {
                a[i][j] = 2.0 * rng_unit() - 1.0;
                placed++;
            }
        if (placed == 0)
            a[(int64_t)(rng_unit() * (double)nrow) % nrow][j] = 1.0;
    }
    sym_clear(m, nrow);
    for (int64_t i = 0; i < nrow; i++)
        for (int64_t k = 0; k < nrow; k++) {
            double s = 0.0;
            for (int64_t j = 0; j < ncol; j++)
                s += a[i][j] * d[j] * a[k][j];
            m->a[i][k] = s;
        }
    for (int64_t i = 0; i < nrow; i++)
        m->a[i][i] += 1e-3;
    sym_pack(m);
}

static void test_two_by_two_by_hand(void)
{
    sym m;
    sym_clear(&m, 2);
    m.a[0][0] = 2; m.a[0][1] = 1;
    m.a[1][0] = 1; m.a[1][1] = 2;
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);

    double b[2] = {3, 3};
    jm_chol_solve(&c, b, nullptr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 1.0, b[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 1.0, b[1]);
    TEST_ASSERT_EQUAL_INT64(0, c.replaced);
    TEST_ASSERT_EQUAL_INT64(3, c.nnz);
    jm_chol_free(&c);
}

static void test_three_by_three_by_hand(void)
{
    sym m;
    sym_clear(&m, 3);
    m.a[0][0] = 4; m.a[0][1] = 1;
    m.a[1][0] = 1; m.a[1][1] = 3; m.a[1][2] = 1;
    m.a[2][1] = 1; m.a[2][2] = 2;
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);

    double b[3] = {6, 10, 8};
    jm_chol_solve(&c, b, nullptr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 1.0, b[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 2.0, b[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 3.0, b[2]);
    TEST_ASSERT_EQUAL_INT64(0, c.replaced);
    TEST_ASSERT_EQUAL_INT64(5, c.nnz);
    jm_chol_free(&c);
}

static void test_factor_reproduces_the_matrix(void)
{
    sym m;
    sym_clear(&m, 4);
    double a[4][4] = {{10, 2, 0, 1}, {2, 8, 3, 0}, {0, 3, 9, 2}, {1, 0, 2, 7}};
    for (int64_t i = 0; i < 4; i++)
        for (int64_t j = 0; j < 4; j++)
            m.a[i][j] = a[i][j];
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);

    double l[4][4] = {0};
    for (int64_t k = 0; k < 4; k++)
        for (int64_t p = c.l_start[k]; p < c.l_start[k + 1]; p++)
            l[c.l_index[p]][k] = c.l_value[p];
    for (int64_t i = 0; i < 4; i++)
        for (int64_t j = 0; j < 4; j++) {
            double s = 0.0;
            for (int64_t k = 0; k < 4; k++)
                s += l[i][k] * l[j][k];
            TEST_ASSERT_DOUBLE_WITHIN(1e-12, m.a[c.perm[i]][c.perm[j]], s);
        }
    for (int64_t k = 0; k < 4; k++)
        TEST_ASSERT_EQUAL_INT64(k, c.inv[c.perm[k]]);
    jm_chol_free(&c);
}

static void test_arrow_orders_the_dense_node_last(void)
{
    sym m;
    constexpr int64_t n = 12;
    sym_clear(&m, n);
    for (int64_t i = 0; i < n; i++)
        m.a[i][i] = (double)n;
    for (int64_t i = 1; i < n; i++) {
        m.a[0][i] = 1;
        m.a[i][0] = 1;
    }
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);
    for (int64_t k = 0; k + 2 < n; k++)
        TEST_ASSERT_EQUAL_INT64(k + 1, c.perm[k]);
    TEST_ASSERT_EQUAL_INT64(0, c.perm[n - 2]);
    TEST_ASSERT_EQUAL_INT64(n - 1, c.perm[n - 1]);
    TEST_ASSERT_EQUAL_INT64(2 * n - 1, c.nnz);

    double b[n];
    for (int64_t i = 0; i < n; i++)
        b[i] = (double)(i + 1);
    TEST_ASSERT_TRUE(residual(&m, &c, b) < 1e-12);
    jm_chol_free(&c);
}

static void test_ties_break_by_index(void)
{
    sym m;
    constexpr int64_t n = 8;
    sym_clear(&m, n);
    for (int64_t i = 0; i < n; i++)
        m.a[i][i] = 4;
    for (int64_t i = 0; i + 1 < n; i++) {
        m.a[i][i + 1] = 1;
        m.a[i + 1][i] = 1;
    }
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);
    for (int64_t k = 0; k < n; k++)
        TEST_ASSERT_EQUAL_INT64(k, c.perm[k]);
    TEST_ASSERT_EQUAL_INT64(2 * n - 1, c.nnz);
    jm_chol_free(&c);
}

static void test_random_normal_equations(void)
{
    rng = 0x9e3779b97f4a7c15u;
    for (int round = 0; round < 6; round++) {
        sym m;
        make_normal_equations(&m, 30 + 5 * round, 50 + 10 * round,
                              0.08 + 0.02 * round);
        jm_chol c;
        must_factor(&m, &c, nullptr);
        TEST_ASSERT_EQUAL_INT64(0, c.replaced);

        double b[MAXN];
        double scale = 0.0;
        for (int64_t i = 0; i < m.n; i++) {
            b[i] = 2.0 * rng_unit() - 1.0;
            for (int64_t j = 0; j < m.n; j++)
                if (fabs(m.a[i][j]) > scale)
                    scale = fabs(m.a[i][j]);
        }
        TEST_ASSERT_TRUE(residual(&m, &c, b) < 1e-9 * scale);
        jm_chol_free(&c);
    }
}

static void test_singular_pivot_is_replaced(void)
{
    sym m;
    sym_clear(&m, 3);
    m.a[0][0] = 1; m.a[0][1] = 1;
    m.a[1][0] = 1; m.a[1][1] = 1;
    m.a[2][2] = 5;
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);
    TEST_ASSERT_EQUAL_INT64(1, c.replaced);

    double b[3] = {2, 2, 10};
    jm_chol_solve(&c, b, nullptr);
    for (int i = 0; i < 3; i++)
        TEST_ASSERT_TRUE(isfinite(b[i]));
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 2.0, b[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-14, 2.0, b[0] + b[1]);
    jm_chol_free(&c);
}

static void test_numeric_refactors_on_the_same_pattern(void)
{
    sym m;
    sym_clear(&m, 3);
    m.a[0][0] = 4; m.a[0][1] = 1;
    m.a[1][0] = 1; m.a[1][1] = 3; m.a[1][2] = 1;
    m.a[2][1] = 1; m.a[2][2] = 2;
    sym_pack(&m);

    jm_chol c;
    must_factor(&m, &c, nullptr);

    for (int64_t k = 0; k < m.n; k++)
        m.a[k][k] *= 2.0;
    sym_pack(&m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_numeric(&c, m.value, nullptr));

    double b[3];
    for (int i = 0; i < 3; i++)
        b[i] = (double)(i + 1);
    TEST_ASSERT_TRUE(residual(&m, &c, b) < 1e-13);
    jm_chol_free(&c);
}

static void test_bit_identical_across_runs(void)
{
    rng = 12345;
    sym m;
    make_normal_equations(&m, 40, 70, 0.1);

    jm_chol c1, c2;
    jm_work w1 = {0}, w2 = {0};
    must_factor(&m, &c1, &w1);
    must_factor(&m, &c2, &w2);

    TEST_ASSERT_EQUAL_INT64(c1.nnz, c2.nnz);
    TEST_ASSERT_EQUAL_MEMORY(c1.perm, c2.perm, sizeof(int64_t) * (size_t)m.n);
    TEST_ASSERT_EQUAL_MEMORY(c1.l_index, c2.l_index,
                             sizeof(int64_t) * (size_t)c1.nnz);
    TEST_ASSERT_EQUAL_MEMORY(c1.l_value, c2.l_value,
                             sizeof(double) * (size_t)c1.nnz);
    TEST_ASSERT_EQUAL_INT64(w1.units, w2.units);

    double b1[MAXN], b2[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        b1[i] = b2[i] = (double)(i % 7) - 3.0;
    jm_chol_solve(&c1, b1, &w1);
    jm_chol_solve(&c2, b2, &w2);
    TEST_ASSERT_EQUAL_MEMORY(b1, b2, sizeof(double) * (size_t)m.n);
    TEST_ASSERT_EQUAL_INT64(w1.units, w2.units);
    jm_chol_free(&c1);
    jm_chol_free(&c2);
}

static void test_work_is_counted(void)
{
    sym m;
    sym_clear(&m, 3);
    m.a[0][0] = 4; m.a[0][1] = 1; m.a[0][2] = 1;
    m.a[1][0] = 1; m.a[1][1] = 3; m.a[1][2] = 1;
    m.a[2][0] = 1; m.a[2][1] = 1; m.a[2][2] = 2;
    sym_pack(&m);

    jm_chol c;
    jm_work w = {0};
    jm_chol_init(&c);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_symbolic(&c, m.n, m.start,
                                                    m.index, &w));
    int64_t symbolic = w.units;
    TEST_ASSERT_TRUE(symbolic > 0);
    TEST_ASSERT_EQUAL_INT64(6, c.nnz);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_numeric(&c, m.value, &w));
    int64_t numeric = w.units - symbolic;
    TEST_ASSERT_EQUAL_INT64(JM_WORK_FACTOR + 9 * JM_WORK_NONZERO +
                                1 * JM_WORK_ELIMINATED,
                            numeric);

    double b[3] = {1, 1, 1};
    jm_chol_solve(&c, b, &w);
    TEST_ASSERT_EQUAL_INT64((2 * 6 + 2 * 3) * JM_WORK_NONZERO,
                            w.units - symbolic - numeric);
    jm_chol_free(&c);
}

static void test_empty_system(void)
{
    jm_chol c;
    jm_chol_init(&c);
    int64_t start[1] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_symbolic(&c, 0, start, nullptr,
                                                    nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_numeric(&c, nullptr, nullptr));
    jm_chol_solve(&c, nullptr, nullptr);
    TEST_ASSERT_EQUAL_INT64(0, c.nnz);
    jm_chol_free(&c);
}

static void test_rejects_bad_input(void)
{
    jm_chol c;
    jm_chol_init(&c);
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_chol_numeric(&c, nullptr, nullptr));

    int64_t start[3] = {0, 1, 2};
    int64_t index[2] = {0, 5};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_chol_symbolic(&c, 2, start, index, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_chol_symbolic(&c, -1, start, index, nullptr));
    jm_chol_free(&c);
}

static void ldlt_factor(const sym *m, jm_chol *c, const int8_t *sign,
                        jm_work *w)
{
    jm_chol_init(c);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_symbolic(c, m->n, m->start,
                                                    m->index, w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
                          jm_ldlt_numeric(c, m->value, sign, 1e-12, w));
}

static double ldlt_residual(const sym *m, const jm_chol *c, const double *b)
{
    double x[MAXN], back[MAXN];
    memcpy(x, b, sizeof(double) * (size_t)m->n);
    jm_ldlt_solve(c, x, nullptr);
    sym_mul(m, x, back);
    double worst = 0.0, scale = 0.0;
    for (int64_t i = 0; i < m->n; i++) {
        const double t = fabs(back[i] - b[i]);
        if (t > worst)
            worst = t;
        if (fabs(b[i]) > scale)
            scale = fabs(b[i]);
    }
    return worst / (1.0 + scale);
}

/* The augmented system of an LP with a quadratic objective:

       [ -(Q + T)   A^T ] [ dx ]   [ r1 ]
       [    A        0  ] [ dy ] = [ r2 ]

   which is quasi-definite when Q + T is positive definite: negative on
   the first block's diagonal, positive on the second's.  This is the
   system a full Q needs and the normal equations cannot take. */
static void make_augmented(sym *m, int64_t ncol, int64_t nrow,
                           double density, double delta, int8_t *sign)
{
    static double a[MAXN][MAXN];
    memset(a, 0, sizeof a);
    for (int64_t j = 0; j < ncol; j++) {
        int64_t placed = 0;
        for (int64_t i = 0; i < nrow; i++)
            if (rng_unit() < density) {
                a[i][j] = 2.0 * rng_unit() - 1.0;
                placed++;
            }
        if (placed == 0)
            a[(int64_t)(rng_unit() * (double)nrow) % nrow][j] = 1.0;
    }
    const int64_t n = ncol + nrow;
    sym_clear(m, n);
    for (int64_t j = 0; j < ncol; j++) {
        m->a[j][j] = -(0.5 + rng_unit());
        sign[j] = -1;
    }
    for (int64_t j = 0; j + 1 < ncol; j++)
        if (rng_unit() < 0.3) {
            const double q = -0.1 * rng_unit();
            m->a[j][j + 1] += q;
            m->a[j + 1][j] += q;
            m->a[j][j] -= 0.2;
            m->a[j + 1][j + 1] -= 0.2;
        }
    for (int64_t i = 0; i < nrow; i++) {
        m->a[ncol + i][ncol + i] = delta;
        sign[ncol + i] = 1;
        for (int64_t j = 0; j < ncol; j++)
            if (a[i][j] != 0.0) {
                m->a[ncol + i][j] = a[i][j];
                m->a[j][ncol + i] = a[i][j];
            }
    }
    sym_pack(m);
}

static void test_ldlt_solves_a_quasi_definite_system(void)
{
    rng = 0x5eed1234u;
    double worst = 0.0;
    for (int trial = 0; trial < 60; trial++) {
        const int64_t ncol = 2 + (int64_t)(rng_unit() * 18.0);
        const int64_t nrow = 1 + (int64_t)(rng_unit() * 12.0);
        sym m;
        int8_t sign[MAXN];
        make_augmented(&m, ncol, nrow, 0.35, 1e-8, sign);
        jm_chol c;
        jm_work w = {0};
        ldlt_factor(&m, &c, sign, &w);
        TEST_ASSERT_EQUAL_INT64(0, c.replaced);
        double b[MAXN];
        for (int64_t i = 0; i < m.n; i++)
            b[i] = 2.0 * rng_unit() - 1.0;
        const double res = ldlt_residual(&m, &c, b);
        if (res > worst)
            worst = res;
        jm_chol_free(&c);
    }

    TEST_ASSERT_TRUE_MESSAGE(worst < 1e-6,
        "with delta at 1e-8 the system's own condition bounds this; the "
        "worst of the sixty measured 2.6e-8");
}

static void test_ldlt_keeps_the_sign_each_block_was_promised(void)
{
    rng = 0xabcdef01u;
    sym m;
    int8_t sign[MAXN];
    make_augmented(&m, 8, 5, 0.4, 1e-8, sign);
    jm_chol c;
    jm_work w = {0};
    ldlt_factor(&m, &c, sign, &w);
    for (int64_t k = 0; k < c.n; k++) {
        const int8_t want = sign[c.perm[k]];
        TEST_ASSERT_TRUE_MESSAGE(c.d[k] * (double)want > 0.0,
            "a quasi-definite factor keeps one sign per block");
        TEST_ASSERT_TRUE(fabs(c.d[k]) >= 1e-12);
    }
    jm_chol_free(&c);
}

static void test_ldlt_replaces_a_pivot_of_the_wrong_sign(void)
{

    sym m;
    sym_clear(&m, 2);
    m.a[0][0] = -1.0;
    m.a[1][1] = -3.0;
    m.a[0][1] = m.a[1][0] = 0.5;
    sym_pack(&m);
    const int8_t sign[2] = {-1, 1};
    jm_chol c;
    jm_work w = {0};
    ldlt_factor(&m, &c, sign, &w);
    TEST_ASSERT_EQUAL_INT64(1, c.replaced);
    for (int64_t k = 0; k < c.n; k++)
        TEST_ASSERT_TRUE(c.d[k] * (double)sign[c.perm[k]] > 0.0);
    jm_chol_free(&c);
}

static void test_ldlt_agrees_with_the_cholesky_on_a_definite_matrix(void)
{
    rng = 0x1357911u;
    for (int trial = 0; trial < 20; trial++) {
        sym m;
        make_normal_equations(&m, 3 + (int64_t)(rng_unit() * 15.0),
                              4 + (int64_t)(rng_unit() * 20.0), 0.4);
        double b[MAXN];
        for (int64_t i = 0; i < m.n; i++)
            b[i] = 2.0 * rng_unit() - 1.0;

        jm_chol c1;
        jm_work w1 = {0};
        must_factor(&m, &c1, &w1);
        double x1[MAXN];
        memcpy(x1, b, sizeof(double) * (size_t)m.n);
        jm_chol_solve(&c1, x1, nullptr);
        jm_chol_free(&c1);

        jm_chol c2;
        jm_work w2 = {0};
        ldlt_factor(&m, &c2, nullptr, &w2);
        double x2[MAXN];
        memcpy(x2, b, sizeof(double) * (size_t)m.n);
        jm_ldlt_solve(&c2, x2, nullptr);
        jm_chol_free(&c2);

        for (int64_t i = 0; i < m.n; i++)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, x1[i], x2[i]);
    }
}

static void test_ldlt_is_bit_identical_across_runs(void)
{
    double first[MAXN];
    for (int run = 0; run < 2; run++) {
        rng = 0x777abcu;
        sym m;
        int8_t sign[MAXN];
        make_augmented(&m, 12, 7, 0.4, 1e-8, sign);
        jm_chol c;
        jm_work w = {0};
        ldlt_factor(&m, &c, sign, &w);
        double x[MAXN];
        for (int64_t i = 0; i < m.n; i++)
            x[i] = 1.0 / (double)(i + 1);
        jm_ldlt_solve(&c, x, nullptr);
        if (run == 0)
            memcpy(first, x, sizeof(double) * (size_t)m.n);
        else
            TEST_ASSERT_EQUAL_MEMORY(first, x,
                                     sizeof(double) * (size_t)m.n);
        jm_chol_free(&c);
    }
}

static void test_ldlt_rejects_bad_input(void)
{
    jm_chol c;
    jm_chol_init(&c);
    jm_work w = {0};
    double v = 1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_ldlt_numeric(&c, &v, nullptr, 1e-12, &w));
    sym m;
    sym_clear(&m, 1);
    m.a[0][0] = -2.0;
    sym_pack(&m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_chol_symbolic(&c, m.n, m.start,
                                                    m.index, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_ldlt_numeric(&c, m.value, nullptr, 0.0, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jm_ldlt_numeric(&c, m.value, nullptr, -1.0, &w));
    jm_chol_free(&c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_two_by_two_by_hand);
    RUN_TEST(test_three_by_three_by_hand);
    RUN_TEST(test_factor_reproduces_the_matrix);
    RUN_TEST(test_arrow_orders_the_dense_node_last);
    RUN_TEST(test_ties_break_by_index);
    RUN_TEST(test_random_normal_equations);
    RUN_TEST(test_singular_pivot_is_replaced);
    RUN_TEST(test_numeric_refactors_on_the_same_pattern);
    RUN_TEST(test_bit_identical_across_runs);
    RUN_TEST(test_work_is_counted);
    RUN_TEST(test_empty_system);
    RUN_TEST(test_rejects_bad_input);
    RUN_TEST(test_ldlt_solves_a_quasi_definite_system);
    RUN_TEST(test_ldlt_keeps_the_sign_each_block_was_promised);
    RUN_TEST(test_ldlt_replaces_a_pivot_of_the_wrong_sign);
    RUN_TEST(test_ldlt_agrees_with_the_cholesky_on_a_definite_matrix);
    RUN_TEST(test_ldlt_is_bit_identical_across_runs);
    RUN_TEST(test_ldlt_rejects_bad_input);
    return UNITY_END();
}
