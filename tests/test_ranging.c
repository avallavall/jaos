/* Sensitivity and ranging (D258): the three calls against models whose
 * ranges are worked by hand, and against the solver itself as the oracle
 * -- a number moved to just inside its range leaves the published basis
 * optimal and a warm re-solve costs nothing, moved to just outside it the
 * re-solve has to pivot. The oracle needs a model presolve leaves alone,
 * because a warm start crosses presolve's mapping and a reduced model that
 * presolve solves by itself never iterates at all. */
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

/* The textbook pair of rows (Chvatal ch. 10 has this shape):
 *
 *   min -x0 - x1   (max x0 + x1 when `maximise`)
 *   row0:  x0 + 2 x1 <= 4
 *   row1: 3 x0 +   x1 <= 6
 *   x >= 0
 *
 * Optimum x = (1.6, 1.2), both basic, both rows at their upper bound.
 * B = [[1, 2], [3, 1]], B^-1 = [[-0.2, 0.4], [0.6, -0.2]].
 * Cost ranging: -x0's cost may sit in [-3, -0.5], -x1's in [-2, -1/3].
 * RHS ranging: either row's 4 or 6 may sit in [2, 12]. */
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
    /* Nothing asked for is nothing done, and not an error. */
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
    /* Each array on its own. */
    double only[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, only, nullptr));
    EXACT_D(lo[0], only[0]);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_cost_ranging(m, nullptr, only));
    EXACT_D(hi[1], only[1]);
    jaos_model_free(m);
}

static void test_textbook_cost_ranging_maximised(void)
{
    /* The same basis in the other sense: the intervals flip sign and side. */
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
    /* The upper bounds are what the activities rest on: the ratio test. */
    NEAR(2.0, ul[0]);
    NEAR(12.0, uh[0]);
    NEAR(2.0, ul[1]);
    NEAR(12.0, uh[1]);
    /* The lower bounds are open below and may rise to the activity. */
    EXACT_D(-INFINITY, ll[0]);
    NEAR(4.0, lh[0]);
    EXACT_D(-INFINITY, ll[1]);
    NEAR(6.0, lh[1]);

    /* Both columns are basic: each bound may close in on the value. */
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

/* A model presolve answers by itself, so every range comes from the
 * postsolved basis (D257) and none from the simplex:
 *
 *   min x0  s.t.  x0 + x1 >= 1,  x0 in [0, 10],  x1 in [0, 0.5] cost 0
 *
 * x0 = 0.5 basic, x1 at its upper bound, the row at its lower. By hand:
 * x0's cost may fall to 0 before x0 would rather grow to 10; x1's may rise
 * to 1 before x0 is the cheaper way to fill the row; the row's 1 may sit
 * in [0.5, 10.5]; x1's 0.5 may sit in [0, 1]. The reference build reaches
 * the same basis through the simplex, so the numbers hold in both. */
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
    EXACT_D(-INFINITY, ll[0]);      /* x0 basic at 0.5 */
    NEAR(0.5, lh[0]);
    NEAR(0.5, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[1]);      /* x1 at its upper bound 0.5 */
    NEAR(0.5, lh[1]);
    NEAR(0.0, ul[1]);
    NEAR(1.0, uh[1]);
    jaos_model_free(m);
#endif
}

/* -- The solver as the oracle -------------------------------------------
 *
 *   min 2 x0 + 3 x1 + x2 + 4 x3 - x4
 *   row0:   x0 + x1 + x2 + x3 + x4 in [2, 8]
 *   row1:   x0 - x1     + 2 x3      in [-3, 3]
 *   row2: 2 x0      + x2 - x3 + x4 in [1, 5]
 *   x0..x3 in [0, 5],  x4 in [1, 1]
 *
 * Every column has at least two entries and a nonzero cost, no row's range
 * lies inside or outside its bounds, so presolve removes nothing but the
 * fixed column and a warm start from the published basis is the published
 * basis. x4 is fixed with a cost that wants it higher: its published status
 * is whichever bound the solver named, and ranging must read the side that
 * holds it from the reduced cost's sign, not from the status. */
#define ORACLE_EPS 1e-4
#define ORACLE_NCOL 5
#define ORACLE_NROW 3

/* The oracle crosses presolve, whose replay is wrong on purpose under the
 * two fault builds, so its helpers exist only where its tests run. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static jaos_model *make_oracle(void)
{
    const double c[]  = {2.0, 3.0, 1.0, 4.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 1.0};
    const double cu[] = {5.0, 5.0, 5.0, 5.0, 1.0};
    const double rl[] = {2.0, -3.0, 1.0}, ru[] = {8.0, 3.0, 5.0};
    /* col0: rows 0,1,2 = 1,1,2. col1: rows 0,1 = 1,-1. col2: rows 0,2 = 1,1.
     * col3: rows 0,1,2 = 1,2,-1. col4: rows 0,2 = 1,1. */
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

/* The same basis: the same statuses, except that a variable fixed in the
 * model as it stood may be named at either bound. */
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

/* Re-solves warm from the published basis with one number moved, and
 * says whether that basis was still the answer: no iteration and the same
 * statuses. Restores the number afterwards. */
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
    /* A move that inverts a box makes the model infeasible, which is one
     * way for the basis not to survive; the solver has to say so (D259). */
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

/* One interval [lo, hi] around `cur`: just inside either finite end the
 * basis survives, just outside it does not. An end equal to `cur` is a
 * degenerate tie and is not probed outward, since the basis is then one
 * of several optimal ones and another may carry the same answer. */
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
    /* The fixed column crosses presolve, whose replay is wrong on purpose. */
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_oracle();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);   /* the simplex, not presolve */
    jaos_basis_status cs[ORACLE_NCOL], rs[ORACLE_NROW];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));
    /* The premise of the oracle: an unchanged model re-solves for nothing. */
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
    /* The probes leave the model at their last re-solve, so the restored
     * model is solved again before it is ranged: from the published basis,
     * for nothing, which is the premise checked above once more. */
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
    /* A quiet oracle proves nothing: most ends here are finite and away
     * from the current value, and the count says the probes ran. */
    TEST_ASSERT_TRUE_MESSAGE(probed >= 10, "fewer than ten range ends were probed");
    jaos_model_free(m);
#endif
}

/* The case the oracle must reject: a range widened by hand, the way a
 * wrong ratio test would widen it, is refused at its new end. */
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
        /* Twice as wide on the upper side: the basis does not survive
         * there, which is what a range that were wrong would be claiming. */
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

/* Bit-identical on a second call, and on a second solve of the same model:
 * the ranges are functions of the basis and nothing else. */
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

/* A model with no rows at all: the basis is empty, every column is
 * nonbasic, and the reduced costs are the costs. Presolve answers it by
 * itself. The review of D258 found an early return that left every
 * reduced cost at zero here and published [1, +inf) for a cost of 1. */
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
    NEAR(0.0, lo[0]);              /* x0 at 0: its cost may fall to 0 */
    EXACT_D(INFINITY, hi[0]);
    EXACT_D(-INFINITY, lo[1]);     /* x1 at 3: its cost may rise to 0 */
    NEAR(0.0, hi[1]);
    double ll[2], lh[2], ul[2], uh[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    EXACT_D(-INFINITY, ll[0]);     /* no basic to hold x0's lower bound */
    NEAR(10.0, lh[0]);             /* up to where it meets the upper */
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

/* A mutual singleton whose row is open below:
 *
 *   min 0 x  s.t.  2 x <= 5,  x free
 *
 * Presolve removes both and the replay puts x at 5/2, basic, with the
 * row's logical out at the end the value was read from -- the UPPER one.
 * Until the review of D258 it was published at the lower end, a bound
 * of -inf the row does not have, and ranging refused the model. The
 * reference build stops at x = 0 with x nonbasic free and the row basic,
 * an equally optimal basis with its own ranges, so the numbers are
 * asserted on presolve's basis and only the calls' success on both. */
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
    /* x basic at 2.5: a positive cost would send it down without limit,
     * any negative one keeps it against the row. */
    EXACT_D(-INFINITY, lo[0]);
    NEAR(0.0, hi[0]);
    /* The row's upper bound moves x with it and nothing limits x. */
    EXACT_D(-INFINITY, ul[0]);
    EXACT_D(INFINITY, uh[0]);
    EXACT_D(-INFINITY, ll[0]);
    NEAR(5.0, lh[0]);
#endif
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_bound_ranging(m, ll, lh, ul, uh));
    jaos_model_free(m);
#endif
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_to_range_before_an_optimum);
    RUN_TEST(test_a_model_with_no_rows_ranges_its_costs);
    RUN_TEST(test_a_mutual_singleton_on_an_open_row_ranges);
    RUN_TEST(test_textbook_cost_ranging);
    RUN_TEST(test_textbook_cost_ranging_maximised);
    RUN_TEST(test_textbook_rhs_and_bound_ranging);
    RUN_TEST(test_a_presolved_basis_ranges_like_any_other);
    RUN_TEST(test_the_solver_agrees_with_every_range);
    RUN_TEST(test_the_oracle_rejects_a_widened_range);
    RUN_TEST(test_ranging_is_reproducible);
    return UNITY_END();
}
