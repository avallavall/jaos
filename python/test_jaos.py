"""Tests for the Python binding.

The binding owns no arithmetic, so what these check is the boundary: that
every argument arrives in the right slot, that arrays come back the right
length, that a C failure becomes an exception rather than a silently ignored
status code, and that the answers agree with the ones the C suite asserts.

Where a number appears here it is the number `tests/test_simplex.c` or
`tests/test_mps.c` already asserts, on the same file. A binding that agreed
with itself but not with the library would pass a suite that invented its
own expected values.

Run with `make python-test` from the repository root.
"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jaos                                                  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def data(name):
    return os.path.join(ROOT, "tests", "data", name)


class TestLibrary(unittest.TestCase):
    def test_the_version_comes_from_the_library(self):
        v = jaos.version()
        self.assertTrue(v)
        self.assertRegex(v, r"^\d+\.\d+\.\d+")

    def test_infinity_is_a_real_infinity(self):
        self.assertEqual(jaos.INFINITY, float("inf"))

    def test_the_loaded_path_is_reported(self):
        self.assertTrue(os.path.exists(jaos.library_path()))


class TestSolving(unittest.TestCase):
    """The golden three-by-three, which tests/data/solve1.mps holds and
    tests/test_simplex.c solves to 29.0."""

    def test_solve1_mps_agrees_with_the_c_suite(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 29.0, places=9)

    def test_the_same_model_built_in_memory(self):
        """min -x - 2y  s.t. x + y <= 4, x, y >= 0. Optimum -8, which
        tests/test_simplex.c asserts on the same data."""
        with jaos.Model() as m:
            m.load(num_col=2, num_row=1,
                   col_cost=[-1.0, -2.0],
                   col_lower=[0.0, 0.0],
                   col_upper=[jaos.INFINITY, jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[4.0],
                   a_start=[0, 1, 2], a_index=[0, 0],
                   a_value=[1.0, 1.0])
            self.assertEqual((m.num_col, m.num_row, m.num_nz), (2, 1, 2))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), -8.0, places=9)
            s = m.solution()
            self.assertEqual(len(s.col_value), 2)
            self.assertEqual(len(s.row_activity), 1)
            self.assertEqual(len(s.row_dual), 1)
            self.assertEqual(len(s.col_dual), 2)

            # The values, not only the lengths. Four arrays of doubles go
            # into one C call and lengths alone cannot tell two of them
            # apart when the model is square; these numbers can.
            # y = 4, x = 0, the row binds at 4, its dual is -2, and the
            # reduced costs are d_x = -1 + 2 = 1 and d_y = -2 + 2 = 0.
            self.assertAlmostEqual(s.col_value[0], 0.0, places=9)
            self.assertAlmostEqual(s.col_value[1], 4.0, places=9)
            self.assertAlmostEqual(s.row_activity[0], 4.0, places=9)
            self.assertAlmostEqual(s.row_dual[0], -2.0, places=9)
            self.assertAlmostEqual(s.col_dual[0], 1.0, places=9)
            self.assertAlmostEqual(s.col_dual[1], 0.0, places=9)

    def test_an_unbounded_model_says_so(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[-1.0], col_lower=[0.0],
                   col_upper=[jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[jaos.INFINITY],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.UNBOUNDED)

    def test_a_matrix_of_no_entries_loads(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=0,
                   col_cost=[1.0], col_lower=[2.0], col_upper=[5.0],
                   row_lower=[], row_upper=[])
            self.assertEqual(m.num_nz, 0)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 2.0, places=9)

    def test_the_basis_comes_back_one_status_per_variable(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            b = m.basis()
            self.assertEqual(len(b.col_status), m.num_col)
            self.assertEqual(len(b.row_status), m.num_row)
            for st in b.col_status + b.row_status:
                self.assertIsInstance(st, jaos.BasisStatus)

    def test_the_counters_move(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            self.assertGreater(m.work_units, 0)
            self.assertGreaterEqual(m.iterations, 0)
            self.assertGreaterEqual(m.solve_time, 0.0)


class TestReadingFiles(unittest.TestCase):
    def test_t1_mps_matches_what_the_c_suite_asserts(self):
        with jaos.Model() as m:
            m.read_mps(data("t1.mps"))
            self.assertEqual((m.num_col, m.num_row, m.num_nz), (3, 3, 6))
            self.assertEqual(m.col_cost(0), 1.0)
            self.assertEqual(m.col_cost(1), 2.0)
            self.assertEqual(m.col_cost(2), -1.0)
            self.assertEqual(m.col_bounds(0), (0.0, 4.0))
            lo, hi = m.col_bounds(1)
            self.assertEqual(lo, -1.0)
            self.assertEqual(hi, jaos.INFINITY)

    def test_a_gzip_file_reads_as_the_plain_one_does(self):
        with jaos.Model() as plain, jaos.Model() as packed:
            plain.read_mps(data("t1.mps"))
            packed.read_mps(data("t1.mps.gz"))
            self.assertEqual(plain.num_col, packed.num_col)
            self.assertEqual(plain.num_row, packed.num_row)
            self.assertEqual(plain.num_nz, packed.num_nz)
            for j in range(plain.num_col):
                self.assertEqual(plain.col_cost(j), packed.col_cost(j))

    def test_an_lp_file_reads(self):
        with jaos.Model() as m:
            m.read_lp(data("g1.lp"))
            self.assertGreater(m.num_col, 0)

    def test_a_round_trip_through_mps_keeps_the_model(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "out.mps")
            with jaos.Model() as a:
                a.read_mps(data("solve1.mps"))
                a.write_mps(path)
                want = (a.num_col, a.num_row, a.num_nz)
            with jaos.Model() as b:
                b.read_mps(path)
                self.assertEqual((b.num_col, b.num_row, b.num_nz), want)
                b.solve()
                self.assertAlmostEqual(b.objective(), 29.0, places=9)


class TestFailuresBecomeExceptions(unittest.TestCase):
    """Every one of these would be a silently ignored status code in C if
    the binding dropped it, so each is checked for the exception AND for the
    specific status."""

    def test_a_missing_file_is_an_io_error(self):
        with jaos.Model() as m:
            with self.assertRaises(jaos.JaosError) as ctx:
                m.read_mps(data("no_such_file.mps"))
            self.assertIs(ctx.exception.status, jaos.Status.ERR_IO)
            self.assertIn("cannot open", ctx.exception.detail)

    def test_a_malformed_file_carries_its_line_number(self):
        with jaos.Model() as m:
            with self.assertRaises(jaos.JaosError) as ctx:
                m.read_mps(data("e_badnum.mps"))
            self.assertIs(ctx.exception.status,
                          jaos.Status.ERR_INVALID_INPUT)
            self.assertIn("line 6", ctx.exception.detail)

    def test_a_damaged_gzip_file_is_refused(self):
        with jaos.Model() as m:
            with self.assertRaises(jaos.JaosError) as ctx:
                m.read_mps(data("eg_badcrc.mps.gz"))
            self.assertIn("checksum", ctx.exception.detail)

    def test_no_objective_before_a_solve(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            with self.assertRaises(jaos.JaosError):
                m.objective()
            with self.assertRaises(jaos.JaosError):
                m.solution()

    def test_no_objective_after_a_solve_that_found_none(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[-1.0], col_lower=[0.0],
                   col_upper=[jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[jaos.INFINITY],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.UNBOUNDED)
            with self.assertRaises(jaos.JaosError):
                m.objective()

    def test_an_index_out_of_range_is_refused(self):
        with jaos.Model() as m:
            m.read_mps(data("t1.mps"))
            with self.assertRaises(jaos.JaosError):
                m.col_cost(99)
            with self.assertRaises(jaos.JaosError):
                m.row_bounds(99)

    def test_a_mismatched_array_length_is_caught_here(self):
        """This one never reaches C: the length is wrong on the Python side
        and the C call would read past the end of the buffer."""
        with jaos.Model() as m:
            with self.assertRaises(ValueError):
                m.load(num_col=2, num_row=1,
                       col_cost=[1.0],                  # one, not two
                       col_lower=[0.0, 0.0], col_upper=[1.0, 1.0],
                       row_lower=[0.0], row_upper=[1.0])

    def test_a_closed_model_refuses_rather_than_crashing(self):
        m = jaos.Model()
        m.read_mps(data("t1.mps"))
        m.close()
        m.close()                                        # twice is fine
        with self.assertRaises(ValueError):
            m.solve()
        self.assertIn("closed", repr(m))


class TestLimitsAndOutput(unittest.TestCase):
    def test_a_work_limit_stops_the_solve_and_says_so(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_work_limit(1)
            self.assertIn(m.solve(), (jaos.SolveStatus.WORK_LIMIT,
                                      jaos.SolveStatus.OPTIMAL))

    def test_the_log_callback_receives_lines(self):
        seen = []

        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_log_callback(lambda lvl, line: seen.append((lvl, line)),
                               jaos.LogLevel.SUMMARY)
            m.solve()

        self.assertTrue(seen, "a solve at SUMMARY produced no output")
        for lvl, line in seen:
            self.assertIsInstance(lvl, jaos.LogLevel)
            self.assertIsInstance(line, str)
            self.assertNotIn("\n", line)

    def test_turning_the_callback_off_silences_it(self):
        seen = []
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_log_callback(lambda lvl, line: seen.append(line))
            m.set_log_callback(None)
            m.solve()
        self.assertEqual(seen, [])

    def test_the_answer_is_the_same_with_logging_on(self):
        """D8 reaches the binding: a model solved noisily returns what the
        same model returns silently."""
        with jaos.Model() as quiet:
            quiet.read_mps(data("solve1.mps"))
            quiet.solve()
            a, wa = quiet.objective(), quiet.work_units

        with jaos.Model() as loud:
            loud.read_mps(data("solve1.mps"))
            loud.set_log_callback(lambda lvl, line: None,
                                  jaos.LogLevel.DETAIL)
            loud.solve()
            b, wb = loud.objective(), loud.work_units

        self.assertEqual(a, b)
        self.assertEqual(wa, wb)


class TestChangingAModel(unittest.TestCase):
    def test_a_changed_bound_changes_the_answer(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[-1.0], col_lower=[0.0], col_upper=[3.0],
                   row_lower=[-jaos.INFINITY], row_upper=[10.0],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            m.solve()
            self.assertAlmostEqual(m.objective(), -3.0, places=9)

            m.set_col_bounds(0, 0.0, 7.0)
            m.solve()
            self.assertAlmostEqual(m.objective(), -7.0, places=9)

            m.set_col_cost(0, 1.0)
            m.solve()
            self.assertAlmostEqual(m.objective(), 0.0, places=9)

    def test_setting_a_coefficient_moves_the_optimum(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[-1.0], col_lower=[0.0],
                   col_upper=[jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[10.0],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            m.solve()
            self.assertAlmostEqual(m.objective(), -10.0, places=9)

            m.set_coefficient(0, 0, 2.0)      # x <= 5 now
            m.solve()
            self.assertAlmostEqual(m.objective(), -5.0, places=9)


class TestGrowingAndShrinking(unittest.TestCase):
    """The append and delete calls. Each optimum here is distinct from the
    one before it, so a call that silently did nothing fails the next
    assertion rather than passing it."""

    def golden(self):
        """min -x - 2y  s.t. x + y <= 4, x, y >= 0: optimum -8, the same
        model tests/test_simplex.c asserts on."""
        m = jaos.Model()
        m.load(num_col=2, num_row=1,
               col_cost=[-1.0, -2.0],
               col_lower=[0.0, 0.0],
               col_upper=[jaos.INFINITY, jaos.INFINITY],
               row_lower=[-jaos.INFINITY], row_upper=[4.0],
               a_start=[0, 1, 2], a_index=[0, 0], a_value=[1.0, 1.0])
        return m

    def test_adding_a_row_binds_and_deleting_it_unbinds(self):
        with self.golden() as m:
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), -8.0, places=9)

            m.add_rows([-jaos.INFINITY], [3.0],           # y <= 3
                       a_start=[0, 1], a_index=[1], a_value=[1.0])
            self.assertEqual((m.num_row, m.num_nz), (2, 3))
            m.solve()                          # y = 3, x = 1
            self.assertAlmostEqual(m.objective(), -7.0, places=9)

            m.delete_rows([1])
            self.assertEqual(m.num_row, 1)
            m.solve()
            self.assertAlmostEqual(m.objective(), -8.0, places=9)

    def test_adding_a_column_improves_the_optimum(self):
        with self.golden() as m:
            m.solve()
            m.add_cols([-3.0], [0.0], [2.0],   # z in [0,2], joins the row
                       a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertEqual((m.num_col, m.num_nz), (3, 3))
            m.solve()                          # z = 2, y = 2
            self.assertAlmostEqual(m.objective(), -10.0, places=9)

    def test_deleting_a_column_removes_its_contribution(self):
        with self.golden() as m:
            m.delete_cols([1])                 # y is gone; min -x, x <= 4
            self.assertEqual((m.num_col, m.num_nz), (1, 1))
            m.solve()
            self.assertAlmostEqual(m.objective(), -4.0, places=9)

    def test_a_repeated_delete_index_is_refused(self):
        with self.golden() as m:
            with self.assertRaises(jaos.JaosError):
                m.delete_cols([0, 0])


class TestBasisRoundTrip(unittest.TestCase):
    def test_a_basis_read_out_can_be_handed_back(self):
        with jaos.Model() as a:
            a.read_mps(data("solve1.mps"))
            a.solve()
            b = a.basis()

        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_basis(b.col_status, b.row_status)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 29.0, places=9)

    def test_a_wrong_length_never_reaches_c(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            with self.assertRaises(ValueError):
                m.set_basis([jaos.BasisStatus.BASIC], [])


class TestTheChecker(unittest.TestCase):
    """jaos_check_solution through the binding. The accepting case alone
    would also pass if the report came back unfilled, so the rejecting case
    is the one that proves the fields land."""

    def test_the_true_solution_checks_out(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            s = m.solution()
            r = m.check_solution(s.col_value, s.row_dual)
            self.assertTrue(r.primal_feasible)
            self.assertTrue(r.dual_feasible)
            self.assertTrue(r.checked_duals)
            self.assertAlmostEqual(r.primal_objective, 29.0, places=9)

    def test_a_corrupted_solution_is_flagged(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            s = m.solution()
            wrong = [v + 100.0 for v in s.col_value]
            r = m.check_solution(wrong, s.row_dual)
            self.assertFalse(r.primal_feasible)
            self.assertGreater(max(r.max_col_violation,
                                   r.max_row_violation), 1.0)

    def test_no_duals_means_no_dual_verdict(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            r = m.check_solution(m.solution().col_value)
            self.assertFalse(r.checked_duals)


class TestCertificates(unittest.TestCase):
    """jaos_certificate, jaos_unbounded_ray and their two checkers through
    the binding. The numbers are the ones tests/test_check.c asserts on the
    same models; the refusing cases are what prove the report's fields
    land and that an OPTIMAL answer hands out no ray."""

    def test_an_infeasible_model_proves_it(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[1.0], col_lower=[0.0], col_upper=[2.0],
                   row_lower=[4.0], row_upper=[jaos.INFINITY],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.INFEASIBLE)
            y = m.certificate()
            self.assertEqual(len(y), 1)
            r = m.check_certificate(y)
            self.assertTrue(r.certified)
            self.assertAlmostEqual(r.inf_rows, 4.0, places=9)
            self.assertAlmostEqual(r.sup_columns, 2.0, places=9)
            self.assertAlmostEqual(r.gap, 2.0, places=9)

    def test_a_feasible_model_has_no_certificate(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            with self.assertRaises(jaos.JaosError):
                m.certificate()
            with self.assertRaises(jaos.JaosError):
                m.unbounded_ray()
            r = m.check_certificate([1.0] * m.num_row)
            self.assertFalse(r.certified)

    def test_an_unbounded_model_proves_it(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[-1.0], col_lower=[0.0],
                   col_upper=[jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[jaos.INFINITY],
                   a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.UNBOUNDED)
            d = m.unbounded_ray()
            self.assertEqual(len(d), 1)
            self.assertGreater(d[0], 0.0)
            r = m.check_ray(d)
            self.assertTrue(r.certified)
            self.assertLess(r.rate, 0.0)
            self.assertEqual(r.max_col_escape, 0.0)

    def test_a_ray_into_a_bound_is_refused(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            r = m.check_ray([1.0] + [0.0] * (m.num_col - 1))
            self.assertFalse(r.certified)
            self.assertGreater(r.max_col_escape, 0.0)

    def test_a_wrong_length_never_reaches_c(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            with self.assertRaises(ValueError):
                m.check_certificate([1.0])
            with self.assertRaises(ValueError):
                m.check_ray([1.0])


class TestProgressCallback(unittest.TestCase):
    def test_the_callback_sees_the_solve(self):
        seen = []
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_progress_callback(lambda p: seen.append(p))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
        # jaos.h promises the first call comes before anything is priced.
        self.assertTrue(seen)
        for p in seen:
            self.assertIsInstance(p, jaos.Progress)
            self.assertGreaterEqual(p.iterations, 0)

    def test_watching_does_not_change_the_answer(self):
        """The header's determinism claim, at the binding: a callback that
        always continues returns the same bits as no callback."""
        with jaos.Model() as quiet:
            quiet.read_mps(data("solve1.mps"))
            quiet.solve()
            a, wa = quiet.objective(), quiet.work_units
        with jaos.Model() as watched:
            watched.read_mps(data("solve1.mps"))
            watched.set_progress_callback(
                lambda p: jaos.CallbackAction.CONTINUE)
            watched.solve()
            b, wb = watched.objective(), watched.work_units
        self.assertEqual(a, b)
        self.assertEqual(wa, wb)

    def test_stop_interrupts_and_leaves_nothing_to_read(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_progress_callback(lambda p: jaos.CallbackAction.STOP)
            self.assertIs(m.solve(), jaos.SolveStatus.INTERRUPTED)
            with self.assertRaises(jaos.JaosError):
                m.objective()
            m.set_progress_callback(None)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)


class TestExpressions(unittest.TestCase):
    """The algebra the modeling layer owns. Coefficients are asserted
    exactly: building an expression is bookkeeping, not arithmetic, and a
    half-lost term here becomes a silently different model."""

    def setUp(self):
        self.p = jaos.Problem()
        self.x = self.p.add_var(name="x")
        self.y = self.p.add_var(name="y")

    def test_terms_combine_and_constants_fold(self):
        e = 2 * self.x + 3 * self.x - self.x + 1 + (self.y - 4) / 2
        self.assertEqual(e._t[self.x], 4.0)
        self.assertEqual(e._t[self.y], 0.5)
        self.assertEqual(e._c, -1.0)

    def test_subtraction_from_a_number(self):
        e = 5 - self.x
        self.assertEqual(e._t[self.x], -1.0)
        self.assertEqual(e._c, 5.0)

    def test_sum_and_quicksum_agree(self):
        xs = [self.x, self.y, self.x]
        a, b = sum(xs), jaos.quicksum(xs)
        self.assertEqual(a._t, b._t)
        self.assertEqual(a._t[self.x], 2.0)

    def test_a_product_of_variables_is_refused(self):
        with self.assertRaises(TypeError):
            self.x * self.y
        with self.assertRaises(TypeError):
            (self.x + 1) * (self.y + 1)
        with self.assertRaises(TypeError):
            1 / self.x

    def test_not_equal_is_refused(self):
        with self.assertRaises(TypeError):
            self.x != self.y

    def test_a_chained_comparison_cannot_lose_a_bound(self):
        """Python evaluates 0 <= e <= 5 as (0 <= e) and (e <= 5), and the
        'and' would silently drop the lower bound. The constraint's refusal
        to have a truth value is what turns that into an error."""
        with self.assertRaises(TypeError):
            0 <= self.x <= 5

    def test_variables_of_two_problems_do_not_mix(self):
        q = jaos.Problem()
        z = q.add_var()
        with self.assertRaises(ValueError):
            self.x + z
        with self.assertRaises(ValueError):
            self.p.add(z <= 1)


class TestProblemSolves(unittest.TestCase):
    def test_the_golden_model_through_the_layer(self):
        """The same LP and the same six numbers as the raw-layer test
        above: min -x - 2y s.t. x + y <= 4."""
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var()
        c = p.add(x + y <= 4)
        p.minimize(-x - 2 * y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, -8.0, places=9)
        self.assertAlmostEqual(x.value, 0.0, places=9)
        self.assertAlmostEqual(y.value, 4.0, places=9)
        self.assertAlmostEqual(c.activity, 4.0, places=9)
        self.assertAlmostEqual(c.dual, -2.0, places=9)
        self.assertAlmostEqual(x.reduced_cost, 1.0, places=9)
        self.assertAlmostEqual(y.reduced_cost, 0.0, places=9)

    def test_maximize_is_not_minimize(self):
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var()
        p.add(x + y <= 4)
        p.maximize(x + 2 * y)
        p.solve()
        self.assertAlmostEqual(p.objective_value, 8.0, places=9)

    def test_an_equality_constraint_holds(self):
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var()
        p += x + y == 3
        p.minimize(-x)
        p.solve()
        self.assertAlmostEqual(p.objective_value, -3.0, places=9)

    def test_a_range_constraint_uses_both_sides(self):
        """The lower side is the binding one here, so a range that lost it
        (the chained-comparison mistake) answers 0 instead of 1."""
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var()
        p.add_range(1, x + y, 5)
        p.minimize(x + y)
        p.solve()
        self.assertAlmostEqual(p.objective_value, 1.0, places=9)

    def test_the_objective_constant_is_reported(self):
        p = jaos.Problem()
        x = p.add_var(lb=2)
        p.minimize(x + 7)
        p.solve()
        self.assertAlmostEqual(p.objective_value, 9.0, places=9)

    def test_a_problem_with_no_constraints_solves_on_bounds(self):
        p = jaos.Problem()
        x = p.add_var(lb=3)
        p.minimize(x)
        p.solve()
        self.assertAlmostEqual(p.objective_value, 3.0, places=9)

    def test_a_variable_in_no_constraint_still_loads(self):
        p = jaos.Problem()
        x = p.add_var()
        z = p.add_var(ub=2)                    # never in a row
        p.add(x <= 4)
        p.minimize(-x - 3 * z)
        p.solve()
        self.assertAlmostEqual(p.objective_value, -10.0, places=9)
        self.assertAlmostEqual(z.value, 2.0, places=9)

    def test_two_identical_builds_answer_identically(self):
        """Determinism reaches the layer: the same script builds the same
        arrays, so the objectives are equal as bits, not as approximations."""
        def build():
            p = jaos.Problem()
            xs = p.add_vars(3, ub=9)
            p.add(jaos.quicksum(xs) <= 10)
            p.add(xs[0] - xs[2] >= -2)
            p.minimize(-2 * xs[0] - 3 * xs[1] - xs[2])
            p.solve()
            return p.objective_value, p.work_units
        self.assertEqual(build(), build())

    def test_infeasible_is_reported_not_raised(self):
        p = jaos.Problem()
        x = p.add_var(ub=1)
        p.add(x >= 2)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)

    def test_the_layers_certificate_is_one_multiplier_per_constraint(self):
        p = jaos.Problem()
        x = p.add_var(ub=1)
        y = p.add_var(ub=1)
        p.add(x + y <= 2)
        p.add(x + y >= 3)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        ray = p.certificate()
        self.assertEqual(len(ray), 2)
        self.assertTrue(p.model.check_certificate(ray).certified)
        # Ahead of its solve, the layer refuses rather than hand out a
        # ray for a model that no longer exists.
        p.add(x >= 0.5)
        with self.assertRaises(ValueError):
            p.certificate()

    def test_the_layers_ray_is_one_step_per_variable(self):
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var(ub=1)
        p.add(x - y >= 0)
        p.maximize(x)
        self.assertIs(p.solve(), jaos.SolveStatus.UNBOUNDED)
        d = p.unbounded_ray()
        self.assertEqual(len(d), 2)
        self.assertGreater(d[0], 0.0)
        self.assertTrue(p.model.check_ray(d).certified)

    def test_the_layers_checker_accepts_its_own_answer(self):
        p = jaos.Problem()
        x = p.add_var()
        p.add(x <= 4)
        p.minimize(-x)
        p.solve()
        r = p.check()
        self.assertTrue(r.primal_feasible)
        self.assertTrue(r.dual_feasible)


class TestRanging(unittest.TestCase):
    """The three ranging calls through the binding, on the textbook pair
    of rows tests/test_ranging.c works by hand: max x0 + x1 subject to
    x0 + 2 x1 <= 4 and 3 x0 + x1 <= 6, optimum (1.6, 1.2)."""

    def load(self, m):
        m.load(num_col=2, num_row=2, sense=jaos.ObjSense.MAXIMIZE,
               col_cost=[1.0, 1.0], col_lower=[0.0, 0.0],
               col_upper=[jaos.INFINITY, jaos.INFINITY],
               row_lower=[-jaos.INFINITY, -jaos.INFINITY],
               row_upper=[4.0, 6.0],
               a_start=[0, 2, 4], a_index=[0, 1, 0, 1],
               a_value=[1.0, 3.0, 2.0, 1.0])

    def test_cost_ranging_reads_the_textbook_intervals(self):
        with jaos.Model() as m:
            self.load(m)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            r = m.cost_ranging()
            self.assertEqual(len(r.lower), 2)
            self.assertAlmostEqual(r.lower[0], 0.5, places=12)
            self.assertAlmostEqual(r.upper[0], 3.0, places=12)
            self.assertAlmostEqual(r.lower[1], 1.0 / 3.0, places=12)
            self.assertAlmostEqual(r.upper[1], 2.0, places=12)

    def test_rhs_and_bound_ranging_come_back_four_lists_each(self):
        with jaos.Model() as m:
            self.load(m)
            m.solve()
            r = m.rhs_ranging()
            self.assertAlmostEqual(r.upper_lo[0], 2.0, places=12)
            self.assertAlmostEqual(r.upper_hi[0], 12.0, places=12)
            self.assertEqual(r.lower_lo[1], -jaos.INFINITY)
            self.assertAlmostEqual(r.lower_hi[1], 6.0, places=12)
            b = m.bound_ranging()
            self.assertEqual(b.lower_lo[0], -jaos.INFINITY)
            self.assertAlmostEqual(b.lower_hi[0], 1.6, places=12)
            self.assertAlmostEqual(b.upper_lo[1], 1.2, places=12)
            self.assertEqual(b.upper_hi[1], jaos.INFINITY)

    def test_ranging_needs_an_optimum(self):
        with jaos.Model() as m:
            self.load(m)
            with self.assertRaises(jaos.JaosError):
                m.cost_ranging()

    def test_the_layer_ranges_in_the_order_variables_were_added(self):
        p = jaos.Problem()
        x = p.add_var()
        y = p.add_var()
        p.add(x + 2 * y <= 4)
        p.add(3 * x + y <= 6)
        p.maximize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        r = p.cost_ranging()
        self.assertAlmostEqual(r.lower[0], 0.5, places=12)
        self.assertAlmostEqual(r.upper[1], 2.0, places=12)
        self.assertEqual(len(p.rhs_ranging().upper_hi), 2)
        self.assertEqual(len(p.bound_ranging().lower_hi), 2)
        y.ub = 1.0
        with self.assertRaises(ValueError):
            p.cost_ranging()


class TestProblemResolves(unittest.TestCase):
    """The change-tracking path: a value moved after a solve goes through
    the C setters, everything else rebuilds. Each case is judged against a
    fresh Problem built directly in the changed state, so a delta applied
    to the wrong slot cannot agree with it."""

    def build(self, row_ub=4.0, x_lb=0.0):
        p = jaos.Problem()
        x = p.add_var(lb=x_lb)
        y = p.add_var()
        c = p.add(x + y <= row_ub)
        p.minimize(-x - 2 * y)
        return p, x, y, c

    def test_moving_a_rhs_agrees_with_a_fresh_build(self):
        p, x, y, c = self.build()
        p.solve()
        c.ub = 3.0
        p.solve()
        fresh, *_ = self.build(row_ub=3.0)
        fresh.solve()
        self.assertAlmostEqual(p.objective_value, fresh.objective_value,
                               places=9)
        self.assertAlmostEqual(p.objective_value, -6.0, places=9)

    def test_moving_a_variable_bound_agrees_with_a_fresh_build(self):
        p, x, y, c = self.build()
        p.solve()
        x.lb = 1.0
        p.solve()                              # x = 1, y = 3
        fresh, *_ = self.build(x_lb=1.0)
        fresh.solve()
        self.assertAlmostEqual(p.objective_value, fresh.objective_value,
                               places=9)
        self.assertAlmostEqual(p.objective_value, -7.0, places=9)

    def test_a_new_objective_goes_through_the_cost_setter(self):
        p, x, y, c = self.build()
        p.solve()
        p.minimize(-3 * x - 2 * y)             # same sense, same constant
        p.solve()                              # x = 4 now
        self.assertAlmostEqual(p.objective_value, -12.0, places=9)

    def test_adding_a_variable_after_a_solve_rebuilds(self):
        p, x, y, c = self.build()
        p.solve()
        z = p.add_var(ub=2)
        p.minimize(-x - 2 * y - 3 * z)
        p.solve()
        self.assertAlmostEqual(p.objective_value, -14.0, places=9)
        self.assertAlmostEqual(z.value, 2.0, places=9)

    def test_a_stale_value_refuses_to_answer(self):
        p, x, y, c = self.build()
        p.solve()
        self.assertAlmostEqual(y.value, 4.0, places=9)
        c.ub = 3.0
        with self.assertRaises(ValueError):
            y.value
        with self.assertRaises(ValueError):
            p.objective_value
        p.solve()
        self.assertAlmostEqual(y.value, 3.0, places=9)


if __name__ == "__main__":
    unittest.main()
