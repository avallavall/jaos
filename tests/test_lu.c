/* LU tests.
 *
 * The factorization is judged the only way that means anything: solve with
 * it, then multiply the answer back through the original matrix and look at
 * the residual. Dense reference arithmetic does the multiplying, so a bug
 * in the sparse machinery cannot hide behind itself.
 *
 * Randomised cases use a PRNG defined here rather than rand(), whose
 * sequence is not reproducible across platforms — a test that cannot be
 * replayed is no use when it fails (D8).
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "jaos_internal.h"
#include "unity.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define MAXN 40
#define PIVOT_TOL 0.1

/* ---- deterministic PRNG: xorshift64* --------------------------------- */

static uint64_t rng;

static void rng_seed(uint64_t s) { rng = s ? s : 0x9e3779b97f4a7c15u; }

static uint64_t rng_next(void)
{
    rng ^= rng >> 12;
    rng ^= rng << 25;
    rng ^= rng >> 27;
    return rng * 0x2545f4914f6cdd1du;
}

/* Uniform in [-1, 1), never tiny enough to be dropped as noise. */
static double rng_val(void)
{
    double u = (double)(rng_next() >> 11) / 9007199254740992.0;
    double v = 2.0 * u - 1.0;
    return fabs(v) < 0.05 ? (v < 0 ? -0.05 : 0.05) : v;
}

/* ---- dense reference -------------------------------------------------- */

typedef struct {
    int64_t n;
    double a[MAXN][MAXN];
    int64_t start[MAXN + 1];
    int64_t index[MAXN * MAXN];
    double value[MAXN * MAXN];
    int64_t nnz;
} mat;

/* Builds the CSC image of the dense matrix. */
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

/* Solve with the factorization, multiply back, report the worst residual
 * of both directions. */
static double solve_residual(const mat *m, const jm_lu *lu, jm_work *w)
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

/* Fills a random sparse matrix, diagonally dominant so it is certainly
 * nonsingular and the residual bound is meaningful. */
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

/* ---- tests ------------------------------------------------------------ */

static void test_identity_factors_and_solves_exactly(void)
{
    mat m;
    m.n = 6;
    memset(m.a, 0, sizeof m.a);
    for (int64_t i = 0; i < m.n; i++)
        m.a[i][i] = 1.0;
    mat_pack(&m);

    jm_lu lu;
    jm_lu_init(&lu);
    jm_work w = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w));
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

    double x[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        x[i] = (double)(i + 1);
    jm_lu_ftran(&lu, x, &w);
    for (int64_t i = 0; i < m.n; i++)
        TEST_ASSERT_EQUAL_DOUBLE((double)(i + 1), x[i]);

    jm_lu_free(&lu);
}

/* A permutation matrix has no fill and exercises the permutation handling
 * on its own, without any arithmetic to hide behind. */
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
    jm_lu_init(&lu);
    jm_work w = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w));
    TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

    rng_seed(11);
    TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-12);
    jm_lu_free(&lu);
}

/* Lower and upper triangular inputs: one of the two solve directions is
 * trivial and the other is not, in each case. */
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
        jm_lu_init(&lu);
        jm_work w = {0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start,
                                        m.index, m.value, PIVOT_TOL, &w));
        TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
        TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-9);
        jm_lu_free(&lu);
    }
}

/* The main property: across many random shapes and densities, solving and
 * multiplying back must reproduce the right-hand side. */
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
            jm_lu_init(&lu);
            jm_work w = {0};
            TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start,
                                            m.index, m.value, PIVOT_TOL, &w));
            TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);

            for (int rep = 0; rep < 3; rep++)
                TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);

            jm_lu_free(&lu);
            cases++;
        }
    }
    TEST_ASSERT_EQUAL_INT(75, cases);
}

/* Dense matrices produce total fill: the elimination path where every
 * update touches every remaining entry. */
static void test_dense_matrices(void)
{
    for (int64_t n = 2; n <= 12; n++) {
        rng_seed((uint64_t)(500 + n));
        mat m;
        make_random(&m, n, 1.0);

        jm_lu lu;
        jm_lu_init(&lu);
        jm_work w = {0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                    m.value, PIVOT_TOL, &w));
        TEST_ASSERT_EQUAL_INT64(m.n, lu.rank);
        TEST_ASSERT_TRUE(solve_residual(&m, &lu, &w) < 1e-8);
        jm_lu_free(&lu);
    }
}

static void test_singular_matrices_are_reported_not_hidden(void)
{
    /* An empty column. */
    {
        mat m;
        m.n = 4;
        memset(m.a, 0, sizeof m.a);
        for (int64_t i = 0; i < m.n; i++)
            m.a[i][i] = 1.0;
        m.a[2][2] = 0.0;                 /* column 2 becomes empty */
        mat_pack(&m);

        jm_lu lu;
        jm_lu_init(&lu);
        jm_work w = {0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                    m.value, PIVOT_TOL, &w));
        TEST_ASSERT_EQUAL_INT64(3, lu.rank);
        jm_lu_free(&lu);
    }
    /* Two identical columns: full pattern, rank deficient. */
    {
        mat m;
        m.n = 3;
        memset(m.a, 0, sizeof m.a);
        m.a[0][0] = 1.0; m.a[1][0] = 2.0;
        m.a[0][1] = 1.0; m.a[1][1] = 2.0;
        m.a[2][2] = 5.0;
        mat_pack(&m);

        jm_lu lu;
        jm_lu_init(&lu);
        jm_work w = {0};
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                    m.value, PIVOT_TOL, &w));
        TEST_ASSERT_TRUE(lu.rank < m.n);
        jm_lu_free(&lu);
    }
}

/* A singleton column costs nothing to eliminate; the search must take it
 * first, which is what keeps a sparse factorization sparse. */
static void test_markowitz_prefers_singletons(void)
{
    /* Column 0 is a singleton hitting row 2; everything else is dense. */
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
    jm_lu_init(&lu);
    jm_work w = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(&lu, m.n, m.start, m.index,
                                                m.value, PIVOT_TOL, &w));
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
    /* A factorization costs at least its fixed overhead. */
    TEST_ASSERT_TRUE(w1.units >= JM_WORK_FACTOR);

    double x[MAXN];
    for (int64_t i = 0; i < m.n; i++)
        x[i] = 1.0;
    int64_t before = w1.units;
    jm_lu_ftran(&lu, x, &w1);
    TEST_ASSERT_TRUE(w1.units > before);
    jm_lu_free(&lu);

    /* Same input, same units: the counter is a deterministic budget, not a
     * measurement of elapsed anything (D16). */
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

    /* A row index outside the matrix must be caught, not trusted. */
    int64_t bad_index[] = {0, 9};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jm_lu_factor(&lu, m.n, m.start, bad_index, m.value, PIVOT_TOL, &w));

    /* A null work counter is allowed: not every caller keeps a budget. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jm_lu_factor(&lu, m.n, m.start, m.index, m.value, PIVOT_TOL, nullptr));
    jm_lu_free(&lu);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_identity_factors_and_solves_exactly);
    RUN_TEST(test_permutation_matrix);
    RUN_TEST(test_triangular_matrices);
    RUN_TEST(test_random_matrices_solve_correctly);
    RUN_TEST(test_dense_matrices);
    RUN_TEST(test_singular_matrices_are_reported_not_hidden);
    RUN_TEST(test_markowitz_prefers_singletons);
    RUN_TEST(test_work_counter_moves_and_repeats);
    RUN_TEST(test_factor_rejects_bad_arguments);
    return UNITY_END();
}
