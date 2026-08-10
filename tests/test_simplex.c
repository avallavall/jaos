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

    /* An infeasible model has no solution to hand out either. */
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
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

    /* No objective and no solution are on offer for a solve that found no
     * optimum: zeros handed out here would be indistinguishable from an
     * answer that is genuinely zero. */
    double obj = 1234.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_DOUBLE(1234.0, obj);   /* left untouched */
    double x[1];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
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

/* The pair below is the whole of the unbounded verdict, and the only thing
 * that differs between them is the ceiling on the row.
 *
 *   min -x  s.t. x <= R, x >= 0, no column upper bound
 *
 * Either way dual phase 1 lends x an upper bound and the optimum of the
 * bounded problem comes to rest exactly on it, so "a variable is sitting on
 * a bound JAOS invented" cannot tell the two apart — which is precisely
 * what the old verdict read. What tells them apart is whether letting x off
 * that bound runs into anything: with R infinite nothing blocks and the
 * model really is unbounded; with R finite the row blocks, and the model
 * has a finite optimum at -R that this phase 1 cannot reach.
 *
 * This one is R infinite, and it passed before the ray existed too — it is
 * here to hold the verdict that was already right. The one that moves is
 * below it. */
static void test_a_ray_is_what_proves_unbounded(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_UNBOUNDED, jaos_status_of(m));
    jaos_model_free(m);
}

/* The same model with the row capped, and the test that actually moved:
 * against the old verdict it returns UNBOUNDED, which is a wrong answer
 * rather than a missing one, on a model whose optimum is a perfectly
 * ordinary -1e11.
 *
 * The cap is ten times ARTIFICIAL_BOUND, and that placement is the point.
 * Enlarging the loan does not turn this green — at 1e12 the model solves to
 * OPTIMAL and the assertion fails on the status instead — so the test
 * cannot be satisfied by a constant that mimics the ray. Both halves were
 * run: UNBOUNDED against the old code, OPTIMAL against a widened loan. */
static void test_an_optimum_past_the_lent_bound_is_refused(void)
{
    const double c[] = {-1.0};
    const double cl[] = {0.0}, cu[] = {INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {1e11};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_NUMERICAL_ERROR, jaos_status_of(m));

    /* Refusing silently would be no better than answering wrongly: a
     * caller has to be able to find out which column could not be sized. */
    const char *err = jaos_model_error(m);
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_NOT_NULL(strstr(err, "phase 1"));
    jaos_model_free(m);
}

/* Both columns get a lent bound and neither is held by one at the end.
 *
 *   min -x - y  s.t. x + y <= 3, x,y >= 0, neither bounded above
 *
 * Every point on x + y = 3 is optimal at -3. One column ends up basic and
 * the other nonbasic at its *real* lower bound, so no ray is ever computed
 * — the verdict comes out of the third branch, the one that says the loans
 * never mattered. Instrumenting the solve confirms that is the branch taken
 * rather than the ray returning blocked.
 *
 * It is here because that branch is what every ordinary model reaches, and
 * a verdict that only handled its own two interesting cases would fail on
 * all the rest. */
static void test_a_lent_bound_that_never_constrained_anything(void)
{
    const double c[] = {-1.0, -1.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {3.0};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

/* Optimality declared on carried numbers rather than computed ones.
 *
 *   min 2a - 3b  s.t. 4a + b <= 19, -3a + b <= 13, a,b >= 0
 *
 * Both rows bind at a = 6/7, b = 109/7, where the objective is exactly -45.
 * Small as it is, this model used to finish with reduced costs the
 * independent checker rejected: x_B and the factorization are both carried
 * forward by the pivots, and the test for optimality was applied to the
 * carried values, which had drifted. Pricing the point again from a fresh
 * factorization is what makes the published duals belong to the basis.
 *
 * Against the old code this fails twice over, and the objective goes first:
 * -44.9999943 for an optimum that is exactly -45, an error of 6e-6 on a
 * model of two rows. The checker rejects the duals as well. Both are the
 * same drift seen from two sides, which is why the assertion is
 * solve_and_verify rather than either one alone.
 *
 * What it cannot be satisfied by is a tolerance. The violation behind it
 * sat far outside the solver's own, in the solver's own scaled space, so
 * loosening anything would hide it rather than fix it. */
static void test_optimality_is_rechecked_before_it_is_believed(void)
{
    const double c[] = {2.0, -3.0};
    const double cl[] = {0.0, 0.0};
    const double cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY, -INFINITY};
    const double ru[] = {19.0, 13.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {4.0, -3.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, -45.0);
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

/* A hundred rows to repair, one iteration each: the refactorization
 * interval (64) falls in the middle, so this is the one test where the
 * mid-solve refresh path — refactor, recompute primal and duals, carry the
 * steepest-edge weights across — actually runs. Every other model in this
 * file finishes in a handful of iterations and never touches it.
 *
 * The model: min sum x_i with a row x_i >= 1 per column, x in [0, 10],
 * plus one coupling row sum x_i <= 200 so the basis matrix is not
 * diagonal and the updates have something to do. Optimum: every x_i = 1,
 * objective 100. Solved twice, and the two runs must agree bit for bit —
 * determinism (D8) across a refactorization, not only across the short
 * solves the other test pins. */
static void test_a_long_solve_crosses_a_refactorization(void)
{
    enum { N = 100 };
    double c[N], cl[N], cu[N];
    double rl[N + 1], ru[N + 1];
    int64_t as[N + 1], ai[2 * N];
    double av[2 * N];

    for (int64_t j = 0; j < N; j++) {
        c[j] = 1.0;
        cl[j] = 0.0;
        cu[j] = 10.0;
        rl[j] = 1.0;
        ru[j] = INFINITY;
        as[j] = 2 * j;
        ai[2 * j] = j;          /* its own row */
        av[2 * j] = 1.0;
        ai[2 * j + 1] = N;      /* the coupling row */
        av[2 * j + 1] = 1.0;
    }
    as[N] = 2 * N;
    rl[N] = -INFINITY;
    ru[N] = 200.0;

    double obj[2];
    int64_t iters[2], work[2];
    for (int run = 0; run < 2; run++) {
        jaos_model *m = fresh();
        TEST_ASSERT_EQUAL_INT(JAOS_OK,
            jaos_load_lp(m, N, N + 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                         2 * N, as, ai, av));
        solve_and_verify(m, 100.0);

        /* One entering column per violated row, and the interval is 64:
         * anything past it proves the mid-solve refresh ran. The exact
         * count is pinned by the determinism assertion below instead. */
        TEST_ASSERT_TRUE(jaos_iterations(m) > 64);

        TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj[run]));
        iters[run] = jaos_iterations(m);
        work[run] = jaos_work_units(m);
        jaos_model_free(m);
    }
    TEST_ASSERT_EQUAL_MEMORY(&obj[0], &obj[1], sizeof(double));
    TEST_ASSERT_EQUAL_INT64(iters[0], iters[1]);
    TEST_ASSERT_EQUAL_INT64(work[0], work[1]);
}

/* A free variable — no bounds, zero cost — is a status of its own
 * (JM_FREE) with its own branches in pricing, the ratio test and the
 * shifting, and no other model in this file has one.
 *
 *   min 2x  s.t.  x + z >= 4,  x - z >= -1,  x in [0, 10],  z free
 *
 * z must enter the basis: it is the only way either row moves. Both rows
 * end tight — z >= 4 - x and z <= 1 + x force 4 - x <= 1 + x, so
 * x = 1.5, z = 2.5, objective 3. The duals come from the two active rows:
 * y1 + y2 = 2 (column x) and y1 - y2 = 0 (column z), so y = (1, 1). */
static void test_free_variable_enters_and_settles(void)
{
    const double c[] = {2.0, 0.0};
    const double cl[] = {0.0, -INFINITY};
    const double cu[] = {10.0, INFINITY};
    const double rl[] = {4.0, -1.0};
    const double ru[] = {INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, -1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, 3.0);

    double x[2], y[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.5, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.5, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, y[1]);
    jaos_model_free(m);
}

/* The work accounting, pinned. PLAN 2.7 defines what gets charged —
 * pricing, ratio test, eliminations, the fixed floors — and D16 makes the
 * count deterministic, so for a fixed model the total is one exact number
 * and any change to what is charged moves it.
 *
 * This replaces an assertion of `work > JM_WORK_FACTOR`, which was
 * vacuous: a single factorization alone satisfies it, so deleting every
 * charge outside the LU left it green.
 *
 * When this fails after a deliberate change to the algorithm or the
 * weights, re-pin: the diff of this constant is the record of what the
 * change did to the accounting. If it fails and you did not intend to
 * change the accounting, that is the bug it exists to catch.
 *
 * D43 did not move it, and that was worked for rather than lucky. The BTRAN
 * reporting where its answer is nonzero went through three shapes here:
 * 8566 when the pattern was ordered whatever its size, 8557 once the
 * thresholds refused to order a dense one, and 8548 while the dense branch
 * still forgot to leave the pattern behind for the exact weight. A basis
 * this small takes the dense branch of every one of those decisions, so the
 * accounting it sees must come out unchanged — and an entry whose baseline
 * diff mixes an accounting correction into a measurement cannot be read at
 * all. The number staying put is the evidence that it does not.
 *
 * Last moved by summing the exact steepest-edge weight over rho's pattern
 * (D42): 8548 -> 8545. The norm is charged for the slots it adds up rather
 * than for the dimension, and over this solve rho held three zeros in total.
 * Three units is the whole of it because a three-row row of B^-1 has almost
 * nothing to skip — the same shape of answer the two entries below give, and
 * for the same reason.
 *
 * Neither D40 nor D41 moved it, which is itself the accounting working: both
 * read the pricing row through its pattern only where the pattern is at most
 * a quarter of the variables, and a quarter of six variables is one.
 *
 * Before that, by BTRAN's reachability search (D38): 8544 -> 8548. The
 * search is billed for the edges it walks, and on a three-row basis it walks
 * almost the whole of U to discover that almost the whole of U is
 * reachable. This test is where the technique costs the most and saves the
 * least; the instance sets are where the question is settled.
 *
 * Before that, by row-wise pricing (D35): 8535 -> 8544. The column-wise pass
 * skipped a basic variable without reading it at all; the row-wise one walks
 * matrix rows, so it reads the entries of basic columns too and is charged
 * for them. On a three-row model with five nonzeros that is the whole of the
 * difference, and it goes the wrong way — the saving is in skipping rows
 * where rho is zero, and on a model this small rho has no zeros to skip.
 * Which way it goes on real models is the question the instance sets answer,
 * not this test.
 *
 * Before that, by the refinement that recheck asks its two solves for (D29):
 * 8517 -> 8535, one extra FTRAN and one extra BTRAN over a three-row basis
 * plus the two residuals they are computed from. Before that, by the recheck
 * itself: 4411 -> 8517, the one extra factorization it costs plus the
 * pricing pass that follows it. */
constexpr int64_t WORK_PINNED = 8545;

static void test_work_accounting_is_pinned(void)
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

    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    TEST_ASSERT_EQUAL_INT64(WORK_PINNED, jaos_work_units(m));
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

/* The pivot the three weight tests below all share: a basis with nothing
 * special about it beyond being nonsingular, an entering column that
 * leaves the replacement nonsingular too, and the two transformed vectors
 * the recurrence needs — both taken against the basis as it stands before
 * the change, which is exactly how the simplex has them when it pivots.
 *
 * Fills `w` with the exact weights before the pivot, `expect` with the
 * exact weights after it, and alpha/tau with what jm_dse_update consumes.
 */
constexpr int64_t DSE_ROW = 1;      /* the basis row the column enters at */

static void dse_pivot_case(double *w, double *expect,
                           double *alpha, double *tau)
{
    double b[DSE_N][DSE_N] = {
        {2.0, 1.0, 0.0},
        {0.0, 3.0, 1.0},
        {1.0, 0.0, 4.0},
    };
    const double aq[DSE_N] = {1.0, 2.0, 3.0};

    exact_weights(b, w);

    csc3 c;
    jm_lu lu;
    factor3(b, &c, &lu);

    memcpy(alpha, aq, DSE_N * sizeof *alpha);
    jm_lu_ftran(&lu, alpha, nullptr);

    memset(tau, 0, DSE_N * sizeof *tau);
    tau[DSE_ROW] = 1.0;
    jm_lu_btran(&lu, tau, nullptr);
    jm_lu_ftran(&lu, tau, nullptr);
    jm_lu_free(&lu);

    double after[DSE_N][DSE_N];
    memcpy(after, b, sizeof after);
    for (int64_t i = 0; i < DSE_N; i++)
        after[i][DSE_ROW] = aq[i];
    exact_weights(after, expect);
}

static void test_dse_weights_match_recomputed_norms(void)
{
    double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
    dse_pivot_case(w, expect, alpha, tau);

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, w[DSE_ROW], 10.0, nullptr, 0);

    for (int64_t i = 0; i < DSE_N; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, expect[i], w[i]);
}

/* A carried weight that has slipped, but not far enough to be worthless,
 * is repaired rather than propagated: the exact value is known for that
 * one row, so the answer must be the same as if it had never slipped. */
static void test_dse_repairs_a_carried_weight_that_slipped(void)
{
    double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
    dse_pivot_case(w, expect, alpha, tau);

    double truth = w[DSE_ROW];
    w[DSE_ROW] = truth * 1.5;       /* inside a factor of ten */

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0, nullptr, 0);

    for (int64_t i = 0; i < DSE_N; i++)
        TEST_ASSERT_DOUBLE_WITHIN(1e-9, expect[i], w[i]);
}

/* Past the factor there is nothing to repair with: the other weights are
 * carried by the same recurrence that produced this one, so all of them
 * are thrown away for the neutral prior. Both directions count — a weight
 * that has shrunk makes its row look urgent, which is the worse of the
 * two. */
static void test_dse_restarts_when_the_carried_weight_has_drifted(void)
{
    for (int trial = 0; trial < 2; trial++) {
        double w[DSE_N], expect[DSE_N], alpha[DSE_N], tau[DSE_N];
        dse_pivot_case(w, expect, alpha, tau);

        double truth = w[DSE_ROW];
        w[DSE_ROW] = trial == 0 ? truth * 1e6 : truth * 1e-6;

        jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0, nullptr, 0);

        for (int64_t i = 0; i < DSE_N; i++)
            TEST_ASSERT_EQUAL_DOUBLE(1.0, w[i]);
    }
}

/* ---- scaling ---------------------------------------------------------- */

/*   min 1e6 x1 + 1.5 x2 + 1e6 x3
 *   s.t. 1e6 x1 +   x2 + 1e3 x3 >= 2
 *        1e6 x1 + 2 x2 + 1e3 x3 >= 3
 *        0 <= x <= 10
 *
 * Coefficients three orders of magnitude apart in both directions, which
 * is what makes the scaling produce factors on rows and columns alike.
 * Measured in what a unit of column 1 contributes — a = 1e6 x1 — the
 * problem reads min a + 1.5 x2 + 1000 (1e3 x3) subject to a + x2 >= 2 and
 * a + 2 x2 >= 3, whose unique optimum is a = 1, x2 = 1. So x1 = 1e-6,
 * x2 = 1, x3 = 0 and the objective is 2.5; column 3 is priced at a
 * thousand times what it contributes and never enters. */
static jaos_model *badly_scaled_model(void)
{
    static const double c[] = {1e6, 1.5, 1e6};
    static const double cl[] = {0.0, 0.0, 0.0};
    static const double cu[] = {10.0, 10.0, 10.0};
    static const double rl[] = {2.0, 3.0};
    static const double ru[] = {INFINITY, INFINITY};
    static const int64_t as[] = {0, 2, 4, 6};
    static const int64_t ai[] = {0, 1, 0, 1, 0, 1};
    static const double av[] = {1e6, 1e6, 1.0, 2.0, 1e3, 1e3};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    return m;
}

/* The solver works on a scaled copy, so the claim to test is that the
 * change of variable is a change of variable: the same optimum comes out
 * whether the arithmetic happened in the model's units or in scaled ones.
 *
 * Both halves of the scaling are asserted to be non-trivial first.
 * Without that this test could pass by scaling nothing at all, which is
 * the failure it exists to catch — and it very nearly did: the model it
 * used before turned out to have every column factor equal to one. */
static void test_scaling_changes_the_arithmetic_not_the_answer(void)
{
    jaos_model *plain = badly_scaled_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(plain, JM_SCALE_NONE));
    solve_and_verify(plain, 2.5);

    jaos_model *scaled = badly_scaled_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jm_model_scale(scaled, JM_SCALE_CURTIS_REID));

    bool rows_moved = false, cols_moved = false;
    for (int64_t i = 0; i < jaos_num_row(scaled); i++)
        if (scaled->row_scale[i] != 1.0)
            rows_moved = true;
    for (int64_t j = 0; j < jaos_num_col(scaled); j++)
        if (scaled->col_scale[j] != 1.0)
            cols_moved = true;
    TEST_ASSERT_TRUE(rows_moved);
    TEST_ASSERT_TRUE(cols_moved);

    solve_and_verify(scaled, 2.5);

    /* And the two agree with each other, not merely each with its own
     * tolerance. */
    double a = 0.0, b = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(plain, &a));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(scaled, &b));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, a, b);

    jaos_model_free(plain);
    jaos_model_free(scaled);
}

/* Every number the solver reports is computed in scaled units and has to
 * arrive in the caller's. The checker covers the primal values and the
 * objective; it does not cover the row activities or the reduced costs,
 * because it recomputes those from the matrix and the duals rather than
 * trusting what it is handed. So they are pinned here, against arithmetic
 * done by hand.
 *
 * Both rows are tight at the optimum and columns 1 and 2 are basic, which
 * fixes the duals: y0 + y1 = 1 from column 1 and y0 + 2 y1 = 1.5 from
 * column 2, so y = (0.5, 0.5). Column 3 then prices at
 * 1e6 - (0.5 + 0.5) * 1e3 = 999000. */
static void test_answers_come_back_in_the_models_units(void)
{
    jaos_model *m = badly_scaled_model();
    solve_and_verify(m, 2.5);     /* no mode chosen: the default applies */

    double x[3], act[2], y[2], dj[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, dj));

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1e-6, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[2]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, act[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, act[1]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, y[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, y[1]);

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, dj[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, dj[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 999000.0, dj[2]);

    jaos_model_free(m);
}

/* Eight boxed columns feeding one row:
 *   min sum j*x_j  s.t. sum x_j >= 5.5, 0 <= x_j <= 1
 * The answer is to fill the five cheapest columns and half-fill the sixth:
 * 1+2+3+4+5 + 3 = 18.
 *
 * Every column starts at its lower bound and every one of them blocks the
 * dual step in turn, so without bound flipping this is one iteration per
 * column filled. With it the step passes each column by swapping it to its
 * upper bound and carries on while the row is still short, and the whole
 * model is one long step: five swaps and a single basis change. The
 * iteration count is asserted for exactly that reason - it is the only
 * place the long step is visible from outside. */
static void test_bound_flipping_fills_columns_in_one_step(void)
{
    const double c[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    const double cl[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    const double cu[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    const double rl[] = {5.5}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    const int64_t ai[] = {0, 0, 0, 0, 0, 0, 0, 0};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 8, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     8, as, ai, av));
    solve_and_verify(m, 18.0);
    TEST_ASSERT_EQUAL_INT64(1, jaos_iterations(m));
    jaos_model_free(m);
}

/* ---- cost shifting and settling up ------------------------------------ */

/* A model built to make the ratio test spend dual feasibility and then to
 * make both outcomes of settling up visible in one solve:
 *
 *   min 0 xA + 2e-8 xC + 5e-7 xB   s.t.  xA + xC + 10 xB >= 1
 *   xA in [0, 100],  xC in [0, 0.001],  xB in [0, 100]
 *
 * Scaling is switched off, so the numbers below are the ones the solver
 * sees. At the slack start every reduced cost is its cost and every pivot
 * entry is minus its coefficient, so the candidates are A at ratio 0, C at
 * 2e-8 and B at 5e-8. Bound flipping stops at A — swapping a box 100 wide
 * would overshoot a violation of 1 — and Harris then takes B, whose pivot
 * is ten times the others', for a step of 5e-8. That step pushes A and C
 * past zero by 5e-8 and 3e-8, which is what the shifting buys back.
 *
 * Settling up hands both cases back. A wants to move to 100, which would
 * drive the basic column to -9.9, so the free repair refuses it; C wants to
 * move to 0.001, which leaves the basic at 0.0999 and is taken. That used
 * to be the end of it, and this test used to assert the result: x = (0,
 * 0.001, 0.0999) at an objective of 5e-8, primal feasible, with a dual
 * certificate that does not carry.
 *
 * **It carries now, and the answer was wrong before.** A's reduced cost of
 * -5e-8 points at an upper bound of 100 it is nowhere near, so its term in
 * `P - D` is 5e-8 * 100 = 5e-6, and the re-entry moves a column when its
 * term is worth moving (D27). Sending A to 100 breaks the primal by a mile
 * and the dual simplex repairs it in one pivot, landing on A basic at 1.
 *
 * A's reduced cost also passes the other half of that test, which is what
 * stops it being noise: the only term in `d_A` is `y * 1 = 5e-8`, so the
 * traffic through the column is 5e-8 and the reduced cost stands 4.5e15
 * times the rounding of its own dot product.
 *
 * That is the true optimum and it is checkable by hand: A costs nothing and
 * satisfies the row on its own, so the objective is 0 and every other
 * answer is worse. The solve used to stop 5e-8 above it on a basis whose
 * duals could not be certified, which PLAN 2.8 recorded as a defect. This
 * test is what closes it, and the reason it is worth more than the Netlib
 * evidence is that nobody has to trust a reference value to read it. */
static void test_settling_up_reaches_the_optimum_a_shifted_basis_hid(void)
{
    const double c[] = {0.0, 2e-8, 5e-7};
    const double cl[] = {0.0, 0.0, 0.0};
    const double cu[] = {100.0, 0.001, 100.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2, 3};
    const int64_t ai[] = {0, 0, 0};
    const double av[] = {1.0, 1.0, 10.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     3, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jm_model_scale(m, JM_SCALE_NONE));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[3], act[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, nullptr, nullptr));

    /* A alone, which is what costs nothing. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, x[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, x[1]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, act[0]);

    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, obj);

    /* And the certificate carries, which is the half that used to fail.
     * The row prices at zero now; y = 5e-8 belonged to the basis the
     * re-entry left behind. */
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, y[0]);

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.objective_gap);

    /* Both halves of the gap are zero, not merely their difference. On the
     * answer this test used to assert, `gap_positive` was 5e-6 — the whole
     * of the suboptimality, and exactly the term the re-entry now reads to
     * decide the flip (D24, D27). */
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, rep.gap_negative);

    jaos_model_free(m);
}

/* A clean-up pass must act on every column it decided wants a pivot, not on
 * the first one and then stop.
 *
 * It used to stop. The predicate that decides "this column wants a pivot"
 * consulted the duals, and the first pivot of the pass overwrote the vector
 * they lived in, so from the second candidate onwards the question was put
 * to the wrong quantity and answered no. Underneath that, the basis change
 * itself lends every other candidate's sign condition away — it sets the
 * reduced cost to zero and books the loan — so even asking correctly finds
 * nothing left to do unless the loan is called in first.
 *
 * Neither half is visible in a model built by hand: the state only arises
 * where settling leaves a column at a bound with nothing on the other side,
 * which needs a solve long enough to accumulate loans. So the model is
 * generated — twenty rows, forty columns, half of them unbounded above, and
 * costs small enough that the ratio test's window can push a reduced cost
 * past zero. The seed is not arbitrary and not tuned to pass: it was found by
 * sweeping, and it is one where the clean-up identifies two columns and
 * pivots both, twice over.
 *
 * **What this can and cannot catch, because the first attempt at it caught
 * nothing.** Re-introducing the defect leaves this model's answer correct and
 * its iteration count identical: the same pivots still happen, one per round
 * instead of several per call, and the re-entry simply goes round more times.
 * The answer only breaks when the round cap runs out, and that needs a model
 * needing more than thirty-two clean-up pivots — `pilot87` scale, not unit
 * test scale. So an assertion on the answer alone passes either way and is
 * worth nothing here.
 *
 * What does separate them is the cost of those extra rounds, each of which
 * is a refactorization and a re-solve. Measured on this model: **58141 work
 * units correct, 67416 with the defect**, and the bound below sits between
 * them with room on both sides. That is the number this test actually
 * guards; the checker assertions guard the answer, which no longer moves.
 *
 * The oracle for the answer is the independent checker rather than a pinned
 * objective, because what went wrong was a certificate that did not carry. */
static void test_a_clean_up_pass_dispatches_every_column_it_identified(void)
{
    constexpr int64_t NR = 20;
    constexpr int64_t NC = 40;

    double c[NC], cl[NC], cu[NC], rl[NR], ru[NR], av[NC * 4];
    int64_t as[NC + 1], ai[NC * 4];

    /* Deterministic and self-contained: a solve must not depend on a library
     * PRNG, and a test model must not depend on one either. */
    uint64_t st = 236;
    #define NEXTU() (st = st * 6364136223846793005u + 1442695040888963407u, \
                     (double)((st >> 11) & 0x1FFFFFFFFFFFFFu) / 9007199254740992.0)

    int64_t nz = 0;
    for (int64_t j = 0; j < NC; j++) {
        as[j] = nz;
        int per = 2 + (int)(NEXTU() * 3.0);
        int64_t rows[8];
        int nrows = 0;
        for (int k = 0; k < per; k++) {
            int64_t r = (int64_t)(NEXTU() * (double)NR);
            bool dup = false;
            for (int t = 0; t < nrows; t++)
                if (rows[t] == r) dup = true;
            if (!dup) rows[nrows++] = r;
        }
        for (int a = 1; a < nrows; a++) {          /* the reader wants them sorted */
            int64_t v = rows[a];
            int b = a - 1;
            while (b >= 0 && rows[b] > v) { rows[b + 1] = rows[b]; b--; }
            rows[b + 1] = v;
        }
        for (int k = 0; k < nrows; k++) {
            ai[nz] = rows[k];
            /* Positive throughout: a row of negatives against a positive
             * lower bound has no feasible point, and an infeasible model
             * never reaches the settling this is about. */
            av[nz] = pow(10.0, NEXTU() * 2.0 - 1.0);
            nz++;
        }
        c[j] = pow(10.0, -6.0 - NEXTU() * 3.0);
        cl[j] = 0.0;
        cu[j] = (j < NC / 2) ? INFINITY : 1.0 + NEXTU() * 10.0;
    }
    as[NC] = nz;
    for (int64_t i = 0; i < NR; i++) { rl[i] = 1.0 + NEXTU(); ru[i] = INFINITY; }
    #undef NEXTU

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, NC, NR, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    double x[NC], act[NR], y[NR], d[NC];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, act, y, d));

    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);

    /* 58141 correct, 67416 with one pivot per call. Not a pinned value: a
     * ceiling with a measurement on each side of it. */
    TEST_ASSERT_TRUE(jaos_work_units(m) < 62000);

    jaos_model_free(m);
}

/* ---- Harris' ratio test ---------------------------------------------- */

/* Same argument as the weights above: which candidate the ratio test picks
 * changes conditioning, not the answer, so the choice is asserted directly
 * on hand-built numbers rather than inferred from a solve. */

constexpr double HARRIS_TOL = 1e-7;

/* A candidate that blocks later does not win by having a large pivot: the
 * window is about how far the step may go, and outside it nothing counts.
 * Ratios here are 1 and 5, and the second candidate's pivot is a hundred
 * times the first's. */
static void test_harris_ignores_a_big_pivot_outside_the_window(void)
{
    const double num[] = {1.0, 500.0};
    const double den[] = {1.0, 100.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, HARRIS_TOL));
}

/* The case the whole two-pass structure exists for. One candidate blocks
 * immediately on a pivot of 1e-8; another blocks a hair later, at 1e-8, on
 * a pivot of 1. Widening by the dual tolerance brings both inside one
 * window, and the second is a hundred million times better conditioned for
 * a step that differs in the eighth decimal.
 *
 * The same data with no tolerance to spend picks the tiny pivot, which is
 * exactly what the window is for. */
static void test_harris_prefers_the_larger_pivot_inside_the_window(void)
{
    const double num[] = {0.0, 1e-8};
    const double den[] = {1e-8, 1.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num, den, HARRIS_TOL));
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(2, num, den, 0.0));
}

/* A degenerate vertex: every candidate blocks at zero, so the step is zero
 * whichever is taken and the only thing left to choose on is the pivot. */
static void test_harris_on_a_degenerate_vertex_takes_the_best_pivot(void)
{
    const double num[] = {0.0, 0.0, 0.0};
    const double den[] = {1.0, 7.0, 3.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(3, num, den, HARRIS_TOL));
}

/* One candidate is the whole answer; no candidates is not an answer at all
 * and must not read as "the first one". */
static void test_harris_edge_counts(void)
{
    const double num[] = {42.0};
    const double den[] = {0.5};
    TEST_ASSERT_EQUAL_INT64(0, jm_harris_pick(1, num, den, HARRIS_TOL));
    TEST_ASSERT_EQUAL_INT64(-1, jm_harris_pick(0, num, den, HARRIS_TOL));
}

/* ---- Bland's rule, for when Harris' has cycled -------------------------
 *
 * Same candidate set, same arithmetic, and deliberately the opposite
 * priorities: no window to spend, and the index decides. The pair of tests
 * below are the Harris cases above with their answers inverted, which is
 * the point — this rule is worse at everything except terminating. */

/* The degenerate vertex Harris' resolves by conditioning. Every candidate
 * blocks at zero, so a rule that picks on pivot size is choosing freely,
 * and free choices repeated at a degenerate vertex are what a cycle is
 * made of. Bland's takes the lowest index and gives the freedom up. */
static void test_bland_on_a_degenerate_vertex_takes_the_lowest_index(void)
{
    const int64_t var[] = {9, 4, 7};
    const double num[] = {0.0, 0.0, 0.0};
    const double den[] = {1.0, 7.0, 3.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_bland_pick(3, var, num, den));
    /* Harris', on the same data, prefers the pivot of 7 — which happens to
     * be the same candidate here. So a case where they differ: */
    const int64_t var2[] = {2, 8};
    const double num2[] = {0.0, 0.0};
    const double den2[] = {1.0, 7.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var2, num2, den2));
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num2, den2, HARRIS_TOL));
}

/* There is no window, so a candidate that blocks a hair earlier wins
 * however badly conditioned it is. This is the Harris case above, and the
 * answers are opposite: 1e-8 of step is not available to trade. */
static void test_bland_has_no_window_to_trade(void)
{
    const int64_t var[] = {5, 1};
    const double num[] = {0.0, 1e-8};
    const double den[] = {1e-8, 1.0};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(2, var, num, den));
    TEST_ASSERT_EQUAL_INT64(1, jm_harris_pick(2, num, den, HARRIS_TOL));
}

/* The index only breaks ties at the minimum. A lower-indexed candidate that
 * blocks later does not win — that would leave a reduced cost past feasible
 * and it is not what the rule says. */
static void test_bland_does_not_let_the_index_beat_the_quotient(void)
{
    const int64_t var[] = {1, 9};
    const double num[] = {5.0, 1.0};
    const double den[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT64(1, jm_bland_pick(2, var, num, den));
}

static void test_bland_edge_counts(void)
{
    const int64_t var[] = {3};
    const double num[] = {42.0};
    const double den[] = {0.5};
    TEST_ASSERT_EQUAL_INT64(0, jm_bland_pick(1, var, num, den));
    TEST_ASSERT_EQUAL_INT64(-1, jm_bland_pick(0, var, num, den));
}

/* jm_pattern_order: the scatter's record of where it wrote, made into
 * something a consumer can walk. Every property below is one that no solve
 * would report — a dropped position only makes the answer different, and a
 * bit left behind in the bitmap only corrupts the iteration after. */

#define PAT_WORDS 8
#define PAT_LIMIT (PAT_WORDS * 64)

/* The bitmap must come back exactly as it went in, or the next call sees a
 * position nobody recorded. Checked on every case below. */
static void assert_mark_clean(const uint64_t *mark)
{
    for (int i = 0; i < PAT_WORDS; i++)
        TEST_ASSERT_EQUAL_UINT64(0, mark[i]);
}

static void test_pattern_order_sorts_and_dedups(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    /* Out of order, one position three times, across four words. */
    int64_t pos[] = {200, 5, 63, 5, 64, 130, 5, 0};
    int64_t k = jm_pattern_order(8, pos, mark, PAT_LIMIT, &words);

    TEST_ASSERT_EQUAL_INT64(6, k);
    const int64_t want[] = {0, 5, 63, 64, 130, 200};
    for (int i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], pos[i]);
    TEST_ASSERT_EQUAL_INT64(4, words);   /* words 0..3 inclusive */
    assert_mark_clean(mark);
}

/* A pattern living in one corner must not pay for the whole bitmap: the
 * scan starts at the first word touched, not at zero. */
static void test_pattern_order_scans_only_the_touched_range(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {450, 449};
    TEST_ASSERT_EQUAL_INT64(2, jm_pattern_order(2, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(449, pos[0]);
    TEST_ASSERT_EQUAL_INT64(450, pos[1]);
    TEST_ASSERT_EQUAL_INT64(1, words);
    assert_mark_clean(mark);
}

/* Nothing may be lost when the pattern is everything. */
static void test_pattern_order_keeps_a_full_pattern(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[PAT_LIMIT];
    for (int64_t i = 0; i < PAT_LIMIT; i++)
        pos[i] = PAT_LIMIT - 1 - i;          /* descending */

    TEST_ASSERT_EQUAL_INT64(PAT_LIMIT,
        jm_pattern_order(PAT_LIMIT, pos, mark, PAT_LIMIT, &words));
    for (int64_t i = 0; i < PAT_LIMIT; i++)
        TEST_ASSERT_EQUAL_INT64(i, pos[i]);
    TEST_ASSERT_EQUAL_INT64(PAT_WORDS, words);
    assert_mark_clean(mark);
}

/* A position with nowhere to be recorded is dropped rather than written
 * past the end of the bitmap. */
static void test_pattern_order_drops_what_it_cannot_hold(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {PAT_LIMIT, -1, 7, PAT_LIMIT + 1000};
    TEST_ASSERT_EQUAL_INT64(1, jm_pattern_order(4, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(7, pos[0]);
    assert_mark_clean(mark);
}

static void test_pattern_order_edge_counts(void)
{
    uint64_t mark[PAT_WORDS] = {0};
    int64_t words = -1;
    int64_t pos[] = {3};

    TEST_ASSERT_EQUAL_INT64(0, jm_pattern_order(0, pos, mark, PAT_LIMIT,
                                                &words));
    TEST_ASSERT_EQUAL_INT64(0, words);
    TEST_ASSERT_EQUAL_INT64(0, jm_pattern_order(1, pos, mark, 0, &words));
    TEST_ASSERT_EQUAL_INT64(0, words);
    assert_mark_clean(mark);
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
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_time_limit(m, 42.0));
    /* Configuring before loading is the natural order to write, and every
     * setting has to survive it. The primal tolerance did not, until this
     * test's sibling caught it. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-4));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-5));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT64(12345, m->work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, m->time_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, m->primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(1e-5, m->dual_tol);
    jaos_model_free(m);
}

/* The two tolerances a caller owns.
 *
 * The first two tests are the ordinary ones: what is refused, and that an
 * untouched model is unchanged. The third is the one that matters, because
 * a setting that is stored and never consulted passes both of the others.
 * `min x subject to x >= 5` starts at x = 0, five units outside the row, and
 * a primal tolerance wider than five makes that starting point feasible —
 * so the solver stops there and reports 0 instead of 5. Nothing but the
 * tolerance reaching the feasibility test can produce that. */
static void test_a_tolerance_must_be_a_tolerance(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, -1e-9));
    TEST_ASSERT_TRUE(jaos_model_error(m)[0] != '\0');
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(m, INFINITY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(m, -1.0));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(m, NAN));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_primal_tolerance(nullptr, 1e-6));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_dual_tolerance(nullptr, 1e-6));
    jaos_model_free(m);
}

static void test_an_untouched_model_carries_no_tolerance_of_its_own(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->dual_tol);
    /* Set, then handed back with zero, which is the only way to say
     * "whatever you would have done" once a value has been given. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 0.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->dual_tol);
    jaos_model_free(m);
}

static void test_a_wide_primal_tolerance_accepts_a_point_it_should_not(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {5.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {1.0};

    /* Default: the row is repaired and the answer is 5. */
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, obj);
    jaos_model_free(m);

    /* A tolerance wider than the violation: the starting point is already
     * "feasible" and the solve stops on it. */
    m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 10.0));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, obj);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    jaos_model_free(m);
}

/* Logging.
 *
 * The claim that has to be tested is not that lines come out — it is that
 * they change nothing. A solver that priced differently when someone was
 * watching would be undebuggable, so the same model is solved silently and
 * at full verbosity and the two answers are compared bit for bit. The
 * reference sets check the same claim across 139 instances; this checks it
 * where a failure would be readable. */
static int64_t g_log_lines;
static jaos_log_level g_log_max;
static char g_log_last[256];

static void collect_log(void *user, jaos_log_level level, const char *line)
{
    *(int *)user += 1;
    g_log_lines++;
    if (level > g_log_max)
        g_log_max = level;
    snprintf(g_log_last, sizeof g_log_last, "%s", line);
}

static jaos_model *log_model(void)
{
    /* Something with enough rows to take a few iterations. */
    const double c[] = {-1.0, -2.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {4.0, 4.0, 4.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {5.0, 6.0};
    const int64_t as[] = {0, 2, 4, 6}, ai[] = {0, 1, 0, 1, 0, 1};
    const double av[] = {1.0, 2.0, 2.0, 1.0, 1.0, 1.0};
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));
    return m;
}

static void test_logging_says_nothing_until_it_is_asked_to(void)
{
    int hits = 0;
    jaos_model *m = log_model();

    /* A level with no callback is silent, and so is a callback at OFF. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(0, hits);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_OFF));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(0, hits);

    /* And at SUMMARY it speaks. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_TRUE(hits >= 2);          /* one opening, one closing */
    TEST_ASSERT_TRUE(g_log_last[0] != '\0');

    /* Turning the callback off again stops it. */
    int after = hits;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(after, hits);

    jaos_model_free(m);
}

static void test_a_level_outside_the_enum_is_refused(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(m, (jaos_log_level)-1));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(m, (jaos_log_level)99));
    TEST_ASSERT_TRUE(jaos_model_error(m)[0] != '\0');
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_level(nullptr, JAOS_LOG_SUMMARY));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_log_callback(nullptr, collect_log, nullptr));
    jaos_model_free(m);
}

static void test_watching_a_solve_does_not_change_it(void)
{
    jaos_model *quiet = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(quiet));
    double qobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(quiet, &qobj));
    const int64_t qiters = jaos_iterations(quiet);
    const int64_t qwork = jaos_work_units(quiet);
    double qx[3] = {0}, qy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(quiet, qx, nullptr, qy, nullptr));

    int hits = 0;
    jaos_model *loud = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(loud, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(loud, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(loud));
    double lobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(loud, &lobj));
    double lx[3] = {0}, ly[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(loud, lx, nullptr, ly, nullptr));

    TEST_ASSERT_TRUE(hits > 0);
    /* Bit for bit, not within a tolerance: the claim is that the arithmetic
     * was untouched, and "close" would not be that claim. */
    TEST_ASSERT_EQUAL_MEMORY(&qobj, &lobj, sizeof qobj);
    TEST_ASSERT_EQUAL_MEMORY(qx, lx, sizeof qx);
    TEST_ASSERT_EQUAL_MEMORY(qy, ly, sizeof qy);
    TEST_ASSERT_EQUAL_INT64(qiters, jaos_iterations(loud));
    TEST_ASSERT_EQUAL_INT64(qwork, jaos_work_units(loud));

    jaos_model_free(quiet);
    jaos_model_free(loud);
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

/* ---- Rank-deficient constraint matrices ----------------------------- *
 *
 * These do not reach the basis repair in src/simplex.c, and no small model
 * can: the dual simplex only pivots on an alpha above PIVOT_MIN, so every
 * basis it assembles is nonsingular in exact arithmetic, and a singular one
 * is always the residue of carried error rather than a property of the
 * model. The instance that actually produces one is `gran` of the
 * infeasible set, at 2658 rows and 1728 iterations of drift.
 *
 * What these cover is the family that instance belongs to, where the danger
 * is a wrong verdict and not a crash. A dependent row is satisfied for
 * nothing, and a method that reads "no pivot available in this row" as "no
 * feasible point" answers INFEASIBLE on a model with a perfectly good
 * optimum — which is the exact failure the revert of 2026-08-07 was forced
 * by. Each answer below is worked out by hand, and each infeasible one is
 * infeasible for a reason no dependency explains away.
 */

/* Two identical rows. Optimum 2, at any point of the segment x + y == 2. */
static void test_duplicate_rows_reach_the_same_optimum(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, 2.0}, ru[] = {INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

/* r2 is r0 + r1 exactly, so the matrix has rank two over three rows and the
 * third constraint adds nothing. min x+y+z with x+y >= 2, y+z >= 2 and
 * x+2y+z >= 4: adding the first two gives obj >= 4 - y, and y <= obj since
 * x and z are non-negative, so obj >= 2 — reached at y = 2, x = z = 0. */
static void test_a_row_that_is_the_sum_of_two_others(void)
{
    const double c[] = {1.0, 1.0, 1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {5.0, 5.0, 5.0};
    const double rl[] = {2.0, 2.0, 4.0};
    const double ru[] = {INFINITY, INFINITY, INFINITY};
    const int64_t as[] = {0, 2, 5, 7};
    const int64_t ai[] = {0, 2, 0, 1, 2, 1, 2};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 2.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     7, as, ai, av));
    solve_and_verify(m, 2.0);
    jaos_model_free(m);
}

/* The same row asked for two things at once: x + y >= 3 and x + y <= 1.
 * Infeasible, and the proof needs no bound on any column — which is what
 * makes it a statement about the dependency rather than about the box. */
static void test_dependent_rows_that_contradict_each_other(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {3.0, -INFINITY}, ru[] = {INFINITY, 1.0};
    const int64_t as[] = {0, 2, 4}, ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* A row no column reaches, demanding an activity of at least one. Its
 * activity is identically zero, so the model is infeasible however the
 * columns move — the case where the structural matrix does not span the
 * row space at all, which is the shape the repair pairs its logicals to. */
static void test_a_row_no_column_reaches(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1.0};
    const double rl[] = {1.0}, ru[] = {2.0};
    const int64_t as[] = {0, 0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, as, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);
}

/* The same empty row, asking for something it can give: activity zero is
 * inside [-1, 2], so the row is simply satisfied and the columns decide. */
static void test_a_row_no_column_reaches_but_that_holds_anyway(void)
{
    const double c[] = {1.0};
    const double cl[] = {-3.0}, cu[] = {1.0};
    const double rl[] = {-1.0}, ru[] = {2.0};
    const int64_t as[] = {0, 0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     0, as, nullptr, nullptr));
    solve_and_verify(m, -3.0);
    jaos_model_free(m);
}

/* ---- The basis, which the values cannot carry ------------------------ *
 *
 *   min 2x + 3y   s.t.  r0: x + y >= 2
 *                       r1: x + y <= 100
 *                       r2: x      <= 1.5
 *                       0 <= x, y <= 5
 *
 * x is the cheaper column, so it is used to its limit: x = 1.5, y = 0.5,
 * objective 4.5, and that optimum is unique. r0 and r2 are what hold it
 * there; r1 is nowhere near. Three rows means exactly three basic variables
 * among the five, and which three is forced: neither column rests on a
 * bound of its own, and neither does r1's activity. */
static void test_the_basis_names_which_rows_hold_the_optimum(void)
{
    const double c[] = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 100.0, 1.5};
    /* x hits r0, r1 and r2; y hits r0 and r1. */
    const int64_t as[] = {0, 3, 5};
    const int64_t ai[] = {0, 1, 2, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
    solve_and_verify(m, 4.5);

    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);     /* x = 1.5, off its bounds */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[1]);     /* y = 0.5, likewise       */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_LOWER, rs[0]);  /* x + y == 2              */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, rs[1]);     /* 2 is far from 100       */
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[2]);  /* x == 1.5                */

    /* Either buffer may be left out, like every other query here. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, nullptr));
    jaos_model_free(m);
}

/* What a consumer of jaos_basis is entitled to assume, checked against the
 * values published beside it rather than against an expectation: exactly
 * num_row variables are basic, and every nonbasic one sits on the bound its
 * status names. A mapping that swapped the two bounds, or logicals
 * published with the rows' orientation reversed, describes a different
 * point and fails here instead of being believed. */
static void test_the_basis_agrees_with_the_values_it_came_with(void)
{
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_read_mps(m, "tests/data/solve1.mps"));
    solve_and_verify(m, 29.0);

    const int64_t nc = jaos_num_col(m), nr = jaos_num_row(m);
    double *x = calloc((size_t)nc, sizeof *x);
    double *act = calloc((size_t)nr, sizeof *act);
    jaos_basis_status *cs = calloc((size_t)nc, sizeof *cs);
    jaos_basis_status *rs = calloc((size_t)nr, sizeof *rs);
    TEST_ASSERT_NOT_NULL(x);
    TEST_ASSERT_NOT_NULL(act);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(rs);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, act, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, rs));

    int64_t basic = 0;
    for (int64_t j = 0; j < nc; j++) {
        if (cs[j] == JAOS_BASIS_BASIC)
            basic++;
        else if (cs[j] == JAOS_BASIS_AT_LOWER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->col_lower[j], x[j]);
        else if (cs[j] == JAOS_BASIS_AT_UPPER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->col_upper[j], x[j]);
        else
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, x[j]);
    }
    for (int64_t i = 0; i < nr; i++) {
        if (rs[i] == JAOS_BASIS_BASIC)
            basic++;
        else if (rs[i] == JAOS_BASIS_AT_LOWER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->row_lower[i], act[i]);
        else if (rs[i] == JAOS_BASIS_AT_UPPER)
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, m->row_upper[i], act[i]);
        else
            TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, act[i]);
    }
    TEST_ASSERT_EQUAL_INT64(nr, basic);

    free(x);
    free(act);
    free(cs);
    free(rs);
    jaos_model_free(m);
}

/* No optimum, no basis — and the reason is sharper here than it is for the
 * values. A buffer of zeros does not read as absent: it reads as a solution
 * in which every variable is basic, which is not something a simplex can
 * report at all. */
static void test_the_basis_is_refused_when_there_is_no_optimum(void)
{
    jaos_basis_status cs[2], rs[1];
    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(nullptr, cs, rs));

    /* x + y >= 5 with both capped at 1: no feasible point. */
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 1.0};
    const double rl[] = {5.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1, 2}, ai[] = {0, 0};
    const double av[] = {1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, as, ai, av));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    jaos_model_free(m);
}

/* ---- Warm re-solve --------------------------------------------------- *
 *
 * Every test below starts from the model the basis tests use:
 *
 *   min 2x + 3y   s.t.  r0: x + y >= 2
 *                       r1: x + y <= 100
 *                       r2: x      <= r2_upper
 *                       0 <= x, y <= 5
 *
 * and for the same reason. Its optimum is unique, so a warm start cannot be
 * judged correct by landing on a different vertex of a tie — which is exactly
 * the mistake a warm start is in a position to make.
 *
 * At r2_upper = 1.5 the answer is x = 1.5, y = 0.5, objective 4.5, and the
 * basis holding it is {x, y, r1} with r0 at its lower bound and r2 at its
 * upper. */
static void load_warm_model(jaos_model *m, double r2_upper)
{
    const double c[] = {2.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY, -INFINITY};
    const double ru[] = {INFINITY, 100.0, r2_upper};
    const int64_t as[] = {0, 3, 5};
    const int64_t ai[] = {0, 1, 2, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 3, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     5, as, ai, av));
}

/* The same model solved twice. The second solve starts where the first
 * finished and therefore has nothing left to do.
 *
 * Iterations are what says so, and nothing else can: an answer alone cannot
 * tell a warm start from a cold one, since both reach the same optimum. A
 * count of zero is the only observation that distinguishes "resumed" from
 * "walked the whole way back". */
static void test_re_solving_an_unchanged_model_costs_no_iterations(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);

    double first[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, first, nullptr, nullptr, nullptr));

    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    /* And it stops at the same point, not merely at one worth the same. */
    double again[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, again, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_MEMORY(first, again, sizeof first);
    jaos_model_free(m);
}

/* A bound moves, and the warm re-solve reaches the answer a model loaded at
 * the new bound reaches from scratch. Two different trajectories, one
 * optimum: that is the whole claim a warm start makes and the only one it is
 * entitled to. The points are compared as numbers rather than as bits,
 * because they come out of different factorizations with different shift
 * histories behind them — equal optima, not equal arithmetic. */
static void test_a_warm_re_solve_agrees_with_a_cold_one(void)
{
    jaos_model *warm = fresh();
    load_warm_model(warm, 1.5);
    solve_and_verify(warm, 4.5);

    /* r2 tightens to x <= 1, so the expensive column takes up the slack:
     * x = 1, y = 1, objective 5. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(warm, 2, -INFINITY, 1.0));
    solve_and_verify(warm, 5.0);

    jaos_model *cold = fresh();
    load_warm_model(cold, 1.0);
    solve_and_verify(cold, 5.0);

    double a[2], b[2];
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(warm, a, nullptr, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(cold, b, nullptr, nullptr, nullptr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, b[0], a[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, b[1], a[1]);

    jaos_model_free(warm);
    jaos_model_free(cold);
}

/* What jaos_set_basis refuses, and the sharp end of accepting one: handed the
 * optimal basis before it has ever run, the solve costs no iterations. That
 * is a stronger statement than "it was accepted" — a basis that was validated
 * and then dropped on the floor would pass every rejection test here and fail
 * this one. */
static void test_a_basis_handed_in_must_be_a_basis(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    jaos_basis_status cs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    jaos_basis_status rs[3] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_BASIC,
                               JAOS_BASIS_AT_UPPER};

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(nullptr, cs, rs));
    /* Half a basis does not say which variables are basic. */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_set_basis(m, cs, nullptr));

    jaos_basis_status bc[2], br[3];
    memcpy(bc, cs, sizeof cs);
    memcpy(br, rs, sizeof rs);
    bc[0] = JAOS_BASIS_AT_LOWER;               /* two basic, and three rows */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    memcpy(bc, cs, sizeof cs);
    br[0] = JAOS_BASIS_BASIC;                  /* four basic */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    memcpy(br, rs, sizeof rs);
    bc[1] = (jaos_basis_status)17;             /* not a status at all */
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_set_basis(m, bc, br));

    /* The one that is a basis is the optimal one, so there is nothing left
     * for the solve to find. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));
    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));
    jaos_model_free(m);
}

/* A basis that is wrong is not a wrong answer.
 *
 * Both structurals pinned to their upper bounds puts x at 5, which r2
 * forbids, and leaves both reduced costs pointing the wrong way — primal
 * infeasible and dual infeasible at once, which is the worst start this API
 * can be handed. It costs iterations. It does not cost the optimum, and it
 * must not, because a warm start is a starting point and never a claim: the
 * solve that follows proves optimality from scratch. */
static void test_a_hostile_basis_costs_iterations_and_not_the_answer(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_UPPER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[3] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
                               JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, 4.5);
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);
    jaos_model_free(m);
}

/* Which start a solve took, read off the line it logs at JAOS_LOG_DETAIL.
 * The alternative is inferring it from an iteration count, which says the
 * same thing only when the warm basis happens to already be optimal. */
static char g_start_line[80];

static void collect_start(void *user, jaos_log_level level, const char *line)
{
    (void)user;
    (void)level;
    if (strncmp(line, "starting from", 13) == 0)
        snprintf(g_start_line, sizeof g_start_line, "%s", line);
}

static void watch_the_start(jaos_model *m)
{
    g_start_line[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, collect_start, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
}

/* Two ways a stored status can name a bound the model no longer has, and they
 * get two different answers.
 *
 * r2's activity rests on an upper bound of 1.5. Give the row a lower bound
 * instead and the status moves to it — there is still somewhere to rest, so
 * the warm start holds.
 *
 * Take *both* bounds away and there is nowhere. A nonbasic with no bounds
 * rests at zero, which pins that row's activity, and therefore x, at zero: a
 * constraint this model does not have and one the method cannot always price
 * its way out of. The whole warm start is dropped and the solve runs cold.
 * Installed instead of refused, it published 6 where the optimum is 4, and
 * called it optimal. */
static void test_a_status_whose_bound_was_retired(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);
    solve_and_verify(m, 4.5);

    jaos_basis_status rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, nullptr, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_AT_UPPER, rs[2]);

    /* x >= 1 now, and only its own bound of 5 caps it: x = 2, y = 0. */
    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_row_bounds(m, 2, 1.0, INFINITY));
    solve_and_verify(m, 4.0);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
    jaos_model_free(m);

    m = fresh();
    load_warm_model(m, 1.5);
    solve_and_verify(m, 4.5);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_row_bounds(m, 2, -INFINITY, INFINITY));
    solve_and_verify(m, 4.0);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the slack basis"));
    jaos_model_free(m);
}

/* Clearing puts the model back where it started, and it has to be exact:
 * the cleared solve costs the same iterations the first one did. Without a
 * way to ask for it, one solve would turn every later solve into a re-solve
 * for good, and "what does this model do cold" would stop being a question
 * this library could answer about its own model. */
static void test_clearing_the_basis_makes_the_next_solve_cold(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    const int64_t cold = jaos_iterations(m);
    TEST_ASSERT_TRUE(cold > 0);

    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(0, jaos_iterations(m));

    jaos_clear_basis(m);
    watch_the_start(m);
    solve_and_verify(m, 4.5);
    TEST_ASSERT_EQUAL_INT64(cold, jaos_iterations(m));
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the slack basis"));

    jaos_clear_basis(m);        /* and clearing twice is not an error */
    jaos_clear_basis(nullptr);
    jaos_model_free(m);
}

/* A basis of nothing but structurally empty columns.
 *
 * The slack basis cannot reach this state — every logical carries an entry —
 * and a warm one can, by keeping a column basic after the last coefficient in
 * it is deleted. The matrix that reaches the factorization then has no
 * entries at all, which is singular and has to be reported as a rank rather
 * than refused as bad input. It was refused, once, and the answer was that
 * the solve failed rather than that the model was infeasible. */
static void test_a_warm_basis_of_empty_columns_factors_and_is_infeasible(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {10.0};
    const double rl[] = {6.0}, ru[] = {INFINITY};
    const int64_t as[] = {0, 1}, ai[] = {0};
    const double av[] = {2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, as, ai, av));
    solve_and_verify(m, 3.0);

    /* x is basic at the optimum, and its only entry is now deleted. */
    jaos_basis_status cs[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_basis(m, cs, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_BASIS_BASIC, cs[0]);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_coefficient(m, 0, 0, 0.0));
    TEST_ASSERT_EQUAL_INT64(0, jaos_num_nz(m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
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
    RUN_TEST(test_a_ray_is_what_proves_unbounded);
    RUN_TEST(test_an_optimum_past_the_lent_bound_is_refused);
    RUN_TEST(test_a_lent_bound_that_never_constrained_anything);
    RUN_TEST(test_optimality_is_rechecked_before_it_is_believed);
    RUN_TEST(test_maximise_without_column_upper_bounds);
    RUN_TEST(test_t1_mps_is_infeasible_and_says_so);
    RUN_TEST(test_zero_objective_is_distinguishable_from_no_answer);
    RUN_TEST(test_a_long_solve_crosses_a_refactorization);
    RUN_TEST(test_free_variable_enters_and_settles);
    RUN_TEST(test_work_accounting_is_pinned);
    RUN_TEST(test_simultaneous_violations_of_wildly_different_size);
    RUN_TEST(test_dse_weights_match_recomputed_norms);
    RUN_TEST(test_dse_repairs_a_carried_weight_that_slipped);
    RUN_TEST(test_dse_restarts_when_the_carried_weight_has_drifted);
    RUN_TEST(test_scaling_changes_the_arithmetic_not_the_answer);
    RUN_TEST(test_answers_come_back_in_the_models_units);
    RUN_TEST(test_bound_flipping_fills_columns_in_one_step);
    RUN_TEST(test_settling_up_reaches_the_optimum_a_shifted_basis_hid);
    RUN_TEST(test_a_clean_up_pass_dispatches_every_column_it_identified);
    RUN_TEST(test_harris_ignores_a_big_pivot_outside_the_window);
    RUN_TEST(test_harris_prefers_the_larger_pivot_inside_the_window);
    RUN_TEST(test_harris_on_a_degenerate_vertex_takes_the_best_pivot);
    RUN_TEST(test_harris_edge_counts);
    RUN_TEST(test_bland_on_a_degenerate_vertex_takes_the_lowest_index);
    RUN_TEST(test_bland_has_no_window_to_trade);
    RUN_TEST(test_bland_does_not_let_the_index_beat_the_quotient);
    RUN_TEST(test_bland_edge_counts);
    RUN_TEST(test_pattern_order_sorts_and_dedups);
    RUN_TEST(test_pattern_order_scans_only_the_touched_range);
    RUN_TEST(test_pattern_order_keeps_a_full_pattern);
    RUN_TEST(test_pattern_order_drops_what_it_cannot_hold);
    RUN_TEST(test_pattern_order_edge_counts);
    RUN_TEST(test_solving_twice_is_bit_identical);
    RUN_TEST(test_work_limit_stops_and_reports);
    RUN_TEST(test_budgets_survive_a_reload);
    RUN_TEST(test_a_tolerance_must_be_a_tolerance);
    RUN_TEST(test_an_untouched_model_carries_no_tolerance_of_its_own);
    RUN_TEST(test_a_wide_primal_tolerance_accepts_a_point_it_should_not);
    RUN_TEST(test_logging_says_nothing_until_it_is_asked_to);
    RUN_TEST(test_a_level_outside_the_enum_is_refused);
    RUN_TEST(test_watching_a_solve_does_not_change_it);
    RUN_TEST(test_queries_before_a_solve);
    RUN_TEST(test_the_basis_names_which_rows_hold_the_optimum);
    RUN_TEST(test_the_basis_agrees_with_the_values_it_came_with);
    RUN_TEST(test_the_basis_is_refused_when_there_is_no_optimum);
    RUN_TEST(test_re_solving_an_unchanged_model_costs_no_iterations);
    RUN_TEST(test_a_warm_re_solve_agrees_with_a_cold_one);
    RUN_TEST(test_a_basis_handed_in_must_be_a_basis);
    RUN_TEST(test_a_hostile_basis_costs_iterations_and_not_the_answer);
    RUN_TEST(test_a_status_whose_bound_was_retired);
    RUN_TEST(test_clearing_the_basis_makes_the_next_solve_cold);
    RUN_TEST(test_a_warm_basis_of_empty_columns_factors_and_is_infeasible);
    RUN_TEST(test_duplicate_rows_reach_the_same_optimum);
    RUN_TEST(test_a_row_that_is_the_sum_of_two_others);
    RUN_TEST(test_dependent_rows_that_contradict_each_other);
    RUN_TEST(test_a_row_no_column_reaches);
    RUN_TEST(test_a_row_no_column_reaches_but_that_holds_anyway);
    return UNITY_END();
}
