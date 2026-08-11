/* Checker tests: two LPs solved by hand, correct and wrong answers for
 * each, in both objective senses.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "jaos.h"
#include "unity.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-9

/* T1 (minimize):  min x0 + x1  s.t.  x0 + x1 >= 1,  0 <= x <= 10.
 * Any point on the facet x0+x1 = 1 is optimal with objective 1;
 * the optimal row dual is y = 1, giving reduced costs d = (0, 0). */
static jaos_model *make_t1(void)
{
    const double c[] = {1.0, 1.0};
    const double cl[] = {0.0, 0.0}, cu[] = {10.0, 10.0};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

/* T2 (maximize):  max 3x0 + 2x1  s.t.  x0 + x1 <= 4,
 * 0 <= x0 <= 2, 0 <= x1 <= 10. Optimum x = (2, 2), objective 10,
 * row dual y = 2, reduced costs d = c - A'y = (1, 0). */
static jaos_model *make_t2(void)
{
    const double c[] = {3.0, 2.0};
    const double cl[] = {0.0, 0.0}, cu[] = {2.0, 10.0};
    const double rl[] = {-INFINITY}, ru[] = {4.0};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MAXIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));
    return m;
}

static void test_t1_accepts_the_true_optimum(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.checked_duals);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

static void test_t1_flags_wrong_dual_sign(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {-1.0}; /* >= row at its lower bound demands y >= 0 */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    /* Two breaches: the row dual points at a bound that is not active
     * (magnitude 1), and with y = -1 the reduced costs become 2 on
     * interior columns (magnitude 2). The worst one wins. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_t1_flags_complementarity_break(void)
{
    jaos_model *m = make_t1();
    /* Feasible but interior point: activity 4 is strictly off the bound,
     * so a nonzero dual there violates complementary slackness. */
    const double x[] = {2.0, 2.0};
    const double y[] = {1.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);
    jaos_model_free(m);
}

static void test_t1_flags_primal_violation(void)
{
    jaos_model *m = make_t1();
    const double x[] = {-1.0, 0.0}; /* below the column lower bound... */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, nullptr, TOL, &r));

    TEST_ASSERT_FALSE(r.primal_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_col_violation);
    /* ...and activity -1 also breaches the row's lower bound of 1 by 2. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 2.0, r.max_row_violation);
    TEST_ASSERT_FALSE(r.checked_duals);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

static void test_t2_accepts_the_true_optimum_maximize(void)
{
    jaos_model *m = make_t2();
    const double x[] = {2.0, 2.0};
    const double y[] = {2.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 10.0, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

static void test_t2_flags_wrong_dual_magnitude(void)
{
    jaos_model *m = make_t2();
    const double x[] = {2.0, 2.0};
    const double y[] = {3.0}; /* right sign, wrong value: d = (0,-1) breaks */
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);
    /* x1 = 2 is interior in [0,10], so its reduced cost 2-3 = -1 must be 0. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.0, r.max_dual_violation);
    jaos_model_free(m);
}

/* T3: what a multiplier below the tolerance contributes to the dual
 * objective. Both tests below are exact in binary floating point, so
 * neither is about rounding.
 *
 *      min   x1 + 1e-7 * x3
 *      s.t.  x1        >= 1
 *            0   <= x1 <= 10
 *            1e6 <= x3 <= 2e6        (x3 appears in no row)
 */
static jaos_model *make_t3(void)
{
    const double c[] = {1.0, 1e-7};
    const double cl[] = {0.0, 1e6}, cu[] = {10.0, 2e6};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

/* A tiny multiplier still carries dual objective when the bound it points
 * at is large, and dropping it invents a gap out of nothing.
 *
 * x1 = 1 sits in the interior with d1 = 0; x3 = 1e6 rests on its lower
 * bound with d3 = 1e-7, which is exactly where a positive reduced cost
 * belongs. Complementary slackness holds exactly and so does strong
 * duality: D = y*1 + d1*0 + d3*1e6 = 1 + 0 + 0.1 = 1.1 = P.
 *
 * d3 is below any tolerance the gate uses. Discarding its contribution
 * would leave D = 1 against P = 1.1 and reject a pair that is optimal on
 * every count, with a relative gap of 9%. This is the shape that rejected
 * pilot-ja (PLAN 2.8). */
static void test_a_tiny_multiplier_on_a_large_bound_still_counts(void)
{
    jaos_model *m = make_t3();
    const double x[] = {1.0, 1e6};
    const double y[] = {1.0};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1.1, r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.objective_gap);
    jaos_model_free(m);
}

/* The repair for the above must not become "small multipliers contribute
 * w * v", which cancels their term and stops measuring them at all.
 *
 *      min  1e-7 * x1   s.t.  x1 + x2 >= 1,  0 <= x1, x2 <= 1e6
 *
 * The optimum is x = (0, 1) at objective 0. The pair below claims
 * x = (1e6, 0) — feasible, and costing 0.1, which is 1e5 tolerances away
 * from optimal. With y = 0 the reduced cost d1 = 1e-7 points at x1's lower
 * bound while x1 sits at 1e6: complementary slackness is broken by exactly
 * 1e-7 * 1e6 = 0.1, the whole of the suboptimality. The sign condition is
 * waived because |d1| is small, so the gap is the only thing left that can
 * catch it, and it must. */
static void test_a_waived_sign_condition_is_still_caught_by_the_gap(void)
{
    const double c[] = {1e-7, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {1e6, 1e6};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double x[] = {1e6, 0.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);      /* it is feasible, just wrong */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.max_dual_violation); /* waived */
    /* 0.1 of absolute gap over a scale of 1 + |0.1| + |0| = 1.1. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.1 / 1.1, r.objective_gap);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* D47's case, which the checker used to certify and now rejects (D87).
 *
 *      min  -1e-7 * x2   s.t.  x1 + x2 <= 1e6,  x1, x2 >= 0, both free above
 *
 * The optimum is -0.1 at x = (0, 1e6). Offered the origin with a zero row
 * dual, every number the checker reported used to be zero — including
 * gap_positive, which jaos.h documents as bounding P - P*, on a point 0.1
 * away from optimal. Raising the row's bound raised the suboptimality without
 * limit and changed not one of them.
 *
 * The cause was one line in sign_condition: x2's reduced cost of -1e-7 points
 * at an upper bound that does not exist, so the term it owes the dual
 * objective is minus infinity and was dropped, leaving a sum belonging to a
 * different problem.
 *
 * **What closed it is that the bound does exist — the row implies it.** With
 * x1 held at its own lower bound of 0, `x1 + x2 <= 1e6` gives `x2 <= 1e6`,
 * and every feasible point already satisfies that, so adding it changes
 * neither the feasible region nor P*. The term is then `-1e-7 * 1e6` and
 * gap_positive is 0.1 — the true suboptimality exactly, and reached with no
 * basis, no factorization and no reference value.
 *
 * This test previously asserted the zeros and `gap_certified == false`, which
 * pinned the defect as the honest description of a checker that could not do
 * better. It is re-pinned deliberately: the numbers below are the repair, and
 * a future change that returned them to zero would be reopening D47. */
static void test_an_implied_bound_makes_the_dropped_term_finite(void)
{
    const double c[] = {0.0, -1e-7};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {-INFINITY}, ru[] = {1e6};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double origin[] = {0.0, 0.0};
    const double y0[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, origin, y0, 1e-6, &r));

    /* Feasible, and refused — which is the whole point of the entry. */
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_FALSE(r.dual_feasible);

    /* The bound is now a bound, and it is the true suboptimality to the digit
     * rather than an order of magnitude near it. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.1, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_negative);

    /* And the identity is complete, so the bound may be believed: nothing was
     * dropped, because nothing had to be. */
    TEST_ASSERT_TRUE(r.gap_certified);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.max_dropped_multiplier);

    /* The suboptimality is now carried by gap_positive rather than by the
     * single-column certificate, which D73 refuted as a verdict for reading
     * ~1e-25 at a vertex. It still reports what one column alone can prove. */
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.1, r.certified_suboptimality);

    /* The case it must NOT flag: the same model at its true optimum, where
     * the row's multiplier points at a bound that exists and x1's points at
     * its own lower bound. A predicate that were simply always false here
     * would pass every assertion above and be worthless. */
    const double best[] = {0.0, 1e6};
    const double ybest[] = {-1e-7};
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_check_solution(m, best, ybest, 1e-6, &r));
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_TRUE(r.gap_certified);
    TEST_ASSERT_EQUAL_INT64(0, r.dropped_terms);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.max_dropped_multiplier);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.certified_suboptimality);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -0.1, r.primal_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, -0.1, r.dual_objective);
    jaos_model_free(m);
}

/* When nothing blocks the move, the certificate stops being one.
 *
 *      min  c * x2   s.t.  x1 + x2 >= 1,  x1, x2 >= 0, both free above
 *
 * x2 can rise for ever and the row never objects, so the step is infinite and
 * `|w| * t` is infinite for *any* nonzero rate — 1e-9 included. That is no
 * longer a certificate; it is D47's unanswerable question wearing one, and
 * five instances of JAOS's own reference set sit exactly there with published
 * finite optima.
 *
 * So the split is on the checker's own definition of a nonzero multiplier and
 * not on a number invented for the occasion. Below it the ray is counted;
 * above it the model really is unbounded and infinity is the right answer.
 * Both halves are built here, because a rule that only ever counted would
 * hide a genuine unbounded model. */
static void test_an_unbounded_ray_is_counted_unless_its_rate_is_real(void)
{
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1.0, 1.0};
    const double x[] = {1.0, 0.0};
    const double y[] = {0.0};
    jaos_check_report r;

    /* A rate the checker calls zero: counted, and nothing is certified. */
    const double tiny[] = {0.0, -1e-9};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, tiny, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_INT64(1, r.unquantified_rays);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.certified_suboptimality);
    TEST_ASSERT_FALSE(r.gap_certified);
    jaos_model_free(m);

    /* A rate that is unmistakably real: the model is unbounded and saying so
     * is correct, not a false alarm. */
    const double real[] = {0.0, -1.0};
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, real, cl, cu, rl, ru,
                     2, s, ix, v));
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_INT64(0, r.unquantified_rays);
    TEST_ASSERT_TRUE(isinf(r.certified_suboptimality));
    jaos_model_free(m);
}

static void test_check_rejects_bad_arguments(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double xnan[] = {NAN, 0.5};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(nullptr, x, nullptr, TOL, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, nullptr, nullptr, TOL, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, x, nullptr, TOL, nullptr));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, x, nullptr, -1.0, &r));
    TEST_ASSERT_EQUAL_INT(JAOS_ERR_INVALID_INPUT,
        jaos_check_solution(m, xnan, nullptr, TOL, &r));
    jaos_model_free(m);
}

/* ---- The scale on the bound-proximity test ------------------------- *
 *
 * A row activity is a sum, and a sum whose terms cancel cannot be pinned to
 * an absolute tolerance. Row 3 of Netlib's `finnis` adds terms totalling
 * 4.0e10 in magnitude and lands 1.5e-6 from its bound, where one ulp at
 * 4.0e10 is 7.6e-6: judged absolutely at 1e-6 it is "not at its bound" and
 * its multiplier of 28 is reported as a violation of 28. So the window is
 * tol times what the row carries.
 *
 * That is a loosening, and it needs a case it must still reject or it is
 * just a way of making a gate go green. The three below are that case and
 * its two neighbours. They share one shape: a single column fixed at 1, so
 * that its own sign condition accepts any multiplier and the row's is the
 * only thing under test.
 *
 * What makes the loosening safe is that this test is a diagnostic and the
 * gap is the proof. P - D = sum of w_v (v - bound_v) over every entity, each
 * term non-negative on a primal-feasible point, so a row waived at distance
 * d with multiplier w still contributes exactly w*d to the gap. The waiver
 * cannot hide anything; it can only decline to report it twice.
 */
static jaos_model *make_scaled_row(double cost, double coef, double row_lo)
{
    const double c[] = {cost};
    const double cl[] = {1.0}, cu[] = {1.0};   /* fixed: accepts any dual */
    const double rl[] = {row_lo}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {coef};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));
    return m;
}

/* The finnis shape: activity 1e10, bound 1e-3 below it, so the distance is a
 * thousand times the old absolute tolerance and 1e-7 of what the row
 * carries. Accepted now; rejected before, on a solution that is right. */
static void test_a_row_at_its_bound_to_its_own_precision_is_accepted(void)
{
    jaos_model *m = make_scaled_row(1e9, 1e10, 1e10 - 1e-3);
    const double x[] = {1.0};
    const double y[] = {28.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    /* The distance is real and far above the absolute tolerance, which is
     * what makes this test say something. */
    TEST_ASSERT_TRUE(1e-3 > 1e-6);
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);
    /* w*d = 28 * 1e-3 against an objective of 1e9: real, and negligible
     * exactly as the identity says it should be. */
    TEST_ASSERT_TRUE(r.objective_gap < 1e-6);
    TEST_ASSERT_TRUE(r.dual_feasible);
    jaos_model_free(m);
}

/* The same row a hundred thousand times further out — ten times the window
 * the scale opens rather than a ten-thousandth of it. Still reported, at the
 * full magnitude of the multiplier. The scale is a scale, not an amnesty. */
static void test_a_row_genuinely_off_its_bound_is_still_reported(void)
{
    jaos_model *m = make_scaled_row(1e9, 1e10, 1e10 - 1e5);
    const double x[] = {1.0};
    const double y[] = {28.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_EQUAL_DOUBLE(28.0, r.max_dual_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* The one that had to be built. The sign condition IS waived here — the
 * distance is half the window — and the answer is nonetheless refused,
 * because w*d is 500 and the gap carries it at full size. Under the old
 * absolute rule this was rejected on the sign condition; the point of the
 * test is that removing that route does not open a way through.
 *
 * The numbers are exact: primal objective 0, dual objective -500, and
 * 0 - (-500) is precisely 1000 * 0.5, which is the identity the waiver
 * rests on, checked rather than asserted. */
static void test_a_waived_row_that_still_costs_is_refused_by_the_gap(void)
{
    jaos_model *m = make_scaled_row(0.0, 1e6, 1e6 - 0.5);
    const double x[] = {1.0};
    const double y[] = {1000.0};

    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    TEST_ASSERT_TRUE(r.primal_feasible);
    /* Waived: the distance of 0.5 is inside the window of 1e-6 * 1e6. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.max_dual_violation);
    /* And refused anyway. */
    TEST_ASSERT_EQUAL_DOUBLE(0.0, r.primal_objective);
    TEST_ASSERT_EQUAL_DOUBLE(-500.0, r.dual_objective);
    TEST_ASSERT_TRUE(r.objective_gap > 0.9);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* A column value is one published number rather than a sum of cancelling
 * terms, so its scale is its own magnitude and nothing more. This pins that
 * the row argument was not quietly applied to columns as well: a column a
 * long way from its bound with a real reduced cost stays a violation
 * however large the row it sits in. */
static void test_the_column_test_did_not_inherit_the_rows_scale(void)
{
    const double c[] = {1.0};
    const double cl[] = {0.0}, cu[] = {1e6};
    const double rl[] = {-INFINITY}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1}, ix[] = {0};
    const double v[] = {1e10};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 1, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    /* x sits at 1, a million away from either bound, with d = 1: the sign
     * condition wants it at its lower bound and it is not. */
    const double x[] = {1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));
    TEST_ASSERT_EQUAL_DOUBLE(1.0, r.max_dual_violation);
    TEST_ASSERT_FALSE(r.dual_feasible);
    jaos_model_free(m);
}

/* ---- The gap is a difference, and the difference can hide its halves ---
 *
 * P - D = sum of w_v (v - bound_v). Every term is non-negative on a point
 * that is exactly primal feasible, so the gap bounds the suboptimality. A
 * primal violation the checker tolerates breaks that: its term is negative
 * and cancels positive ones elsewhere, and the total goes to zero while
 * neither half does.
 *
 * The model below is D24's construction, and it is built so that the two
 * halves are equal and the gap vanishes:
 *
 *      min  1e9 x0 + 1e-7 x1   s.t.  x0 >= 1,  x0 >= 0,  0 <= x1 <= 9e9
 *
 * Judged at tol = 1e-6 with x = (1 - 9e-7, 9e9) and y = 1e9:
 *
 *   - The row is 9e-7 below its bound, inside the tolerance, so the point
 *     passes as primal feasible. Its multiplier is 1e9, so its term is
 *     1e9 * (-9e-7) = -900.
 *   - x1's reduced cost is 1e-7, which is at or below tol, so D22 waives
 *     its sign condition. It contributes 1e-7 * (9e9 - 0) = +900 anyway,
 *     which is the rule D21 settled: a multiplier too small to impose a
 *     condition still carries w * bound.
 *
 * The two cancel exactly. The gap reports zero on a point carrying 900 of
 * each, which is what the gap alone cannot show and what the two halves
 * are for. No verdict moves: this point is accepted before the change and
 * after it, and the test asserts that too.
 *
 * Honesty about what this demonstrates, kept from D24: the cancellation,
 * not a false acceptance. Nothing here is materially suboptimal — hiding a
 * negative half costs an equal positive one, and the positive half is
 * exactly what bounds the suboptimality. */
static void test_the_gap_can_be_two_large_halves_cancelling(void)
{
    const double c[] = {1e9, 1e-7};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, 9e9};
    const double rl[] = {1.0}, ru[] = {INFINITY};
    /* x1 appears in no row, so its reduced cost is its cost. */
    const int64_t s[] = {0, 1, 1}, ix[] = {0};
    const double v[] = {1.0};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     1, s, ix, v));

    const double x[] = {1.0 - 9e-7, 9e9};
    const double y[] = {1e9};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    /* Accepted, exactly as before: 9e-7 of row violation is inside 1e-6,
     * and the waived sign condition reports nothing. */
    TEST_ASSERT_TRUE(r.primal_feasible);
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-13, 9e-7, r.max_row_violation);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 0.0, r.max_dual_violation);

    /* The gap is zero to the last bit the objectives can carry, on a point
     * whose two halves are 900 each. */
    TEST_ASSERT_TRUE(r.objective_gap < 1e-15);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 900.0, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 900.0, r.gap_negative);

    /* And the halves are the gap, reached the other way round: the same
     * quantity out of the term sum instead of out of the two objectives. */
    double scale = 1.0 + fabs(r.primal_objective) + fabs(r.dual_objective);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, r.objective_gap,
                              fabs(r.gap_positive - r.gap_negative) / scale);
    jaos_model_free(m);
}

/* The ordinary case, so that the halves are pinned where nothing cancels:
 * on T1's true optimum every term is zero, and on the waived-but-costly
 * point of the test above this one, the whole 0.1 of gap sits in the
 * positive half with nothing against it. That is the shape an honest
 * certificate has, and it is what makes the cancelling case tell apart. */
static void test_a_clean_point_carries_no_negative_half(void)
{
    jaos_model *m = make_t1();
    const double x[] = {0.5, 0.5};
    const double y[] = {1.0};
    jaos_check_report r;

    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, TOL, &r));
    TEST_ASSERT_TRUE(r.dual_feasible);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_positive);
    TEST_ASSERT_DOUBLE_WITHIN(1e-15, 0.0, r.gap_negative);
    jaos_model_free(m);
}

/* D24 keeps the relative row residue in the report and out of the
 * predicate. `finnis` is the case it exists for: a row whose terms total
 * 4.0e10 in magnitude, landing 8.44e-7 from its bound — 2.1e-17 of what it
 * carries, a tenth of one ulp at that size.
 *
 * Those numbers cannot be built from two terms, and that is not a
 * limitation of the test: one ulp at 4.0e10 is 7.6e-6, so a two-term
 * difference at that size cannot land 8.44e-7 from anything. It takes a
 * long sum for a residue to end up finer than the ulp of what it is made
 * of. So the shape is reproduced at a scale where the arithmetic is exact
 * and the ratio is what is under test:
 *
 *      x0 - x1 >= 0,  x0 = 1 - 2^-40,  x1 = 1,  coefficients +/- 1e6
 *
 * Activity -9.09e-7 against a lower bound of zero, out of traffic 2e6.
 * Every quantity there is a power of two times an exact integer, so both
 * figures are exact rather than approximately right. */
static void test_the_relative_row_residue_is_reported_and_decides_nothing(void)
{
    const double c[] = {0.0, 0.0};
    const double cl[] = {0.0, 0.0}, cu[] = {INFINITY, INFINITY};
    const double rl[] = {0.0}, ru[] = {INFINITY};
    const int64_t s[] = {0, 1, 2}, ix[] = {0, 0};
    const double v[] = {1e6, -1e6};
    jaos_model *m = nullptr;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_model_new(&m));
    TEST_ASSERT_EQUAL_INT(JAOS_OK,
        jaos_load_lp(m, 2, 1, JAOS_MINIMIZE, 0.0, c, cl, cu, rl, ru,
                     2, s, ix, v));

    const double x[] = {1.0 - 0x1p-40, 1.0};
    const double y[] = {0.0};
    jaos_check_report r;
    TEST_ASSERT_EQUAL_INT(JAOS_OK, jaos_check_solution(m, x, y, 1e-6, &r));

    const double viol = 1e6 * 0x1p-40;            /* 9.0949...e-7 */
    const double traffic = 2e6 - viol;
    TEST_ASSERT_DOUBLE_WITHIN(1e-18, viol, r.max_row_violation);
    TEST_ASSERT_DOUBLE_WITHIN(1e-24, viol / traffic,
                              r.max_row_violation_relative);
    /* 9.09e-7 is inside 1e-6, so the point passes — on the absolute test,
     * and on that alone. The relative figure is 4.5e-13 and decides
     * nothing; D24 is the argument for why it is not allowed to. */
    TEST_ASSERT_TRUE(r.primal_feasible);
    jaos_model_free(m);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_gap_can_be_two_large_halves_cancelling);
    RUN_TEST(test_a_clean_point_carries_no_negative_half);
    RUN_TEST(test_the_relative_row_residue_is_reported_and_decides_nothing);
    RUN_TEST(test_a_row_at_its_bound_to_its_own_precision_is_accepted);
    RUN_TEST(test_a_row_genuinely_off_its_bound_is_still_reported);
    RUN_TEST(test_a_waived_row_that_still_costs_is_refused_by_the_gap);
    RUN_TEST(test_the_column_test_did_not_inherit_the_rows_scale);
    RUN_TEST(test_t1_accepts_the_true_optimum);
    RUN_TEST(test_t1_flags_wrong_dual_sign);
    RUN_TEST(test_t1_flags_complementarity_break);
    RUN_TEST(test_t1_flags_primal_violation);
    RUN_TEST(test_t2_accepts_the_true_optimum_maximize);
    RUN_TEST(test_t2_flags_wrong_dual_magnitude);
    RUN_TEST(test_a_tiny_multiplier_on_a_large_bound_still_counts);
    RUN_TEST(test_a_waived_sign_condition_is_still_caught_by_the_gap);
    RUN_TEST(test_an_implied_bound_makes_the_dropped_term_finite);
    RUN_TEST(test_an_unbounded_ray_is_counted_unless_its_rate_is_real);
    RUN_TEST(test_check_rejects_bad_arguments);
    return UNITY_END();
}
