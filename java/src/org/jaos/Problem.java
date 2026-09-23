// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

/**
 * A model written in variables and expressions and loaded into a
 * {@link Model} whole at {@link #solve}:
 * <pre>
 * Problem p = new Problem();
 * Var x = p.addVar(0, 4, false);
 * Var y = p.addVar(0, Double.POSITIVE_INFINITY, true);
 * p.addLe(new Expr().add(x).add(y), 4);
 * p.maximize(new Expr().add(x).add(2, y));
 * p.solve();
 * </pre>
 */
public final class Problem implements AutoCloseable {
    private record RowSpec(Expr e, double lo, double hi) {}

    private record Cone(ConeType type, long[] cols) {}

    private final List<Double> lower = new ArrayList<>(), upper = new ArrayList<>();
    private final List<Boolean> integer = new ArrayList<>();
    private final List<RowSpec> rows = new ArrayList<>();
    private final TreeMap<Long, Double> quad = new TreeMap<>();
    private final List<Cone> cones = new ArrayList<>();
    private Expr objective = new Expr();
    private Sense sense = Sense.MINIMIZE;
    private Model model = new Model();
    private Solution solution;

    public Var addVar(double lb, double ub, boolean isInteger) {
        lower.add(lb);
        upper.add(ub);
        integer.add(isInteger);
        return new Var(lower.size() - 1);
    }

    /** A row {@code lo <= e <= hi}, the constant of {@code e} moved to its sides; returns its index. */
    public int addRange(Expr e, double lo, double hi) {
        rows.add(new RowSpec(e, lo - e.constant, hi - e.constant));
        return rows.size() - 1;
    }

    public int addLe(Expr e, double rhs) { return addRange(e, Double.NEGATIVE_INFINITY, rhs); }
    public int addGe(Expr e, double rhs) { return addRange(e, rhs, Double.POSITIVE_INFINITY); }
    public int addEq(Expr e, double rhs) { return addRange(e, rhs, rhs); }

    /** Adds {@code coef * a * b} to the objective, {@code coef * a²} when they are one variable. */
    public void addQuadratic(Var a, Var b, double coef) {
        int i = Math.max(a.index, b.index), j = Math.min(a.index, b.index);
        long key = ((long) i << 32) | j;
        quad.merge(key, a.index == b.index ? 2.0 * coef : coef, Double::sum);
    }

    public void addCone(ConeType type, Var... vars) {
        long[] cols = new long[vars.length];
        for (int k = 0; k < vars.length; k++)
            cols[k] = vars[k].index;
        cones.add(new Cone(type, cols));
    }

    public void minimize(Expr e) {
        objective = e;
        sense = Sense.MINIMIZE;
    }

    public void maximize(Expr e) {
        objective = e;
        sense = Sense.MAXIMIZE;
    }

    /** Loads the problem into a fresh {@link Model} and solves it. */
    public SolveStatus solve() {
        int nc = lower.size(), nr = rows.size();
        double[] cost = new double[nc];
        for (int k = 0; k < objective.cols.size(); k++)
            cost[objective.cols.get(k)] += objective.coefs.get(k);
        List<List<double[]>> byCol = new ArrayList<>();
        for (int j = 0; j < nc; j++)
            byCol.add(new ArrayList<>());
        for (int i = 0; i < nr; i++) {
            TreeMap<Integer, Double> merged = new TreeMap<>();
            Expr e = rows.get(i).e();
            for (int k = 0; k < e.cols.size(); k++)
                merged.merge(e.cols.get(k), e.coefs.get(k), Double::sum);
            for (Map.Entry<Integer, Double> t : merged.entrySet())
                if (t.getValue() != 0.0)
                    byCol.get(t.getKey()).add(new double[] {i, t.getValue()});
        }
        long[] start = new long[nc + 1];
        for (int j = 0; j < nc; j++)
            start[j + 1] = start[j] + byCol.get(j).size();
        long[] index = new long[(int) start[nc]];
        double[] value = new double[(int) start[nc]];
        for (int j = 0, p = 0; j < nc; j++)
            for (double[] t : byCol.get(j)) {
                index[p] = (long) t[0];
                value[p++] = t[1];
            }
        double[] cl = new double[nc], cu = new double[nc];
        for (int j = 0; j < nc; j++) {
            cl[j] = lower.get(j);
            cu[j] = upper.get(j);
        }
        double[] rl = new double[nr], ru = new double[nr];
        for (int i = 0; i < nr; i++) {
            rl[i] = rows.get(i).lo();
            ru[i] = rows.get(i).hi();
        }
        Model m = new Model();
        m.loadLp(sense, objective.constant, cost, cl, cu, rl, ru, start, index, value);
        for (int j = 0; j < nc; j++)
            if (integer.get(j))
                m.setColInteger(j, true);
        if (!quad.isEmpty()) {
            long[] qi = new long[quad.size()], qj = new long[quad.size()];
            double[] qv = new double[quad.size()];
            int k = 0;
            for (Map.Entry<Long, Double> t : quad.entrySet()) {
                qi[k] = t.getKey() >>> 32;
                qj[k] = t.getKey() & 0xffffffffL;
                qv[k++] = t.getValue();
            }
            m.setQuadratic(qi, qj, qv);
        }
        for (Cone c : cones)
            m.addCone(c.type(), c.cols());
        model.copySettingsTo(m);
        model.close();
        model = m;
        solution = null;
        m.solve();
        if (m.status() == SolveStatus.OPTIMAL)
            solution = m.solution();
        return m.status();
    }

    /** The model the last {@link #solve} loaded, for every other call. */
    public Model model() { return model; }

    public SolveStatus status() { return model.status(); }

    public double objective() { return model.objective(); }

    private Solution answer() {
        if (solution == null)
            throw new IllegalStateException(
                "the last solve did not end optimal, so there are no values");
        return solution;
    }

    public double value(Var v) { return answer().x()[v.index]; }

    public double dual(int row) { return answer().rowDual()[row]; }

    @Override
    public void close() { model.close(); }
}
