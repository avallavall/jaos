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
 * Last moved by the recheck that ends a solve (see run()): 4411 -> 8517,
 * which is the one extra factorization it costs, plus the pricing pass
 * that follows it. */
constexpr int64_t WORK_PINNED = 8517;

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

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, w[DSE_ROW], 10.0);

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

    jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0);

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

        jm_dse_update(DSE_N, w, DSE_ROW, alpha, tau, truth, 10.0);

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
 * drive the basic column to -9.9 and is refused. C wants to move to 0.001,
 * which leaves the basic at 0.0999 and is taken. So the assertion that
 * matters is on the two column values, not on an objective which is 5e-8
 * either way. */
static void test_settling_up_flips_what_is_free_and_leaves_the_rest(void)
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

    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, x[0]);      /* refused */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.001, x[1]);    /* taken */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0999, x[2]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, act[0]);     /* still feasible */

    /* The primal point settling up produces is feasible in the model's own
     * space, which is the point of settling up at all. */
    double y[1];
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(m, x, nullptr, y, nullptr));
    jaos_check_report rep;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);

    /* The dual certificate that comes with it does not carry, and the
     * checker is right to say so. The solve stops on the basis with xB
     * basic, which prices the row at y = 5e-8; the optimal basis has xA
     * basic and prices it at zero. A row multiplier of 5e-8 makes xA's
     * reduced cost -5e-8, pointing at an upper bound of 100 that xA is
     * nowhere near, and 5e-8 * 100 is 5e-6 of unproven complementary
     * slackness — past CHECK_TOL, so no certificate.
     *
     * It is the y that fails, not the x. Handed the dual the optimal basis
     * would have produced, the same primal point is accepted, which is
     * what pins the defect to where it belongs. That the solver stops on a
     * suboptimal basis here is recorded in PLAN.md 2.8; it costs 5e-8 of
     * objective and a certificate. */
    TEST_ASSERT_FALSE(rep.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-7, 5e-6, rep.objective_gap);

    double y_optimal[1] = {0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, x, y_optimal, CHECK_TOL, &rep));
    TEST_ASSERT_TRUE(rep.primal_feasible);
    TEST_ASSERT_TRUE(rep.dual_feasible);

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
    RUN_TEST(test_settling_up_flips_what_is_free_and_leaves_the_rest);
    RUN_TEST(test_harris_ignores_a_big_pivot_outside_the_window);
    RUN_TEST(test_harris_prefers_the_larger_pivot_inside_the_window);
    RUN_TEST(test_harris_on_a_degenerate_vertex_takes_the_best_pivot);
    RUN_TEST(test_harris_edge_counts);
    RUN_TEST(test_solving_twice_is_bit_identical);
    RUN_TEST(test_work_limit_stops_and_reports);
    RUN_TEST(test_budgets_survive_a_reload);
    RUN_TEST(test_queries_before_a_solve);
    RUN_TEST(test_duplicate_rows_reach_the_same_optimum);
    RUN_TEST(test_a_row_that_is_the_sum_of_two_others);
    RUN_TEST(test_dependent_rows_that_contradict_each_other);
    RUN_TEST(test_a_row_no_column_reaches);
    RUN_TEST(test_a_row_no_column_reaches_but_that_holds_anyway);
    return UNITY_END();
}
