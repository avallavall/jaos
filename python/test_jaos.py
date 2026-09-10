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

import fractions
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import jaos

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

    def test_the_library_name_follows_the_platform(self):
        names, dirs = jaos._library_names("linux")
        self.assertEqual(names, ["libjaos.so"])
        self.assertIn(os.path.join("build", "release"), dirs)
        names, dirs = jaos._library_names("win32")
        self.assertEqual(names, ["jaos.dll", "libjaos.dll"])
        self.assertIn(os.path.join("build", "cmake"), dirs)
        names, _ = jaos._library_names("darwin")
        self.assertEqual(names[0], "libjaos.dylib")
        here, _ = jaos._library_names()
        self.assertTrue(jaos.library_path().endswith(tuple(here))
                        or "JAOS_LIBRARY" in os.environ)

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
    def test_the_thread_count_is_kept_and_changes_no_answer(self):
        p = jaos.Problem()
        p.set_threads(1)
        with self.assertRaises(jaos.JaosError):
            p.set_threads(0)
        with self.assertRaises(jaos.JaosError):
            p.set_threads(-2)
        answers = []
        for n in (1, 3):
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.set_algorithm(jaos.Algorithm.CONCURRENT)
                m.set_threads(n)
                self.assertEqual(m.threads, n)
                self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
                answers.append((m.objective(), m.work_units))
        self.assertEqual(answers[0], answers[1])

    def test_an_nl_file_reads_with_its_names_and_a_nonlinear_one_is_refused(self):
        with jaos.Model() as m:
            m.read_nl(data("t_lin.nl"))
            self.assertEqual((m.num_col, m.num_row, m.num_nz), (3, 3, 6))
            self.assertEqual(m.col_name(2), "y")
            self.assertEqual(m.row_name(1), "c2")
            self.assertTrue(m.col_integer(2))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), -4.0, places=9)
        with jaos.Model() as m:
            with self.assertRaises(jaos.JaosError) as ctx:
                m.read_nl(data("e_nonlin.nl"))
            self.assertIn("nonlinear", str(ctx.exception))

    def test_a_separable_qp_reads_solves_and_is_written_by_expressions(self):
        with jaos.Model() as m:
            m.read_lp(data("g_quad.lp"))
            self.assertEqual(m.col_quadratic(0), 2.0)
            self.assertEqual(m.col_quadratic(1), 2.0)
            self.assertEqual(m.statistics().quadratic_col, 2)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 4.0, places=6)
            m.set_col_quadratic(0, -1.0)
            with self.assertRaises(jaos.JaosError) as ctx:
                m.solve()
            self.assertIn("convex", str(ctx.exception))
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=10, name="x")
        y = p.add_var(lb=0, ub=10, name="y")
        p.add(x + y >= 2, name="c1")
        p.minimize(x + y + x * x + y ** 2)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 4.0, places=6)
        self.assertAlmostEqual(x.value, 1.0, places=5)
        self.assertEqual(p._m.col_quadratic(0), 2.0)
        with self.assertRaises(TypeError):
            p.minimize(x * y)
        with self.assertRaises(TypeError):
            p.add(x * x <= 4)
        q = jaos.Problem()
        a = q.add_var(lb=0, ub=5, name="a", integer=True)
        b = q.add_var(lb=0, ub=5, name="b", integer=True)
        q.add(a + b <= 3)
        q.minimize(a * a + b * b - 5.2 * a - 2.6 * b)
        self.assertIs(q.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(q.objective_value, -8.0, places=6)
        self.assertAlmostEqual(a.value, 2.0, places=6)
        self.assertAlmostEqual(b.value, 1.0, places=6)
        p.minimize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertEqual(p._m.col_quadratic(0), 0.0)
        self.assertAlmostEqual(p.objective_value, 2.0, places=9)

    def test_qplib_round_trips_and_osil_is_written(self):
        with jaos.Model() as m, tempfile.TemporaryDirectory() as d:
            m.read_qplib(data("g_quad.qplib"))
            self.assertEqual((m.num_col, m.num_row), (2, 1))
            self.assertEqual(m.col_name(1), "y")
            self.assertEqual(m.col_quadratic(0), 2.0)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 4.0, places=6)
            path = os.path.join(d, "out.qplib")
            m.write_qplib(path)
            with jaos.Model() as back:
                back.read_qplib(path)
                self.assertEqual(back.row_name(0), "c1")
                self.assertEqual(back.col_quadratic(1), 2.0)
            osil = os.path.join(d, "out.osil")
            m.write_osil(osil)
            with open(osil) as f:
                text = f.read()
            self.assertIn('<var name="x"', text)
            self.assertIn('<qTerm idx="-1"', text)
            with jaos.Model() as back:
                back.read_osil(osil)
                self.assertEqual((back.num_col, back.num_row), (2, 1))
                self.assertEqual(back.col_name(0), "x")
                self.assertEqual(back.row_name(0), "c1")
                self.assertEqual(back.col_quadratic(0), 2.0)
                self.assertIs(back.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertAlmostEqual(back.objective(), 4.0, places=6)
            with jaos.Model() as back:
                back.read_osil(data("g_osil_rowwise.osil"))
                self.assertEqual((back.num_col, back.num_row), (3, 2))
                self.assertEqual(back.col_name(0), "x")
                self.assertTrue(back.col_integer(1))
                back.read_osil(data("e_osil_offdiag.osil"))
                self.assertEqual(back.quadratic_nz(), 1)
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=4, name="x", integer=True)
        p.add(x >= 1.5, name="floor")
        p.minimize(x)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "p.qplib")
            p.write_qplib(path)
            with jaos.Model() as back:
                back.read_qplib(path)
                self.assertTrue(back.col_integer(0))
                self.assertIs(back.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertEqual(back.objective(), 2.0)
            p.write_osil(os.path.join(d, "p.osil"))
            self.assertTrue(os.path.exists(os.path.join(d, "p.osil")))
            with jaos.Model() as back:
                back.read_osil(os.path.join(d, "p.osil"))
                self.assertTrue(back.col_integer(0))
                self.assertIs(back.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertEqual(back.objective(), 2.0)

    def test_a_paired_q_reaches_python_and_solves(self):
        with jaos.Model() as m:
            m.load(2, 1, [-3.0, -3.0], [0.0, 0.0], [3.0, 3.0],
                   [-float("inf")], [4.0], [0, 1, 2], [0, 0], [1.0, 1.0])
            m.set_quadratic([(0, 0, 2.0), (1, 1, 2.0), (1, 0, 1.0)])
            self.assertEqual(m.quadratic_nz(), 3)
            back = m.quadratic()
            self.assertEqual(len(back), 3)
            for r, c, _ in back:
                self.assertGreaterEqual(r, c)
            self.assertIn((1, 0, 1.0), back)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), -3.0, places=5)

            with self.assertRaises(jaos.JaosError):
                m.set_quadratic([(0, 1, 1.0), (1, 0, 1.0)])
            m.set_quadratic([])
            self.assertEqual(m.quadratic_nz(), 0)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), -12.0, places=6)

    def test_what_a_quadratic_objective_is_refused_for_at_both_layers(self):
        with jaos.Model() as m, tempfile.TemporaryDirectory() as d:
            m.load(2, 1, [-3.0, -3.0], [0.0, 0.0], [3.0, 3.0],
                   [-float("inf")], [4.0], [0, 1, 2], [0, 0], [1.0, 1.0])
            m.set_quadratic([(0, 0, 2.0), (1, 1, 2.0), (1, 0, 1.0)])
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            path = os.path.join(d, "q.nl")
            with self.assertRaises(jaos.JaosError) as ctx:
                m.write_nl(path)
            self.assertIn("quadratic", str(ctx.exception))
            self.assertFalse(os.path.exists(path))
            with self.assertRaises(jaos.JaosError) as ctx:
                m.cost_ranging()
            self.assertIn("quadratic", str(ctx.exception))
            with self.assertRaises(jaos.JaosError) as ctx:
                m.verify()
            self.assertIn("quadratic", str(ctx.exception))

        p = jaos.Problem()
        x = p.add_var(lb=0, ub=3, name="x")
        y = p.add_var(lb=0, ub=3, name="y")
        p.add(x + y <= 4, name="cap")
        p.minimize(-3 * x - 3 * y + x * x + y * y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "q.nl")
            with self.assertRaises(jaos.JaosError) as ctx:
                p.write_nl(path)
            self.assertIn("quadratic", str(ctx.exception))
            self.assertFalse(os.path.exists(path))
        with self.assertRaises(jaos.JaosError) as ctx:
            p.cost_ranging()
        self.assertIn("quadratic", str(ctx.exception))
        with self.assertRaises(jaos.JaosError) as ctx:
            p.verify()
        self.assertIn("quadratic", str(ctx.exception))

    def test_a_mip_is_refused_by_ranging_and_by_the_proof_at_both_layers(self):
        with jaos.Model() as m:
            m.load(2, 2, [5.0, 4.0], [0.0, 0.0],
                   [float("inf"), float("inf")],
                   [-float("inf"), -float("inf")], [24.0, 6.0],
                   [0, 2, 4], [0, 1, 0, 1], [6.0, 1.0, 4.0, 2.0],
                   sense=jaos.ObjSense.MAXIMIZE)
            m.set_col_integer(0, True)
            m.set_col_integer(1, True)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            with self.assertRaises(jaos.JaosError) as ctx:
                m.cost_ranging()
            self.assertIn("MIP", str(ctx.exception))
            with self.assertRaises(jaos.JaosError) as ctx:
                m.verify()
            self.assertIn("MIP", str(ctx.exception))

        p = jaos.Problem()
        x = p.add_var(lb=0, name="x", integer=True)
        y = p.add_var(lb=0, name="y", integer=True)
        p.add(6 * x + 4 * y <= 24, name="a")
        p.add(x + 2 * y <= 6, name="b")
        p.maximize(5 * x + 4 * y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        with self.assertRaises(jaos.JaosError) as ctx:
            p.cost_ranging()
        self.assertIn("MIP", str(ctx.exception))
        with self.assertRaises(jaos.JaosError) as ctx:
            p.verify()
        self.assertIn("MIP", str(ctx.exception))

    def test_a_written_nl_reads_back_with_its_names_and_integers_last(self):
        with jaos.Model() as m, tempfile.TemporaryDirectory() as d:
            m.read_nl(data("t_lin.nl"))
            path = os.path.join(d, "out.nl")
            m.write_nl(path)
            self.assertTrue(os.path.exists(os.path.join(d, "out.col")))
            self.assertTrue(os.path.exists(os.path.join(d, "out.row")))
            with jaos.Model() as back:
                back.read_nl(path)
                self.assertEqual((back.num_col, back.num_row, back.num_nz),
                                 (3, 3, 6))
                self.assertEqual(back.col_name(1), "z")
                self.assertEqual(back.row_name(1), "c2")
                self.assertEqual(back.objective_name, "obj")
                self.assertTrue(back.col_integer(2))
                self.assertIs(back.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertAlmostEqual(back.objective(), -4.0, places=9)
        p = jaos.Problem()
        n = p.add_var(lb=0, ub=5, name="n", integer=True)
        x = p.add_var(lb=0, ub=4, name="x")
        p.add(n + x <= 6, name="cap")
        p.maximize(2 * n + x)
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "p.nl")
            p.write_nl(path)
            with jaos.Model() as back:
                back.read_nl(path)
                self.assertEqual(back.col_name(0), "x")
                self.assertEqual(back.col_name(1), "n")
                self.assertTrue(back.col_integer(1))
                self.assertFalse(back.col_integer(0))
                self.assertEqual(back.row_name(0), "cap")
                self.assertIs(back.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertAlmostEqual(back.objective(), 11.0, places=9)

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
                       col_cost=[1.0],
                       col_lower=[0.0, 0.0], col_upper=[1.0, 1.0],
                       row_lower=[0.0], row_upper=[1.0])

    def test_a_closed_model_refuses_rather_than_crashing(self):
        m = jaos.Model()
        m.read_mps(data("t1.mps"))
        m.close()
        m.close()
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

            m.set_coefficient(0, 0, 2.0)
            m.solve()
            self.assertAlmostEqual(m.objective(), -5.0, places=9)

    def test_the_sense_and_constant_read_back_and_flip(self):
        """max x + 10 with x <= 3 is 13; the same model as a minimum is 10,
        and with the constant taken away, 0."""
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[1.0], col_lower=[0.0], col_upper=[3.0],
                   row_lower=[-jaos.INFINITY], row_upper=[10.0],
                   a_start=[0, 1], a_index=[0], a_value=[1.0],
                   sense=jaos.ObjSense.MAXIMIZE, obj_offset=10.0)
            self.assertIs(m.sense, jaos.ObjSense.MAXIMIZE)
            self.assertEqual(m.obj_offset, 10.0)
            m.solve()
            self.assertAlmostEqual(m.objective(), 13.0, places=9)

            m.set_sense(jaos.ObjSense.MINIMIZE)
            self.assertIs(m.sense, jaos.ObjSense.MINIMIZE)
            self.assertIs(m.status, jaos.SolveStatus.NOT_RUN)
            m.solve()
            self.assertAlmostEqual(m.objective(), 10.0, places=9)

            m.set_obj_offset(0.0)
            self.assertEqual(m.obj_offset, 0.0)
            m.solve()
            self.assertAlmostEqual(m.objective(), 0.0, places=9)

            with self.assertRaises(jaos.JaosError):
                m.set_obj_offset(float("nan"))
            with self.assertRaises(ValueError):
                m.set_sense(7)

    def test_the_matrix_reads_back(self):
        """A = [1 0 2; 0 3 4], given with column 2 unsorted and an explicit
        zero in column 0 that the load drops -- the C suite's example."""
        with jaos.Model() as m:
            m.load(num_col=3, num_row=2,
                   col_cost=[1.0, 1.0, 1.0],
                   col_lower=[0.0, 0.0, 0.0], col_upper=[10.0, 10.0, 10.0],
                   row_lower=[0.0, 0.0], row_upper=[5.0, 5.0],
                   a_start=[0, 2, 3, 5], a_index=[0, 1, 1, 1, 0],
                   a_value=[1.0, 0.0, 3.0, 4.0, 2.0])
            self.assertEqual(m.num_nz, 4)
            self.assertEqual(m.col_entries(0), ([0], [1.0]))
            self.assertEqual(m.col_entries(1), ([1], [3.0]))
            self.assertEqual(m.col_entries(2), ([0, 1], [2.0, 4.0]))
            self.assertEqual(m.row_entries(0), ([0, 2], [1.0, 2.0]))
            self.assertEqual(m.row_entries(1), ([1, 2], [3.0, 4.0]))
            self.assertEqual(m.coefficient(1, 2), 4.0)
            self.assertEqual(m.coefficient(1, 0), 0.0)
            self.assertEqual(m.coefficient(0, 1), 0.0)

            m.set_coefficient(1, 0, 7.0)
            self.assertEqual(m.row_entries(1), ([0, 1, 2], [7.0, 3.0, 4.0]))
            m.set_coefficient(0, 0, 0.0)
            m.set_coefficient(1, 0, 0.0)
            self.assertEqual(m.col_entries(0), ([], []))

            with self.assertRaises(jaos.JaosError):
                m.col_entries(3)
            with self.assertRaises(jaos.JaosError):
                m.coefficient(2, 0)

class TestNames(unittest.TestCase):
    """Names (D284): the file's, or one set here, or the position."""

    def test_a_file_s_names_come_through(self):
        with jaos.Model() as m:
            m.read_mps(data("t1.mps"))
            self.assertEqual([m.col_name(j) for j in range(3)],
                             ["X1", "X2", "X3"])
            self.assertEqual([m.row_name(i) for i in range(3)],
                             ["LIM1", "LIM2", "EQ1"])
            self.assertEqual(m.objective_name, "COST")
            self.assertEqual(m.col_index("X2"), 1)
            self.assertEqual(m.row_index("EQ1"), 2)
            with self.assertRaises(jaos.JaosError):
                m.row_index("COST")

    def test_a_name_set_here_reads_back_and_the_rest_are_positional(self):
        with jaos.Model() as m:
            m.load(num_col=2, num_row=1,
                   col_cost=[-1.0, -2.0],
                   col_lower=[0.0, 0.0],
                   col_upper=[jaos.INFINITY, jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[4.0],
                   a_start=[0, 1, 2], a_index=[0, 0],
                   a_value=[1.0, 1.0])
            self.assertEqual(m.col_name(0), "C1")
            self.assertEqual(m.row_name(0), "R1")
            m.set_col_name(1, "y")
            m.set_row_name(0, "cap")
            m.set_objective_name("profit")
            self.assertEqual(m.col_name(1), "y")
            self.assertEqual(m.col_name(0), "C1")
            self.assertEqual(m.row_name(0), "cap")
            self.assertEqual(m.objective_name, "profit")
            self.assertEqual(m.col_index("y"), 1)
            self.assertEqual(m.col_index("C1"), 0)
            self.assertEqual(m.row_index("cap"), 0)
            with self.assertRaises(jaos.JaosError):
                m.col_index("C2")
            m.set_col_name(1, None)
            self.assertEqual(m.col_name(1), "C2")
            with self.assertRaises(jaos.JaosError):
                m.set_col_name(0, "a b")
            with self.assertRaises(jaos.JaosError):
                m.set_col_name(0, "x" * (jaos.NAME_MAX + 1))
            m.set_col_name(0, "x" * jaos.NAME_MAX)
            self.assertEqual(len(m.col_name(0)), jaos.NAME_MAX)

            m.solve()
            m.set_row_name(0, "capacity")
            self.assertIs(m.status, jaos.SolveStatus.OPTIMAL)

    def test_the_model_name_and_a_copy(self):
        with jaos.Model() as m:
            m.read_mps(data("t1.mps"))
            self.assertEqual(m.name, "T1")
            m.name = "renamed"
            self.assertEqual(m.name, "renamed")
            with self.assertRaises(jaos.JaosError):
                m.name = "two words"
            m.solve()
            c = m.copy()
            with c:
                self.assertEqual(c.name, "renamed")
                self.assertEqual(c.col_name(2), "X3")
                self.assertEqual((c.num_col, c.num_row, c.num_nz),
                                 (m.num_col, m.num_row, m.num_nz))
                self.assertIs(c.status, jaos.SolveStatus.NOT_RUN)
                self.assertIs(c.solve(), m.status)
                c.set_col_name(0, "mine")
                self.assertEqual(m.col_name(0), "X1")

    def test_a_problem_s_names_reach_the_library(self):
        with tempfile.TemporaryDirectory() as d:
            p = jaos.Problem()
            x = p.add_var(ub=4, name="apples")
            y = p.add_var()
            p.add(x + y <= 4, name="crate")
            p.maximize(x + 2 * y)
            p.solve()
            self.assertEqual(p._m.col_name(0), "apples")
            self.assertEqual(p._m.col_name(1), "x1")
            self.assertEqual(p._m.row_name(0), "crate")
            path = os.path.join(d, "p.lp")
            p.write_lp(path)
            with open(path) as f:
                text = f.read()
            self.assertIn("apples", text)
            self.assertIn("crate:", text)

    def test_names_round_trip_through_a_file(self):
        with jaos.Model() as m, tempfile.TemporaryDirectory() as d:
            m.read_lp(data("g1.lp"))
            self.assertEqual(m.objective_name, "obj")
            self.assertEqual(m.row_name(1), "c2")
            m.set_row_name(1, "second")
            path = os.path.join(d, "named.mps")
            m.write_mps(path)
            with jaos.Model() as back:
                back.read_mps(path)
                self.assertEqual(back.row_name(1), "second")
                self.assertEqual(back.col_name(2), "z")
                self.assertEqual(back.objective_name, "obj")

            m.set_col_name(0, "z")
            with self.assertRaises(jaos.JaosError) as cm:
                m.write_lp(os.path.join(d, "dup.lp"))
            self.assertIn("'z'", str(cm.exception))

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

            m.add_rows([-jaos.INFINITY], [3.0],
                       a_start=[0, 1], a_index=[1], a_value=[1.0])
            self.assertEqual((m.num_row, m.num_nz), (2, 3))
            m.solve()
            self.assertAlmostEqual(m.objective(), -7.0, places=9)

            m.delete_rows([1])
            self.assertEqual(m.num_row, 1)
            m.solve()
            self.assertAlmostEqual(m.objective(), -8.0, places=9)

    def test_adding_a_column_improves_the_optimum(self):
        with self.golden() as m:
            m.solve()
            m.add_cols([-3.0], [0.0], [2.0],
                       a_start=[0, 1], a_index=[0], a_value=[1.0])
            self.assertEqual((m.num_col, m.num_nz), (3, 3))
            m.solve()
            self.assertAlmostEqual(m.objective(), -10.0, places=9)

    def test_deleting_a_column_removes_its_contribution(self):
        with self.golden() as m:
            m.delete_cols([1])
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

class TestIIS(unittest.TestCase):
    """jaos_iis through the binding: the two-row model tests/test_iis.c
    works by hand, on Model and on Problem, and the refusals."""

    def test_two_rows_and_not_the_column_bound(self):
        with jaos.Model() as m:
            m.load(num_col=1, num_row=2,
                   col_cost=[1.0], col_lower=[0.0], col_upper=[jaos.INFINITY],
                   row_lower=[1.0, -jaos.INFINITY],
                   row_upper=[jaos.INFINITY, 0.0],
                   a_start=[0, 2], a_index=[0, 1], a_value=[1.0, 1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.INFEASIBLE)
            found = m.iis()
            self.assertEqual(found.row_side,
                             [jaos.IISSide.LOWER, jaos.IISSide.UPPER])
            self.assertEqual(found.col_side, [jaos.IISSide.NONE])
            self.assertEqual(found.report.members, 2)
            self.assertEqual(found.report.solves, 3)
            self.assertTrue(found.report.from_certificate)

            self.assertIs(m.status, jaos.SolveStatus.INFEASIBLE)
            self.assertEqual(len(m.certificate()), 2)

    def test_a_feasible_model_has_none(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            with self.assertRaises(jaos.JaosError):
                m.iis()

    def test_the_layer_names_constraints_and_variables(self):
        p = jaos.Problem()
        x = p.add_var(ub=0.0)
        y = p.add_var()
        c0 = p.add(x + y >= 1)
        c1 = p.add(y <= 0)
        p.minimize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        found = p.iis()
        self.assertEqual([(c, s) for c, s in found.row_side],
                         [(c0, jaos.IISSide.LOWER), (c1, jaos.IISSide.UPPER)])
        self.assertEqual(found.col_side, [(x, jaos.IISSide.UPPER)])
        self.assertEqual(found.report.members, 3)
        p.add(x >= -1)
        with self.assertRaises(ValueError):
            p.iis()

    def test_the_subsystem_comes_back_as_a_model(self):
        """D343: and the check that settles it is that the model is
        infeasible. The control is one member dropped, which makes it
        feasible."""
        with jaos.Model() as m:
            m.load(num_col=1, num_row=2,
                   col_cost=[1.0], col_lower=[0.0], col_upper=[jaos.INFINITY],
                   row_lower=[1.0, -jaos.INFINITY],
                   row_upper=[jaos.INFINITY, 0.0],
                   a_start=[0, 2], a_index=[0, 1], a_value=[1.0, 1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.INFEASIBLE)
            found = m.iis()
            with m.iis_model(found) as sub:
                self.assertIs(sub.solve(), jaos.SolveStatus.INFEASIBLE)
                self.assertEqual(sub.num_row, 2)
                self.assertEqual(sub.num_col, 1)

            sides = list(found.row_side)
            sides[0] = jaos.IISSide.NONE
            with m.iis_model((sides, found.col_side)) as loose:
                self.assertIs(loose.solve(), jaos.SolveStatus.OPTIMAL)

            with self.assertRaises(ValueError):
                m.iis_model(([jaos.IISSide.NONE], found.col_side))

    def test_the_layer_reaches_the_subsystem_too(self):
        p = jaos.Problem()
        x = p.add_var(ub=0.0)
        y = p.add_var()
        p.add(x + y >= 1)
        p.add(y <= 0)
        p.minimize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        with p.iis_model() as sub:
            self.assertIs(sub.solve(), jaos.SolveStatus.INFEASIBLE)

class TestVerify(unittest.TestCase):
    """jaos_verify through the binding.

    The first test is the one that matters and it is not about Python. A
    ctypes Structure that does not match the C layout reads adjacent fields
    as its own and every assertion after it is about the wrong bytes.
    `capacity_bits` is the only field whose exact value is known from
    outside -- 32 * JM_EXACT_LIMBS, 4096 at the shipping setting -- so it is
    what pins the layout."""

    def test_the_struct_matches_the_c_layout(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            r = m.verify()

            self.assertEqual(r.capacity_bits, 4096.0,
                             "the report's field order does not match "
                             "jaos_verify_report in jaos.h")
            self.assertGreaterEqual(r.bound_bits, 0.0)
            self.assertGreaterEqual(r.blocks, 1)
            self.assertGreaterEqual(r.largest_block, 1)
            self.assertLessEqual(r.largest_block, m.num_row)

    def test_a_small_optimum_is_proved(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            r = m.verify()
            self.assertIs(r.status, jaos.Proof.OPTIMAL)
            self.assertIs(r.stage, jaos.ProofStage.NONE)
            self.assertEqual(r.at_row, -1)
            self.assertEqual(r.at_col, -1)
            self.assertEqual(r.violation, 0.0)

    def test_it_needs_an_optimum(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            with self.assertRaises(jaos.JaosError):
                m.verify()

    def test_the_layer_reaches_it_and_refuses_a_stale_answer(self):
        p = jaos.Problem()
        x = p.add_var(ub=3.0)
        y = p.add_var()
        c = p.add(x + y <= 4)
        p.minimize(-x - y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        r = p.verify()
        self.assertIs(r.status, jaos.Proof.OPTIMAL)
        c.ub = 3.0
        with self.assertRaises(ValueError):
            p.verify()

    def test_a_basis_from_outside_is_proved_with_no_solve(self):
        """D339. Both ways round: the basis of the optimum proves, and the
        slack basis, which is legal and not optimal, is broken. Without the
        second the first would pass on a prover that proved everything."""
        B = jaos.BasisStatus
        with jaos.Model() as m:

            m.load(2, 1, [-1.0, -1.0], [0.0, 0.0], [3.0, float("inf")],
                   [-float("inf")], [4.0], [0, 1, 2], [0, 0], [1.0, 1.0])
            r = m.verify_basis([B.AT_UPPER, B.BASIC], [B.AT_UPPER])
            self.assertIs(r.status, jaos.Proof.OPTIMAL)

            self.assertIs(m.status, jaos.SolveStatus.NOT_RUN)
            self.assertEqual(str(m.exact_col_value(0)), "3")
            self.assertEqual(str(m.exact_objective()), "-4")

            r = m.verify_basis([B.AT_LOWER, B.AT_LOWER], [B.BASIC])
            self.assertIs(r.status, jaos.Proof.BROKEN)
            self.assertIs(r.stage, jaos.ProofStage.DUAL)

            with self.assertRaises(jaos.JaosError):
                m.verify_basis([B.BASIC, B.BASIC], [B.BASIC])
            with self.assertRaises(ValueError):
                m.verify_basis([B.BASIC], [B.AT_UPPER])

    def test_the_layer_verifies_a_basis_too(self):
        B = jaos.BasisStatus
        p = jaos.Problem()
        x = p.add_var(ub=3.0)
        y = p.add_var()
        p.add(x + y <= 4)
        p.minimize(-x - y)
        r = p.verify_basis([B.AT_UPPER, B.BASIC], [B.AT_UPPER])
        self.assertIs(r.status, jaos.Proof.OPTIMAL)

class TestProgressCallback(unittest.TestCase):
    def test_the_callback_sees_the_solve(self):
        seen = []
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.set_progress_callback(lambda p: seen.append(p))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)

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
        z = p.add_var(ub=2)
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
        p.solve()
        fresh, *_ = self.build(x_lb=1.0)
        fresh.solve()
        self.assertAlmostEqual(p.objective_value, fresh.objective_value,
                               places=9)
        self.assertAlmostEqual(p.objective_value, -7.0, places=9)

    def test_a_new_objective_goes_through_the_cost_setter(self):
        p, x, y, c = self.build()
        p.solve()
        p.minimize(-3 * x - 2 * y)
        p.solve()
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

    def test_flipping_the_sense_keeps_the_loaded_model(self):
        """Until D283 a new sense rebuilt the model and the next solve ran
        cold. Now it goes through the setter: the Model object is the same
        one, nothing is marked structural, and the answer agrees with a
        fresh build of the flipped problem."""
        p, x, y, c = self.build()
        p.solve()
        loaded = p._m
        p.maximize(-x - 2 * y)
        self.assertFalse(p._structural)
        self.assertTrue(p._dirty_objective)
        with self.assertRaises(ValueError):
            p.objective_value
        p.solve()
        self.assertIs(p._m, loaded)
        self.assertAlmostEqual(p.objective_value, 0.0, places=9)

        fresh = jaos.Problem()
        fx = fresh.add_var()
        fy = fresh.add_var()
        fresh.add(fx + fy <= 4.0)
        fresh.maximize(-fx - 2 * fy)
        fresh.solve()
        self.assertAlmostEqual(p.objective_value, fresh.objective_value,
                               places=9)

    def test_a_new_constant_goes_through_the_setter(self):
        p, x, y, c = self.build()
        p.solve()
        p.minimize(-x - 2 * y + 100)
        self.assertFalse(p._structural)
        p.solve()
        self.assertAlmostEqual(p.objective_value, 92.0, places=9)
        self.assertEqual(p._m.obj_offset, 100.0)

class TestBranchAndBound(unittest.TestCase):
    """Integer columns through both layers (D288). The knapsack is the one
    tests/test_mip.c solves to 9 against a relaxation of 10.67."""

    def test_a_run_of_changes_agrees_with_a_fresh_build(self):
        """The cases above move one thing each. This one interleaves the
        two paths, since a change that rebuilds and a change that goes
        through the setters have to leave the same model behind whichever
        order they arrive in."""
        p = jaos.Problem()
        xs = [p.add_var(ub=4.0), p.add_var(ub=3.0),
              p.add_var(ub=2.0, integer=True)]
        p.add(xs[0] + xs[1] + xs[2] <= 5)
        p.minimize(-xs[0] - 2 * xs[1] - 3 * xs[2])
        p.solve()

        xs[0].ub = 2.0
        p.solve()
        p.add(2 * xs[0] + xs[1] <= 3)
        p.solve()
        p.maximize(xs[0] + 2 * xs[1] + 3 * xs[2])
        p.solve()
        xs[1].lb = 1.0
        p.solve()
        xs.append(p.add_var(ub=1.0))
        p.maximize(xs[0] + 2 * xs[1] + 3 * xs[2] + 5 * xs[3])
        p.solve()

        got = p.objective_value
        self.assertEqual(p.status, jaos.SolveStatus.OPTIMAL)
        p.close()

        fresh = jaos.Problem()
        ys = [fresh.add_var(ub=2.0), fresh.add_var(lb=1.0, ub=3.0),
              fresh.add_var(ub=2.0, integer=True), fresh.add_var(ub=1.0)]
        fresh.add(ys[0] + ys[1] + ys[2] <= 5)
        fresh.add(2 * ys[0] + ys[1] <= 3)
        fresh.maximize(ys[0] + 2 * ys[1] + 3 * ys[2] + 5 * ys[3])
        fresh.solve()
        self.assertEqual(fresh.status, jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(got, fresh.objective_value, places=9)
        fresh.close()

    def test_the_knapsack_through_the_problem_layer(self):
        p = jaos.Problem()
        a = p.add_var(binary=True, name="a")
        b = p.add_var(binary=True, name="b")
        c = p.add_var(binary=True, name="c")
        p.add(2 * a + 3 * b + c <= 5)
        p.maximize(5 * a + 4 * b + 3 * c)
        p.set_mip_tighten(0)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 9.0, places=9)
        self.assertEqual((a.value, b.value, c.value), (1.0, 1.0, 0.0))
        rep = p.mip_report()
        self.assertTrue(rep.has_incumbent)
        obj, point = p.mip_incumbent()
        self.assertAlmostEqual(obj, 9.0, places=9)
        self.assertEqual((point[a], point[b], point[c]), (1.0, 1.0, 0.0))

        self.assertGreaterEqual(rep.cuts, 1)
        self.assertEqual(rep.nodes, 1)
        self.assertTrue(p._m.col_integer(0))
        p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_dive(True).set_mip_heuristics(False)
        p.set_mip_dive_heuristic(0).set_mip_feaspump(0)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 9.0, places=9)
        rep = p.mip_report()
        self.assertEqual(rep.cuts, 0)
        self.assertEqual(rep.heuristic_points, 0)
        self.assertGreaterEqual(rep.nodes, 2)

    def test_a_node_limit_stops_with_the_incumbent_and_the_callback_sees_it(self):

        seen = []
        p = jaos.Problem()
        x = p.add_var(integer=True, name="x")
        y = p.add_var(integer=True, name="y")
        p.add(x + y <= 3.6)
        p.add(x <= 2.2)
        p.add(y <= 1.4)
        p.maximize(x + y)
        p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_node_limit(1)
        p.set_incumbent_callback(lambda inc: seen.append(inc))
        self.assertIs(p.solve(), jaos.SolveStatus.NODE_LIMIT)
        self.assertEqual(len(seen), 1)
        self.assertEqual(seen[0].node, 1)
        self.assertTrue(seen[0].by_rounding)
        self.assertEqual((seen[0].values[x], seen[0].values[y]), (2.0, 1.0))
        obj, point = p.mip_incumbent()
        self.assertAlmostEqual(obj, 3.0, places=9)

        p.set_mip_node_limit(0)
        p.set_incumbent_callback(lambda inc: jaos.CallbackAction.STOP)
        self.assertIs(p.solve(), jaos.SolveStatus.INTERRUPTED)
        self.assertTrue(p.mip_report().has_incumbent)

    def test_the_node_callback_adds_lazy_rows_and_user_cuts_and_steers(self):
        p = jaos.Problem()
        x = p.add_var(binary=True, name="x")
        y = p.add_var(binary=True, name="y")
        z = p.add_var(binary=True, name="z")
        p.add(x + y + z <= 2)
        p.maximize(2 * x + 2 * y + z)
        seen = []

        def lazy(ev):
            seen.append((ev.node, ev.integral))
            if ev.integral and ev.values[x] + ev.values[y] > 1.5:
                ev.add(x + y <= 1)
        p.set_node_callback(lazy)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 3.0, places=9)
        self.assertLessEqual(x.value + y.value, 1.0 + 1e-9)
        self.assertTrue(any(integral for _, integral in seen))

        p.set_node_callback(None)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 4.0, places=9)

        q = jaos.Problem()
        a = q.add_var(binary=True, name="a")
        b = q.add_var(binary=True, name="b")
        c = q.add_var(binary=True, name="c")
        q.add(2 * a + 2 * b + 2 * c <= 3)
        q.maximize(3 * a + 2.5 * b + 2 * c)
        q.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
        q.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_flow_cover_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(False)
        q.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_tighten(0)
        self.assertIs(q.solve(), jaos.SolveStatus.OPTIMAL)
        plain = q.mip_report().nodes
        self.assertGreater(plain, 1)
        choices = []

        def cut(ev):
            if ev.node == 1 and not ev.integral:
                choices.append(ev.branch_var)
                ev.add(a + b + c <= 1)
        q.set_node_callback(cut)
        self.assertIs(q.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(q.objective_value, 3.0, places=9)
        self.assertEqual(q.mip_report().nodes, 1)
        self.assertIs(choices[0], b)

        s = jaos.Problem()
        a = s.add_var(binary=True, name="a")
        b = s.add_var(binary=True, name="b")
        c = s.add_var(binary=True, name="c")
        s.add(2 * a + 2 * b <= 3)
        s.add(2 * a + 2 * c <= 3)
        s.maximize(3 * a + b + c)
        s.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
        s.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_flow_cover_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(False)
        s.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_tighten(0)
        depth1 = []
        steered = []

        def steer(ev):
            if ev.depth == 0 and not ev.integral:
                other = c if ev.branch_var is b else b
                steered.append(other)
                ev.branch_on(other)
            if ev.depth == 1:
                depth1.append(ev.values[steered[0]])
        s.set_node_callback(steer)
        self.assertIs(s.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(s.objective_value, 3.0, places=9)
        self.assertTrue(depth1)
        self.assertTrue(all(v in (0.0, 1.0) for v in depth1))

        def bad(ev):
            ev.add_row([7], [1.0], 0.0, 1.0)
        q._m.set_node_callback(bad)
        hook = sys.excepthook
        sys.excepthook = lambda *args: None
        try:
            self.assertIs(q.solve(), jaos.SolveStatus.INTERRUPTED)
        finally:
            sys.excepthook = hook
        q.set_node_callback(lambda ev: jaos.CallbackAction.STOP)
        self.assertIs(q.solve(), jaos.SolveStatus.INTERRUPTED)

    def test_an_indicator_row_holds_only_while_its_variable_says_so(self):
        p = jaos.Problem()
        x = p.add_var(ub=10, name="x")
        z = p.add_var(binary=True, name="z")
        p.maximize(x - 3 * z)
        c = p.add_indicator(z, 1, x <= 2, name="c1")
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 10.0, places=9)
        self.assertEqual((x.value, z.value), (10.0, 0.0))
        self.assertEqual(p._m.row_indicator(c._i), (z._i, 1))
        q = jaos.Problem()
        x = q.add_var(ub=10, name="x")
        z = q.add_var(binary=True, name="z")
        q.maximize(x - 3 * z)
        q.add_indicator(z, 0, x <= 2)
        self.assertIs(q.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(q.objective_value, 7.0, places=9)
        self.assertEqual((x.value, z.value), (10.0, 1.0))

    def test_special_ordered_sets_limit_the_nonzero_members(self):
        for sos_type, want in ((1, 1.0), (2, 2.0)):
            p = jaos.Problem()
            xs = [p.add_var(ub=1, name=f"x{k}") for k in range(3)]
            p.add(xs[0] + xs[1] + xs[2] <= 10)
            p.maximize(xs[0] + xs[1] + xs[2])
            p.add_sos(sos_type, xs)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, want, places=9)
            nz = [k for k, v in enumerate(xs) if abs(v.value) > 1e-9]
            self.assertEqual(len(nz), int(want))
            if sos_type == 2:
                self.assertEqual(nz[1] - nz[0], 1)
            self.assertEqual(p._m.num_sos(), 1)
            t, cols, ws = p._m.sos(0)
            self.assertEqual((t, cols, ws), (sos_type, [0, 1, 2],
                                             [1.0, 2.0, 3.0]))
        with self.assertRaises(jaos.JaosError):
            p._m.add_sos(1, [0, 0], [1.0, 2.0])

    def test_a_semicontinuous_variable_rests_at_zero_or_above_its_floor(self):
        p = jaos.Problem()
        x = p.add_var(lb=2, ub=10, name="x", semicontinuous=True)
        y = p.add_var(ub=1, name="y")
        p.add(x + y >= 1)
        p.minimize(x + 5 * y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 2.0, places=9)
        self.assertEqual((x.value, y.value), (2.0, 0.0))
        p.minimize(10 * x + 5 * y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 5.0, places=9)
        self.assertEqual((x.value, y.value), (0.0, 1.0))
        m = p._m
        self.assertTrue(m.col_semicontinuous(0))
        self.assertFalse(m.col_semicontinuous(1))
        self.assertFalse(m.col_integer(0))
        m.set_col_semicontinuous(0, False)
        self.assertFalse(m.col_semicontinuous(0))

    def test_clique_cuts_close_a_pairwise_conflict_at_the_root(self):
        for rounds, want_nodes in ((1, 1), (0, None)):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(3)]
            p.add(xs[0] + xs[1] <= 1)
            p.add(xs[1] + xs[2] <= 1)
            p.add(xs[0] + xs[2] <= 1)
            p.maximize(xs[0] + xs[1] + xs[2])
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0)
            p.set_mip_clique_rounds(rounds)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 1.0, places=9)
            rep = p.mip_report()
            if want_nodes is not None:
                self.assertEqual(rep.nodes, want_nodes)
                self.assertGreaterEqual(rep.cuts, 1)
            else:
                self.assertGreater(rep.nodes, 1)

    def test_statistics_count_the_new_kinds(self):
        p = jaos.Problem()
        x = p.add_var(lb=2, ub=10, semicontinuous=True, name="x")
        z = p.add_var(binary=True, name="z")
        y = p.add_var(ub=1, name="y")
        p.add_indicator(z, 1, x + y <= 5)
        p.add_sos(1, [x, y])
        p.minimize(x + y + z)
        st = p.statistics()
        self.assertEqual((st.semicontinuous_col, st.sos_set, st.indicator_row),
                         (1, 1, 1))
        self.assertEqual(st.integer_col, 1)

    def test_the_module_runs_as_a_command(self):
        import os
        import subprocess
        import sys
        here = os.path.dirname(os.path.abspath(__file__))
        root = os.path.dirname(here)
        env = dict(os.environ)
        env["PYTHONPATH"] = here
        env["JAOS_LIBRARY"] = jaos.library_path()
        run = lambda *a: subprocess.run(
            [sys.executable, "-m", "jaos"] + list(a), cwd=root, env=env,
            capture_output=True, text=True)
        r = run("solve", os.path.join("tests", "data", "solve1.mps"),
                "--opt", "mip_cut_rounds=0")
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("status optimal", r.stdout)
        self.assertIn("objective ", r.stdout)
        r = run("solve", os.path.join("tests", "data", "g_int.lp"))
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("objective 2", r.stdout)
        self.assertEqual(run("frobnicate").returncode, 5)
        self.assertEqual(run("solve").returncode, 5)
        self.assertEqual(run("solve", "no_such.mps").returncode, 5)
        r = run("version")
        self.assertEqual(r.returncode, 0)
        self.assertEqual(r.stdout.strip(), jaos.version())

    def test_options_by_name_round_trip(self):
        p = jaos.Problem()
        x = p.add_var(ub=4, name="x")
        p.add(x >= 1)
        p.minimize(x)
        self.assertEqual(p.get_option("algorithm"), "dual")
        p.set_option("algorithm", "primal").set_option("mip_cut_rounds", 3)
        p.set_option("mip_heuristics", False)
        self.assertEqual(p.get_option("algorithm"), "primal")
        self.assertEqual(p.get_option("mip_cut_rounds"), "3")
        self.assertEqual(p.get_option("mip_heuristics"), "false")
        self.assertIs(p.algorithm, jaos.Algorithm.PRIMAL)
        with self.assertRaises(jaos.JaosError):
            p.set_option("no_such_option", 1)
        with self.assertRaises(jaos.JaosError):
            p.set_option("mip_cut_rounds", "three")
        names = jaos.Model.option_names()
        self.assertIn("mip_gap", names)
        self.assertGreater(len(names), 40)
        for name in names:
            v = p.get_option(name)
            p.set_option(name, v)
            self.assertEqual(p.get_option(name), v)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 1.0, places=12)

    def test_the_algorithm_is_a_caller_option(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=5, name="x")
        y = p.add_var(lb=0, ub=5, name="y")
        p.add(x + y >= 2)
        p.minimize(x + 2 * y)
        self.assertIs(p.algorithm, jaos.Algorithm.DUAL)
        for alg in (jaos.Algorithm.PRIMAL, jaos.Algorithm.DUAL):
            p.set_algorithm(alg)
            self.assertIs(p.algorithm, alg)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 2.0, places=12)
            self.assertEqual((x.value, y.value), (2.0, 0.0))
        with self.assertRaises(jaos.JaosError):
            p.set_algorithm(9)
        m = jaos.Model()
        self.assertIs(m.algorithm, jaos.Algorithm.DUAL)
        m.set_algorithm(jaos.Algorithm.PRIMAL)
        self.assertIs(m.algorithm, jaos.Algorithm.PRIMAL)

    def test_the_barrier_reaches_the_same_optimum(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=5, name="x")
        y = p.add_var(lb=0, ub=5, name="y")
        p.add(x + y >= 2)
        p.minimize(x + 2 * y)
        p.set_algorithm(jaos.Algorithm.BARRIER)
        self.assertIs(p.algorithm, jaos.Algorithm.BARRIER)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 2.0, places=6)
        self.assertAlmostEqual(x.value, 2.0, places=5)
        self.assertAlmostEqual(y.value, 0.0, places=5)
        m = jaos.Model()
        m.set_algorithm(jaos.Algorithm.BARRIER)
        self.assertIs(m.algorithm, jaos.Algorithm.BARRIER)
        m.set_option("algorithm", "dual")
        self.assertIs(m.algorithm, jaos.Algorithm.DUAL)
        m.set_option("algorithm", "barrier")
        self.assertEqual(m.get_option("algorithm"), "barrier")

    def test_pdlp_reaches_the_same_vertex(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=5, name="x")
        y = p.add_var(lb=0, ub=5, name="y")
        p.add(x + y >= 2)
        p.minimize(x + 2 * y)
        p.set_algorithm(jaos.Algorithm.PDLP)
        self.assertIs(p.algorithm, jaos.Algorithm.PDLP)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertEqual(p.objective_value, 2.0)
        self.assertEqual(x.value, 2.0)
        self.assertEqual(y.value, 0.0)
        m = jaos.Model()
        m.set_option("algorithm", "pdlp")
        self.assertIs(m.algorithm, jaos.Algorithm.PDLP)
        self.assertEqual(m.get_option("algorithm"), "pdlp")

    def test_concurrent_reaches_the_same_vertex(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=5, name="x")
        y = p.add_var(lb=0, ub=5, name="y")
        p.add(x + y >= 2)
        p.minimize(x + 2 * y)
        p.set_algorithm(jaos.Algorithm.CONCURRENT)
        self.assertIs(p.algorithm, jaos.Algorithm.CONCURRENT)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertEqual(p.objective_value, 2.0)
        self.assertEqual((x.value, y.value), (2.0, 0.0))
        m = jaos.Model()
        m.set_option("algorithm", "concurrent")
        self.assertIs(m.algorithm, jaos.Algorithm.CONCURRENT)
        self.assertEqual(m.get_option("algorithm"), "concurrent")
        m.read_mps(data("solve1.mps"))
        self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
        conc = m.objective()
        with jaos.Model() as d:
            d.read_mps(data("solve1.mps"))
            self.assertIs(d.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertEqual(conc, d.objective())

    def test_the_barrier_says_infeasible_and_unbounded_like_the_dual(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, ub=1, name="x")
        y = p.add_var(lb=0, ub=1, name="y")
        p.add(x + y >= 3)
        p.minimize(x + y)
        p.set_algorithm(jaos.Algorithm.BARRIER)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        with jaos.Model() as m:
            m.read_mps(data("unbounded.mps"))
            m.set_algorithm(jaos.Algorithm.BARRIER)
            self.assertIs(m.solve(), jaos.SolveStatus.UNBOUNDED)

    def test_both_branching_rules_reach_the_knapsack_optimum(self):

        for rule in (jaos.Branching.MOST_FRACTIONAL, jaos.Branching.PSEUDOCOST):
            p = jaos.Problem()
            a = p.add_var(binary=True, name="a")
            b = p.add_var(binary=True, name="b")
            c = p.add_var(binary=True, name="c")
            p.add(2 * a + 3 * b + c <= 5)
            p.maximize(5 * a + 4 * b + 3 * c)
            p.set_mip_tighten(0)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_branching(rule)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 9.0, places=9)
            self.assertEqual((a.value, b.value, c.value), (1.0, 1.0, 0.0))
        with self.assertRaises(jaos.JaosError):
            p.set_mip_branching(7)

        for rel in (0, 8):
            p.set_mip_branching(jaos.Branching.PSEUDOCOST).set_mip_reliability(rel)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 9.0, places=9)
            rep = p.mip_report()
            if rel == 0:
                self.assertEqual(rep.lp_solves, rep.nodes)
            else:
                self.assertGreater(rep.lp_solves, rep.nodes)

    def test_a_probe_cap_and_every_dive_child_rule_keep_the_knapsack_optimum(self):

        def knapsack():
            p = jaos.Problem()
            a = p.add_var(binary=True, name="a")
            b = p.add_var(binary=True, name="b")
            c = p.add_var(binary=True, name="c")
            p.add(2 * a + 3 * b + c <= 5)
            p.maximize(5 * a + 4 * b + 3 * c)
            p.set_mip_tighten(0)
            return p, (a, b, c)
        for cap in (0.5, 0.0, 4.0):
            p, (a, b, c) = knapsack()
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_reliability(8).set_mip_probe_cap(cap)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 9.0, places=9)
            self.assertEqual((a.value, b.value, c.value), (1.0, 1.0, 0.0))
            self.assertGreater(p.mip_report().lp_solves, p.mip_report().nodes)
        with self.assertRaises(jaos.JaosError):
            p.set_mip_probe_cap(float("nan"))
        for rule in jaos.DiveChild:
            p, (a, b, c) = knapsack()
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_dive(True).set_mip_dive_child(rule)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 9.0, places=9)
            self.assertEqual((a.value, b.value, c.value), (1.0, 1.0, 0.0))
            self.assertGreaterEqual(p.mip_report().nodes, 2)
        with self.assertRaises(jaos.JaosError):
            p.set_mip_dive_child(7)

    def test_cuts_below_the_root_keep_the_optimum(self):

        for depth in (0, 1, 100):
            p = jaos.Problem()
            v = [p.add_var(binary=True, name=n) for n in "abcde"]
            a, b, c, d, e = v
            p.add(3 * a + 5 * b + 2 * c + 4 * d + 2 * e <= 8)
            p.maximize(10 * a + 13 * b + 7 * c + 9 * d + 5 * e)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(depth)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual([x.value for x in v], [1.0, 1.0, 0.0, 0.0, 0.0])
            rep = p.mip_report()
            self.assertGreaterEqual(rep.nodes, 2)
            if depth == 0:
                self.assertEqual(rep.cuts, 0)
        p.set_mip_cut_depth(-1)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 23.0, places=9)

    def test_the_solution_pool_and_the_two_depth_switches(self):

        def knapsack5():
            p = jaos.Problem()
            v = [p.add_var(binary=True, name=n) for n in "abcde"]
            a, b, c, d, e = v
            p.add(3 * a + 5 * b + 2 * c + 4 * d + 2 * e <= 8)
            p.maximize(10 * a + 13 * b + 7 * c + 9 * d + 5 * e)
            return p, v
        p, v = knapsack5()
        p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_pool_size(3)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        pool = p.mip_pool()
        self.assertTrue(1 <= len(pool) <= 3)
        self.assertAlmostEqual(pool[0][0], 23.0, places=9)
        self.assertEqual((pool[0][1][v[0]], pool[0][1][v[1]]), (1.0, 1.0))
        objs = [obj for obj, _ in pool]
        self.assertEqual(objs, sorted(objs, reverse=True))
        self.assertEqual(len({tuple(x[u] for u in v) for _, x in pool}), len(pool))
        with self.assertRaises(jaos.JaosError):
            p.set_mip_pool_size(0)
        p, v = knapsack5()
        p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(100).set_mip_cut_drop(False)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 23.0, places=9)
        p.set_mip_cut_drop(True).set_mip_reliability(8).set_mip_probe_depth(0)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 23.0, places=9)
        self.assertGreater(p.mip_report().lp_solves, p.mip_report().nodes)

    def test_a_cover_cut_closes_the_knapsack_at_the_root(self):

        p = jaos.Problem()
        a = p.add_var(binary=True, name="a")
        b = p.add_var(binary=True, name="b")
        c = p.add_var(binary=True, name="c")
        p.add(2 * a + 3 * b + c <= 5)
        p.maximize(5 * a + 4 * b + 3 * c)
        p.set_mip_tighten(0)
        p.set_mip_cut_rounds(0).set_mip_mir_rounds(0).set_mip_cover_rounds(1)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 9.0, places=9)
        rep = p.mip_report()
        self.assertEqual(rep.nodes, 1)
        self.assertGreaterEqual(rep.cuts, 1)
        p.set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertGreaterEqual(p.mip_report().nodes, 2)

    def test_a_cap_on_a_nodes_cuts_keeps_the_optimum(self):

        p = jaos.Problem()
        v = [p.add_var(binary=True, name=n) for n in "abcde"]
        a, b, c, d, e = v
        p.add(3 * a + 5 * b + 2 * c + 4 * d + 2 * e <= 8)
        p.maximize(10 * a + 13 * b + 7 * c + 9 * d + 5 * e)
        p.set_mip_cut_rounds(0).set_mip_cut_depth(100).set_mip_node_cut_cap(1)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 23.0, places=9)
        self.assertEqual((a.value, b.value), (1.0, 1.0))
        p.set_mip_node_cut_cap(-1)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, 23.0, places=9)

    def _knapsack5(self):
        p = jaos.Problem()
        a, b, c, d, e = [p.add_var(binary=True, name=n) for n in "abcde"]
        p.add(3 * a + 5 * b + 2 * c + 4 * d + 2 * e <= 8)
        p.maximize(10 * a + 13 * b + 7 * c + 9 * d + 5 * e)
        return p, (a, b)

    def test_a_stalled_root_round_is_the_last(self):

        for stall, cuts in ((0.0, 3), (1.0, 1)):
            p, (a, b) = self._knapsack5()
            p.set_mip_cut_rounds(5).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0)
            p.set_mip_cut_stall(stall)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
            self.assertEqual(p.mip_report().cuts, cuts)
        with self.assertRaises(jaos.JaosError):
            p.set_mip_cut_stall(float("nan"))
        p.set_mip_cut_stall(-1)

    def test_a_stalled_round_ends_the_cuts_under_it(self):

        counts = []
        for stall in (0.0, 1.0):
            p, (a, b) = self._knapsack5()
            p.set_mip_mir_rounds(0).set_mip_cut_depth(100).set_mip_node_cut_cap(0).set_mip_node_cut_stall(stall)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
            counts.append(p.mip_report().cuts)
        self.assertLess(counts[1], counts[0])
        with self.assertRaises(jaos.JaosError):
            p.set_mip_node_cut_stall(float("inf"))
        p.set_mip_node_cut_stall(-1)

    def test_root_cuts_may_leave_below_a_node(self):

        for depth in (0, 100):
            p, (a, b) = self._knapsack5()
            p.set_mip_cut_rounds(1).set_mip_cover_rounds(1).set_mip_cut_depth(depth)
            p.set_mip_root_cut_drop(True)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
        p.set_mip_root_cut_drop(False).set_mip_root_cut_drop(None)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)

    def test_a_lifted_cover_closes_what_the_extended_cover_leaves(self):

        for lift, one_node in ((False, False), (True, True)):
            p = jaos.Problem()
            a, b, f, d = [p.add_var(binary=True, name=n) for n in "abfd"]
            p.add(4 * a + 4 * b + 3 * f + 8 * d <= 10)
            p.maximize(10 * a + 10 * b + 6 * f + 15 * d)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(1).set_mip_cut_depth(0)
            p.set_mip_mir_rounds(0).set_mip_cover_lift(lift)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 20.0, places=9)
            self.assertEqual((a.value, b.value, f.value, d.value), (1.0, 1.0, 0.0, 0.0))
            rep = p.mip_report()
            self.assertGreaterEqual(rep.cuts, 1)
            self.assertEqual(rep.nodes == 1, one_node)
        p.set_mip_cover_lift(None)

    def test_a_mir_cut_closes_the_halved_row_at_the_root(self):

        for rounds, one_node in ((1, True), (0, False)):
            p = jaos.Problem()
            x = p.add_var(integer=True, name="x")
            y = p.add_var(integer=True, name="y")
            p.add(2 * x + 2 * y <= 3)
            p.maximize(x + y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0)
            p.set_mip_mir_rounds(rounds)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 1.0, places=9)
            rep = p.mip_report()
            self.assertEqual(rep.nodes == 1, one_node)
            self.assertEqual(rep.cuts >= 1, one_node)
        p.set_mip_mir_rounds(-1)

    def test_a_backtracking_dive_reaches_the_same_optimum(self):

        for times in (0, 1, 1000):
            p, (a, b) = self._knapsack5()
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0)
            p.set_mip_dive(True).set_mip_dive_backtrack(times)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
        p.set_mip_dive_backtrack(-1)

    def test_mir_cuts_at_the_nodes_keep_the_optimum(self):

        counts = []
        for on in (False, True):
            p, (a, b) = self._knapsack5()
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(100).set_mip_node_cut_cap(0).set_mip_node_mir(on)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
            counts.append(p.mip_report().cuts)
        self.assertGreaterEqual(counts[1], counts[0])
        p.set_mip_node_mir(None)

    def test_a_dive_bounded_by_the_gap_reaches_the_same_optimum(self):

        for frac in (0.01, 1.0):
            p, (a, b) = self._knapsack5()
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0)
            p.set_mip_dive(True).set_mip_dive_backtrack(0).set_mip_dive_gap(frac)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            self.assertEqual((a.value, b.value), (1.0, 1.0))
        with self.assertRaises(jaos.JaosError):
            p.set_mip_dive_gap(float("nan"))
        p.set_mip_dive_gap(-1)

    def test_an_aggregated_mir_cut_closes_what_one_row_leaves(self):

        for rows, one_node in ((0, False), (1, True)):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=3, name="x")
            y = p.add_var(integer=True, ub=3, name="y")
            s = p.add_var(ub=10, name="s")
            p.add(2 * x - s <= 0)
            p.add(2 * y + s <= 3)
            p.add(x + y <= 4)
            p.maximize(3 * x + 2 * y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_cut_depth(0)
            p.set_mip_mir_rounds(4).set_mip_mir_aggregate(rows)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 3.0, places=9)
            self.assertEqual(p.mip_report().nodes == 1, one_node)
        p.set_mip_mir_aggregate(-1)

    def test_the_dive_heuristic_finds_the_first_incumbent(self):

        firsts = []
        for solves in (0, 5):
            p = jaos.Problem()
            x = p.add_var(integer=True, name="x")
            y = p.add_var(integer=True, name="y")
            p.add(x + y <= 3.6)
            p.add(x <= 2.2)
            p.add(y <= 1.4)
            p.maximize(x + y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(solves).set_mip_feaspump(0)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 3.0, places=9)
            self.assertEqual((x.value, y.value), (2.0, 1.0))
            firsts.append(p.mip_report().first_incumbent_node)
        self.assertGreater(firsts[0], 1)
        self.assertEqual(firsts[1], 1)
        p.set_mip_dive_heuristic(-1)

    def test_the_feasibility_pump_finds_a_point_at_the_root(self):

        firsts = []
        for rounds in (0, 5):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=1, name="x")
            y = p.add_var(integer=True, ub=1, name="y")
            z = p.add_var(integer=True, ub=1, name="z")
            p.add(3 * x + 5 * y + 2 * z <= 8)
            p.maximize(10 * x + 13 * y + 7 * z)
            p.set_mip_tighten(0)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(rounds)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
            firsts.append(p.mip_report().first_incumbent_node)
        self.assertGreater(firsts[0], 1)
        self.assertEqual(firsts[1], 1)
        p.set_mip_feaspump(-1)

    def test_the_pumps_two_extensions_keep_the_answer(self):

        for general, decay in ((0, 0.0), (1, 0.0), (0, 0.9), (1, 0.9)):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=4, name="x")
            y = p.add_var(integer=True, ub=4, name="y")
            p.add(x + y <= 3.6)
            p.add(2 * x + y <= 5.5)
            p.maximize(3 * x + 2 * y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(10)
            p.set_mip_pump_general(general).set_mip_pump_obj(decay)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 8.0, places=9)
        with self.assertRaises(jaos.JaosError):
            p.set_mip_pump_obj(1.0)
        p.set_mip_pump_general(-1)
        p.set_mip_pump_obj(-1.0)

    def test_the_presolve_report_reaches_python(self):

        p = jaos.Problem()
        x = p.add_var(name="x")
        y = p.add_var(name="y")
        p.add(x + y >= 3)
        p.add(x <= 10)
        p.minimize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        rep = p.presolve_report()
        self.assertGreaterEqual(rep.rounds, 0)
        self.assertLessEqual(rep.num_row, 2)
        self.assertEqual(rep.duplicate_row, 0)

    def test_a_starting_point_a_cutoff_and_the_statistics(self):

        def build():
            p = jaos.Problem()
            a = p.add_var(integer=True, ub=1, name="a")
            b = p.add_var(integer=True, ub=1, name="b")
            c = p.add_var(integer=True, ub=1, name="c")
            p.add(3 * a + 5 * b + 2 * c <= 8)
            p.maximize(10 * a + 13 * b + 7 * c)
            return p, a, b, c

        p, a, b, c = build()
        st = p.statistics()
        self.assertEqual(st.num_col, 3)
        self.assertEqual(st.num_row, 1)
        self.assertEqual(st.integer_col, 3)
        self.assertEqual(st.binary_col, 3)
        self.assertEqual(st.one_sided_row, 1)

        self.assertEqual(st.num_row, st.equality_row + st.ranged_row
                         + st.one_sided_row + st.free_row)
        self.assertEqual(st.num_col, st.fixed_col + st.ranged_col
                         + st.one_sided_col + st.free_col)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        best = p.objective_value

        p, a, b, c = build()
        p.set_mip_start({a: 1, b: 1})
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, best, places=9)

        p, a, b, c = build()
        p.set_mip_start({a: 9, b: 9, c: 9})
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, best, places=9)

        p, a, b, c = build()
        p.set_mip_cutoff(best + 1000.0)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)

        p, a, b, c = build()
        p.set_mip_cutoff(best - 1.0)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertAlmostEqual(p.objective_value, best, places=9)
        p.set_mip_cutoff(float("inf"))
        p.set_mip_start(None)

    def test_an_exact_proof_round_trips_through_a_file(self):

        import os
        import tempfile
        p = jaos.Problem()
        x = p.add_var(name="x")
        p.add(3 * x >= 1)
        p.minimize(x)
        self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
        self.assertIs(p.verify().status, jaos.Proof.OPTIMAL)
        path = os.path.join(tempfile.gettempdir(), "jaos_py_test.proof")
        try:
            p.write_proof(path)
            with open(path, encoding="ascii") as f:
                body = f.read()
            self.assertIn("proof optimal", body)
            self.assertIn("1/3", body)

            q = jaos.Problem()
            y = q.add_var(name="x")
            q.add(3 * y >= 1)
            q.minimize(y)
            rep = q.check_proof(path)
            self.assertTrue(rep.primal)
            self.assertTrue(rep.dual)
            self.assertTrue(rep.objective)
            self.assertEqual(rep.bad_row, -1)
            self.assertGreater(rep.terms, 0)

            with open(path, encoding="ascii") as f:
                body = f.read()
            with open(path, "w", encoding="ascii") as f:
                f.write(body.replace("col x 1/3", "col x 1/4"))
            bad = q.check_proof(path)
            self.assertFalse(bad.primal)
            self.assertFalse(bad.certified)
            self.assertIs(bad.kind, jaos.ProofKind.OPTIMAL)

            r = jaos.Problem()
            z = r.add_var(lb=float("-inf"), name="z")
            r.add(z >= 1)
            r.add(z <= 0)
            r.minimize(z)
            self.assertIs(r.solve(), jaos.SolveStatus.INFEASIBLE)
            r.write_proof(path)
            rep2 = r.check_proof(path)
            self.assertIs(rep2.kind, jaos.ProofKind.INFEASIBLE)
            self.assertTrue(rep2.certified)
        finally:
            if os.path.exists(path):
                os.remove(path)

    def test_propagation_and_reduced_cost_fixing_keep_the_answer(self):

        moved = []
        for rounds in (0, 4):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=10, name="x")
            y = p.add_var(integer=True, ub=10, name="y")
            z = p.add_var(integer=True, ub=10, name="z")
            p.add(x + y + z <= 3)
            p.add(2 * x + y <= 3)
            p.maximize(3 * x + 2.4 * y + 2 * z)
            p.set_mip_propagate(rounds).set_mip_rcfix(1)
            p.set_mip_propagate_depth(-1)
            p.set_mip_pump_always(1)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 7.4, places=9)
            moved.append(p.mip_report().tightened)
        self.assertEqual(moved[0], 0)
        self.assertGreaterEqual(moved[1], 4)
        p.set_mip_propagate(-1)
        p.set_mip_propagate_depth(0)
        p.set_mip_rcfix(-1)
        p.set_mip_pump_always(-1)

    def test_coefficient_tightening_is_a_switch_and_changes_no_answer(self):
        objs = []
        for on in (0, 1, -1):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=1, name="x")
            y = p.add_var(integer=True, ub=1, name="y")
            p.add(5 * x + 3 * y <= 4)
            p.maximize(x + y)
            p.set_mip_tighten(on)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            objs.append(p.objective_value)
        self.assertEqual(objs, [1.0, 1.0, 1.0])

    def test_orbital_branching_is_a_switch_that_keeps_the_optimum(self):
        nodes = []
        for on in (0, 1):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(6)]
            p.add(sum(xs) >= 2.5)
            p.minimize(sum(xs))
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_clique_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_tighten(0)
            p.set_mip_conflicts(0).set_mip_orbital(on)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 3.0, places=9)
            nodes.append(p.mip_report().nodes)
        self.assertLess(nodes[1], nodes[0])

    def test_symmetry_detection_reports_the_orbits_of_three_alike_columns(self):
        for on, want in ((1, 1), (0, 0)):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(3)]
            p.add(xs[0] + xs[1] + xs[2] >= 1)
            p.minimize(xs[0] + xs[1] + xs[2])
            p.set_mip_symmetry(on).set_mip_orbital(on)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            rep = p.mip_report()
            self.assertEqual(rep.symmetry_orbits, want)
            if want:
                self.assertGreaterEqual(rep.symmetry_generators, 1)

    def test_conflict_analysis_is_a_switch_and_keeps_the_verdict(self):
        nodes = []
        for on in (0, 1, -1):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(3)]
            p.add(xs[0] + xs[1] <= 1)
            p.add(xs[1] + xs[2] <= 1)
            p.add(xs[0] + xs[2] <= 1)
            p.add(xs[0] + xs[1] + xs[2] >= 1.5)
            p.maximize(xs[0] + xs[1] + xs[2])
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_flow_cover_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False).set_mip_tighten(0)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_conflicts(on)
            self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
            nodes.append(p.mip_report().nodes)
        self.assertLessEqual(nodes[1], nodes[0])
        self.assertEqual(nodes[2], nodes[1])

    def test_a_flow_cover_cut_closes_the_fixed_charge_row_at_the_root(self):
        nodes = []
        for rounds in (0, 1):
            p = jaos.Problem()
            x1 = p.add_var(name="x1")
            x2 = p.add_var(name="x2")
            y1 = p.add_var(binary=True, name="y1")
            y2 = p.add_var(binary=True, name="y2")
            p.add(x1 + x2 <= 5)
            p.add(x1 - 4 * y1 <= 0)
            p.add(x2 - 3 * y2 <= 0)
            p.maximize(x1 + x2 - 2 * y1 - 2 * y2)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_cut_depth(0)
            p.set_mip_heuristics(False).set_mip_dive_heuristic(0).set_mip_feaspump(0)
            p.set_mip_tighten(0).set_mip_flow_cover_rounds(rounds)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 2.0, places=9)
            nodes.append(p.mip_report().nodes)
        self.assertGreater(nodes[0], 1)
        self.assertEqual(nodes[1], 1)

    def test_zero_half_cuts_close_an_odd_cycle_at_the_root(self):
        nodes = []
        for rounds in (0, 1, -1):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(3)]
            p.add(xs[0] + xs[1] <= 1)
            p.add(xs[1] + xs[2] <= 1)
            p.add(xs[0] + xs[2] <= 1)
            p.maximize(xs[0] + xs[1] + xs[2])
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_flow_cover_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_tighten(0)
            p.set_mip_zero_half_rounds(rounds)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 1.0, places=9)
            nodes.append(p.mip_report().nodes)
        self.assertGreater(nodes[0], 1)
        self.assertEqual(nodes[1], 1)

    def test_clique_fixing_at_a_node_shortens_the_tree(self):
        nodes = []
        for on in (0, 1, -1):
            p = jaos.Problem()
            xs = [p.add_var(binary=True, name=f"x{k}") for k in range(3)]
            p.add(3 * xs[0] + 3 * xs[1] + 2 * xs[2] <= 5)
            p.maximize(4 * xs[0] + 4 * xs[1] + xs[2])
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_clique_rounds(0).set_mip_zero_half_rounds(0).set_mip_flow_cover_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0).set_mip_tighten(0)
            p.set_mip_clique_fix(on)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 5.0, places=9)
            nodes.append(p.mip_report().nodes)
        self.assertLess(nodes[1], nodes[0])
        self.assertEqual(nodes[2], nodes[0])

    def test_probing_is_a_switch_and_changes_no_answer(self):
        objs = []
        for on, cap in ((0, -1), (1, 0), (1, 0.5), (-1, -1)):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=1, name="x")
            y = p.add_var(integer=True, ub=1, name="y")
            z = p.add_var(integer=True, ub=1, name="z")
            p.add(x + y <= 1)
            p.add(x + z <= 1)
            p.add(y + z >= 1)
            p.maximize(3 * x + y + z)
            p.set_mip_probing(on).set_mip_probing_cap(cap)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            objs.append(p.objective_value)
        self.assertEqual(objs, [2.0, 2.0, 2.0, 2.0])
        with self.assertRaises(jaos.JaosError):
            p.set_mip_probing_cap(float("nan"))

    def test_the_dive_heuristic_runs_below_the_root(self):

        solves = []
        for depth in (0, 20):
            p = jaos.Problem()
            x = p.add_var(integer=True, name="x")
            y = p.add_var(integer=True, name="y")
            p.add(x + y <= 3.6)
            p.add(x <= 2.2)
            p.add(y <= 1.4)
            p.maximize(x + y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_heuristics(False)
            p.set_mip_dive_heuristic(5).set_mip_dive_heuristic_depth(depth)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 3.0, places=9)
            solves.append(p.mip_report().lp_solves)
        self.assertGreater(solves[1], solves[0])
        p.set_mip_dive_heuristic_depth(-1)

    def test_rins_keeps_the_optimum(self):

        for budget in (0, 20):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=1, name="x")
            y = p.add_var(integer=True, ub=1, name="y")
            z = p.add_var(integer=True, ub=1, name="z")
            p.add(3 * x + 5 * y + 2 * z <= 8)
            p.maximize(10 * x + 13 * y + 7 * z)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_dive_heuristic(0)
            p.set_mip_rins(budget)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
        p.set_mip_rins(-1)

    def test_a_dive_bounded_by_the_degradation_keeps_the_optimum(self):

        for frac in (0.0, 0.01, 1.0):
            p = jaos.Problem()
            x = p.add_var(integer=True, ub=1, name="x")
            y = p.add_var(integer=True, ub=1, name="y")
            z = p.add_var(integer=True, ub=1, name="z")
            p.add(3 * x + 5 * y + 2 * z <= 8)
            p.maximize(10 * x + 13 * y + 7 * z)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0)
            p.set_mip_cut_depth(0).set_mip_dive(True)
            p.set_mip_dive_degrade(frac)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 23.0, places=9)
        p.set_mip_dive_degrade(-1.0)

    def test_the_rounding_heuristic_finds_the_root_relaxations_neighbour(self):

        for on in (True, False):
            p = jaos.Problem()
            x = p.add_var(integer=True, name="x")
            y = p.add_var(integer=True, name="y")
            p.add(x + y <= 3.6)
            p.add(x <= 2.2)
            p.add(y <= 1.4)
            p.maximize(x + y)
            p.set_mip_cut_rounds(0).set_mip_cover_rounds(0).set_mip_mir_rounds(0).set_mip_cut_depth(0).set_mip_heuristics(on)
            p.set_mip_dive_heuristic(0).set_mip_feaspump(0)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(p.objective_value, 3.0, places=9)
            self.assertEqual((x.value, y.value), (2.0, 1.0))
            rep = p.mip_report()
            if on:
                self.assertGreaterEqual(rep.heuristic_points, 1)
                self.assertEqual(rep.first_incumbent_node, 1)
            else:
                self.assertEqual(rep.heuristic_points, 0)
                self.assertGreaterEqual(rep.first_incumbent_node, 2)

    def test_the_model_layer_marks_and_reports(self):
        with jaos.Model() as m:
            m.read_mps(data("t4_int.mps"))
            self.assertTrue(m.col_integer(0))
            self.assertFalse(m.col_integer(1))
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            self.assertAlmostEqual(m.objective(), 3.5, places=9)
            obj, x = m.mip_incumbent()
            self.assertAlmostEqual(obj, 3.5, places=9)
            self.assertEqual(x[0], float(int(x[0])))
            ck = m.check_solution(m.solution().col_value)
            self.assertTrue(ck.primal_feasible)
            self.assertEqual(ck.max_integrality_violation, 0.0)
            with self.assertRaises(jaos.JaosError):
                m.set_mip_gap(-1.0)

class TestExactValues(unittest.TestCase):
    """The proved basis's exact coordinates (D286), as Fractions."""

    def test_a_third_comes_back_as_a_third(self):
        """min x s.t. 3x >= 1: x = 1/3, dual 1/3, and with a constant of
        1/2 the objective is 5/6, none of them a double."""
        from fractions import Fraction
        with jaos.Model() as m:
            m.load(num_col=1, num_row=1,
                   col_cost=[1.0], col_lower=[0.0], col_upper=[jaos.INFINITY],
                   row_lower=[1.0], row_upper=[jaos.INFINITY],
                   a_start=[0, 1], a_index=[0], a_value=[3.0],
                   obj_offset=0.5)
            with self.assertRaises(jaos.JaosError):
                m.exact_col_value(0)
            self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
            rep = m.verify()
            self.assertIs(rep.status, jaos.Proof.OPTIMAL)
            self.assertEqual(m.exact_col_value(0), Fraction(1, 3))
            self.assertEqual(m.exact_row_dual(0), Fraction(1, 3))
            self.assertEqual(m.exact_objective(), Fraction(5, 6))
            self.assertAlmostEqual(m.objective(), 5 / 6, places=12)
            m.set_col_cost(0, 2.0)
            with self.assertRaises(jaos.JaosError):
                m.exact_objective()

class TestCertificateFile(unittest.TestCase):
    """The solution file carries the certificate of an infeasible or an
    unbounded answer (D285), and it reads back as what the C calls hand
    out."""

    def test_an_infeasible_answer_writes_its_certificate(self):
        with tempfile.TemporaryDirectory() as tmp, jaos.Model() as m:
            path = os.path.join(tmp, "t1.sol")
            m.read_mps(data("t1.mps"))
            self.assertIs(m.solve(), jaos.SolveStatus.INFEASIBLE)
            want = m.certificate()
            m.write_solution(path)
            self.assertIs(m.solution_file_status(path),
                          jaos.SolveStatus.INFEASIBLE)
            status, ray = m.read_certificate(path)
            self.assertIs(status, jaos.SolveStatus.INFEASIBLE)
            self.assertEqual(ray, want)
            self.assertTrue(m.check_certificate(ray).certified)
            with self.assertRaises(jaos.JaosError) as cm:
                m.read_solution(path)
            self.assertIn("read_certificate", str(cm.exception))

    def test_an_unbounded_answer_writes_its_ray(self):
        with tempfile.TemporaryDirectory() as tmp, jaos.Model() as m:
            path = os.path.join(tmp, "ray.sol")
            m.load(num_col=2, num_row=1,
                   col_cost=[-1.0, -1.0], col_lower=[0.0, 0.0],
                   col_upper=[jaos.INFINITY, jaos.INFINITY],
                   row_lower=[-jaos.INFINITY], row_upper=[1.0],
                   a_start=[0, 1, 2], a_index=[0, 0], a_value=[1.0, -1.0])
            self.assertIs(m.solve(), jaos.SolveStatus.UNBOUNDED)
            m.write_solution(path)
            status, ray = m.read_certificate(path)
            self.assertIs(status, jaos.SolveStatus.UNBOUNDED)
            self.assertEqual(ray, m.unbounded_ray())
            self.assertTrue(m.check_ray(ray).certified)

class TestExactCertificate(unittest.TestCase):
    """D333. The oracle is arithmetic by hand: for `x + y <= 1` beside
    `x + y >= 2` the only multipliers that certify are opposite and
    equal, so both columns price at exactly zero."""

    def conflict(self):
        p = jaos.Problem()
        x = p.add_var(lb=0, name="x")
        y = p.add_var(lb=0, name="y")
        p.add(x + y <= 1, "low")
        p.add(x + y >= 2, "high")
        p.minimize(x + y)
        return p

    def test_the_multipliers_are_derived_exactly(self):
        p = self.conflict()
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        rep = p.exact_certificate()
        self.assertTrue(rep.derived)
        self.assertLessEqual(rep.bound_bits, rep.capacity_bits)
        self.assertEqual(p.exact_row_multiplier(0), fractions.Fraction(-1))
        self.assertEqual(p.exact_row_multiplier(1), fractions.Fraction(1))

        self.assertEqual(p.exact_row_multiplier(p._cons[1]), fractions.Fraction(1))

    def test_the_derived_file_is_judged_by_the_independent_checker(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "cert.proof")
            p = self.conflict()
            p.solve()
            p.exact_certificate()
            p.write_proof(path)
            rep = p.check_proof(path)
            self.assertIs(rep.kind, jaos.ProofKind.INFEASIBLE)
            self.assertTrue(rep.certified)

    def test_the_unbounded_direction_is_derived_exactly(self):
        """D336, with the same kind of oracle: for `min -x` under
        `x - y <= 1` any ray must move x and y together, or the row's
        activity rises and it has an upper bound."""
        p = jaos.Problem()
        x = p.add_var(lb=0, name="x")
        y = p.add_var(lb=0, name="y")
        p.add(x - y <= 1)
        p.minimize(-x)
        self.assertIs(p.solve(), jaos.SolveStatus.UNBOUNDED)
        rep = p.exact_unbounded_ray()
        self.assertTrue(rep.derived)
        self.assertEqual(rep.at_row, -1)
        dx = p.exact_col_direction(x)
        dy = p.exact_col_direction(y)
        self.assertEqual(dx, dy)
        self.assertGreater(dx, 0)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "u.proof")
            p.write_proof(path)
            r = p.check_proof(path)
            self.assertIs(r.kind, jaos.ProofKind.UNBOUNDED)
            self.assertTrue(r.certified)

    def test_the_model_layer_refuses_what_has_no_basis(self):
        with jaos.Model() as m:
            m.read_mps(data("solve1.mps"))
            m.solve()
            with self.assertRaises(jaos.JaosError):
                m.exact_certificate()

class TestFeasibilityRelaxation(unittest.TestCase):
    """D331. The oracle is the solver: the moves are applied to a problem
    built again with the moved bound, and that one has to be feasible. A
    total that is right but not achievable passes a check on a number and
    fails this one."""

    def asks_too_much(self, row_lo=30.0, ub=10.0):
        p = jaos.Problem()
        x = p.add_var(lb=0.0, ub=ub, name="x")
        y = p.add_var(lb=0.0, ub=ub, name="y")
        p.add(x + y >= row_lo, "big")
        p.minimize(x + y)
        return p

    def test_the_smallest_total_and_the_bound_it_falls_on(self):
        p = self.asks_too_much()
        r = p.feasrelax()
        self.assertAlmostEqual(r.report.total, 10.0, places=9)
        self.assertEqual([(c.name, mv) for c, mv in r.row_move],
                         [("big", -10.0)])
        self.assertEqual(r.col_move, [])

        moved = self.asks_too_much(row_lo=30.0 + r.row_move[0][1])
        self.assertIs(moved.solve(), jaos.SolveStatus.OPTIMAL)

    def test_the_scope_decides_which_bound_moves(self):
        p = self.asks_too_much()
        rows = p.feasrelax(jaos.RelaxScope.ROWS)
        self.assertEqual(rows.report.cols_moved, 0)
        self.assertEqual(rows.report.rows_moved, 1)
        cols = p.feasrelax(jaos.RelaxScope.COLS)
        self.assertEqual(cols.report.rows_moved, 0)
        self.assertEqual(cols.report.cols_moved, 1)
        self.assertAlmostEqual(rows.report.total, cols.report.total, places=9)

    def test_an_sos_set_reaches_the_relaxation_at_both_layers(self):
        with jaos.Model() as m:
            m.load(2, 2, [1.0, 1.0], [0.0, 0.0], [10.0, 10.0],
                   [2.0, 2.0], [float("inf"), float("inf")],
                   [0, 1, 2], [0, 1], [1.0, 1.0])
            m.add_sos(1, [0, 1], [1.0, 2.0])
            self.assertIs(m.solve(), jaos.SolveStatus.INFEASIBLE)
            r = m.feasrelax(jaos.RelaxScope.ROWS)
            self.assertAlmostEqual(r.report.total, 2.0, places=9)
            self.assertEqual(r.report.rows_moved, 1)

        p = jaos.Problem()
        x = p.add_var(lb=0.0, ub=10.0, name="x")
        y = p.add_var(lb=0.0, ub=10.0, name="y")
        p.add(x >= 2.0, "r1")
        p.add(y >= 2.0, "r2")
        p.add_sos(1, [x, y], [1.0, 2.0])
        p.minimize(x + y)
        self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
        r = p.feasrelax(jaos.RelaxScope.ROWS)
        self.assertAlmostEqual(r.report.total, 2.0, places=9)
        self.assertEqual(r.report.rows_moved, 1)

    def test_a_feasible_problem_moves_nothing(self):
        p = jaos.Problem()
        x = p.add_var(lb=0.0, ub=10.0)
        p.add(x >= 3.0)
        p.minimize(x)
        r = p.feasrelax()
        self.assertEqual(r.report.total, 0.0)
        self.assertEqual(r.row_move, [])
        self.assertEqual(r.col_move, [])
        self.assertEqual(r.report.at_row, -1)
        self.assertEqual(r.report.at_col, -1)

    def test_the_model_layer_answers_and_leaves_the_model_unsolved(self):
        with jaos.Model() as m:
            m.read_mps(data("t1.mps"))
            r = m.feasrelax()
            self.assertGreater(r.report.total, 0.0)
            self.assertEqual(len(r.row_move), m.num_row)
            self.assertEqual(len(r.col_move), m.num_col)

            self.assertIs(m.status, jaos.SolveStatus.NOT_RUN)
            self.assertGreater(r.report.work_units, 0)

    def test_a_model_with_no_relaxation_raises(self):
        with jaos.Model() as m:

            m.load(1, 1, [0.0], [5.0], [3.0], [0.0], [jaos.INFINITY],
                   [0, 1], [0], [1.0])
            with self.assertRaises(jaos.JaosError):
                m.feasrelax()

class TestSolutionFileRoundTrip(unittest.TestCase):
    """jaos_read_solution through the binding. The accepting case alone
    would also pass if every list came back empty, so the shape and the
    values are checked, and the refusing case is what proves the C side is
    reached at all."""

    def test_a_solution_file_reads_back_as_the_same_answer(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "answer.sol")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                want_obj = m.objective()
                want = m.solution()
                want_basis = m.basis()
                m.write_solution(path)

                obj, sol, basis = m.read_solution(path)

                self.assertEqual(obj, want_obj)
                self.assertEqual(sol.col_value, want.col_value)
                self.assertEqual(sol.col_dual, want.col_dual)
                self.assertEqual(sol.row_activity, want.row_activity)
                self.assertEqual(sol.row_dual, want.row_dual)
                self.assertEqual(basis.col_status, want_basis.col_status)
                self.assertEqual(basis.row_status, want_basis.row_status)

    def test_a_read_basis_warm_starts_the_next_solve(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "answer.sol")
            with jaos.Model() as a:
                a.read_mps(data("solve1.mps"))
                a.solve()
                a.write_solution(path)

            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                _, _, basis = m.read_solution(path)
                m.set_basis(basis.col_status, basis.row_status)
                self.assertIs(m.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertAlmostEqual(m.objective(), 29.0, places=9)

    def test_an_mps_basis_file_round_trips_at_both_layers(self):
        """D338: the format the field exchanges a basis in. Model writes
        it and reads it back as the same statuses, and Problem does the
        same over its own model."""
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "a.bas")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                want = m.basis()
                m.write_mps_basis(path)
                got = m.read_mps_basis(path)
                self.assertEqual(got.col_status, want.col_status)
                self.assertEqual(got.row_status, want.row_status)
                with open(path) as f:
                    text = f.read()
                self.assertIn("ENDATA", text)

            with jaos.Model() as n:
                n.read_mps(data("solve1.mps"))
                b = n.read_mps_basis(path)
                n.set_basis(b.col_status, b.row_status)
                self.assertIs(n.solve(), jaos.SolveStatus.OPTIMAL)
                self.assertEqual(n.iterations, 0)

        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "p.bas")
            p = jaos.Problem()
            x = p.add_var(lb=0, ub=10, name="x")
            p.add(x >= 3)
            p.minimize(x)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            p.write_mps_basis(path)
            b = p.read_mps_basis(path)
            self.assertEqual(len(b.col_status), 1)
            self.assertEqual(len(b.row_status), 1)

    def test_a_point_file_round_trips_at_both_layers(self):
        """D342: the smallest thing that can carry an answer between two
        programs, and what makes the checker usable on somebody else's."""
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "p.txt")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                want = m.solution().col_value
                m.write_point(path)
                got = m.read_point(path)
                self.assertEqual(got, want)
                rep = m.check_solution(got)
                self.assertTrue(rep.primal_feasible)
                self.assertFalse(rep.checked_duals)

                with open(path) as f:
                    lines = [ln for ln in f if not ln.startswith("#")]
                with open(path, "w") as f:
                    f.writelines(lines[:-1])
                with self.assertRaises(Exception):
                    m.read_point(path)

        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "q.txt")
            p = jaos.Problem()
            x = p.add_var(lb=0, ub=10, name="x")
            p.add(x >= 3)
            p.minimize(x)
            self.assertIs(p.solve(), jaos.SolveStatus.OPTIMAL)
            p.write_point(path)
            self.assertEqual(p.read_point(path), [3.0])

    def test_a_point_file_is_written_from_given_values(self):
        """D344: a point that is not the answer -- a pool entry, say --
        and no solve is needed."""
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "given.txt")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                with self.assertRaises(jaos.JaosError):
                    m.write_point(path)
                m.write_point_values(path, [1.5, -2.0, 0.0])
                self.assertEqual(m.read_point(path), [1.5, -2.0, 0.0])
                with self.assertRaises(ValueError):
                    m.write_point_values(path, [1.0])

    def test_the_duals_file_is_written_as_well_as_read(self):
        """D348: both halves of `check --point --duals` come out of this
        library now, with no awk in between."""
        with tempfile.TemporaryDirectory() as tmp:
            p = os.path.join(tmp, "p.txt")
            d = os.path.join(tmp, "d.txt")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                sol = m.solution()
                m.write_point(p)
                m.write_duals(d)
                self.assertEqual(m.read_point(p), sol.col_value)
                self.assertEqual(m.read_duals(d), sol.row_dual)
                rep = m.check_solution(m.read_point(p), m.read_duals(d))
                self.assertTrue(rep.checked_duals)
                self.assertTrue(rep.dual_feasible)

                m.write_dual_values(d, [0.0] * m.num_row)
                self.assertEqual(m.read_duals(d), [0.0] * m.num_row)
                with self.assertRaises(ValueError):
                    m.write_dual_values(d, [0.0])

    def test_a_duals_file_is_the_same_shape_over_the_rows(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "d.txt")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                sol = m.solution()
                with open(path, "w") as f:
                    for i in range(m.num_row):
                        f.write("%s %.17g\n"
                                % (m.row_name(i), sol.row_dual[i]))
                back = m.read_duals(path)
                self.assertEqual(back, sol.row_dual)
                rep = m.check_solution(sol.col_value, back)
                self.assertTrue(rep.checked_duals)
                self.assertTrue(rep.dual_feasible)

    def test_an_mps_basis_file_is_refused_when_it_is_wrong(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "bad.bas")
            with open(path, "w") as f:
                f.write(" XL nosuch    nosuch\nENDATA\n")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                with self.assertRaises(Exception):
                    m.read_mps_basis(path)

            with jaos.Model() as n:
                n.read_mps(data("solve1.mps"))
                with self.assertRaises(Exception):
                    n.write_mps_basis(os.path.join(tmp, "none.bas"))

    def test_a_file_from_another_model_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "wrong.sol")
            with open(path, "w") as f:
                f.write("status optimal\nobjective 1\n"
                        "columns 99\nrows 99\nend\n")
            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                with self.assertRaises(Exception):
                    m.read_solution(path)

    def test_a_certificate_file_carries_a_basis_and_read_basis_takes_it(self):
        """D332: either kind of file, read by the one call."""
        with tempfile.TemporaryDirectory() as tmp:
            cert = os.path.join(tmp, "cert.sol")
            opt = os.path.join(tmp, "opt.sol")

            p = jaos.Problem()
            x = p.add_var(lb=0, name="x")
            y = p.add_var(lb=0, name="y")
            p.add(x + y <= 1)
            p.add(x + y >= 2)
            p.minimize(x + y)
            self.assertIs(p.solve(), jaos.SolveStatus.INFEASIBLE)
            p.write_solution(cert)
            with open(cert) as f:
                lines = [ln for ln in f if ln.startswith("basis ")]
            self.assertEqual(len(lines), 4)
            b = p.read_basis(cert)
            self.assertEqual(len(b.col_status), 2)
            self.assertEqual(len(b.row_status), 2)
            self.assertEqual(sum(st is jaos.BasisStatus.BASIC
                                 for st in b.col_status + b.row_status), 2)

            with jaos.Model() as m:
                m.read_mps(data("solve1.mps"))
                m.solve()
                m.write_solution(opt)
                self.assertEqual(len(m.read_basis(opt).row_status),
                                 m.num_row)

    def test_a_problem_reads_its_own_solution_file_back(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "p.sol")
            p = jaos.Problem()
            x = p.add_var(lb=0, ub=10, name="x")
            p.add(x >= 3)
            p.minimize(x)
            p.solve()
            p.write_solution(path)
            obj, sol, basis = p.read_solution(path)
            self.assertAlmostEqual(obj, 3.0, places=9)
            self.assertEqual(len(sol.col_value), 1)
            self.assertEqual(len(basis.col_status), 1)

if __name__ == "__main__":
    unittest.main()
