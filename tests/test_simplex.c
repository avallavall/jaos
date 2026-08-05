/* Dual simplex tests.
 *
 * Every solve is judged twice: against the optimum worked out by hand, and
 * by the independent checker, which recomputes activities from the
 * original matrix and verifies primal feasibility, dual sign conditions
 * and the objective gap without consulting any solver bookkeeping. A
 * simplex bug cannot sign its own approval.
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

constexpr double CHECK_TOL = 1e-6;

static jaos_model *fresh(void)
{
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    return m;
}

/* Solves, then puts the answer through the independent checker. */
static void solve_and_verify(jaos_model *m, double expect_obj)
{
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expect_obj, obj);

    int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)(nc > 0 ? nc : 1), sizeof(double));
    double *y = calloc((size_t)(nr > 0 ? nr : 1), sizeof(double));
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(y);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, expect_obj, rep.primal_objective);

    free(x);
    free(y);
}

/* min x + y  s.t. x + y >= 2, 0 <= x,y <= 5. Optimum 2. */
static void test_minimise_over_a_ge_row(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

/* max 3x + 2y  s.t. x + y <= 4, x <= 2, 0 <= x <= 2, 0 <= y <= 10.
 * Optimum x=2, y=2, objective 10.
 *
 * The column bounds are finite on purpose: maximising pushes both reduced
 * costs negative, so the slack basis is only dual feasible if each column
 * has an upper bound to sit at. Without one the model needs a dual
 * phase 1 — see test_model_needing_dual_phase_one_says_so. */
static void test_maximise_with_two_rows(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {2.0, 10.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    /* col0 hits both rows, col1 only the first */
    const int64_t as[] = {0, 2, 3};
    const int64_t ai[] = {0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    solve_and_verify(m, 10.0);
    jaos_model_free(m);
}

/* An equality row: min x + 2y s.t. x + y = 3, 0<=x<=1, 0<=y<=5.
 * x is capped at 1, so y = 2 and the objective is 1 + 4 = 5. */
static void test_equality_row_with_a_capped_column(void)
{
    const double c[] = {1.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 5.0};
    const double rl[] = {3.0}, ru[] = {3.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, 5.0);
    jaos_model_free(m);
}

/* A ranged row exercises the bound machinery on logicals:
 * min -x s.t. 1 <= x <= 3 (as a row), 0 <= x <= 10. Optimum -3. */
static void test_ranged_row(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {1.0}, ru[] = {3.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

/* The objective constant must survive into the reported value. */
static void test_objective_offset_is_carried(void)
{
    const double c[] = {1.0};
    const double cl[] = {2.0}, cu[] = {8.0};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 7.5, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, 9.5);   /* x = 2 at its lower bound, plus 7.5 */
    jaos_model_free(m);
}

/* A model with no feasible point must be reported as such, not as a bad
 * optimum: x >= 3 and x <= 1 cannot both hold. */
static void test_infeasible_model_is_reported(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {3.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* Several rows and columns at once, still small enough to verify by hand:
 *   min 2a + 3b + 4c
 *   s.t. a + b + c >= 10
 *        a       <= 4
 *              b <= 3
 *   0 <= a,b,c <= 100
 * Cheapest is a=4, b=3, c=3 -> 8 + 9 + 12 = 29. */
static void test_three_by_three(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    /* a: rows 0,1 ; b: rows 0,2 ; c: row 0 */
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    solve_and_verify(m, 29.0);
    jaos_model_free(m);
}

/* Reading a model from disk and solving it must agree with the same model
 * built through the API — the readers and the solver meeting for the first
 * time. */
static void test_solving_a_model_read_from_mps(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    /* Same problem as test_three_by_three, this time off disk. */
    solve_and_verify(m, 29.0);
    jaos_model_free(m);
}

/* A genuinely unbounded model: min -x with x >= 0 and no ceiling, and a
 * row that does not restrain it. Dual phase 1 lends x an artificial upper
 * bound; the optimum settles on it, which is the evidence that the
 * objective wanted to run past a bound that was never real. */
static void test_unbounded_model_is_reported(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));

    /* No objective is on offer for a solve that found no optimum. */
    double obj = 1234.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(1234.0, obj);   /* left untouched */
    jaos_model_free(m);
}

/* The same shape of column — cost pushing towards a missing bound — but
 * held back by a constraint. The artificial bound is never reached, so it
 * never mattered, and the answer is the real problem's.
 *
 *   min -x - 2y  s.t. x + y <= 4, x,y >= 0, neither bounded above
 * Optimum puts everything on y: y = 4, objective -8. */
static void test_missing_bound_that_a_row_restrains_solves_normally(void)
{
    const double c[] = {-1.0, -2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, -8.0);
    jaos_model_free(m);
}

/* Maximisation with no upper bounds is the same situation mirrored, and
 * was the case that used to be refused outright.
 *   max 3x + 2y  s.t. x + y <= 4, x <= 2  ->  x=2, y=2, objective 10 */
static void test_maximise_without_column_upper_bounds(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {4.0, 2.0};
    const int64_t as[] = {0, 2, 3};
    const int64_t ai[] = {0, 1, 0};
    const double av[] = {1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    solve_and_verify(m, 10.0);
    jaos_model_free(m);
}

/* t1.mps was written to exercise the reader and turns out to have no
 * feasible point at all: EQ1 forces X3 = 7 + X2, so X2 >= -1 puts X3 at 6
 * or more, while LIM2 caps X1 + X3 at 3.5 with X1 >= 0. Worth keeping
 * exactly for that — an infeasibility that comes off disk through the
 * reader, in a model nobody constructed to be infeasible. It also needs
 * phase 1 to start, since X3 has a negative cost and no upper bound. */
static void test_t1_mps_is_infeasible_and_says_so(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/t1.mps"));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* An objective of exactly zero must be reported as an answer, not
 * confused with "nothing to report". */
static void test_zero_objective_is_distinguishable_from_no_answer(void)
{
    const double c[] = {0.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {0.0}, ru[] = {1.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double obj = -1.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, obj);
    jaos_model_free(m);
}

/* Pricing must be charged to the work counter, not just the LU kernels:
 * PLAN 2.7 weights "nonzero touched ... in pricing" the same as any other
 * solve traffic, and a budget that ignored it would not bound the run. */
static void test_pricing_is_charged_to_the_work_counter(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* Iterations happened, so pricing happened, so the counter must have
     * moved well past the factorization's own fixed charge. */
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    TEST_ASSERT_TRUE(jaos_work_units(m) > JM_WORK_FACTOR);
    jaos_model_free(m);
}

/* Three rows violated at once, by amounts three million apart, because the
 * same constraints are written in different units:
 *      x1 + x2       >= 2
 *   1000 x2 + 1000 x3 >= 3000        (x2 + x3 >= 3)
 *  0.001 x1 + 0.001 x3 >= 0.001      (x1 + x3 >= 1)
 * minimising x1 + x2 + x3 over [0, 10]^3. Adding the three constraints
 * gives 2(x1+x2+x3) >= 6, and x = (0, 2, 1) attains it: the optimum is 3,
 * whichever order the rows are repaired in.
 *
 * The units are the point. A rule that ranks rows by raw violation sees
 * one row breached by 3000 and another by a thousandth, and reads that as
 * importance rather than as millimetres against kilometres. */
static void test_simultaneous_violations_of_wildly_different_size(void)
{
    const double c[] = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {10.0, 10.0, 10.0};
    const double rl[] = {2.0, 3000.0, 0.001};
    const double ru[] = {INFINITY, INFINITY, INFINITY};
    /* x1: rows 0,2 ; x2: rows 0,1 ; x3: rows 1,2 */
    const int64_t as[] = {0, 2, 4, 6};
    const int64_t ai[] = {0, 2, 0, 1, 1, 2};
    const double av[] = {1.0, 0.001, 1.0, 1000.0, 1000.0, 0.001};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    solve_and_verify(m, 3.0);
    jaos_model_free(m);
}

/* ---- dual steepest-edge weights -------------------------------------- */

/* The weight recurrence is a heuristic: get it wrong and JAOS still
 * answers correctly, only slower, so no solve-level assertion can catch
 * it. It is checked directly instead, against the row norms of B^-1
 * recomputed from a factorization of the resulting basis — which shares no
 * arithmetic with the recurrence under test. */

constexpr int64_t DSE_N = 3;

typedef struct {
    int64_t start[DSE_N + 1];
    int64_t index[DSE_N * DSE_N];
    double  value[DSE_N * DSE_N];
} csc3;

static void pack3(double a[DSE_N][DSE_N], csc3 *out)
{
    int64_t nz = 0;
    for (int64_t j = 0; j < DSE_N; j++) {
        out->start[j] = nz;
        for (int64_t i = 0; i < DSE_N; i++)
            if (a[i][j] != 0.0) {
                out->index[nz] = i;
                out->value[nz] = a[i][j];
                nz++;
            }
    }
    out->start[DSE_N] = nz;
}

static void factor3(double a[DSE_N][DSE_N], csc3 *c, jm_lu *lu)
{
    pack3(a, c);
    jm_lu_init(lu);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_lu_factor(lu, DSE_N, c->start, c->index,
                                                c->value, 0.1, nullptr));
    TEST_ASSERT_EQUAL_INT64(DSE_N, lu->rank);
}

/* ||row i of B^-1||^2, for every i. */
static void exact_weights(double b[DSE_N][DSE_N], double *w)
{
    csc3 c;
    jm_lu lu;
    factor3(b, &c, &lu);
    for (int64_t i = 0; i < DSE_N; i++) {
        double rho[DSE_N] = {0.0};
        rho[i] = 1.0;
        jm_lu_btran(&lu, rho, nullptr);
        double sum = 0.0;
        for (int64_t k = 0; k < DSE_N; k++)
            sum += rho[k] * rho[k];
        w[i] = sum;
    }
    jm_lu_free(&lu);
}

static void test_dse_weights_match_recomputed_norms(void)
{
    /* Nothing special about this basis beyond being nonsingular, and an
     * entering column that leaves the replacement nonsingular too. */
    double b[DSE_N][DSE_N] = {
        {2.0, 1.0, 0.0},
        {0.0, 3.0, 1.0},
        {1.0, 0.0, 4.0},
    };
    const double aq[DSE_N] = {1.0, 2.0, 3.0};
    const int64_t r = 1;            /* the basis row the column enters at */

    double w[DSE_N];
    exact_weights(b, w);

    /* alpha = B^-1 a_q and tau = B^-1 (B^-T e_r), both against the basis
     * as it stands before the change — which is exactly how the simplex
     * has them when it pivots. */
    csc3 c;
    jm_lu lu;
    factor3(b, &c, &lu);

    double alpha[DSE_N];
    memcpy(alpha, aq, sizeof alpha);
    jm_lu_ftran(&lu, alpha, nullptr);

    double tau[DSE_N] = {0.0};
    tau[r] = 1.0;
    jm_lu_btran(&lu, tau, nullptr);
    jm_lu_ftran(&lu, tau, nullptr);
    jm_lu_free(&lu);

    jm_dse_update(DSE_N, w, r, alpha, tau);

    double after[DSE_N][DSE_N];
    memcpy(after, b, sizeof after);
    for (int64_t i = 0; i < DSE_N; i++)
        after[i][r] = aq[i];

    double expect[DSE_N];
    exact_weights(after, expect);

    for (int64_t i = 0; i < DSE_N; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, expect[i], w[i]);
}

/* Determinism (D8): the same model solved twice must produce the same
 * objective bit for bit, the same iteration count, and the same work. */
static void test_solving_twice_is_bit_identical(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    double obj[2];
    int64_t iters[2], work[2];
    for (int run = 0; run < 2; run++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         5, as, ai, av));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[run]));
        iters[run] = jaos_iterations(m);
        work[run] = jaos_work_units(m);
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_MEMORY(&obj[0], &obj[1], sizeof(double));
    TEST_ASSERT_EQUAL_INT64(iters[0], iters[1]);
    TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
}

/* A work limit must stop the solve and say so, rather than running on or
 * pretending to have found an optimum. */
static void test_work_limit_stops_and_reports(void)
{
    const double c[] = {2.0, 3.0, 4.0};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 100.0, 100.0};
    const double rl[] = {10.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 4.0, 3.0};
    const int64_t as[] = {0, 2, 4, 5};
    const int64_t ai[] = {0, 1, 0, 2, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    jaos_model_free(m);
}

/* Loading a new problem must not silently discard configured budgets. */
static void test_budgets_survive_a_reload(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {0.0}, ru[] = {1.0};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 12345));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(12345, m->work_limit);
    jaos_model_free(m);
}

static void test_queries_before_a_solve(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NOT_RUN, jaos_status_of(nullptr));
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));

    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_solve(nullptr));
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_minimise_over_a_ge_row);
    RUN_TEST(test_maximise_with_two_rows);
    RUN_TEST(test_equality_row_with_a_capped_column);
    RUN_TEST(test_ranged_row);
    RUN_TEST(test_objective_offset_is_carried);
    RUN_TEST(test_infeasible_model_is_reported);
    RUN_TEST(test_three_by_three);
    RUN_TEST(test_solving_a_model_read_from_mps);
    RUN_TEST(test_unbounded_model_is_reported);
    RUN_TEST(test_missing_bound_that_a_row_restrains_solves_normally);
    RUN_TEST(test_maximise_without_column_upper_bounds);
    RUN_TEST(test_t1_mps_is_infeasible_and_says_so);
    RUN_TEST(test_zero_objective_is_distinguishable_from_no_answer);
    RUN_TEST(test_pricing_is_charged_to_the_work_counter);
    RUN_TEST(test_simultaneous_violations_of_wildly_different_size);
    RUN_TEST(test_dse_weights_match_recomputed_norms);
    RUN_TEST(test_solving_twice_is_bit_identical);
    RUN_TEST(test_work_limit_stops_and_reports);
    RUN_TEST(test_budgets_survive_a_reload);
    RUN_TEST(test_queries_before_a_solve);
    return UNITY_END();
}
