// SPDX-License-Identifier: Apache-2.0
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.List;
import java.util.stream.Stream;

import org.jaos.Algorithm;
import org.jaos.Basis;
import org.jaos.BasisStatus;
import org.jaos.BoundRanging;
import org.jaos.Branching;
import org.jaos.CallbackAction;
import org.jaos.Certificate;
import org.jaos.CertificateReport;
import org.jaos.CheckReport;
import org.jaos.Cone;
import org.jaos.ConeType;
import org.jaos.CostRanging;
import org.jaos.DiveChild;
import org.jaos.Entries;
import org.jaos.ExactRayReport;
import org.jaos.Expr;
import org.jaos.IncumbentEvent;
import org.jaos.Iis;
import org.jaos.IisSide;
import org.jaos.Incumbent;
import org.jaos.Indicator;
import org.jaos.JaosException;
import org.jaos.LogLevel;
import org.jaos.MipReport;
import org.jaos.Model;
import org.jaos.ModelStats;
import org.jaos.NodeEvent;
import org.jaos.Problem;
import org.jaos.Progress;
import org.jaos.Proof;
import org.jaos.ProofKind;
import org.jaos.ProofReport;
import org.jaos.ProofStage;
import org.jaos.QuadraticTerms;
import org.jaos.RayReport;
import org.jaos.RelaxScope;
import org.jaos.Relaxation;
import org.jaos.Sense;
import org.jaos.Solution;
import org.jaos.SolutionFile;
import org.jaos.SolveStatus;
import org.jaos.Sos;
import org.jaos.Status;
import org.jaos.Var;
import org.jaos.VerifyReport;

/** The Java binding's checks: one line each, exit 1 when any fails. */
public final class Check {
    private static int passed, failed;
    private static final double INF = Double.POSITIVE_INFINITY;
    private static final SolveStatus OPTIMAL = SolveStatus.OPTIMAL;

    private static void check(boolean ok, String what) {
        if (ok) {
            passed++;
            System.out.println("ok   " + what);
        } else {
            failed++;
            System.out.println("FAIL " + what);
        }
    }

    private static boolean near(double a, double b) {
        return Math.abs(a - b) <= 1e-6;
    }

    private static boolean throwsA(Class<? extends Throwable> type, Runnable r) {
        try {
            r.run();
            return false;
        } catch (RuntimeException e) {
            return type.isInstance(e);
        }
    }

    private static double[] d(double... v) { return v; }
    private static long[] l(long... v) { return v; }

    public static void main(String[] args) throws IOException {
        String data = args.length > 0 ? args[0] : "tests/data";
        Path tmp = Files.createTempDirectory("jaos-java-check");
        try {
            original(data);
            inspection();
            names(data);
            writers(data, tmp);
            files(data, tmp);
            structures();
            quadraticAndCones(tmp);
            pool();
            callbacks(data);
            checkers(data);
            exact(tmp);
            iisAndRelaxation();
            ranging();
            reports();
            options(data, tmp);
            problemLayer();
        } finally {
            try (Stream<Path> walk = Files.walk(tmp)) {
                for (Path p : walk.sorted(Comparator.reverseOrder()).toList())
                    Files.delete(p);
            }
        }
        System.out.println(passed + " passed, " + failed + " failed");
        System.exit(failed == 0 ? 0 : 1);
    }

    private static void original(String data) {
        check(Model.version().startsWith("0."),
              "the library answers with its version " + Model.version());

        try (Model m = Model.read(Path.of(data, "g1.lp").toString())) {
            m.solve();
            check(m.status() == SolveStatus.OPTIMAL, "g1.lp solves optimal");
            check(near(m.objective(), -5.0), "to objective -5");
            Solution s = m.solution();
            check(near(s.x()[0], 0.0) && near(s.x()[1], -1.0) && near(s.x()[2], 8.0),
                  "at (0, -1, 8)");
            check(Model.statusString(m.status()).equals("optimal"), "and says 'optimal'");
        }

        try (Model m = Model.read(Path.of(data, "no_such_file.lp").toString())) {
            check(m.numCol() < 0, "a missing file throws");
        } catch (JaosException e) {
            check(e.status() == Status.IO && !e.getMessage().isEmpty(),
                  "a missing file throws with its message: " + e.getMessage());
        }

        try (Model m = new Model()) {
            try {
                m.setOption("no_such_option", "1");
                check(false, "an unknown option throws");
            } catch (JaosException e) {
                check(e.status() == Status.INVALID_INPUT, "an unknown option throws");
            }
            m.setOption("mip_gap", "0.001");
            check(m.getOption("mip_gap").startsWith("0.001"),
                  "an option reads back as it was set");
        }

        try (Problem p = new Problem()) {
            double[] w = {2, 3, 1, 4, 5}, v = {5, 4, 3, 7, 6};
            Var[] x = new Var[5];
            Expr weight = new Expr(), value = new Expr();
            for (int j = 0; j < 5; j++) {
                x[j] = p.addVar(0, 1, true);
                weight.add(w[j], x[j]);
                value.add(v[j], x[j]);
            }
            p.addLe(weight, 9);
            p.maximize(value);
            check(p.solve() == SolveStatus.OPTIMAL, "a knapsack solves optimal");
            check(near(p.objective(), 16.0), "to 16");
            check(near(p.value(x[0]) + p.value(x[1]) + p.value(x[3]), 3.0),
                  "with the first, second and fourth items");
            MipReport r = p.model().mipResult();
            check(r.hasIncumbent() && Math.abs(r.bound() - 16.0) <= 1e-4,
                  "and the tree's report agrees");
            check(near(p.model().mipIncumbent().objective(), 16.0),
                  "and the incumbent carries its objective");

            List<String> lines = new ArrayList<>();
            p.model().setOption("mip_gap", "0.25");
            p.model().setLog(LogLevel.SUMMARY, lines::add);
            p.addLe(new Expr().add(1, x[0]).add(1, x[1]), 1);
            p.solve();
            check(p.model().getOption("mip_gap").equals("0.25") && !lines.isEmpty(),
                  "a solve after an edit keeps the options and the log set on its model");
            check(Arrays.asList(Model.optionNames()).contains("mip_gap"),
                  "the option list names mip_gap");
        }

        try (Problem p = new Problem()) {
            Var y1 = p.addVar(0, Double.POSITIVE_INFINITY, false);
            Var y2 = p.addVar(0, Double.POSITIVE_INFINITY, false);
            int row = p.addLe(new Expr().add(y1).add(y2), 0.5);
            p.addQuadratic(y1, y1, 1.0);
            p.addQuadratic(y2, y2, 1.0);
            p.minimize(new Expr().add(-1, y1).add(-1, y2));
            check(p.solve() == SolveStatus.OPTIMAL, "a QP solves optimal");
            check(near(p.objective(), -0.375), "to -0.375");
            check(near(p.value(y1), 0.25) && near(p.value(y2), 0.25), "at (0.25, 0.25)");
            check(near(p.dual(row), -0.5), "with the row's dual -0.5");
        }

        try (Problem p = new Problem()) {
            double inf = Double.POSITIVE_INFINITY;
            Var t = p.addVar(-inf, inf, false);
            Var a = p.addVar(-inf, inf, false);
            Var b = p.addVar(-inf, inf, false);
            p.addEq(Expr.of(a), 3);
            p.addEq(Expr.of(b), 4);
            p.addCone(ConeType.QUADRATIC, t, a, b);
            p.minimize(Expr.of(t));
            check(p.solve() == SolveStatus.OPTIMAL, "a cone model solves optimal");
            check(near(p.objective(), 5.0), "to 5");
            double[] z = p.model().coneDual(0);
            check(z.length == 3 && near(z[0], 1.0) && near(z[1], -0.6) && near(z[2], -0.8),
                  "with the cone's dual (1, -0.6, -0.8)");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(2.5, Double.POSITIVE_INFINITY, false);
            p.addLe(Expr.of(x), 1.4);
            p.minimize(Expr.of(x));
            check(p.solve() == SolveStatus.INFEASIBLE, "an infeasible LP says so");
            double[] y = p.model().certificate();
            check(y != null && y.length == 1 && y[0] < 0.0,
                  "with a Farkas multiplier on its upper side");
        }

        try (Model m = new Model()) {
            List<String> lines = new ArrayList<>();
            m.readFile(Path.of(data, "g1.lp").toString());
            m.setLog(LogLevel.SUMMARY, lines::add);
            m.solve();
            check(!lines.isEmpty(), "the log reaches the sink, " + lines.size() + " lines");
            m.setLog(LogLevel.OFF, null);
            lines.clear();
            m.setColCost(0, 3.5);
            m.solve();
            check(lines.isEmpty(), "and stops when it is turned off");

            List<LogLevel> levels = new ArrayList<>();
            m.setLogCallback(LogLevel.SUMMARY, (level, line) -> levels.add(level));
            m.setColCost(0, 3.0);
            m.solve();
            check(!levels.isEmpty() && !levels.contains(LogLevel.OFF),
                  "a log callback receives each line's level");
        }
        check(Model.infinity() == INF && !Model.statusString(Status.IO).isEmpty(),
              "the library's infinity and its name for a status reach Java");
    }

    private static Model golden() {
        Model m = new Model();
        m.loadLp(Sense.MINIMIZE, 0.0, d(-1, -2), d(0, 0), d(INF, INF), d(-INF), d(4),
                 l(0, 1, 2), l(0, 0), d(1, 1));
        return m;
    }

    private static void inspection() {
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(1, 1, 1), d(0, 0, 0), d(10, 10, 10), d(0, 0),
                     d(5, 5), l(0, 2, 3, 5), l(0, 1, 1, 1, 0), d(1, 0, 3, 4, 2));
            check(m.numNz() == 4, "a load drops its explicit zero: 4 entries");
            Entries c2 = m.colEntries(2), r1 = m.rowEntries(1);
            check(Arrays.equals(c2.index(), l(0, 1)) && Arrays.equals(c2.value(), d(2, 4))
                  && Arrays.equals(r1.index(), l(1, 2)) && Arrays.equals(r1.value(), d(3, 4)),
                  "a column and a row read back sorted");
            check(m.coefficient(1, 2) == 4.0 && m.coefficient(1, 0) == 0.0,
                  "one coefficient reads back, and 0 where there is none");
            m.setCoefficient(1, 0, 7.0);
            check(Arrays.equals(m.rowEntries(1).index(), l(0, 1, 2)),
                  "a set coefficient joins its row");
            check(throwsA(JaosException.class, () -> m.colEntries(3)),
                  "a column out of range throws");
        }

        try (Model m = golden()) {
            check(m.solve() == OPTIMAL && near(m.objective(), -8), "the golden LP solves to -8");
            check(m.colCost(1) == -2.0 && m.colBounds(0).lower() == 0.0
                  && m.colBounds(0).upper() == INF && m.rowBounds(0).upper() == 4.0
                  && m.rowBounds(0).lower() == -INF,
                  "its costs and bounds read back");
            try (Model c = m.copy()) {
                check(c.status() == SolveStatus.NOT_RUN && c.numNz() == m.numNz(),
                      "a copy holds the problem and not the answer");
                c.deleteCols(1);
                c.solve();
                check(c.numCol() == 1 && near(c.objective(), -4) && m.numCol() == 2,
                      "deleting a column of the copy leaves -4 and the original whole");
            }
            m.addRows(d(-INF), d(3), l(0, 1), l(1), d(1));
            m.solve();
            check(m.numRow() == 2 && m.numNz() == 3 && near(m.objective(), -7),
                  "an added row binds: -7");
            m.deleteRows(1);
            m.solve();
            check(m.numRow() == 1 && near(m.objective(), -8), "deleting it unbinds: -8");
            m.addCols(d(-3), d(0), d(2), l(0, 1), l(0), d(1));
            m.solve();
            check(m.numCol() == 3 && m.numNz() == 3 && near(m.objective(), -10),
                  "an added column improves it: -10");
            check(throwsA(JaosException.class, () -> m.deleteCols(0, 0)),
                  "a repeated delete index throws");
        }

        try (Model m = golden()) {
            m.setColBounds(1, 0, 1);
            check(!m.hasInteger() && m.solve() == OPTIMAL && near(m.objective(), -5),
                  "a moved column bound gives -5");
            m.setColInteger(0, true);
            check(m.hasInteger() && m.colInteger(0), "and an integer mark makes it a MIP");
        }

        try (Model m = new Model()) {
            m.loadLp(Sense.MAXIMIZE, 10.0, d(1), d(0), d(3), d(-INF), d(10), l(0, 1), l(0),
                     d(1));
            check(m.sense() == Sense.MAXIMIZE && m.objectiveOffset() == 10.0,
                  "the sense and the constant read back");
            m.solve();
            check(near(m.objective(), 13), "max x + 10 with x <= 3 is 13");
            m.setSense(Sense.MINIMIZE);
            check(m.sense() == Sense.MINIMIZE && m.status() == SolveStatus.NOT_RUN,
                  "a new sense discards the answer");
            m.solve();
            check(near(m.objective(), 10), "and the minimum is 10");
            m.setObjectiveOffset(0.0);
            m.solve();
            check(m.objectiveOffset() == 0.0 && near(m.objective(), 0), "and 0 with no constant");
            check(throwsA(JaosException.class, () -> m.setObjectiveOffset(Double.NaN)),
                  "a NaN constant throws");
        }
    }

    private static void names(String data) {
        try (Model m = Model.read(Path.of(data, "t1.mps").toString())) {
            check(m.colName(0).equals("X1") && m.colName(2).equals("X3")
                  && m.rowName(2).equals("EQ1") && m.objectiveName().equals("COST")
                  && m.name().equals("T1"),
                  "a file's column, row, objective and model names come through");
            check(m.colIndex("X2") == 1 && m.rowIndex("EQ1") == 2, "and look up their index");
            check(throwsA(JaosException.class, () -> m.rowIndex("COST")),
                  "the objective is not a row");
            m.setName("renamed");
            m.setColName(1, "y");
            m.setRowName(0, "cap");
            m.setObjectiveName("profit");
            check(m.name().equals("renamed") && m.colName(1).equals("y")
                  && m.rowName(0).equals("cap") && m.objectiveName().equals("profit")
                  && m.colIndex("y") == 1,
                  "names set here read back");
            check(throwsA(JaosException.class, () -> m.setColName(0, "a b")),
                  "a name with a space throws");
            try (Model c = m.copy()) {
                c.setColName(0, "mine");
                check(c.name().equals("renamed") && c.colName(2).equals("X3")
                      && m.colName(0).equals("X1"),
                      "a copy carries the names and renames on its own");
            }
            m.setColName(1, null);
            check(m.colName(1).equals("C2"), "a null name restores the positional one");
        }
    }

    private static void writers(String data, Path tmp) throws IOException {
        try (Model m = golden()) {
            Path mps = tmp.resolve("golden.mps"), lp = tmp.resolve("golden.lp");
            m.writeMps(mps.toString());
            m.writeLp(lp.toString());
            try (Model a = new Model(); Model b = new Model()) {
                a.readMps(mps.toString());
                b.readLp(lp.toString());
                check(a.solve() == OPTIMAL && near(a.objective(), -8) && b.solve() == OPTIMAL
                      && near(b.objective(), -8),
                      "MPS and LP files round-trip through their own readers");
            }
        }
        try (Model m = Model.read(Path.of(data, "t_lin.nl").toString())) {
            Path nl = tmp.resolve("out.nl");
            m.writeNl(nl.toString());
            check(Files.exists(tmp.resolve("out.col")) && Files.exists(tmp.resolve("out.row")),
                  "an NL file is written with its .col and .row files");
            try (Model back = new Model()) {
                back.readNl(nl.toString());
                check(back.numCol() == 3 && back.numRow() == 3 && back.numNz() == 6
                      && back.colName(1).equals("z") && back.rowName(1).equals("c2")
                      && back.objectiveName().equals("obj") && back.colInteger(2),
                      "and reads back with its names and its integer column last");
                check(back.solve() == OPTIMAL && near(back.objective(), -4), "and solves to -4");
            }
        }
        try (Model m = Model.read(Path.of(data, "g_quad.qplib").toString())) {
            m.solve();
            Path q = tmp.resolve("out.qplib");
            m.writeQplib(q.toString());
            try (Model back = new Model()) {
                back.readQplib(q.toString());
                check(back.rowName(0).equals("c1") && back.colQuadratic(1) == 2.0,
                      "a QPLIB file round-trips with its names and Q");
            }
            Path o = tmp.resolve("out.osil");
            m.writeOsil(o.toString());
            check(Files.readString(o).contains("<var name=\"x\""), "an OSiL file is written");
            try (Model back = new Model()) {
                back.readOsil(o.toString());
                check(back.numCol() == 2 && back.numRow() == 1 && back.colName(0).equals("x")
                      && back.colQuadratic(0) == 2.0,
                      "and reads back with its names and Q");
                check(back.solve() == OPTIMAL && near(back.objective(), 4.0), "and solves to 4");
            }
        }
        try (Model m = Model.read(Path.of(data, "g_cone.mps").toString())) {
            Path c = tmp.resolve("cone.cbf");
            m.writeCbf(c.toString());
            try (Model back = new Model()) {
                back.readCbf(c.toString());
                Cone k = back.cone(0);
                check(k.type() == ConeType.QUADRATIC && Arrays.equals(k.cols(), l(0, 1, 2)),
                      "a CBF file round-trips its cone");
                check(back.solve() == OPTIMAL && near(back.objective(), 5.0), "and solves to 5");
            }
        }
        try (Model m = Model.read(Path.of(data, "t_ampl_lp.nl").toString())) {
            m.solve();
            Path s = tmp.resolve("amp.sol");
            m.writeSolAmpl(s.toString(), null);
            List<String> lines = Files.readAllLines(s);
            check(lines.get(0).equals("JAOS " + Model.version() + ": optimal; objective -7")
                  && lines.get(2).equals("Options") && lines.get(16).equals("objno 0 0"),
                  "an AMPL .sol file hands the answer back");
            m.writeSolAmpl(s.toString(), "solved by hand");
            check(Files.readAllLines(s).get(0).equals("solved by hand"),
                  "with the caller's message when there is one");
        }
    }

    private static void files(String data, Path tmp) {
        String solve1 = Path.of(data, "solve1.mps").toString();
        String sol = tmp.resolve("answer.sol").toString(), bas = tmp.resolve("a.bas").toString();
        String pt = tmp.resolve("p.txt").toString(), du = tmp.resolve("d.txt").toString();
        try (Model m = Model.read(solve1)) {
            m.solve();
            Solution want = m.solution();
            Basis b = m.basis();
            m.writeSolution(sol);
            SolutionFile f = m.readSolution(sol);
            check(f.objective() == m.objective() && Arrays.equals(f.solution().x(), want.x())
                  && Arrays.equals(f.solution().rowDual(), want.rowDual())
                  && Arrays.equals(f.solution().reducedCost(), want.reducedCost())
                  && Arrays.equals(f.basis().colStatus(), b.colStatus())
                  && Arrays.equals(f.basis().rowStatus(), b.rowStatus()),
                  "a solution file reads back as the same answer and basis");
            check(m.solutionFileStatus(sol) == OPTIMAL
                  && Arrays.equals(m.readBasis(sol).rowStatus(), b.rowStatus()),
                  "it holds an optimum, and its basis reads out alone");
            m.writeMpsBasis(bas);
            Basis got = m.readMpsBasis(bas);
            check(Arrays.equals(got.colStatus(), b.colStatus())
                  && Arrays.equals(got.rowStatus(), b.rowStatus()),
                  "an MPS basis file round-trips");
            m.writePoint(pt);
            m.writeDuals(du);
            double[] x = m.readPoint(pt), y = m.readDuals(du);
            check(Arrays.equals(x, want.x()) && Arrays.equals(y, want.rowDual()),
                  "point and duals files round-trip");
            CheckReport r = m.checkSolution(x, y, 1e-7);
            check(r.primalFeasible() && r.checkedDuals() && r.dualFeasible(),
                  "and the checker accepts what they hold");
            m.writeDualValues(du, new double[(int) m.numRow()]);
            check(Arrays.equals(m.readDuals(du), new double[(int) m.numRow()]),
                  "a duals file is written from given values");
            check(throwsA(IllegalArgumentException.class, () -> m.writeDualValues(du, d(0))),
                  "a duals array of the wrong length never reaches C");
        }
        try (Model m = Model.read(solve1)) {
            check(throwsA(JaosException.class, () -> m.writePoint(pt)),
                  "a point file needs an answer");
            m.writePointValues(pt, d(1.5, -2.0, 0.0));
            check(Arrays.equals(m.readPoint(pt), d(1.5, -2.0, 0.0)),
                  "or is written from given values");
            m.setBasis(m.readMpsBasis(bas));
            check(m.solve() == OPTIMAL && m.iterations() == 0,
                  "a basis handed in warm-starts the solve: 0 iterations");
            m.clearBasis();
            check(m.solve() == OPTIMAL && near(m.objective(), 29.0),
                  "and a cleared one solves cold to the same 29");
            check(throwsA(IllegalArgumentException.class, () -> m.setBasis(
                      new Basis(new BasisStatus[] {BasisStatus.BASIC}, new BasisStatus[0]))),
                  "a basis of the wrong length never reaches C");
        }
        try (Model m = Model.read(Path.of(data, "t1.mps").toString())) {
            check(m.solve() == SolveStatus.INFEASIBLE, "t1.mps is infeasible");
            double[] want = m.certificate();
            m.writeSolution(sol);
            Certificate c = m.readCertificate(sol);
            check(m.solutionFileStatus(sol) == SolveStatus.INFEASIBLE
                  && c.status() == SolveStatus.INFEASIBLE && Arrays.equals(c.ray(), want),
                  "its certificate file reads back as the certificate");
            check(m.checkCertificate(c.ray(), 1e-7).certified(), "which certifies");
            check(m.readBasis(sol).colStatus().length == 3, "and carries a basis");
        }
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(-1, -1), d(0, 0), d(INF, INF), d(-INF), d(1),
                     l(0, 1, 2), l(0, 0), d(1, -1));
            check(m.solve() == SolveStatus.UNBOUNDED, "an unbounded LP says so");
            m.writeSolution(sol);
            Certificate c = m.readCertificate(sol);
            check(c.status() == SolveStatus.UNBOUNDED && Arrays.equals(c.ray(), m.unboundedRay())
                  && m.checkRay(c.ray(), 1e-7).certified(),
                  "its file reads back as its ray, which certifies");
        }
    }

    private static void structures() {
        for (int type = 1; type <= 2; type++) {
            try (Problem p = new Problem()) {
                Var[] xs = {p.addVar(0, 1, false), p.addVar(0, 1, false), p.addVar(0, 1, false)};
                Expr sum = new Expr().add(xs[0]).add(xs[1]).add(xs[2]);
                p.addLe(sum, 10);
                p.maximize(sum);
                p.addSos(type, xs);
                check(p.solve() == OPTIMAL && near(p.objective(), type),
                      "an SOS" + type + " set leaves " + type + " member(s) nonzero");
                Sos s = p.model().sos(0);
                check(p.model().numSos() == 1 && s.type() == type
                      && Arrays.equals(s.cols(), l(0, 1, 2)) && Arrays.equals(s.weights(), d(1, 2, 3)),
                      "and reads back with its members and weights");
            }
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(0, 10, false);
            Var z = p.addVar(0, 1, true);
            p.maximize(new Expr().add(x).add(-3, z));
            int row = p.addLe(Expr.of(x), 2);
            p.setIndicator(row, z, 1);
            check(p.solve() == OPTIMAL && near(p.objective(), 10) && near(p.value(z), 0),
                  "an indicator row holds only while its variable is 1");
            Indicator ind = p.model().rowIndicator(row);
            check(ind != null && ind.col() == z.index() && ind.value() == 1,
                  "and reads back as its column and value");
            p.model().setRowIndicator(row, -1, 0);
            check(p.model().rowIndicator(row) == null, "a cleared indicator reads back as none");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(2, 10, false, true);
            Var y = p.addVar(0, 1, false);
            p.addGe(new Expr().add(x).add(y), 1);
            p.minimize(new Expr().add(x).add(5, y));
            check(p.solve() == OPTIMAL && near(p.objective(), 2) && near(p.value(x), 2),
                  "a semi-continuous variable rests above its floor");
            p.minimize(new Expr().add(10, x).add(5, y));
            check(p.solve() == OPTIMAL && near(p.objective(), 5) && near(p.value(x), 0),
                  "or at zero");
            Model m = p.model();
            check(m.colSemicontinuous(0) && !m.colSemicontinuous(1) && !m.colInteger(0),
                  "the marks read back");
            m.setColSemicontinuous(0, false);
            check(!m.colSemicontinuous(0), "and a mark comes off");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(2, 10, false, true);
            Var z = p.addVar(0, 1, true);
            Var y = p.addVar(0, 1, false);
            p.setIndicator(p.addLe(new Expr().add(x).add(y), 5), z, 1);
            p.addSos(1, x, y);
            p.minimize(new Expr().add(x).add(y).add(z));
            p.solve();
            ModelStats st = p.model().statistics();
            check(st.semicontinuousCol() == 1 && st.sosSet() == 1 && st.indicatorRow() == 1
                  && st.integerCol() == 1,
                  "the statistics count the semi-continuous column, the set and the indicator");
        }
    }

    private static Model normModel(Sense sense) {
        Model m = new Model();
        double sg = sense == Sense.MAXIMIZE ? -1 : 1;
        m.loadLp(sense, 0.0, d(sg, 0, 0), d(-INF, -INF, -INF), d(INF, INF, INF), d(3, 4),
                 d(3, 4), l(0, 0, 1, 2), l(0, 1), d(1, 1));
        m.addCone(ConeType.QUADRATIC, l(0, 1, 2));
        return m;
    }

    private static void quadraticAndCones(Path tmp) {
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(-3, -3), d(0, 0), d(3, 3), d(-INF), d(4),
                     l(0, 1, 2), l(0, 0), d(1, 1));
            m.setQuadratic(l(0, 1, 1), l(0, 1, 0), d(2, 2, 1));
            QuadraticTerms q = m.quadratic();
            boolean lower = true, pair = false;
            for (int k = 0; k < q.values().length; k++) {
                lower &= q.rows()[k] >= q.cols()[k];
                pair |= q.rows()[k] == 1 && q.cols()[k] == 0 && q.values()[k] == 1.0;
            }
            check(m.quadraticNz() == 3 && q.values().length == 3 && lower && pair,
                  "a paired Q reads back as its lower triangle");
            check(m.colQuadratic(0) == 2.0 && m.solve() == OPTIMAL && near(m.objective(), -3),
                  "its diagonal reads back and it solves to -3");
            m.setColQuadratic(0, 4.0);
            check(m.colQuadratic(0) == 4.0 && m.quadraticNz() == 3, "a diagonal entry is set alone");
        }

        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(-1, -1), d(-INF, -INF), d(INF, INF), d(-INF), d(2),
                     l(0, 0, 0), l(), d());
            m.setRowQuadratic(0, l(0, 1), l(0, 1), d(2, 2));
            QuadraticTerms q = m.rowQuadratic(0);
            check(m.rowQuadraticNz(0) == 2 && Arrays.equals(q.rows(), l(0, 1))
                  && Arrays.equals(q.values(), d(2, 2)) && m.statistics().quadraticRow() == 1,
                  "a quadratic row reads back");
            check(m.solve() == OPTIMAL && near(m.solution().x()[0], 1.0)
                  && near(m.solution().rowActivity()[0], 2.0),
                  "and solves: x = 1 on the circle's edge");
            m.setRowQuadratic(0, l(), l(), d());
            check(m.rowQuadraticNz(0) == 0 && m.rowQuadratic(0).values().length == 0,
                  "an empty Q clears it");
        }

        try (Model m = normModel(Sense.MINIMIZE)) {
            Cone k = m.cone(0);
            check(m.numCones() == 1 && k.type() == ConeType.QUADRATIC
                  && Arrays.equals(k.cols(), l(0, 1, 2)) && m.statistics().coneSet() == 1,
                  "a cone reads back as its type and columns");
            check(m.solve() == OPTIMAL && near(m.objective(), 5.0), "and solves to the norm 5");
            double[] z = m.coneDual(0);
            Solution s = m.solution();
            CheckReport ck = m.checkConicSolution(s.x(), s.rowDual(), new double[][] {z}, 1e-7);
            check(ck.primalFeasible() && ck.checkedDuals() && ck.dualFeasible()
                  && ck.maxConeViolation() <= 1e-7,
                  "the conic checker accepts the answer with its cone dual");
            check(throwsA(IllegalArgumentException.class, () -> m.checkConicSolution(
                      s.x(), s.rowDual(), new double[][] {Arrays.copyOf(z, 2)}, 1e-7)),
                  "a cone dual of the wrong length never reaches C");
            String path = tmp.resolve("norm.sol").toString();
            m.writeSolution(path);
            double[][] back = m.readConeDuals(path);
            check(back.length == 1 && Arrays.equals(back[0], z),
                  "the cone duals read back from the solution file");
            m.deleteCones(0);
            check(m.numCones() == 0 && throwsA(JaosException.class, () -> m.cone(0)),
                  "a deleted cone is gone");
        }

        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(0, 0), d(-INF, 2), d(1, 2), d(), d(), l(0, 0, 0),
                     l(), d());
            m.addCone(ConeType.QUADRATIC, l(0, 1));
            check(m.solve() == SolveStatus.INFEASIBLE, "an infeasible cone says so");
            double[] z = m.coneDual(0);
            check(m.checkConicCertificate(d(), new double[][] {z}, 1e-7).certified()
                  && !m.checkConicCertificate(d(), new double[][] {d(-1, 0)}, 1e-7).certified(),
                  "its cone certificate certifies, and a wrong one does not");
        }
    }

    private static Problem knapsack5(Var[] v) {
        Problem p = new Problem();
        double[] w = {3, 5, 2, 4, 2}, c = {10, 13, 7, 9, 5};
        Expr weight = new Expr(), value = new Expr();
        for (int k = 0; k < 5; k++) {
            v[k] = p.addVar(0, 1, true);
            weight.add(w[k], v[k]);
            value.add(c[k], v[k]);
        }
        p.addLe(weight, 8);
        p.maximize(value);
        return p;
    }

    private static void pool() {
        Var[] v = new Var[5];
        try (Problem p = knapsack5(v)) {
            Model m = p.model();
            m.setMipCutRounds(0);
            m.setMipCoverRounds(0);
            m.setMipMirRounds(0);
            m.setMipCutDepth(0);
            m.setMipPoolSize(3);
            check(p.solve() == OPTIMAL, "a knapsack with a pool of 3 solves");
            List<Incumbent> pool = p.mipPool();
            boolean sorted = true;
            for (int k = 1; k < pool.size(); k++)
                sorted &= pool.get(k - 1).objective() >= pool.get(k).objective();
            check(!pool.isEmpty() && pool.size() <= 3 && near(pool.get(0).objective(), 23)
                  && pool.get(0).x()[v[0].index()] == 1.0 && pool.get(0).x()[v[1].index()] == 1.0
                  && sorted,
                  "the pool holds up to 3 points, best first: 23 with a and b");
            check(throwsA(JaosException.class, () -> m.setMipPoolSize(0)), "a pool of 0 throws");
        }
    }

    private static void mipSwitchesOff(Model m) {
        m.setMipCutRounds(0);
        m.setMipCoverRounds(0);
        m.setMipMirRounds(0);
        m.setMipCliqueRounds(0);
        m.setMipZeroHalfRounds(0);
        m.setMipFlowCoverRounds(0);
        m.setMipCutDepth(0);
        m.setMipHeuristics(false);
        m.setMipDiveHeuristic(0);
        m.setMipFeaspump(0);
        m.setMipTighten(0);
    }

    private static void callbacks(String data) {
        String solve1 = Path.of(data, "solve1.mps").toString();
        long quietWork;
        try (Model m = Model.read(solve1)) {
            m.solve();
            quietWork = m.workUnits();
        }
        try (Model m = Model.read(solve1)) {
            List<Progress> seen = new ArrayList<>();
            m.setProgressCallback(pr -> {
                seen.add(pr);
                return CallbackAction.CONTINUE;
            });
            check(m.solve() == OPTIMAL && !seen.isEmpty() && seen.get(0).iterations() >= 0
                  && m.workUnits() == quietWork,
                  "the progress callback sees the solve and changes nothing");
            m.setProgressCallback(pr -> CallbackAction.STOP);
            check(m.solve() == SolveStatus.INTERRUPTED && throwsA(JaosException.class, m::objective),
                  "STOP interrupts and leaves nothing to read");
            m.setProgressCallback(pr -> {
                throw new IllegalStateException("broken callback");
            });
            check(throwsA(IllegalStateException.class, m::solve)
                  && m.status() == SolveStatus.INTERRUPTED,
                  "an exception in a callback stops the solve and reaches the caller");
            m.setProgressCallback(null);
            check(m.solve() == OPTIMAL, "and a removed callback lets it run");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(0, INF, true), y = p.addVar(0, INF, true);
            p.addLe(new Expr().add(x).add(y), 3.6);
            p.addLe(Expr.of(x), 2.2);
            p.addLe(Expr.of(y), 1.4);
            p.maximize(new Expr().add(x).add(y));
            Model m = p.model();
            m.setMipCutRounds(0);
            m.setMipCoverRounds(0);
            m.setMipMirRounds(0);
            m.setMipCutDepth(0);
            m.setMipNodeLimit(1);
            List<IncumbentEvent> seen = new ArrayList<>();
            m.setIncumbentCallback(e -> {
                seen.add(e);
                return CallbackAction.CONTINUE;
            });
            check(p.solve() == SolveStatus.NODE_LIMIT && seen.size() == 1
                  && seen.get(0).node() == 1 && seen.get(0).byRounding()
                  && seen.get(0).x()[0] == 2.0 && seen.get(0).x()[1] == 1.0,
                  "the incumbent callback sees the rounded point (2, 1) at node 1");
            check(near(m.mipIncumbent().objective(), 3.0), "and the stopped tree keeps it");
            m.setMipNodeLimit(0);
            m.setIncumbentCallback(e -> CallbackAction.STOP);
            check(p.solve() == SolveStatus.INTERRUPTED && m.mipResult().hasIncumbent(),
                  "STOP from it interrupts with the incumbent kept");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(0, 1, true), y = p.addVar(0, 1, true), z = p.addVar(0, 1, true);
            p.addLe(new Expr().add(x).add(y).add(z), 2);
            p.maximize(new Expr().add(2, x).add(2, y).add(z));
            List<Boolean> integral = new ArrayList<>();
            p.model().setNodeCallback(ev -> {
                integral.add(ev.integral());
                double[] v = ev.x();
                if (ev.integral() && v[0] + v[1] > 1.5)
                    ev.addRow(l(0, 1), d(1, 1), -INF, 1);
                return CallbackAction.CONTINUE;
            });
            check(p.solve() == OPTIMAL && near(p.objective(), 3)
                  && p.value(x) + p.value(y) <= 1 + 1e-9 && integral.contains(true),
                  "a lazy row from the node callback holds: 3");
            p.model().setNodeCallback(null);
            check(p.solve() == OPTIMAL && near(p.objective(), 4), "and without it the answer is 4");
        }

        try (Problem q = new Problem()) {
            Var a = q.addVar(0, 1, true), b = q.addVar(0, 1, true), c = q.addVar(0, 1, true);
            q.addLe(new Expr().add(2, a).add(2, b).add(2, c), 3);
            q.maximize(new Expr().add(3, a).add(2.5, b).add(2, c));
            mipSwitchesOff(q.model());
            q.solve();
            long plain = q.model().mipResult().nodes();
            List<Long> choices = new ArrayList<>();
            List<NodeEvent> kept = new ArrayList<>();
            q.model().setNodeCallback(ev -> {
                kept.add(ev);
                if (ev.node() == 1 && !ev.integral()) {
                    choices.add(ev.branchCol());
                    ev.addRow(l(0, 1, 2), d(1, 1, 1), -INF, 1);
                }
                return CallbackAction.CONTINUE;
            });
            check(plain > 1 && q.solve() == OPTIMAL && near(q.objective(), 3)
                  && q.model().mipResult().nodes() == 1 && choices.get(0) == b.index(),
                  "a user cut at the root closes the tree, and the event names b to branch on");
            check(throwsA(IllegalStateException.class, () -> kept.get(0).branchCol()),
                  "an event is dead once its callback returns");
            q.model().setNodeCallback(ev -> {
                ev.addRow(l(7), d(1), 0, 1);
                return CallbackAction.CONTINUE;
            });
            check(throwsA(JaosException.class, q::solve)
                  && q.status() == SolveStatus.INTERRUPTED,
                  "a row the callback may not add throws and stops the solve");
            q.model().setNodeCallback(ev -> CallbackAction.STOP);
            check(q.solve() == SolveStatus.INTERRUPTED, "STOP from the node callback interrupts");
        }

        try (Problem s = new Problem()) {
            Var a = s.addVar(0, 1, true), b = s.addVar(0, 1, true), c = s.addVar(0, 1, true);
            s.addLe(new Expr().add(2, a).add(2, b), 3);
            s.addLe(new Expr().add(2, a).add(2, c), 3);
            s.maximize(new Expr().add(3, a).add(b).add(c));
            mipSwitchesOff(s.model());
            List<Long> steered = new ArrayList<>();
            List<Double> depth1 = new ArrayList<>();
            s.model().setNodeCallback(ev -> {
                if (ev.depth() == 0 && !ev.integral()) {
                    long other = ev.branchCol() == b.index() ? c.index() : b.index();
                    steered.add(other);
                    ev.setBranchCol(other);
                }
                if (ev.depth() == 1)
                    depth1.add(ev.x()[steered.get(0).intValue()]);
                return CallbackAction.CONTINUE;
            });
            boolean binary = true;
            check(s.solve() == OPTIMAL && near(s.objective(), 3), "a steered tree keeps the optimum 3");
            for (double v : depth1)
                binary &= v == 0.0 || v == 1.0;
            check(!depth1.isEmpty() && binary, "and branches on the column the callback chose");
        }
    }

    private static void checkers(String data) {
        try (Model m = Model.read(Path.of(data, "solve1.mps").toString())) {
            m.solve();
            Solution s = m.solution();
            CheckReport r = m.checkSolution(s.x(), s.rowDual(), 1e-7);
            check(r.primalFeasible() && r.dualFeasible() && r.checkedDuals()
                  && near(r.primalObjective(), 29.0),
                  "the checker accepts the true solution, objective 29");
            double[] wrong = s.x().clone();
            for (int k = 0; k < wrong.length; k++)
                wrong[k] += 100.0;
            r = m.checkSolution(wrong, s.rowDual(), 1e-7);
            check(!r.primalFeasible() && Math.max(r.maxColViolation(), r.maxRowViolation()) > 1.0,
                  "and flags a corrupted one");
            check(!m.checkSolution(s.x(), null, 1e-7).checkedDuals(),
                  "no duals means no dual verdict");
            RayReport ray = m.checkRay(d(1, 0, 0), 1e-7);
            check(!ray.certified() && ray.maxColEscape() > 0.0, "a ray into a bound is refused");
            check(!m.checkCertificate(d(1, 1, 1), 1e-7).certified(),
                  "a feasible model has no certificate to certify");
        }
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(1), d(0), d(2), d(4), d(INF), l(0, 1), l(0), d(1));
            check(m.solve() == SolveStatus.INFEASIBLE, "x in [0, 2] with x >= 4 is infeasible");
            CertificateReport r = m.checkCertificate(m.certificate(), 1e-7);
            check(r.certified() && near(r.infRows(), 4) && near(r.supColumns(), 2) && near(r.gap(), 2),
                  "its certificate certifies with halves 4 and 2");
        }
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(-1), d(0), d(INF), d(-INF), d(INF), l(0, 1), l(0), d(1));
            check(m.solve() == SolveStatus.UNBOUNDED, "min -x over x >= 0 is unbounded");
            double[] ray = m.unboundedRay();
            RayReport r = m.checkRay(ray, 1e-7);
            check(ray[0] > 0 && r.certified() && r.rate() < 0 && r.maxColEscape() == 0.0,
                  "its ray certifies with a falling rate");
        }
    }

    private static Model third() {
        Model m = new Model();
        m.loadLp(Sense.MINIMIZE, 0.0, d(1), d(0), d(INF), d(1), d(INF), l(0, 1), l(0), d(3));
        return m;
    }

    private static void exact(Path tmp) throws IOException {
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.5, d(1), d(0), d(INF), d(1), d(INF), l(0, 1), l(0), d(3));
            check(throwsA(JaosException.class, () -> m.exactColValue(0)),
                  "no exact value before a proof");
            m.solve();
            VerifyReport r = m.verify();
            check(r.status() == Proof.OPTIMAL && r.stage() == ProofStage.NONE
                  && r.capacityBits() == 4096.0 && r.blocks() >= 1 && r.atRow() == -1
                  && r.atCol() == -1 && r.violation() == 0.0,
                  "min x with 3x >= 1 is proved, the report field by field");
            check(m.exactColValue(0).equals("1/3") && m.exactRowDual(0).equals("1/3")
                  && m.exactObjective().equals("5/6"),
                  "x and its dual are 1/3, the objective with its constant 5/6");
        }
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(-1, -1), d(0, 0), d(3, INF), d(-INF), d(4),
                     l(0, 1, 2), l(0, 0), d(1, 1));
            BasisStatus up = BasisStatus.AT_UPPER, lo = BasisStatus.AT_LOWER, in = BasisStatus.BASIC;
            VerifyReport r = m.verifyBasis(new Basis(new BasisStatus[] {up, in},
                                                     new BasisStatus[] {up}));
            check(r.status() == Proof.OPTIMAL && m.status() == SolveStatus.NOT_RUN
                  && m.exactColValue(0).equals("3") && m.exactObjective().equals("-4"),
                  "a basis handed in is proved with no solve");
            r = m.verifyBasis(new Basis(new BasisStatus[] {lo, lo}, new BasisStatus[] {in}));
            check(r.status() == Proof.BROKEN && r.stage() == ProofStage.DUAL,
                  "and the slack basis is broken at the dual stage");
        }

        Path proof = tmp.resolve("x.proof");
        try (Model m = third()) {
            m.solve();
            m.verify();
            m.writeProof(proof.toString());
            String body = Files.readString(proof);
            check(body.contains("proof optimal") && body.contains("1/3"),
                  "the proof file holds the exact optimum");
        }
        try (Model m = third()) {
            ProofReport r = m.checkProof(proof.toString());
            check(r.primal() && r.dual() && r.objective() && r.badRow() == -1 && r.terms() > 0
                  && r.certified() && r.kind() == ProofKind.OPTIMAL,
                  "and the model alone judges it certified");
            List<String> lines = new ArrayList<>(Files.readAllLines(proof));
            for (int k = 0; k < lines.size(); k++)
                if (lines.get(k).startsWith("col "))
                    lines.set(k, lines.get(k).replace("1/3", "1/4"));
            Files.write(proof, lines);
            ProofReport bad = m.checkProof(proof.toString());
            check(!bad.primal() && !bad.certified() && bad.kind() == ProofKind.OPTIMAL,
                  "a proof with a wrong value is refused");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(0, INF, false), y = p.addVar(0, INF, false);
            p.addLe(new Expr().add(x).add(y), 1);
            p.addGe(new Expr().add(x).add(y), 2);
            p.minimize(new Expr().add(x).add(y));
            check(p.solve() == SolveStatus.INFEASIBLE, "x + y <= 1 beside x + y >= 2 is infeasible");
            ExactRayReport r = p.model().exactCertificate();
            check(r.derived() && r.boundBits() <= r.capacityBits()
                  && p.model().exactRowMultiplier(0).equals("-1")
                  && p.model().exactRowMultiplier(1).equals("1"),
                  "its multipliers are derived exactly: -1 and 1");
            p.model().writeProof(proof.toString());
            ProofReport pr = p.model().checkProof(proof.toString());
            check(pr.kind() == ProofKind.INFEASIBLE && pr.certified(),
                  "and its proof file certifies");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(0, INF, false), y = p.addVar(0, INF, false);
            p.addLe(new Expr().add(x).add(-1, y), 1);
            p.minimize(new Expr().add(-1, x));
            check(p.solve() == SolveStatus.UNBOUNDED, "min -x under x - y <= 1 is unbounded");
            ExactRayReport r = p.model().exactUnboundedRay();
            String dx = p.model().exactColDirection(0), dy = p.model().exactColDirection(1);
            check(r.derived() && r.atRow() == -1 && dx.equals(dy) && !dx.startsWith("-")
                  && !dx.equals("0"),
                  "its ray is derived exactly, x and y moving together");
            p.model().writeProof(proof.toString());
            ProofReport pr = p.model().checkProof(proof.toString());
            check(pr.kind() == ProofKind.UNBOUNDED && pr.certified(),
                  "and its proof file certifies");
        }
    }

    private static void iisAndRelaxation() {
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(1), d(0), d(INF), d(1, -INF), d(INF, 0), l(0, 2),
                     l(0, 1), d(1, 1));
            check(m.solve() == SolveStatus.INFEASIBLE, "x >= 1 beside x <= 0 is infeasible");
            Iis found = m.iis();
            check(Arrays.equals(found.rowSide(), new IisSide[] {IisSide.LOWER, IisSide.UPPER})
                  && Arrays.equals(found.colSide(), new IisSide[] {IisSide.NONE})
                  && found.report().members() == 2 && found.report().solves() == 3
                  && found.report().fromCertificate(),
                  "its IIS is both rows and not the bound, the report field by field");
            check(m.status() == SolveStatus.INFEASIBLE && m.certificate().length == 2,
                  "and the model keeps its answer");
            try (Model sub = m.iisModel(found)) {
                check(sub.solve() == SolveStatus.INFEASIBLE && sub.numRow() == 2
                      && sub.numCol() == 1,
                      "the subsystem as a model is infeasible");
            }
            IisSide[] loose = found.rowSide().clone();
            loose[0] = IisSide.NONE;
            try (Model sub = m.iisModel(loose, found.colSide())) {
                check(sub.solve() == OPTIMAL, "and one member less makes it feasible");
            }
            check(throwsA(IllegalArgumentException.class,
                          () -> m.iisModel(new IisSide[] {IisSide.NONE}, found.colSide())),
                  "sides of the wrong length never reach C");
        }
        try (Model m = new Model()) {
            m.loadLp(Sense.MINIMIZE, 0.0, d(1, 1), d(0, 0), d(10, 10), d(30), d(INF),
                     l(0, 1, 2), l(0, 0), d(1, 1));
            Relaxation r = m.feasrelax(RelaxScope.BOTH);
            check(near(r.report().total(), 10) && r.rowMove()[0] == -10.0
                  && r.colMove()[0] == 0.0 && r.colMove()[1] == 0.0,
                  "the smallest relaxation lowers the row's floor by 10");
            Relaxation rows = m.feasrelax(RelaxScope.ROWS), cols = m.feasrelax(RelaxScope.COLS);
            check(rows.report().colsMoved() == 0 && rows.report().rowsMoved() == 1
                  && cols.report().rowsMoved() == 0 && cols.report().colsMoved() == 1
                  && near(rows.report().total(), cols.report().total()),
                  "the scope decides which bound moves");
            check(m.status() == SolveStatus.NOT_RUN && r.report().workUnits() > 0
                  && r.report().status() == OPTIMAL,
                  "and the model is left unsolved");
        }
    }

    private static void ranging() {
        try (Model m = new Model()) {
            m.loadLp(Sense.MAXIMIZE, 0.0, d(1, 1), d(0, 0), d(INF, INF), d(-INF, -INF), d(4, 6),
                     l(0, 2, 4), l(0, 1, 0, 1), d(1, 3, 2, 1));
            check(throwsA(JaosException.class, m::costRanging), "ranging needs an optimum");
            m.solve();
            CostRanging c = m.costRanging();
            check(near(c.lower()[0], 0.5) && near(c.upper()[0], 3.0) && near(c.lower()[1], 1.0 / 3)
                  && near(c.upper()[1], 2.0),
                  "cost ranging reads the textbook intervals");
            BoundRanging r = m.rhsRanging();
            check(near(r.upperLo()[0], 2.0) && near(r.upperHi()[0], 12.0)
                  && r.lowerLo()[1] == -INF && near(r.lowerHi()[1], 6.0),
                  "and so does rhs ranging");
            BoundRanging b = m.boundRanging();
            check(b.lowerLo()[0] == -INF && near(b.lowerHi()[0], 1.6) && near(b.upperLo()[1], 1.2)
                  && b.upperHi()[1] == INF,
                  "and bound ranging");
        }
    }

    private static void reports() {
        try (Problem p = new Problem()) {
            Var a = p.addVar(0, 1, true), b = p.addVar(0, 1, true), c = p.addVar(0, 1, true);
            p.addLe(new Expr().add(3, a).add(5, b).add(2, c), 8);
            p.maximize(new Expr().add(10, a).add(13, b).add(7, c));
            p.solve();
            ModelStats st = p.model().statistics();
            check(st.numCol() == 3 && st.numRow() == 1 && st.integerCol() == 3
                  && st.binaryCol() == 3 && st.oneSidedRow() == 1 && st.minAbs() == 2.0
                  && st.maxAbs() == 5.0 && st.objMinAbs() == 7.0 && st.objMaxAbs() == 13.0
                  && st.numRow() == st.equalityRow() + st.rangedRow() + st.oneSidedRow() + st.freeRow(),
                  "the statistics count the knapsack field by field");
        }
        try (Problem p = new Problem()) {
            Var[] x = {p.addVar(0, INF, false), p.addVar(0, INF, false), p.addVar(0, INF, false),
                       p.addVar(0, INF, false)};
            p.addEq(new Expr().add(x[0]).add(-1, x[1]), 1);
            p.addGe(new Expr().add(x[0]).add(x[2]), 3);
            p.addLe(new Expr().add(x[0]).add(x[3]), 10);
            p.addGe(new Expr().add(x[1]).add(x[2]).add(x[3]), 2);
            p.minimize(new Expr().add(2, x[0]).add(x[1]).add(3, x[2]).add(x[3]));
            check(p.solve() == OPTIMAL && near(p.objective(), 8.0)
                  && p.model().presolveReport().aggregatedCol() == 1
                  && p.model().presolveReport().rounds() >= 0,
                  "the presolve report counts one aggregated column");
        }
        for (int on = 1; on >= 0; on--) {
            try (Problem p = new Problem()) {
                Var[] x = {p.addVar(0, 1, true), p.addVar(0, 1, true), p.addVar(0, 1, true)};
                p.addGe(new Expr().add(x[0]).add(x[1]).add(x[2]), 1);
                p.minimize(new Expr().add(x[0]).add(x[1]).add(x[2]));
                p.model().setMipSymmetry(on);
                p.model().setMipOrbital(on);
                p.solve();
                MipReport r = p.model().mipResult();
                check(r.symmetryOrbits() == on && (on == 0 || r.symmetryGenerators() >= 1),
                      "the MIP report's symmetry fields land, detection " + (on == 1 ? "on" : "off"));
            }
        }
        long[] moved = new long[2];
        for (int arm = 0; arm < 2; arm++) {
            try (Model m = new Model()) {
                m.loadLp(Sense.MAXIMIZE, 0.0, d(3, 2.4, 2, 1), d(0, 0, 0, 0), d(10, 10, 10, 10),
                         d(-INF, -INF), d(3, 3), l(0, 2, 4, 5, 6), l(0, 1, 0, 1, 0, 0),
                         d(1, 2, 1, 1, 1, 1));
                for (int j = 0; j < 4; j++)
                    m.setColInteger(j, true);
                m.setMipPropagate(arm == 0 ? 0 : 4);
                m.solve();
                moved[arm] = m.mipResult().tightened();
                check(near(m.objective(), 7.4), "the propagation model solves to 7.4, arm " + arm);
            }
        }
        check(moved[0] == 0 && moved[1] >= 5, "and the report's tightened count lands: 0, then 5 or more");
    }

    private static void reads(Model m, List<String> wrong, String option, String want) {
        String got = m.getOption(option);
        boolean same = got.equals(want);
        if (!same) {
            try {
                same = Double.parseDouble(got) == Double.parseDouble(want);
            } catch (NumberFormatException e) {
                same = false;
            }
        }
        if (!same)
            wrong.add(option + "=" + got);
    }

    private static void typedSetters() {
        try (Model m = new Model()) {
            List<String> wrong = new ArrayList<>();
            m.setWorkLimit(12345); reads(m, wrong, "work_limit", "12345");
            m.setTimeLimit(99.5); reads(m, wrong, "time_limit", "99.5");
            m.setPrimalTolerance(1e-6); reads(m, wrong, "primal_tolerance", "1e-6");
            m.setDualTolerance(1e-8); reads(m, wrong, "dual_tolerance", "1e-8");
            m.setAlgorithm(Algorithm.BARRIER); reads(m, wrong, "algorithm", "barrier");
            m.setThreads(2); reads(m, wrong, "threads", "2");
            m.setMipGap(0.002); reads(m, wrong, "mip_gap", "0.002");
            m.setMipNodeLimit(77); reads(m, wrong, "mip_node_limit", "77");
            m.setMipTreeBatch(4); reads(m, wrong, "mip_tree_batch", "4");
            m.setMipBranching(Branching.MOST_FRACTIONAL);
            reads(m, wrong, "mip_branching", "most-fractional");
            m.setMipReliability(8); reads(m, wrong, "mip_reliability", "8");
            m.setMipProbeCap(2.5); reads(m, wrong, "mip_probe_cap", "2.5");
            m.setMipProbeDepth(3); reads(m, wrong, "mip_probe_depth", "3");
            m.setMipCutRounds(2); reads(m, wrong, "mip_cut_rounds", "2");
            m.setMipCutDepth(5); reads(m, wrong, "mip_cut_depth", "5");
            m.setMipCutDrop(false); reads(m, wrong, "mip_cut_drop", "false");
            m.setMipNodeCutCap(7); reads(m, wrong, "mip_node_cut_cap", "7");
            m.setMipCoverRounds(2); reads(m, wrong, "mip_cover_rounds", "2");
            m.setMipCutStall(0.25); reads(m, wrong, "mip_cut_stall", "0.25");
            m.setMipNodeCutStall(0.125); reads(m, wrong, "mip_node_cut_stall", "0.125");
            m.setMipRootCutDrop(0); reads(m, wrong, "mip_root_cut_drop", "false");
            m.setMipCoverLift(1); reads(m, wrong, "mip_cover_lift", "true");
            m.setMipMirRounds(3); reads(m, wrong, "mip_mir_rounds", "3");
            m.setMipNodeMir(1); reads(m, wrong, "mip_node_mir", "true");
            m.setMipMirAggregate(2); reads(m, wrong, "mip_mir_aggregate", "2");
            m.setMipDive(true); reads(m, wrong, "mip_dive", "true");
            m.setMipDiveChild(DiveChild.UP); reads(m, wrong, "mip_dive_child", "up");
            m.setMipDiveBacktrack(2); reads(m, wrong, "mip_dive_backtrack", "2");
            m.setMipDiveGap(0.5); reads(m, wrong, "mip_dive_gap", "0.5");
            m.setMipDiveDegrade(0.25); reads(m, wrong, "mip_dive_degrade", "0.25");
            m.setMipDiveHeuristic(10); reads(m, wrong, "mip_dive_heuristic", "10");
            m.setMipDiveHeuristicDepth(2); reads(m, wrong, "mip_dive_heuristic_depth", "2");
            m.setMipRins(5); reads(m, wrong, "mip_rins", "5");
            m.setMipFeaspump(3); reads(m, wrong, "mip_feaspump", "3");
            m.setMipPumpGeneral(1); reads(m, wrong, "mip_pump_general", "true");
            m.setMipPumpObj(0.25); reads(m, wrong, "mip_pump_obj", "0.25");
            m.setMipPumpAlways(1); reads(m, wrong, "mip_pump_always", "true");
            m.setMipRcfix(1); reads(m, wrong, "mip_rcfix", "true");
            m.setMipTighten(0); reads(m, wrong, "mip_tighten", "false");
            m.setMipProbing(1); reads(m, wrong, "mip_probing", "true");
            m.setMipProbingCap(3.0); reads(m, wrong, "mip_probing_cap", "3");
            m.setMipCliqueFix(1); reads(m, wrong, "mip_clique_fix", "true");
            m.setMipConflicts(0); reads(m, wrong, "mip_conflicts", "false");
            m.setMipSymmetry(1); reads(m, wrong, "mip_symmetry", "true");
            m.setMipOrbital(0); reads(m, wrong, "mip_orbital", "false");
            m.setMipPropagate(2); reads(m, wrong, "mip_propagate", "2");
            m.setMipPropagateDepth(4); reads(m, wrong, "mip_propagate_depth", "4");
            m.setMipHeuristics(false); reads(m, wrong, "mip_heuristics", "false");
            m.setMipPoolSize(5); reads(m, wrong, "mip_pool_size", "5");
            m.setMipCutoff(123.5); reads(m, wrong, "mip_cutoff", "123.5");
            m.setMipCliqueRounds(2); reads(m, wrong, "mip_clique_rounds", "2");
            m.setMipZeroHalfRounds(3); reads(m, wrong, "mip_zero_half_rounds", "3");
            m.setMipFlowCoverRounds(2); reads(m, wrong, "mip_flow_cover_rounds", "2");
            m.setMipLocalBranching(6); reads(m, wrong, "mip_local_branching", "6");
            m.setMipNodeSelect(0); reads(m, wrong, "mip_node_select", "0");
            m.setMipRestart(1); reads(m, wrong, "mip_restart", "true");
            check(wrong.isEmpty(), "each of the 56 typed setters reaches its option"
                  + (wrong.isEmpty() ? "" : ": " + wrong));
        }
    }

    private static void options(String data, Path tmp) throws IOException {
        typedSetters();
        try (Model m = Model.read(Path.of(data, "solve1.mps").toString())) {
            Path f = tmp.resolve("opts.txt");
            Files.writeString(f, "mip_cut_rounds 2\n# a comment\nalgorithm primal\n");
            m.readOptions(f.toString());
            check(m.getOption("mip_cut_rounds").equals("2") && m.algorithm() == Algorithm.PRIMAL,
                  "an options file is read");
            m.setThreads(3);
            m.setAlgorithm(Algorithm.DUAL);
            check(m.threads() == 3 && m.algorithm() == Algorithm.DUAL,
                  "the thread count and the algorithm read back");
            check(m.solve() == OPTIMAL && m.solveTime() >= 0.0 && m.workUnits() > 0
                  && m.iterations() >= 0,
                  "and the solve reports its time, work and iterations");
        }
    }

    private static void problemLayer() {
        try (Problem p = new Problem()) {
            Var x = p.addVar(0, INF, false), y = p.addVar(0, INF, false);
            int row = p.addLe(new Expr().add(x).add(y), 4);
            p.minimize(new Expr().add(-1, x).add(-2, y));
            check(p.solve() == OPTIMAL && near(p.objective(), -8),
                  "the golden problem solves to -8 in " + p.model().iterations() + " iterations");
            p.setRowBounds(row, -INF, 3);
            check(throwsA(IllegalStateException.class, () -> p.value(y))
                  && throwsA(IllegalStateException.class, p::objective),
                  "a moved row refuses a stale value");
            check(p.solve() == OPTIMAL && near(p.objective(), -6) && near(p.value(y), 3),
                  "and re-solves to -6");
            p.setBounds(x, 1, INF);
            check(p.solve() == OPTIMAL && near(p.objective(), -5), "a moved bound gives -5");
            p.minimize(new Expr().add(-3, x).add(-2, y));
            check(p.solve() == OPTIMAL && near(p.objective(), -9), "a new objective gives -9");
            p.minimize(new Expr().add(-3, x).add(-2, y).add(100));
            check(p.solve() == OPTIMAL && near(p.objective(), 91)
                  && p.model().objectiveOffset() == 100.0 && p.model().iterations() == 0,
                  "a new constant goes through its setter and re-solves warm: 91, 0 iterations");
            p.maximize(new Expr().add(-3, x).add(-2, y).add(100));
            check(p.solve() == OPTIMAL && near(p.objective(), 97)
                  && p.model().sense() == Sense.MAXIMIZE,
                  "a new sense goes through its setter: 97");
            p.setCost(y, 5);
            check(p.solve() == OPTIMAL && near(p.objective(), 107), "a new cost gives 107");
            Var z = p.addVar(0, 2, false);
            p.setCost(z, 4);
            check(p.solve() == OPTIMAL && near(p.objective(), 115) && near(p.value(z), 2)
                  && p.model().numCol() == 3,
                  "a new variable reloads the model: 115");
            Expr e = new Expr().add(x).add(new Expr().add(2, y).add(1));
            check(near(p.value(e), 1 + 4 + 1) && near(p.activity(row), 3)
                  && near(p.reducedCost(x), p.model().solution().reducedCost()[0]),
                  "an expression that adds an expression has a value, and the row its activity");
        }

        double got;
        try (Problem p = new Problem()) {
            Var[] xs = {p.addVar(0, 4, false), p.addVar(0, 3, false), p.addVar(0, 2, true)};
            p.addLe(new Expr().add(xs[0]).add(xs[1]).add(xs[2]), 5);
            p.minimize(new Expr().add(-1, xs[0]).add(-2, xs[1]).add(-3, xs[2]));
            p.solve();
            p.setBounds(xs[0], 0, 2);
            p.solve();
            p.addLe(new Expr().add(2, xs[0]).add(xs[1]), 3);
            p.solve();
            p.maximize(new Expr().add(xs[0]).add(2, xs[1]).add(3, xs[2]));
            p.solve();
            p.setBounds(xs[1], 1, 3);
            p.solve();
            Var w = p.addVar(0, 1, false);
            p.maximize(new Expr().add(xs[0]).add(2, xs[1]).add(3, xs[2]).add(5, w));
            check(p.solve() == OPTIMAL, "a run of warm and cold changes solves");
            got = p.objective();
        }
        try (Problem q = new Problem()) {
            Var[] ys = {q.addVar(0, 2, false), q.addVar(1, 3, false), q.addVar(0, 2, true),
                        q.addVar(0, 1, false)};
            q.addLe(new Expr().add(ys[0]).add(ys[1]).add(ys[2]), 5);
            q.addLe(new Expr().add(2, ys[0]).add(ys[1]), 3);
            q.maximize(new Expr().add(ys[0]).add(2, ys[1]).add(3, ys[2]).add(5, ys[3]));
            check(q.solve() == OPTIMAL && near(got, q.objective()) && near(got, 17),
                  "and agrees with a fresh build of the end state: 17");
        }

        try (Problem p = new Problem()) {
            Var x = p.addVar(-INF, INF, false), y = p.addVar(-INF, INF, false);
            int ball = p.addLe(new Expr(), 2);
            p.addRowQuadratic(ball, x, x, 1);
            p.addRowQuadratic(ball, y, y, 1);
            p.minimize(new Expr().add(-1, x).add(-1, y));
            check(p.solve() == OPTIMAL && near(p.objective(), -2) && near(p.value(x), 1)
                  && near(p.activity(ball), 2) && Math.abs(p.dual(ball) + 0.5) <= 1e-5,
                  "a quadratic row in a problem: x² + y² <= 2 gives -2 with dual -0.5");
        }

        Var[] v = new Var[5];
        try (Problem p = knapsack5(v)) {
            p.model().setMipStart(d(1, 1, 0, 0, 0));
            check(p.solve() == OPTIMAL && near(p.objective(), 23), "a MIP start set before the first solve keeps 23");
            p.model().setMipCutoff(1000.0);
            check(p.solve() == SolveStatus.INFEASIBLE, "and a cutoff past the optimum answers infeasible");
        }
    }
}
