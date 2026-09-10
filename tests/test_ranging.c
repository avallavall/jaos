#include "jaos.h"
#include "unity.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

#define EXACT_D(want, got)                                                 \
    TEST_ASSERT_TRUE_MESSAGE((want) == (got), #got " is not exactly " #want)
#define NEAR(want, got) TEST_ASSERT_DOUBLE_WITHIN(1e-12, (want), (got))

static jaos_model *make_textbook(bool maximise)
{
    const double sgn = maximise ? 1.0 : -1.0;
    const double c[]  = {sgn, sgn};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 6.0};
    const int64_t s[]  = {0, 2, 4};
    const int64_t ix[] = {0, 1, 0, 1};
    const double v[]   = {1.0, 3.0, 2.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, maximise ? JAOS_MAXIMIZE : JAOS_MINIMIZE,
                     0.0, c, cl, cu, rl, ru, 4, s, ix, v));
    return m;
}

static void test_nothing_to_range_before_an_optimum(void)
{
    jaos_model *m = make_textbook(false);
    double lo[2], hi[2];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_cost_ranging(m, lo, hi));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_rhs_ranging(m, lo, hi, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_bound_ranging(m, lo, hi, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_cost_ranging(nullptr, lo, hi));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, nullptr, nullptr));
    jaos_model_free(m);
}

static void test_textbook_cost_ranging(void)
{
    jaos_model *m = make_textbook(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double x[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, nullptr, nullptr));
    NEAR(1.6, x[0]);
    NEAR(1.2, x[1]);

    double lo[2], hi[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    NEAR(-3.0, lo[0]);
    NEAR(-0.5, hi[0]);
    NEAR(-2.0, lo[1]);
    NEAR(-1.0 / 3.0, hi[1]);

    double only[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, only, nullptr));
    EXACT_D(lo[0], only[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, nullptr, only));
    EXACT_D(hi[1], only[1]);
    jaos_model_free(m);
}

static void test_textbook_cost_ranging_maximised(void)
{

    jaos_model *m = make_textbook(true);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double lo[2], hi[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    NEAR(0.5, lo[0]);
    NEAR(3.0, hi[0]);
    NEAR(1.0 / 3.0, lo[1]);
    NEAR(2.0, hi[1]);
    jaos_model_free(m);
}

static void test_textbook_rhs_and_bound_ranging(void)
{
    jaos_model *m = make_textbook(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double ll[2], lh[2], ul[2], uh[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, ll, lh, ul, uh));

    NEAR(2.0, ul[0]);
    NEAR(12.0, uh[0]);
    NEAR(2.0, ul[1]);
    NEAR(12.0, uh[1]);

    EXACT_D(-INFINITY, ll[0]);
    NEAR(4.0, lh[0]);
    EXACT_D(-INFINITY, ll[1]);
    NEAR(6.0, lh[1]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    EXACT_D(-INFINITY, ll[0]);
    NEAR(1.6, lh[0]);
    NEAR(1.6, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[1]);
    NEAR(1.2, lh[1]);
    NEAR(1.2, ul[1]);
    EXACT_D(INFINITY, uh[1]);
    jaos_model_free(m);
}

static void test_a_presolved_basis_ranges_like_any_other(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 0.5};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[]  = {0, 1, 2};
    const int64_t ix[] = {0, 0};
    const double v[]   = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double lo[2], hi[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    NEAR(0.0, lo[0]);
    EXACT_D(INFINITY, hi[0]);
    EXACT_D(-INFINITY, lo[1]);
    NEAR(1.0, hi[1]);

    double ll[2], lh[2], ul[2], uh[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, ll, lh, ul, uh));
    NEAR(0.5, ll[0]);
    NEAR(10.5, lh[0]);
    NEAR(1.0, ul[0]);
    EXACT_D(INFINITY, uh[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    EXACT_D(-INFINITY, ll[0]);
    NEAR(0.5, lh[0]);
    NEAR(0.5, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[1]);
    NEAR(0.5, lh[1]);
    NEAR(0.0, ul[1]);
    NEAR(1.0, uh[1]);
    jaos_model_free(m);
#endif
}

#define ORACLE_EPS 1e-4
#define ORACLE_NCOL 5
#define ORACLE_NROW 3

#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_oracle(void)
{
    const double c[]  = {2.0, 3.0, 1.0, 4.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 1.0};
    const double cu[] = {5.0, 5.0, 5.0, 5.0, 1.0};
    const double rl[] = {2.0, -3.0, 1.0}, ru[] = {8.0, 3.0, 5.0};

    const int64_t s[]  = {0, 3, 5, 7, 10, 12};
    const int64_t ix[] = {0, 1, 2, 0, 1, 0, 2, 0, 1, 2, 0, 2};
    const double v[]   = {1.0, 1.0, 2.0, 1.0, -1.0, 1.0, 1.0, 1.0, 2.0, -1.0,
                          1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ORACLE_NCOL, ORACLE_NROW, JAOS_MINIMIZE, 0.0,
                     c, cl, cu, rl, ru, 12, s, ix, v));
    return m;
}

static bool same_basis(const jaos_model *m, const jaos_basis_status *cs,
                       const jaos_basis_status *rs,
                       const jaos_basis_status *cs2,
                       const jaos_basis_status *rs2)
{
    for (int64_t j = 0; j < ORACLE_NCOL; j++) {
        double l, u;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, j, &l, &u));
        if (l == u ? (cs[j] == JAOS_BASIS_BASIC) != (cs2[j] == JAOS_BASIS_BASIC)
                   : cs[j] != cs2[j])
            return false;
    }
    return memcmp(rs, rs2, ORACLE_NROW * sizeof *rs) == 0;
}

typedef enum { MOVE_COST, MOVE_ROW_LO, MOVE_ROW_HI, MOVE_COL_LO, MOVE_COL_HI } move_kind;

static bool basis_survives(jaos_model *m, move_kind what, int64_t idx,
                           double value, const jaos_basis_status *cs,
                           const jaos_basis_status *rs)
{
    double c = 0.0, l = 0.0, u = 0.0;
    switch (what) {
    case MOVE_COST:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, idx, &c));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, idx, value));
        break;
    case MOVE_ROW_LO: case MOVE_ROW_HI:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, idx, &l, &u));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, idx,
            what == MOVE_ROW_LO ? value : l, what == MOVE_ROW_HI ? value : u));
        break;
    case MOVE_COL_LO: case MOVE_COL_HI:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, idx, &l, &u));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, idx,
            what == MOVE_COL_LO ? value : l, what == MOVE_COL_HI ? value : u));
        break;
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    bool survived = false;
    if (jaos_status_of(m) == JAOS_SOLVE_OPTIMAL) {
        jaos_basis_status cs2[ORACLE_NCOL], rs2[ORACLE_NROW];
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs2, rs2));
        survived = jaos_iterations(m) == 0 && same_basis(m, cs, rs, cs2, rs2);
    }
    switch (what) {
    case MOVE_COST:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, idx, c));
        break;
    case MOVE_ROW_LO: case MOVE_ROW_HI:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, idx, l, u));
        break;
    case MOVE_COL_LO: case MOVE_COL_HI:
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, idx, l, u));
        break;
    }
    return survived;
}

static char probe_msg[160];

static const char *probe_name(move_kind what, int64_t idx, double cur,
                              double lo, double hi, const char *side)
{
    static const char *kinds[] = {"cost", "row lower", "row upper",
                                  "col lower", "col upper"};
    snprintf(probe_msg, sizeof probe_msg,
             "%s of %lld at %.17g, range [%.17g, %.17g], %s",
             kinds[what], (long long)idx, cur, lo, hi, side);
    return probe_msg;
}

static void probe_interval(jaos_model *m, move_kind what, int64_t idx,
                           double cur, double lo, double hi, int *probed,
                           const jaos_basis_status *cs,
                           const jaos_basis_status *rs)
{
    TEST_ASSERT_TRUE_MESSAGE(lo <= cur && cur <= hi,
                             probe_name(what, idx, cur, lo, hi, "holds cur"));
    if (isfinite(lo) && lo < cur) {
        TEST_ASSERT_TRUE_MESSAGE(
            basis_survives(m, what, idx, lo + ORACLE_EPS, cs, rs),
            probe_name(what, idx, cur, lo, hi, "inside the lower end"));
        TEST_ASSERT_FALSE_MESSAGE(
            basis_survives(m, what, idx, lo - ORACLE_EPS, cs, rs),
            probe_name(what, idx, cur, lo, hi, "outside the lower end"));
        (*probed)++;
    }
    if (isfinite(hi) && hi > cur) {
        TEST_ASSERT_TRUE_MESSAGE(
            basis_survives(m, what, idx, hi - ORACLE_EPS, cs, rs),
            probe_name(what, idx, cur, lo, hi, "inside the upper end"));
        TEST_ASSERT_FALSE_MESSAGE(
            basis_survives(m, what, idx, hi + ORACLE_EPS, cs, rs),
            probe_name(what, idx, cur, lo, hi, "outside the upper end"));
        (*probed)++;
    }
}
#endif

static void test_the_solver_agrees_with_every_range(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)

    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_oracle();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    jaos_basis_status cs[ORACLE_NCOL], rs[ORACLE_NROW];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    double lo[ORACLE_NCOL], hi[ORACLE_NCOL], ll[ORACLE_NCOL], lh[ORACLE_NCOL], ul[ORACLE_NCOL], uh[ORACLE_NCOL];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, ll, lh, ul, uh));
    int probed = 0;
    for (int64_t j = 0; j < ORACLE_NCOL; j++) {
        double c;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, j, &c));
        probe_interval(m, MOVE_COST, j, c, lo[j], hi[j], &probed, cs, rs);
    }
    for (int64_t i = 0; i < ORACLE_NROW; i++) {
        double l, u;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_row_bounds(m, i, &l, &u));
        probe_interval(m, MOVE_ROW_LO, i, l, ll[i], lh[i], &probed, cs, rs);
        probe_interval(m, MOVE_ROW_HI, i, u, ul[i], uh[i], &probed, cs, rs);
    }

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    for (int64_t j = 0; j < ORACLE_NCOL; j++) {
        double l, u;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, j, &l, &u));
        probe_interval(m, MOVE_COL_LO, j, l, ll[j], lh[j], &probed, cs, rs);
        probe_interval(m, MOVE_COL_HI, j, u, ul[j], uh[j], &probed, cs, rs);
    }

    TEST_ASSERT_TRUE_MESSAGE(probed >= 10, "fewer than ten range ends were probed");
    jaos_model_free(m);
#endif
}

static void test_the_oracle_rejects_a_widened_range(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_oracle();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    jaos_basis_status cs[ORACLE_NCOL], rs[ORACLE_NROW];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    double lo[ORACLE_NCOL], hi[ORACLE_NCOL];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    int found = 0;
    for (int64_t j = 0; j < ORACLE_NCOL && !found; j++) {
        if (!isfinite(hi[j]))
            continue;

        double c;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_cost(m, j, &c));
        const double wider = hi[j] + (hi[j] - c) + 1.0;
        TEST_ASSERT_FALSE(basis_survives(m, MOVE_COST, j, wider, cs, rs));
        found = 1;
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "no finite upper cost end to widen");
    jaos_model_free(m);
#endif
}

static void test_ranging_is_reproducible(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_oracle();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    double a[ORACLE_NCOL], b[ORACLE_NCOL], c[ORACLE_NCOL], d[ORACLE_NCOL];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, a, b));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, c, d));
    TEST_ASSERT_EQUAL_MEMORY(a, c, sizeof a);
    TEST_ASSERT_EQUAL_MEMORY(b, d, sizeof b);
    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, c, d));
    TEST_ASSERT_EQUAL_MEMORY(a, c, sizeof a);
    TEST_ASSERT_EQUAL_MEMORY(b, d, sizeof b);
    jaos_model_free(m);
#endif
}

static void test_a_model_with_no_rows_ranges_its_costs(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {1.0, -2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 3.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 0, JAOS_MINIMIZE, 0.0, c, cl, cu, nullptr, nullptr,
                     0, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double lo[2], hi[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    NEAR(0.0, lo[0]);
    EXACT_D(INFINITY, hi[0]);
    EXACT_D(-INFINITY, lo[1]);
    NEAR(0.0, hi[1]);
    double ll[2], lh[2], ul[2], uh[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    EXACT_D(-INFINITY, ll[0]);
    NEAR(10.0, lh[0]);
    NEAR(0.0, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[1]);
    NEAR(3.0, lh[1]);
    NEAR(0.0, ul[1]);
    EXACT_D(INFINITY, uh[1]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, ll, lh, ul, uh));
    jaos_model_free(m);
#endif
}

static void test_a_mutual_singleton_on_an_open_row_ranges(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    const double c[]  = {0.0};
    const double cl[] = {-INFINITY}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {5.0};
    const int64_t s[]  = {0, 1};
    const int64_t ix[] = {0};
    const double v[]   = {2.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double lo[1], hi[1], ll[1], lh[1], ul[1], uh[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, lo, hi));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, ll, lh, ul, uh));
    TEST_ASSERT_TRUE(lo[0] <= 0.0 && 0.0 <= hi[0]);
    TEST_ASSERT_TRUE(ul[0] <= 5.0 && 5.0 <= uh[0]);
#if !defined(JAOS_NO_PRESOLVE)
    jaos_basis_status cs[1], rs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[0]);

    EXACT_D(-INFINITY, lo[0]);
    NEAR(0.0, hi[0]);

    EXACT_D(-INFINITY, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[0]);
    NEAR(5.0, lh[0]);
#endif
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    jaos_model_free(m);
#endif
}

/* A degenerate optimum: three constraints are tight at (1, 1) where two
   columns need only two, because r0 and r1 are the same row written twice.
   Several bases hold that point and they do not all report the same
   interval, so this is where ranging can be wrong while every non-degenerate
   model agrees. What ranging claims is that the basis holds across the
   interval, which makes the objective linear in the parameter; the check is
   that and needs no sign convention. */
static jaos_model *degenerate_pair(void)
{
    const double c[]  = {-2.0, -1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {3.0, 3.0};
    const double rl[] = {-INFINITY, -INFINITY, -INFINITY};
    const double ru[] = {2.0, 2.0, 1.0};
    const int64_t s[]  = {0, 3, 5};
    const int64_t ix[] = {0, 1, 2, 0, 1};
    const double v[]   = {1.0, 1.0, 1.0, 1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, s, ix, v));
    return m;
}

static double solve_variant(int which, int64_t k, double value)
{
    jaos_model *m = degenerate_pair();
    if (which == 0)
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_cost(m, k, value));
    else if (which == 1)
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_set_row_bounds(m, k, -INFINITY, value));
    else {
        double lo, up;
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_col_bounds(m, k, &lo, &up));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, which == 2
            ? jaos_set_col_bounds(m, k, value, up)
            : jaos_set_col_bounds(m, k, lo, value));
    }
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double z = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &z));
    jaos_model_free(m);
    return z;
}

static void linear_across(int which, int64_t k, double lo, double hi)
{
    if (!isfinite(lo) || !isfinite(hi) || hi <= lo)
        return;
    const double za = solve_variant(which, k, lo);
    const double zb = solve_variant(which, k, hi);
    const double zm = solve_variant(which, k, lo + 0.5 * (hi - lo));
    const double want = 0.5 * (za + zb);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9 * (1.0 + fabs(want)), want, zm);
}

static void test_a_degenerate_optimum_ranges_like_any_other(void)
{
    jaos_model *m = degenerate_pair();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[2], act[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, nullptr, nullptr));
    NEAR(1.0, x[0]);
    NEAR(1.0, x[1]);
    NEAR(2.0, act[0]);
    NEAR(2.0, act[1]);
    NEAR(1.0, act[2]);

    double z0 = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &z0));
    NEAR(-3.0, z0);

    double clo[2], cup[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, clo, cup));
    double rll[3], rlh[3], rul[3], ruh[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_rhs_ranging(m, rll, rlh, rul, ruh));
    double bll[2], blh[2], bul[2], buh[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, bll, blh, bul, buh));
    jaos_model_free(m);

    for (int64_t j = 0; j < 2; j++) {
        const double c0 = j == 0 ? -2.0 : -1.0;
        if (isfinite(clo[j]))
            TEST_ASSERT_DOUBLE_WITHIN(1e-9 * (1.0 + fabs(z0)),
                z0 + (clo[j] - c0) * x[j], solve_variant(0, j, clo[j]));
        if (isfinite(cup[j]))
            TEST_ASSERT_DOUBLE_WITHIN(1e-9 * (1.0 + fabs(z0)),
                z0 + (cup[j] - c0) * x[j], solve_variant(0, j, cup[j]));
    }
    for (int64_t i = 0; i < 3; i++)
        linear_across(1, i, rul[i], ruh[i]);
    for (int64_t j = 0; j < 2; j++) {
        linear_across(2, j, bll[j], blh[j]);
        linear_across(3, j, bul[j], buh[j]);
    }
}

static void test_a_quadratic_objective_is_refused_by_name(void)
{
    const int64_t qr[] = {0, 1, 1};
    const int64_t qc[] = {0, 0, 1};
    const double  qv[] = {2.0, 1.0, 2.0};
    double lo[2], hi[2];

    jaos_model *m = make_textbook(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_quadratic(m, 0, 2.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_cost_ranging(m, lo, hi));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic"));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_rhs_ranging(m, lo, hi, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
                          jaos_bound_ranging(m, lo, hi, nullptr, nullptr));
    jaos_model_free(m);

    m = make_textbook(false);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_quadratic(m, 3, qr, qc, qv));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_cost_ranging(m, lo, hi));
    TEST_ASSERT_NOT_NULL(strstr(jaos_model_error(m), "quadratic"));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_to_range_before_an_optimum);
    RUN_TEST(test_a_quadratic_objective_is_refused_by_name);
    RUN_TEST(test_a_model_with_no_rows_ranges_its_costs);
    RUN_TEST(test_a_mutual_singleton_on_an_open_row_ranges);
    RUN_TEST(test_textbook_cost_ranging);
    RUN_TEST(test_textbook_cost_ranging_maximised);
    RUN_TEST(test_textbook_rhs_and_bound_ranging);
    RUN_TEST(test_a_presolved_basis_ranges_like_any_other);
    RUN_TEST(test_the_solver_agrees_with_every_range);
    RUN_TEST(test_the_oracle_rejects_a_widened_range);
    RUN_TEST(test_ranging_is_reproducible);
    RUN_TEST(test_a_degenerate_optimum_ranges_like_any_other);
    return UNITY_END();
}
