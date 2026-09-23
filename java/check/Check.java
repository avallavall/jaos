// SPDX-License-Identifier: Apache-2.0
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import org.jaos.ConeType;
import org.jaos.Expr;
import org.jaos.JaosException;
import org.jaos.LogLevel;
import org.jaos.MipReport;
import org.jaos.Model;
import org.jaos.Problem;
import org.jaos.Solution;
import org.jaos.SolveStatus;
import org.jaos.Status;
import org.jaos.Var;

/** The Java binding's checks: one line each, exit 1 when any fails. */
public final class Check {
    private static int passed, failed;

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

    public static void main(String[] args) {
        String data = args.length > 0 ? args[0] : "tests/data";
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
            check(java.util.Arrays.asList(Model.optionNames()).contains("mip_gap"),
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
        }

        System.out.println(passed + " passed, " + failed + " failed");
        System.exit(failed == 0 ? 0 : 1);
    }
}
