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

/* Solves, then puts the answer through the independent checker.
 *
 * Every caller is a positive test, so all of them are skipped under either
 * fault build: those builds make presolve wrong on purpose, and `jaos_solve`
 * runs presolve, so the checker refuses the answer and the assertion that
 * fires is the fault doing its job rather than a defect. Guarding here rather
 * than at fifteen call sites — TEST_IGNORE marks the calling test ignored,
 * which is what the fifteen would each have said.
 *
 * `test_simplex.c` carried no fault guard at all until 2026-08-19, so both
 * fault builds failed here while `test_presolve.c`'s thirty guards kept its
 * own negative tests green. `make configs` is what found it. */
static void solve_and_verify(jaos_model *m, double expect_obj)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    (void)m;
    (void)expect_obj;
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
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
#endif
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
/* 02-03: this model is a singleton row (x's only constraint, x <= 1e11) on
 * a column with no other bound, so presolve now solves it directly — a
 * bound fold and an empty-column favourable-bound pick, both exact
 * arithmetic, neither touching the artificial-loan mechanism this test
 * means to exercise at all. The refusal this test checks for is a
 * *simplex* safety net for a case its own ray-based unboundedness check
 * cannot resolve; presolve sidesteps the ambiguity structurally rather
 * than resolving it numerically, and there is no loan left to be refused
 * once the reduction has already computed x = 1e11 exactly. Guarded to run
 * under JAOS_NO_PRESOLVE only, the same way test_presolve.c's own
 * fault-injection tests are guarded the other way — this is still the
 * simplex's own behavior, tested with presolve compiled out rather than
 * accidentally exercised by a model presolve no longer leaves for it. */
static void test_an_optimum_past_the_lent_bound_is_refused(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now solves this "
                        "model directly (02-03); runs only under "
                        "EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
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
#endif
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
/* 02-03: every one of the N per-column rows (`x_i >= 1`) is a singleton
 * row, and presolve folds all N of them into column bounds directly,
 * leaving only the coupling row — nowhere near the refactorization
 * interval this test means to cross. Guarded to run under
 * JAOS_NO_PRESOLVE only, same reasoning as
 * test_an_optimum_past_the_lent_bound_is_refused above: still testing the
 * simplex's own mid-solve refresh path, just with presolve compiled out
 * rather than silently deprived of the 100 rows it needs. */
static void test_a_long_solve_crosses_a_refactorization(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds every "
                        "per-column row into a bound (02-03); runs only "
                        "under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
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
#endif
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
 * Last moved by the ratio test's dense branch charging the variables its
 * scan actually visited rather than the dimension (D93): 8545 -> 8536. The
 * bitmap walk that replaced the scan over [0, nvar) reaches only the
 * nonbasic variables, so billing `nvar` for it charged for variables the
 * scan no longer reads. Nine units is the whole of it, and the arithmetic
 * closes exactly: three iterations, each taking a dense ratio test, each now
 * billing this model's three nonbasic variables instead of its six variables
 * — 3 x (6 - 3). That it closes exactly is also the evidence that the dense
 * branch is taken on every one of the three, which is what the D40/D41 note
 * below would predict: a quarter of six variables is one, and the pricing
 * row's pattern is never that small.
 *
 * Before that, by summing the exact steepest-edge weight over rho's pattern
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
#if defined(JAOS_NO_PRESOLVE)
constexpr int64_t WORK_PINNED = 8536;
#endif

/* 02-03: rows 1 and 2 (`x1 <= 4`, `x2 <= 3`) are both singleton rows;
 * presolve folds both into column bounds directly, leaving row 0 alone —
 * a different kernel shape than the three-row basis this constant is
 * pinned against and everything in the history above is reasoning about.
 * Guarded to run under JAOS_NO_PRESOLVE only, same reasoning as the two
 * tests above it in this file: still testing the simplex's own kernel
 * accounting, on the three-row model the accounting history is actually
 * about, with presolve compiled out rather than silently handed a
 * one-row problem instead. */
static void test_work_accounting_is_pinned(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds two of "
                        "the three rows into bounds (02-03); runs only "
                        "under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
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
#endif
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
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    /* The only caller in this file that asserts an exact answer without going
     * through solve_and_verify, so it needs the guard the helper carries. */
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
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
#endif
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
 * is a refactorization and a re-solve. Measured on this model: **60701 work
 * units correct, 64633 with the defect**, and the bound below sits between
 * them with room on both sides. That is the number this test actually
 * guards; the checker assertions guard the answer, which no longer moves.
 *
 * Both figures were re-measured under D93's accounting; the room on either
 * side of the bound is narrower than the 58141/67416 they replace, and the
 * note at the assertion says by how much and why.
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

    /* 60701 correct, 64633 with one pivot per call. Not a pinned value: a
     * ceiling with a measurement on each side of it, and both sides were
     * re-measured on this tree under D93's accounting rather than carried.
     *
     * They replace 58141 and 67416, and only 240 units of that move is D93's:
     * the correct side measured 60941 immediately before the dense ratio test
     * stopped billing `nvar`, and 60701 after. The other ~2800 had accumulated
     * since the pair was last measured, unnoticed, because a ceiling is
     * consulted only when it trips and nothing re-measures it on the way up.
     * The margin is now 1299 units where it was 3859, and the gap the test
     * lives on has closed from 16% to 6.5%. Left at 62000 because it still
     * separates the two; moving it would need a measurement of its own rather
     * than headroom chosen by eye. */
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

/* ---- Bland's rule on the primal side ----------------------------------
 *
 * The dual chooses a row and then a column, so its Bland's rule falls on the
 * entering variable and `jm_bland_pick` above is the whole of it. The primal
 * chooses a column and then a row, so the rule falls on the LEAVING one, and
 * `jm_primal_row_wins` is that half.
 *
 * **Both halves or neither.** Between 2026-08-25 and the change these tests
 * arrived with, phase 2 had the entering half and not the leaving half: the
 * ratio test kept whichever row it scanned FIRST among equal ratios, which is
 * a choice the basis order makes and not one the variable index makes. Phase 1
 * had neither. A solve cannot report that: it terminates on every instance
 * anyone has run, because a cycle needs a degenerate vertex revisited in a
 * particular order and no small model reaches one. So the case is built here.
 */

/* The cycling case, and the reason the rule exists. Every candidate row
 * blocks at a step of exactly zero — a degenerate vertex — so a rule that
 * keeps the first row scanned is choosing by basis order, and the basis order
 * is what a pivot changes. Under Bland the lowest variable index wins and the
 * choice stops being free. */
static void test_primal_bland_breaks_a_degenerate_tie_on_the_lowest_index(void)
{
    /* Incumbent: row holding variable 9, blocking at 0. Candidate: variable
     * 4, blocking at 0 as well. */
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 4, 0.0, 9, true));
    /* And the other way round, which is the half that was missing: the
     * incumbent already holds the lower index, so nothing displaces it. */
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 9, 0.0, 4, true));
}

/* Without the flag the tie goes to the incumbent, which is the first row
 * scanned. That is the pre-change behaviour and it is deliberate: Bland's
 * rule is armed only after a stall, and until then the ratio test is free to
 * keep whatever it found first. */
static void test_primal_without_bland_a_tie_keeps_the_first_row(void)
{
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 4, 0.0, 9, false));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 9, 0.0, 4, false));
}

/* The index only breaks ties at the minimum, exactly as on the dual side. A
 * lower-indexed row that blocks later does not win: taking it would step past
 * a bound and leave the point primal infeasible, which is the one invariant
 * the whole method rests on. */
static void test_primal_bland_does_not_let_the_index_beat_the_step(void)
{
    TEST_ASSERT_FALSE(jm_primal_row_wins(5.0, 1, 1.0, 9, true));
    TEST_ASSERT_TRUE(jm_primal_row_wins(1.0, 9, 5.0, 1, true));
}

/* The tie is exact and not a window. One ulp of daylight is a strictly
 * smaller step and wins on the quotient, whatever the indices say — the same
 * refusal `jm_bland_pick` makes, and for the same reason: a tolerance here
 * hands back the freedom the rule exists to remove. */
static void test_primal_bland_has_no_window(void)
{
    const double a = 1.0;
    const double b = nextafter(1.0, 2.0);
    TEST_ASSERT_TRUE(b > a);
    TEST_ASSERT_TRUE(jm_primal_row_wins(a, 9, b, 1, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(b, 1, a, 9, true));
}

/* No incumbent yet. `best_var < 0` is how the ratio tests say so, and the
 * first finite step must win against the `HUGE_VAL` they start from — under
 * either rule, because a scan that rejected its own first candidate would
 * return -1 and the caller would read that as an unbounded ray. */
static void test_primal_bland_first_row_always_wins(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 7, HUGE_VAL, -1, true));
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 7, HUGE_VAL, -1, false));
    /* And an equal step against no incumbent is not a tie to break. It
     * cannot arise from the callers, whose `best_step` starts at `HUGE_VAL`
     * and whose steps are finite, but the predicate must not read
     * `best_var` when it is -1. */
    TEST_ASSERT_FALSE(jm_primal_row_wins(HUGE_VAL, 7, HUGE_VAL, -1, true));
}

/* Variable 0 is a real index and beats every other under the rule, and an
 * incumbent already holding it is never displaced on a tie. That second half
 * is the rule's terminal case, and it is also why `best_var >= 0` and
 * `best_var > 0` are the same program here: no basis variable index is
 * negative, so `var < 0` is unreachable and the two guards differ nowhere.
 * The guard's whole job is the `best_var < 0` case above. Written down
 * because doctoring the `>=` to a `>` was tried as a negative control and
 * changed nothing, which is a fact about the code and not a gap in the
 * tests. */
static void test_primal_bland_variable_zero_is_an_index(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 0, 0.0, 1, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 1, 0.0, 0, true));
}

/* The direction of the comparison, pinned. A rule that took the HIGHEST index
 * among equal ratios would satisfy every "a tie is broken deterministically"
 * reading of the code and terminate nothing: Bland's proof is on the lowest.
 */
static void test_primal_bland_takes_the_lowest_and_not_the_highest(void)
{
    TEST_ASSERT_TRUE(jm_primal_row_wins(0.0, 2, 0.0, 8, true));
    TEST_ASSERT_FALSE(jm_primal_row_wins(0.0, 8, 0.0, 2, true));
}

/* `jm_primal_row_wins` is the comparison of a strict total order on
 * `(step, basis)`, so a greedy scan's winner is a function of the candidate
 * SET alone: it survives every subset that keeps it, and no ordering of the
 * candidates changes it. The primal ratio tests' Bland branch runs that scan
 * over a list `primal_apply_floor` may have compacted (D207, D212); this is
 * what says the compaction cannot change the answer. Checked over every
 * subset of a fixed set, in both tie-break modes. */
static int64_t greedy_winner(const double *step, const int64_t *var, int n,
                             unsigned mask, bool bland)
{
    int64_t best = -1;
    double best_step = HUGE_VAL;
    for (int i = 0; i < n; i++) {
        if ((mask & (1u << i)) == 0)
            continue;
        if (jm_primal_row_wins(step[i], var[i], best_step,
                               best >= 0 ? var[best] : -1, bland))
            best = i, best_step = step[i];
    }
    return best;
}

static void test_primal_row_wins_minimum_survives_every_subset(void)
{
    /* Degenerate ties, ordinary ratios, one that blocks late, and indices
     * deliberately out of scan order so a lazy tie-break shows up. */
    const double step[6] = { 0.0, 2.5, 0.0, 7.0, 2.5, 0.0 };
    const int64_t var[6] = {  9,   3,   4,   0,   1,   7 };

    for (int mode = 0; mode < 2; mode++) {
        const bool bland = mode == 1;
        const int64_t full = greedy_winner(step, var, 6, 0x3fu, bland);
        TEST_ASSERT_TRUE(full >= 0);
        for (unsigned mask = 1; mask < 64u; mask++) {
            if ((mask & (1u << full)) == 0)
                continue;      /* only subsets that keep the winner */
            TEST_ASSERT_EQUAL_INT64(full, greedy_winner(step, var, 6,
                                                        mask, bland));
        }
    }
}

/* And the case the floor must NOT be allowed to hide: with the winner
 * removed, the answer is free to change. If this ever stopped changing the
 * test above would be vacuous. */
static void test_primal_row_wins_dropping_the_winner_can_change_it(void)
{
    const double step[3] = { 1.0, 2.0, 3.0 };
    const int64_t var[3] = {  5,   6,   7  };
    const int64_t full = greedy_winner(step, var, 3, 0x7u, false);
    TEST_ASSERT_EQUAL_INT64(0, full);
    TEST_ASSERT_EQUAL_INT64(1, greedy_winner(step, var, 3, 0x6u, false));
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

/* ---- The nonbasic set the ratio test walks --------------------------- */

/* The bitmap that holds `{v : status[v] != JM_BASIC}`, which the dual ratio
 * test's dense branch walks instead of every variable in the model. Every
 * property below is one no solve would report: a variable dropped from the
 * set is left out of a ratio test that would have been correct with it, so
 * the solve carries on and publishes an answer that is merely different.
 *
 * Note what is NOT asserted here. This bitmap is persistent by design and
 * nothing clears it on the way out, so assert_mark_clean above is the wrong
 * check for it — running it here would fail a correct implementation. */

#define NB_WORDS 4
#define NB_VARS (NB_WORDS * 64)

/* The property every maintenance sequence has to end in: the bitmap expands
 * to exactly the variables the status array says are not basic, in exactly
 * that order. A predicate rather than an assertion because the last test in
 * this cluster is the one that needs the case where it does not hold. */
static bool expansion_matches_status(int64_t nvar,
                                     const jm_var_status *status,
                                     const uint64_t *mark)
{
    uint64_t rebuilt[NB_WORDS] = {0};
    int64_t want[NB_VARS], got[NB_VARS];

    int64_t nwant = jm_nonbasic_build(nvar, status, rebuilt);
    if (jm_nonbasic_expand(nvar, rebuilt, want) != nwant)
        return false;
    if (jm_nonbasic_expand(nvar, mark, got) != nwant)
        return false;
    for (int64_t k = 0; k < nwant; k++)
        if (want[k] != got[k])
            return false;
    return true;
}

/* Membership, and never "has a finite bound". A rule keyed on the bounds
 * drops every nonbasic free variable — and a free variable is admitted to
 * the ratio test with a zero numerator and may move either way, so losing
 * one costs a candidate rather than raising an error. */
static void test_nonbasic_build_keeps_free_variables(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};
    int64_t out[NB_VARS];

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_BASIC;
    status[3]   = JM_AT_LOWER;
    status[70]  = JM_AT_UPPER;
    status[131] = JM_FREE;          /* the two a bound-keyed rule loses */
    status[255] = JM_FREE;

    TEST_ASSERT_EQUAL_INT64(4, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(4, jm_nonbasic_expand(NB_VARS, mark, out));

    const int64_t want[] = {3, 70, 131, 255};
    for (int i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], out[i]);

    /* And nothing basic crept in, which the count above cannot see on its
     * own: two errors that cancel would leave it at four. */
    for (int64_t v = 0; v < NB_VARS; v++) {
        bool set = ((mark[v >> 6] >> (v & 63)) & 1) != 0;
        if (status[v] == JM_BASIC)
            TEST_ASSERT_FALSE(set);
        else
            TEST_ASSERT_TRUE(set);
    }
}

/* Bit position is the variable index, so ascending is what the walk gives
 * rather than what a sort restores. It has to be: bfrt_walk, jm_harris_pick
 * and apply_flips each break an exact tie by whichever candidate they meet
 * first, so any other order is a different trajectory. */
static void test_nonbasic_expand_is_ascending_across_words(void)
{
    uint64_t mark[NB_WORDS] = {0};
    int64_t out[NB_VARS];

    /* Inserted back to front and out of word order, on purpose, including
     * both variables either side of every word boundary. */
    const int64_t put[] = {255, 64, 192, 0, 63, 128, 191, 65};
    for (size_t i = 0; i < sizeof put / sizeof *put; i++)
        jm_nonbasic_insert(mark, put[i]);

    TEST_ASSERT_EQUAL_INT64(8, jm_nonbasic_expand(NB_VARS, mark, out));
    const int64_t want[] = {0, 63, 64, 65, 128, 191, 192, 255};
    for (int i = 0; i < 8; i++)
        TEST_ASSERT_EQUAL_INT64(want[i], out[i]);
    for (int i = 1; i < 8; i++)
        TEST_ASSERT_TRUE(out[i] > out[i - 1]);
}

/* No bit set, one bit set, every bit set. The first is the state where the
 * ratio test admits nothing and has to return -1 exactly as the dense scan
 * did; the last is where the bitmap saves nothing and has to stay right
 * anyway. */
static void test_nonbasic_expand_handles_the_degenerate_counts(void)
{
    jm_var_status status[NB_VARS];
    /* Deliberately not zeroed: jm_nonbasic_build writes every word, and a
     * build that only set bits would leave whatever was here. */
    uint64_t mark[NB_WORDS];
    int64_t out[NB_VARS];

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_BASIC;
    out[0] = -7;
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(0, jm_nonbasic_expand(NB_VARS, mark, out));
    TEST_ASSERT_EQUAL_INT64(-7, out[0]);        /* nothing was written */

    /* Exactly one, in the last word, where an off-by-one in the word count
     * loses it. */
    status[NB_VARS - 1] = JM_AT_UPPER;
    TEST_ASSERT_EQUAL_INT64(1, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(1, jm_nonbasic_expand(NB_VARS, mark, out));
    TEST_ASSERT_EQUAL_INT64(NB_VARS - 1, out[0]);

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = JM_AT_LOWER;
    TEST_ASSERT_EQUAL_INT64(NB_VARS, jm_nonbasic_build(NB_VARS, status, mark));
    TEST_ASSERT_EQUAL_INT64(NB_VARS, jm_nonbasic_expand(NB_VARS, mark, out));
    for (int64_t v = 0; v < NB_VARS; v++)
        TEST_ASSERT_EQUAL_INT64(v, out[v]);
}

/* The sequence the maintenance actually runs, with the two variables
 * interleaved: A leaves the set, B enters it from inside the gap A left, B
 * leaves again, A comes back. An insertion-ordered structure has to compute
 * where B goes and can get it wrong; a bitmap has no position to compute,
 * which is the reason this representation was chosen over a list. */
static void test_nonbasic_survives_interleaved_eviction(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = (v % 3 == 0) ? JM_BASIC : JM_AT_LOWER;
    status[100] = JM_FREE;
    jm_nonbasic_build(NB_VARS, status, mark);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    /* A = 98 enters the basis and leaves the set. Its neighbours in the set
     * are 97 and 100, and the gap it leaves behind spans 98 and 99. */
    status[98] = JM_BASIC;
    jm_nonbasic_remove(mark, 98);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    /* B = 99 lands inside that gap — the position a list would have to find
     * by walking from a neighbour that has just been unlinked. */
    status[99] = JM_AT_UPPER;
    jm_nonbasic_insert(mark, 99);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    status[99] = JM_BASIC;
    jm_nonbasic_remove(mark, 99);
    status[98] = JM_AT_LOWER;
    jm_nonbasic_insert(mark, 98);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));

    /* An insert and a remove of the same variable are exact inverses — the
     * words come back bit for bit, not merely expanding to the same list. */
    uint64_t before[NB_WORDS];
    for (int i = 0; i < NB_WORDS; i++)
        before[i] = mark[i];
    jm_nonbasic_insert(mark, 42);       /* 42 is basic, so this is a lie */
    jm_nonbasic_remove(mark, 42);       /* and this takes it back exactly */
    for (int i = 0; i < NB_WORDS; i++)
        TEST_ASSERT_EQUAL_UINT64(before[i], mark[i]);
}

/* The instrument, pointed at the failure this representation is actually
 * vulnerable to. A bitmap has no neighbour pointer to go stale, so the only
 * way it desynchronises is a hook nobody called — which is exactly what the
 * two memcpy restore sites in simplex.c look like, since a wholesale copy
 * over `status` carries no assignment for a reader to notice.
 *
 * If this test passes, every test above it is asserting nothing. */
static void test_nonbasic_notices_a_missed_hook(void)
{
    jm_var_status status[NB_VARS];
    uint64_t mark[NB_WORDS] = {0};

    for (int64_t v = 0; v < NB_VARS; v++)
        status[v] = (v % 3 == 0) ? JM_BASIC : JM_AT_LOWER;
    jm_nonbasic_build(NB_VARS, status, mark);

    status[98] = JM_BASIC;
    jm_nonbasic_remove(mark, 98);
    status[99] = JM_AT_UPPER;           /* and the hook that never ran */
    TEST_ASSERT_FALSE(expansion_matches_status(NB_VARS, status, mark));

    /* Calling it puts the two back into agreement, so what was caught above
     * was the missing call and not the sequence around it. */
    jm_nonbasic_insert(mark, 99);
    TEST_ASSERT_TRUE(expansion_matches_status(NB_VARS, status, mark));
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
    TEST_ASSERT_EQUAL_INT64(12345, m->cfg.work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(42.0, m->cfg.time_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-4, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(1e-5, m->cfg.dual_tol);
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
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.dual_tol);
    /* Set, then handed back with zero, which is the only way to say
     * "whatever you would have done" once a value has been given. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 0.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_dual_tolerance(m, 1e-3));
    TEST_ASSERT_EQUAL_DOUBLE(1e-3, m->cfg.dual_tol);
    jaos_model_free(m);
}

/* 02-03: `x >= 5` is a singleton row, so presolve folds it into x's own
 * bound directly and reaches the answer (x = 5) by exact arithmetic before
 * the simplex — and before the simplex's own scaled-space primal
 * tolerance — ever enters the picture at all. This test means to exercise
 * that solver-level tolerance specifically (a wide one letting the cold
 * start pass as "already feasible"), which a presolve-resolved model
 * never reaches. Guarded to run under JAOS_NO_PRESOLVE only, same
 * reasoning as the tests above it in this file. */
static void test_a_wide_primal_tolerance_accepts_a_point_it_should_not(void)
{
#if !defined(JAOS_NO_PRESOLVE)
    TEST_IGNORE_MESSAGE("simplex-internal test — presolve now folds the "
                        "row into x's own bound directly (02-03); runs "
                        "only under EXTRA_CFLAGS=-DJAOS_NO_PRESOLVE");
#else
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
#endif
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

/* ---------------------------------------------------------------------- */
/* Watching a solve, and stopping one                                      */
/* ---------------------------------------------------------------------- */

typedef struct {
    int calls;
    int stop_after;              /* -1 never */
    int64_t last_iters;
    int64_t last_work;
    bool iters_on_the_beat;      /* every call landed on a fixed multiple */
    bool work_never_went_back;
} watcher;

static jaos_callback_action watch(const jaos_progress *p, void *user)
{
    watcher *w = user;
    if (p->work_units < w->last_work)
        w->work_never_went_back = false;
    if (p->iterations % 64 != 0)
        w->iters_on_the_beat = false;
    w->last_iters = p->iterations;
    w->last_work = p->work_units;
    w->calls++;
    return (w->stop_after >= 0 && w->calls > w->stop_after)
               ? JAOS_CALLBACK_STOP : JAOS_CALLBACK_CONTINUE;
}

static watcher fresh_watcher(int stop_after)
{
    return (watcher){.stop_after = stop_after, .iters_on_the_beat = true,
                     .work_never_went_back = true};
}

static void test_a_watcher_is_asked_and_changes_nothing(void)
{
    jaos_model *quiet = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(quiet));
    double qobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(quiet, &qobj));
    const int64_t qiters = jaos_iterations(quiet);
    const int64_t qwork = jaos_work_units(quiet);
    double qx[3] = {0}, qy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(quiet, qx, nullptr, qy, nullptr));

    watcher w = fresh_watcher(-1);
    jaos_model *seen = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(seen, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(seen));
    double sobj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(seen, &sobj));
    double sx_[3] = {0}, sy[2] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solution(seen, sx_, nullptr, sy, nullptr));

    /* The instrument, before the claim: a watcher that was never asked would
     * pass every assertion below without proving anything. */
    TEST_ASSERT_TRUE(w.calls > 0);
    TEST_ASSERT_TRUE(w.iters_on_the_beat);
    TEST_ASSERT_TRUE(w.work_never_went_back);

    /* Bit for bit, for the reason the logging test gives: the claim is that
     * the arithmetic was untouched, and "close" would not be that claim. */
    TEST_ASSERT_EQUAL_MEMORY(&qobj, &sobj, sizeof qobj);
    TEST_ASSERT_EQUAL_MEMORY(qx, sx_, sizeof qx);
    TEST_ASSERT_EQUAL_MEMORY(qy, sy, sizeof qy);
    TEST_ASSERT_EQUAL_INT64(qiters, jaos_iterations(seen));
    TEST_ASSERT_EQUAL_INT64(qwork, jaos_work_units(seen));

    jaos_model_free(quiet);
    jaos_model_free(seen);
}

static void test_a_watcher_can_stop_a_solve_and_it_resumes(void)
{
    watcher w = fresh_watcher(0);          /* stop at the first question */
    jaos_model *m = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m));
    TEST_ASSERT_EQUAL_INT(1, w.calls);

    /* A stopping point is not a solution, and the two are not readable
     * through one call. */
    double x[3] = {0};
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));

    /* But it kept where it stopped, exactly as a budget stop does. */
    TEST_ASSERT_NOT_NULL(m->start_col_status);
    TEST_ASSERT_NOT_NULL(m->start_row_status);

    /* Stop asking, and the next solve finishes the job. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_progress_callback(m, nullptr, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    jaos_model *cold = log_model();
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(cold));
    double cobj = 0.0, robj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(cold, &cobj));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &robj));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, cobj, robj);

    jaos_model_free(m);
    jaos_model_free(cold);
}

/* The defect D78 found, kept from coming back: configuration is not problem
 * data, so loading a problem must not silently discard it. Logging was lost
 * this way from the day it landed. */
static void test_configuration_survives_a_load(void)
{
    int hits = 0;
    watcher w = fresh_watcher(-1);
    jaos_model *m = fresh();

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 1000000));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_primal_tolerance(m, 1e-8));

    /* Configure first, load second — the order anyone writes it in. */
    const double c[] = {-1.0, -2.0, -1.0};
    const double cl[] = {0.0, 0.0, 0.0}, cu[] = {4.0, 4.0, 4.0};
    const double rl[] = {-INFINITY, -INFINITY}, ru[] = {5.0, 6.0};
    const int64_t as[] = {0, 2, 4, 6}, ai[] = {0, 1, 0, 1, 0, 1};
    const double av[] = {1.0, 2.0, 2.0, 1.0, 1.0, 1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     6, as, ai, av));

    TEST_ASSERT_EQUAL_INT64(1000000, m->cfg.work_limit);
    TEST_ASSERT_EQUAL_DOUBLE(1e-8, m->cfg.primal_tol);
    TEST_ASSERT_EQUAL_INT(JAOS_LOG_DETAIL, m->cfg.log_level);

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_TRUE(hits > 0);     /* the callback survived the load */
    TEST_ASSERT_TRUE(w.calls > 0);
    jaos_model_free(m);
}

/* What can be asserted about a clock without the test becoming a flake: that
 * it starts at zero, that it is finite and not negative after a solve, that a
 * modification retires it along with the rest of the answer, and that a model
 * that never solved reports nothing. Not how long anything took. */
static void test_solve_time_is_reported_and_retired(void)
{
    jaos_model *m = log_model();
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(m));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(nullptr));

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    const double t = jaos_solve_time(m);
    TEST_ASSERT_TRUE(t >= 0.0);
    TEST_ASSERT_TRUE(isfinite(t));

    /* The answer is stale after a modification and so is the time it took to
     * reach it: reporting seconds for a solve whose result has been withdrawn
     * would be a number about nothing. */
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_col_bounds(m, 0, 0.0, 1.0));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, jaos_solve_time(m));
    TEST_ASSERT_EQUAL_INT64(0, jaos_work_units(m));
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

/* --------------------------------------------------------------------- */
/* The primal method                                                     */
/* --------------------------------------------------------------------- */

/* A model presolve leaves alone. Every primal test below uses it.
 *
 * **`load_warm_model` cannot be used for any of them, and finding that out
 * cost two rounds.** Its third row is `x <= 1.5`, a singleton, and presolve
 * folds a singleton row straight into the column's own bound (02-03) — on
 * this model it takes three rows down to one. Two consequences, both of which
 * made a test say something it did not mean. A basis with both structurals at
 * their upper bounds reads as plainly primal infeasible on the model as
 * written, `x` at 5 against a row capping it at 1.5, and is nothing of the
 * kind by the time the simplex sees it, because `x`'s upper bound *is* 1.5
 * now. And on the one row that survives, every basis the model admits is
 * already dual feasible, so the primal has no work whatever it is handed.
 *
 * `min x + 3y` over `x + y >= 2` and `x + 2y <= 10`, both columns in `[0, 5]`.
 * Every row has two entries and every column has two, so there is no
 * singleton of either kind for presolve to take, and no fixed or empty
 * anything. The optimum is `x = 2, y = 0` at 2. */
static void load_unreducible_model(jaos_model *m)
{
    const double c[] = {1.0, 3.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {2.0, -INFINITY};
    const double ru[] = {INFINITY, 10.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
}
/* The primal simplex reaches the same optimum as the dual, from a basis it
 * can actually start from.
 *
 * **Getting one is the whole difficulty and it is worth writing down.** The
 * cold basis is dual feasible by construction, so if it were also primal
 * feasible it would already be optimal and there would be nothing to watch.
 * The primal needs a start that is primal feasible and *not* dual feasible,
 * and no cold start is ever that. So it has to be handed one.
 *
 * `load_warm_model` is `min 2x + 3y` over `x + y >= 2`, `x + y <= 100`,
 * `x <= 1.5`, with both columns in `[0, 5]`. Put `x` on its lower bound and
 * `y` on its upper and every logical is basic: the row activities are 5, 5
 * and 0, each inside its own bounds, so the point is primal feasible. It is
 * not dual feasible — `y` sits at an upper bound with a reduced cost of +3,
 * which points the wrong way — and it is not optimal, at an objective of 15
 * against the true 4.5.
 *
 * **`jaos_iterations() > 0` is NOT enough to say the primal ran, and asserting
 * it was a false green that a negative control caught.** With `run_primal`
 * doctored to declare optimality immediately and pivot not once, all four
 * tests in this section still passed: the settling re-entry calls `run()`,
 * the dual repaired the point, and the answer, the checker and the iteration
 * count were all satisfied by the wrong algorithm. So the assertion is on the
 * one count only `run_primal` can raise, read off the closing summary line
 * through the caller's own log callback.
 *
 * The other two assertions stay and are load-bearing too. The objective is the
 * answer, and the independent checker accepting the point is what says that
 * answer is defensible rather than merely equal to a number typed in this
 * file. */
static void test_the_primal_reaches_the_optimum_from_a_feasible_basis(void)
{
    int hits = 0;
    jaos_model *m = fresh();
    load_unreducible_model(m);

    /* `x` on its lower bound, `y` on its upper, both logicals basic. The
     * point is `(0, 5)`: row 0 reads 5 against a lower bound of 2 and row 1
     * reads 10 against an upper bound of 10, so it is primal feasible, and
     * its objective is 15 against the true 2. */
    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));

    m->cfg.force_primal = true;
    solve_and_verify(m, 2.0);

    /* The closing summary is the last line logged. It must report primal
     * iterations, and must not report none of them. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(g_log_last, "primal iterations"),
                                 "the summary must count them at all");
    TEST_ASSERT_NULL_MESSAGE(strstr(g_log_last, " 0 primal iterations"),
                             "the primal method did not take a single pivot");
    jaos_model_free(m);
}

/* And it is the same answer the dual gives, which is the only comparison
 * that means anything.
 *
 * A test asserting 4.5 against a constant proves the primal agrees with
 * whoever typed 4.5. This one solves the identical model both ways in one
 * process and compares the two objectives against each other, which is what
 * `bench/primal.c` does over the reference set and what this is the small
 * version of. */
static void test_the_primal_and_the_dual_agree_on_the_same_model(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    int hits = 0;
    char dual_line[256], primal_line[256];
    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_UPPER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};

    jaos_model *d = fresh();
    load_unreducible_model(d);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(d, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(d, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(d, JAOS_LOG_SUMMARY));
    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(d));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(d));
    snprintf(dual_line, sizeof dual_line, "%s", g_log_last);
    double obj_d = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(d, &obj_d));

    jaos_model *p = fresh();
    load_unreducible_model(p);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(p, cs, rs));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(p, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(p, JAOS_LOG_SUMMARY));
    g_log_last[0] = '\0';
    p->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(p));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(p));
    snprintf(primal_line, sizeof primal_line, "%s", g_log_last);
    double obj_p = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(p, &obj_p));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, obj_d, obj_p);

    /* And the two really were different methods. Without this the test is
     * satisfied by solving the same model with the dual twice, which is
     * exactly what happened when `run_primal` was doctored to do nothing. */
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(dual_line, " 0 primal iterations"),
                                 "the dual solve took primal iterations");
    TEST_ASSERT_NULL_MESSAGE(strstr(primal_line, " 0 primal iterations"),
                             "the primal solve took no primal iterations");
    jaos_model_free(d);
    jaos_model_free(p);
#endif
}

/* A watcher can stop the solve from inside the primal phase 1.
 *
 * **Written because it could not, for a whole milestone.** Phase 2 and the
 * dual both offer `progress_cb`; phase 1 did not, so a caller could neither
 * see nor stop the part of a forced-primal solve that spends 39.5% of its
 * iterations (D197, D200). A budget could end it and a person could not.
 *
 * **The assertion that separates the two is the log, not the status.** A stop
 * on the first callback returns `INTERRUPTED` either way — before this change
 * the first call simply happened later, in phase 2, by which time phase 1 had
 * finished and said so. So the test requires the solve to stop WITHOUT that
 * line, which is only reachable from inside phase 1.
 *
 * `load_unreducible_model`'s first row is `x + y >= 2`, so the slack basis is
 * primal infeasible and phase 1 runs. `PROGRESS_EVERY` is 64 and `s->iters` is
 * 0 on a cold start, so the first phase-1 iteration is on the beat.
 *
 * It brings its own log collector rather than reusing `collect_log`, which
 * keeps only the last line and is depended on by several tests above. */
#if !defined(JAOS_PRESOLVE_FAULT_OFFBYONE) && \
    !defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
static bool g_saw_phase1_finish;

static void watch_phase1_log(void *user, jaos_log_level level,
                             const char *line)
{
    (void)level;
    *(int *)user += 1;
    if (strstr(line, "phase 1 reached a feasible") != nullptr)
        g_saw_phase1_finish = true;
}
#endif

static void test_a_watcher_can_stop_the_primal_phase_1(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    int hits = 0;
    watcher w = fresh_watcher(0);           /* stop on the first call */
    jaos_model *m = fresh();
    load_unreducible_model(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_log_callback(m, watch_phase1_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_DETAIL));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_progress_callback(m, watch, &w));
    g_saw_phase1_finish = false;
    m->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));

    TEST_ASSERT_EQUAL_INT_MESSAGE(JAOS_SOLVE_INTERRUPTED, jaos_status_of(m),
        "the watcher asked to stop and the solve did not");
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, w.calls,
        "the watcher was never called at all");
    TEST_ASSERT_FALSE_MESSAGE(g_saw_phase1_finish,
        "the stop came after phase 1 finished, so it was not phase 1's");
    jaos_model_free(m);
#endif
}

/* The primal's iteration split, read off the model.
 *
 * **Written because the absence of this number made a published one wrong.**
 * Before it, phase 1's count was readable only from the log line `phase 1
 * reached a feasible point in N iterations`, which is emitted on SUCCESS. A
 * phase 1 that ran and did not finish emitted nothing, so a probe reading that
 * line recorded zero and the instance read as a solve with no phase 1 at all --
 * and therefore as a pure phase-2 run. D194 published exactly that about eight
 * netlib instances and D195 corrected it. `solve_primal_iters` and
 * `solve_phase1_iters` are written on every exit from `jm_dual_simplex`, the
 * abandoned one included.
 *
 * **It reads the two fields and no longer parses the summary sentence.** The
 * parser it used to carry had a twin in `bench/primal.c` that matched on a
 * substring this one did not require, so an edit to that sentence could leave
 * this test green while the campaign silently reported no split at all on all
 * 94 instances. Two copies of one backwards character walk, agreeing about
 * nothing. The sentence itself is still tested, by the two tests above that
 * assert on ` 0 primal iterations` -- what is gone is reading NUMBERS out of
 * prose.
 *
 * **The non-success path is not reproduced here and the reason is the usual
 * one**: it needs a phase 1 that takes many iterations and then runs out of
 * budget, which no two-row model reaches. The campaign covers it and names the
 * case -- `wood1p` reports 3820 primal iterations, all 3820 of them phase 1,
 * ending `work limit reached` (`bench/measurements/02-108/`). What is tested
 * here is the property that failed: the count is present, non-zero when phase
 * 1 ran, and never larger than the primal count it is a part of.
 *
 * The dual's own counts must read zero on both, which is the same separation
 * `solve_primal_iters` exists for: without it this test is satisfied by a
 * solve that never entered the primal at all. */
static void test_the_summary_separates_phase_1_from_phase_2(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test -- skipped under either fault build");
#else
    /* The dual, on the same model: both counts must be zero. */
    jaos_model *d = fresh();
    load_unreducible_model(d);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(d));
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, d->solve_primal_iters,
                                    "the dual took primal iterations");
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, d->solve_phase1_iters,
                                    "the dual entered a primal phase 1");
    jaos_model_free(d);

    /* The primal, cold. `load_unreducible_model`'s first row is `x + y >= 2`,
     * so the slack basis sits at zero and is primal INFEASIBLE by 2 -- which is
     * what makes phase 1 run at all. A cold basis that were primal feasible
     * would already be optimal and there would be nothing to count. */
    jaos_model *p = fresh();
    load_unreducible_model(p);
    p->cfg.force_primal = true;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(p));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(p));
    /* Phase 1 ran, and the count says so rather than reading as absent. */
    TEST_ASSERT_GREATER_THAN_INT64_MESSAGE(0, p->solve_phase1_iters,
        "phase 1 ran from a primal infeasible start and reported none");
    /* And it is a part of the primal count, not a second total beside it.
     * A field that exceeded its container would be a different defect wearing
     * the same name. */
    TEST_ASSERT_TRUE_MESSAGE(p->solve_phase1_iters <= p->solve_primal_iters,
        "more phase-1 iterations than primal iterations");
    /* And the primal count is a part of the solve's total, for the same
     * reason: the record's `dual re-entry` column is the difference between
     * the two and a negative one would be nonsense. */
    TEST_ASSERT_TRUE_MESSAGE(p->solve_primal_iters <= p->solve_iters,
        "more primal iterations than iterations");
    jaos_model_free(p);
#endif
}

/* The three counts belong to the solve that just ran, not to the one before.
 *
 * **Written because a solve that was ABANDONED published the previous solve's
 * total.** `solve_iters` has one writer inside `publish`, and `publish` runs
 * only when the solve returned `JAOS_OK`. So a refused solve left the field
 * holding whatever the model was solved with last time, and any reader
 * subtracting the primal counts from it got a difference between two
 * different solves. `bench/primal.c` does exactly that subtraction, and it
 * printed `dual:20835` for `pilot87` -- which is 38000 - 17165, the dual
 * reference solve's total minus the primal's own count. `jm_dual_simplex`
 * zeroes all three on entry now and writes the total on the abandoned branch.
 *
 * The shape is the one that failed: the same model solved twice, the dual
 * first, so a carried-over total has a wrong value to carry rather than a
 * zero that would pass by accident.
 *
 * **This model is too small to separate the two on its own, and saying so is
 * the point.** Measured under `-DJAOS_NO_PRESOLVE`: the dual solve costs 1
 * iteration and the refused primal solve costs 1 as well, so a carried total
 * and an honest one are the same integer here. What the assertions below can
 * still hold is the SHAPE -- a refused primal solve did no dual re-entry, so
 * `solve_iters` and `solve_primal_iters` must be equal -- and that shape is
 * what breaks on any model where the two solves differ. The instance that
 * caught it is `pilot87`, at 38000 against 17165, and the negative control
 * that proves these assertions are worth having is
 * `bench/measurements/02-115/`.
 *
 * **The refusal branch is conditional, exactly as
 * `test_the_primal_refuses_to_call_a_model_infeasible` is**: presolve may
 * prove this model infeasible before the simplex runs. The chain above it is
 * not conditional and holds on every solve in this test. The abandoned path
 * is covered where it actually occurs -- 1 of 94 in the primal campaign --
 * and the negative control for it is `bench/measurements/02-115/`. */
static void test_the_counts_belong_to_the_solve_that_just_ran(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test -- skipped under either fault build");
#else
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 3.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));

    /* A dual solve first, so there is a total on the model to carry. */
    (void)jaos_solve(m);
    const int64_t carried = m->solve_iters;
    TEST_ASSERT_EQUAL_INT64_MESSAGE(0, m->solve_primal_iters,
        "the dual took primal iterations");

    jaos_clear_basis(m);
    m->cfg.force_primal = true;
    const jaos_status st = jaos_solve(m);

    /* The chain, on every outcome. A count that is a part of another cannot
     * exceed it, and the record's `dual re-entry` column is the outer
     * difference. */
    TEST_ASSERT_TRUE_MESSAGE(m->solve_phase1_iters >= 0,
        "a negative phase-1 count");
    TEST_ASSERT_TRUE_MESSAGE(
        m->solve_phase1_iters <= m->solve_primal_iters,
        "more phase-1 iterations than primal iterations");
    TEST_ASSERT_TRUE_MESSAGE(m->solve_primal_iters <= m->solve_iters,
        "more primal iterations than iterations");

    if (st != JAOS_OK) {
        TEST_ASSERT_TRUE_MESSAGE(m->solve_primal_iters > 0,
            "the primal was forced and reported no primal iterations");

        /* **The equality holds only when the refusal came from inside
         * `run_primal`, and the test asks before it asserts.** There is a
         * second way to reach `st != JAOS_OK`: `run_primal` reaches OPTIMAL
         * and `reenter_after_settling` fails afterwards. On that path
         * `primal_cleanup`'s pivots and the re-entry's own `run()` have both
         * raised `s->iters` without touching `n_primal_iters`, so the
         * difference is correctly positive. Asserting equality unconditionally
         * would be asserting the defect. `bench/primal.c` reaches that path
         * whenever its work limit bites during the re-entry.
         *
         * Phase 1's refusal names itself, so the question is answerable
         * without new state. When it is the answer, nothing after `run_primal`
         * ran, and the record's `dual re-entry` column must be zero. */
        const char *why = jaos_model_error(m);
        if (why != nullptr &&
            strstr(why, "the primal phase 1 cannot reduce") != nullptr)
            TEST_ASSERT_EQUAL_INT64_MESSAGE(m->solve_primal_iters,
                m->solve_iters,
                "a refused primal solve reports a dual re-entry that never ran");
    }
    (void)carried;
    jaos_model_free(m);
#endif
}

/* A model where the entering column's own box binds before any row does.
 *
 * `min -x - 0.5y` over `x + y <= 10` and `x + 2y <= 12`, with `x` in `[0, 1]`
 * and `y` in `[0, 10]`. The optimum is `x = 1, y = 5.5` at -4.25... on the
 * costs alone; the published objective is -3.75 because `x + 2y <= 12` binds.
 *
 * From the origin the primal prices `x` first — `|d|` of 1 against 0.5 — and
 * moves it up. **Nothing basic stops it before 10, and `x`'s own upper bound
 * is 1.** */
static void load_boxed_model(jaos_model *m)
{
    const double c[]  = {-1.0, -0.5};
    const double cl[] = {0.0, 0.0}, cu[] = {1.0, 10.0};
    const double rl[] = {-INFINITY, -INFINITY};
    const double ru[] = {10.0, 12.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));
}

/* The entering column must not walk past its own bound, and this is the case
 * that proves it does not.
 *
 * **It was a wrong answer, not a hypothetical.** Before the bound flip existed,
 * this model published `x = 10` against a declared upper bound of 1, as
 * `OPTIMAL`, at an objective of -10 against a true -3.75. The independent
 * checker refused the point and the solver said optimal anyway. Stage 1's
 * pricing rule is what made the case reachable: the primal clean-up only ever
 * enters columns with no declared bound on the improving side, so nothing
 * before it could reach a column whose own box binds first.
 *
 * The assertion is the answer *and* the bound, because those are two different
 * failures. A solve that lands on the right objective through a point outside
 * its bounds has still published something the model forbids. */
static void test_the_entering_column_stops_at_its_own_bound(void)
{
    jaos_model *m = fresh();
    load_boxed_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    m->cfg.force_primal = true;
    solve_and_verify(m, -3.75);

    double x[2] = {0.0, 0.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_solution(m, x, nullptr, nullptr, nullptr));
    TEST_ASSERT_TRUE_MESSAGE(x[0] <= 1.0 + 1e-9,
                             "the entering column walked past its own bound");
    TEST_ASSERT_TRUE_MESSAGE(x[0] >= -1e-9, "and below its lower one");
    jaos_model_free(m);
}

/* Phase 1 repairs a start phase 2 has no invariant for.
 *
 * **This test used to assert a refusal.** Before the phase 1 landed, the
 * primal was handed the origin — where `x + y >= 2` is violated by 2 — and
 * said so rather than answering, because the two alternatives were both worse:
 * reporting `INFEASIBLE` would be a wrong answer about a model that has an
 * optimum, and running anyway would drive an objective across a region the
 * point is not in. It now repairs the point and solves.
 *
 * The same basis is also dual feasible — both costs positive, both columns at
 * a lower bound — so it is exactly the shape a cold start has, which is what
 * makes it the case worth pinning. The test below this one asserts the dual
 * reaches the same answer from it.
 *
 * `n_primal_iters` covers both phases, so a positive count says the primal
 * method did the work and not the settling re-entry's dual solve. */
static void test_the_primal_phase_1_repairs_an_infeasible_start(void)
{
    int hits = 0;
    jaos_model *m = fresh();
    load_unreducible_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    g_log_last[0] = '\0';
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_callback(m, collect_log, &hits));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_log_level(m, JAOS_LOG_SUMMARY));

    m->cfg.force_primal = true;
    solve_and_verify(m, 2.0);
    TEST_ASSERT_NULL_MESSAGE(strstr(g_log_last, " 0 primal iterations"),
                             "phase 1 did not take a single pivot");
    jaos_model_free(m);
}

/* The refusal that is left, and it is the one the method must not turn into a
 * verdict.
 *
 * `min x + y` over `x + y >= 10` and `x + 2y <= 3`, both columns in `[0, 5]`.
 * The second row caps `x + y` at 3, so there is no feasible point at all.
 *
 * **The primal must not answer `INFEASIBLE` here, and that is not caution for
 * its own sake.** "No improving direction with infeasibility left" is the
 * textbook proof of primal infeasibility, and this method is not entitled to
 * it: phase 1 measures against `real_lower`/`real_upper`, the bounds the model
 * declared, while the columns may be pinned by bounds dual phase 1 invented.
 * So "nothing improves" can be a statement about the loans rather than about
 * the model, and D19 refuses exactly that inference. The infeasible instance
 * set is the dual method's to answer.
 *
 * What is asserted is that it refuses **and says why**, read through
 * `jaos_model_error` on the caller's own model — the only place a caller can
 * look, and where no simplex message arrived at all until D188 on any model
 * presolve had reduced. */
static void test_the_primal_refuses_to_call_a_model_infeasible(void)
{
    const double c[]  = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {5.0, 5.0};
    const double rl[] = {10.0, -INFINITY};
    const double ru[] = {INFINITY, 3.0};
    const int64_t as[] = {0, 2, 4};
    const int64_t ai[] = {0, 1, 0, 1};
    const double av[] = {1.0, 1.0, 1.0, 2.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     4, as, ai, av));

    m->cfg.force_primal = true;
    jaos_status st = jaos_solve(m);

    /* Presolve may prove it infeasible before the simplex ever runs, and that
     * verdict is sound because a reduction proved it rather than a method
     * failing to improve. Only the case that reaches the simplex is this
     * test's subject. */
    if (st == JAOS_OK) {
        TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    } else {
        TEST_ASSERT_EQUAL_INT(JAOS_ERR_NUMERICAL, st);
        const char *why = jaos_model_error(m);
        TEST_ASSERT_NOT_NULL(why);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(why, "D19"),
                                     "the refusal must say what it is");
    }
    jaos_model_free(m);
}

/* The same model with the switch off must be untouched by any of this.
 *
 * `force_primal` is a development switch, and the whole claim about it is
 * that a solve which never sets it behaves exactly as it did before the
 * switch existed. The reference sets test that across every instance; this
 * tests it where a reader can see it, on the exact basis the refusal above
 * uses — primal infeasible, which the dual does not mind at all, because
 * driving primal infeasibility out is what the dual method is for. */
static void test_the_dual_agrees_from_the_same_infeasible_start(void)
{
    jaos_model *m = fresh();
    load_unreducible_model(m);

    jaos_basis_status cs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, 2.0);
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

/* A nonbasic free variable whose reduced cost is negative must not be
 * invisible — the defect D68 recorded and PLAN.md carried as its fourth.
 *
 *   min -f      r0: f + 2g in [2, 6]      r1: h in [0, 1]
 *   f free,     g in [0, 2],              h in [0, 1]
 *
 * The optimum is -6 at f = 6, g = 0, and a cold solve finds it.
 *
 * The hostile basis {f, g} is what reaches the state: both columns live in
 * r0 alone, so it has rank 1, and repair_singular_basis evicts f — which has
 * neither bound — to nonbasic free. That pins f at zero, and the point that
 * results is *primal feasible*, so the dual method stops without an iteration
 * and never gets the chance to price f back in. Everything then rests on the
 * primal clean-up, which is where the defect lived: f's reduced cost is -1,
 * and `wants_a_pivot` read a free variable as sitting at an upper bound, so
 * it repaired a positive reduced cost and dropped a negative one.
 *
 * What this test asserts is the promise jaos_set_basis makes in the header:
 * a hostile basis costs iterations and cannot produce a wrong verdict.
 *
 * Calibrated against the defect rather than written blind: before the repair
 * this model published **0.0 with a verdict of OPTIMAL** — the checker caught
 * the dual infeasibility, and nothing else did. Confirmed on an instrumented
 * build, which also confirmed that the eviction happens and that
 * `wants_a_pivot` returned false on a breach of 1. */
static void test_a_free_nonbasic_with_a_negative_reduced_cost_is_repaired(void)
{
    const double inf = jaos_infinity();
    const double cost[3] = {-1.0, 0.0, 0.0};
    const double lo[3]   = {-inf, 0.0, 0.0};
    const double up[3]   = { inf, 2.0, 1.0};
    const double rlo[2]  = { 2.0, 0.0};
    const double rup[2]  = { 6.0, 1.0};
    const int64_t st[4]  = {0, 1, 2, 3};
    const int64_t idx[3] = {0, 0, 1};
    const double val[3]  = {1.0, 2.0, 1.0};

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 3, 2, JAOS_MINIMIZE, 0.0, cost, lo, up, rlo, rup,
                     3, st, idx, val));

    /* Cold first, so the expected value is the solver's own and not a number
     * copied out of this comment. */
    solve_and_verify(m, -6.0);

    jaos_basis_status cs[3] = {JAOS_BASIS_BASIC, JAOS_BASIS_BASIC,
                               JAOS_BASIS_AT_LOWER};
    jaos_basis_status rs[2] = {JAOS_BASIS_AT_LOWER, JAOS_BASIS_AT_LOWER};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_basis(m, cs, rs));

    solve_and_verify(m, -6.0);
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

    /* Take *both* bounds away and the row's logical has nowhere to rest but
     * zero, which pins that row's activity — and therefore x — at zero. This
     * used to abandon the warm start, because the method could not price a
     * free nonbasic back off zero and published 6 where the optimum is 4.
     *
     * It is D68's own example, and it is now the case that proves the two
     * repairs meet: D85 taught the primal clean-up to move a free column in
     * the direction its reduced cost points, so D90 could stop refusing to
     * create one. The warm start holds and the answer is still 4. */
    m = fresh();
    load_warm_model(m, 1.5);
    solve_and_verify(m, 4.5);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_set_row_bounds(m, 2, -INFINITY, INFINITY));
    solve_and_verify(m, 4.0);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
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

/* A budget is for stopping and coming back, and until the basis survived the
 * stop it was only for stopping.
 *
 * The interrupted solve publishes no answer, because it has none — but the
 * basis it stopped on is where the next one starts, so raising the limit and
 * solving again continues instead of beginning. Two assertions make this a
 * test of that rather than of the budget: the interrupted run has to have got
 * past its first iteration, or the basis it left is the slack basis and
 * "resuming" from it proves nothing; and the resumed run has to cost fewer
 * iterations than a whole cold solve, which is the only evidence that the
 * first run's work was kept. */
static void test_a_budget_stop_can_be_resumed(void)
{
    jaos_model *m = fresh();
    load_warm_model(m, 1.5);

    solve_and_verify(m, 4.5);
    const int64_t whole_work = jaos_work_units(m);
    const int64_t whole_iters = jaos_iterations(m);

    jaos_clear_basis(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, whole_work / 2));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_WORK_LIMIT, jaos_status_of(m));
    TEST_ASSERT_TRUE(jaos_iterations(m) > 0);

    /* No answer to read, and no basis behind one: a stopping point is not a
     * solution and must not be readable through the call that publishes one. */
    double obj = 0.0;
    jaos_basis_status cs[2], rs[3];
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_objective(m, &obj));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT, jaos_basis(m, cs, rs));
    /* It is kept where the next solve looks, which is somewhere else. */
    TEST_ASSERT_NOT_NULL(m->start_col_status);

    watch_the_start(m);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_set_work_limit(m, 0));
    solve_and_verify(m, 4.5);
    TEST_ASSERT_NOT_NULL(strstr(g_start_line, "the basis on the model"));
    TEST_ASSERT_TRUE(jaos_iterations(m) < whole_iters);
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

/* ---- A row activity that meets a large term before many small ones ---- *
 *
 * `x_B = -B^-1 (N x_N)` builds its right-hand side by walking the nonbasic
 * columns in column order and adding each one's entries into the rows it
 * touches. A row that meets a large term first can then lose every small term
 * after it: each is below half an ulp of the running total, so each addition
 * returns the total unchanged.
 *
 *   row R:  x0 + x1 + (256 smalls) + w1 + w2  ==  256*2^-25 + 1e-7
 *   row S:  x1 + z                            == -1e9
 *
 *   x0      fixed at +1e9
 *   x1      in [-1e9-1, -1e9+1]
 *   smalls  fixed at 2^-25, a quarter of an ulp of 1e9
 *   w1, w2  in [0, 2e-7], cost 1
 *   z       fixed at 0
 *
 * Every value here is a dyadic rational a double holds exactly, and the model
 * is feasible at x0 = 1e9, x1 = -1e9, every small at 2^-25, w1 = 1e-7, w2 = 0.
 * Summed in column order the 256 smalls fall off the end of 1e9 and the
 * activity comes back short by 2^-17 = 7.63e-6, which nothing left in the
 * model can make up: the solve reads INFEASIBLE.
 *
 * It is D162's model (`bench/measurements/02-72/`) and it is what named the
 * defect; D168 compensates the accumulation and closes it
 * (`bench/measurements/02-78/`).
 *
 * **The shipping build already answered this one and the reference build did
 * not.** Presolve removes the fixed columns before the simplex sees them, and
 * since D165 it subtracts them with the residue kept, so the row it hands over
 * is the row the model has. `-DJAOS_NO_PRESOLVE` hands the whole model to the
 * simplex and is where the defect is visible. The assertion is not guarded by
 * build, because OPTIMAL is the right answer in every one of them.
 *
 * `k` is a parameter of the model and not of the defect: 64, 128 and 512 read
 * the same way. 256 is kept because it makes this the same model 02-72's
 * record carries, and for no other reason — **not** because of presolve's own
 * window, which this test never reaches: `-DJAOS_NO_PRESOLVE` is the
 * configuration where the defect is visible and it consults no such window,
 * and 02-72 §4 records the reference build refusing all four counts alike. */
#define ACT_K 256
#define ACT_NC (ACT_K + 5)
#define ACT_NNZ (ACT_K + 6)

static jaos_model *make_lost_terms_model(double slack)
{
    static double c[ACT_NC], cl[ACT_NC], cu[ACT_NC], av[ACT_NNZ];
    static int64_t as[ACT_NC + 1], ai[ACT_NNZ];
    const double t = ldexp(1.0, -25);
    const double bound = (double)ACT_K * t + 1e-7 + slack;
    const double rl[] = {bound, -1e9}, ru[] = {bound, -1e9};
    int64_t nz = 0;

    for (int64_t j = 0; j < ACT_NC; j++) {
        as[j] = nz;
        c[j] = 0.0;
        if (j == 0) {                      /* x0 */
            cl[j] = cu[j] = 1e9;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j == 1) {               /* x1 */
            cl[j] = -1e9 - 1.0; cu[j] = -1e9 + 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
            ai[nz] = 1; av[nz++] = 1.0;
        } else if (j < ACT_K + 2) {        /* the smalls */
            cl[j] = cu[j] = t;
            ai[nz] = 0; av[nz++] = 1.0;
        } else if (j < ACT_K + 4) {        /* w1, w2 */
            cl[j] = 0.0; cu[j] = 2e-7; c[j] = 1.0;
            ai[nz] = 0; av[nz++] = 1.0;
        } else {                           /* z */
            cl[j] = cu[j] = 0.0;
            ai[nz] = 1; av[nz++] = 1.0;
        }
    }
    as[ACT_NC] = nz;

    jaos_model *m = fresh();
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, ACT_NC, 2, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     nz, as, ai, av));
    return m;
}

static void test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total(void)
{
#if defined(JAOS_PRESOLVE_FAULT_OFFBYONE) || defined(JAOS_PRESOLVE_FAULT_WRONGDUAL)
    TEST_IGNORE_MESSAGE("positive test — skipped under either fault build");
#else
    jaos_model *m = make_lost_terms_model(0.0);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_OPTIMAL, jaos_status_of(m));

    /* OPTIMAL alone is not enough, and 02-72 is why: on an earlier tree this
     * model published `obj = 4e-07`, and before D165 the shipping build
     * published 0 at three of the four counts. A repair that reported OPTIMAL
     * with either would pass a status assertion.
     *
     * **The value is bounded, not pinned, and the two builds differ.** Only
     * `w1` and `w2` carry a cost, so the objective is `w1 + w2` and
     * feasibility asks that for `T - 2^-17`; the optimum is 1e-7 at
     * `x1 = -1e9`. The shipping build reads 1.0000000000000074e-07 and
     * `-DJAOS_NO_PRESOLVE` reads 1.1920928955078125e-07, which is 2^-23 —
     * one ulp of x1's own magnitude, and the last step of the ratio test is
     * not on that grid. The window here admits both and rejects 0, 2e-7 and
     * 4e-7, which are the answers that would mean something was wrong.
     *
     * **The rejecting case is measured and not argued.** The same model with
     * `slack = 1e-7` publishes 2.0000000000000147e-07, which this window
     * refuses — so the pin discriminates on a real reading of this model and
     * not only on the arithmetic. `bench/measurements/02-78/controls.txt`
     * carries the sweep. */
    double obj = 0.0;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_objective(m, &obj));
    TEST_ASSERT_DOUBLE_WITHIN(5e-8, 1.1e-7, obj);
    jaos_model_free(m);
#endif
}

/* The two controls the compensation must still refuse, and the near one is
 * the one that means something.
 *
 * **1e-2 is D162's inherited control and it separates nothing here.** It is
 * 1300 times the 7.63e-6 the compensation recovers, so any repair that
 * widened a feasibility window by anything up to a hundredth would pass it.
 * It is kept because it is the shape that must be refused on every build at
 * every count.
 *
 * **5e-6 is the one that tells an accurate sum from a wider window.** The
 * model is infeasible by about 4.7e-6 there — 47 times `PRIMAL_TOL` and 40
 * times the row's own ulp noise of 1.19e-7 — and `w1 + w2` cannot reach it,
 * since it would need 5.1e-6 against a cap of 4e-7. It sits INSIDE a window
 * widened to cover the 7.63e-6 that was lost, so such a repair accepts it and
 * an accurate sum refuses it. Both read INFEASIBLE on both builds, before
 * D168 and after. Found by `numerics-reviewer`.
 *
 * **This one runs under the fault builds and is meant to.** The convention in
 * `tests/test_presolve.c` guards POSITIVE tests off there, because a fault
 * build breaks the answer they assert. This asserts a refusal, and a fault
 * that made presolve accept either model would be worth hearing about. Row S
 * becomes a singleton row on `x1` once `z` is removed, which is the path
 * `JAOS_PRESOLVE_FAULT_WRONGDUAL` perturbs, so the coverage is real. Both
 * counts pass under both fault builds; this sentence is what records that it
 * was checked rather than overlooked. */
static void test_a_row_activity_still_refuses_a_real_shortfall(void)
{
    jaos_model *m = make_lost_terms_model(1e-2);
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_solve(m));
    TEST_ASSERT_EQUAL_INT(JAOS_SOLVE_INFEASIBLE, jaos_status_of(m));
    jaos_model_free(m);

    m = make_lost_terms_model(5e-6);
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
    RUN_TEST(test_primal_bland_breaks_a_degenerate_tie_on_the_lowest_index);
    RUN_TEST(test_primal_without_bland_a_tie_keeps_the_first_row);
    RUN_TEST(test_primal_bland_does_not_let_the_index_beat_the_step);
    RUN_TEST(test_primal_bland_has_no_window);
    RUN_TEST(test_primal_bland_first_row_always_wins);
    RUN_TEST(test_primal_bland_variable_zero_is_an_index);
    RUN_TEST(test_primal_bland_takes_the_lowest_and_not_the_highest);
    RUN_TEST(test_primal_row_wins_minimum_survives_every_subset);
    RUN_TEST(test_primal_row_wins_dropping_the_winner_can_change_it);
    RUN_TEST(test_pattern_order_sorts_and_dedups);
    RUN_TEST(test_pattern_order_scans_only_the_touched_range);
    RUN_TEST(test_pattern_order_keeps_a_full_pattern);
    RUN_TEST(test_pattern_order_drops_what_it_cannot_hold);
    RUN_TEST(test_pattern_order_edge_counts);
    RUN_TEST(test_nonbasic_build_keeps_free_variables);
    RUN_TEST(test_nonbasic_expand_is_ascending_across_words);
    RUN_TEST(test_nonbasic_expand_handles_the_degenerate_counts);
    RUN_TEST(test_nonbasic_survives_interleaved_eviction);
    RUN_TEST(test_nonbasic_notices_a_missed_hook);
    RUN_TEST(test_solving_twice_is_bit_identical);
    RUN_TEST(test_work_limit_stops_and_reports);
    RUN_TEST(test_budgets_survive_a_reload);
    RUN_TEST(test_a_tolerance_must_be_a_tolerance);
    RUN_TEST(test_an_untouched_model_carries_no_tolerance_of_its_own);
    RUN_TEST(test_a_wide_primal_tolerance_accepts_a_point_it_should_not);
    RUN_TEST(test_logging_says_nothing_until_it_is_asked_to);
    RUN_TEST(test_a_level_outside_the_enum_is_refused);
    RUN_TEST(test_watching_a_solve_does_not_change_it);
    RUN_TEST(test_a_watcher_is_asked_and_changes_nothing);
    RUN_TEST(test_a_watcher_can_stop_a_solve_and_it_resumes);
    RUN_TEST(test_configuration_survives_a_load);
    RUN_TEST(test_solve_time_is_reported_and_retired);
    RUN_TEST(test_queries_before_a_solve);
    RUN_TEST(test_the_basis_names_which_rows_hold_the_optimum);
    RUN_TEST(test_the_basis_agrees_with_the_values_it_came_with);
    RUN_TEST(test_the_basis_is_refused_when_there_is_no_optimum);
    RUN_TEST(test_re_solving_an_unchanged_model_costs_no_iterations);
    RUN_TEST(test_a_warm_re_solve_agrees_with_a_cold_one);
    RUN_TEST(test_a_basis_handed_in_must_be_a_basis);
    RUN_TEST(test_the_primal_reaches_the_optimum_from_a_feasible_basis);
    RUN_TEST(test_the_primal_and_the_dual_agree_on_the_same_model);
    RUN_TEST(test_a_watcher_can_stop_the_primal_phase_1);
    RUN_TEST(test_the_summary_separates_phase_1_from_phase_2);
    RUN_TEST(test_the_counts_belong_to_the_solve_that_just_ran);
    RUN_TEST(test_the_entering_column_stops_at_its_own_bound);
    RUN_TEST(test_the_primal_phase_1_repairs_an_infeasible_start);
    RUN_TEST(test_the_primal_refuses_to_call_a_model_infeasible);
    RUN_TEST(test_the_dual_agrees_from_the_same_infeasible_start);
    RUN_TEST(test_a_hostile_basis_costs_iterations_and_not_the_answer);
    RUN_TEST(test_a_free_nonbasic_with_a_negative_reduced_cost_is_repaired);
    RUN_TEST(test_a_status_whose_bound_was_retired);
    RUN_TEST(test_clearing_the_basis_makes_the_next_solve_cold);
    RUN_TEST(test_a_budget_stop_can_be_resumed);
    RUN_TEST(test_a_warm_basis_of_empty_columns_factors_and_is_infeasible);
    RUN_TEST(test_duplicate_rows_reach_the_same_optimum);
    RUN_TEST(test_a_row_that_is_the_sum_of_two_others);
    RUN_TEST(test_dependent_rows_that_contradict_each_other);
    RUN_TEST(test_a_row_no_column_reaches);
    RUN_TEST(test_a_row_no_column_reaches_but_that_holds_anyway);
    RUN_TEST(test_a_row_activity_keeps_terms_below_an_ulp_of_its_own_total);
    RUN_TEST(test_a_row_activity_still_refuses_a_real_shortfall);
    return UNITY_END();
}
