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


if __name__ == "__main__":
    unittest.main()
