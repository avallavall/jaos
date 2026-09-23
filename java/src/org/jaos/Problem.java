// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.TreeSet;

/**
 * A model written in variables and expressions:
 * <pre>
 * Problem p = new Problem();
 * Var x = p.addVar(0, 4, false);
 * Var y = p.addVar(0, Double.POSITIVE_INFINITY, true);
 * p.addLe(new Expr().add(x).add(y), 4);
 * p.maximize(new Expr().add(x).add(2, y));
 * p.solve();
 * </pre>
 * The first {@link #solve} loads it into its {@link Model} whole. After
 * that, bounds, costs, the sense and the constant reach the C setters and the
 * next solve starts warm; a new variable, row or structure reloads the model
 * and the next solve starts cold. The model, and every setting on it, stays
 * the same for the life of the problem.
 */
public final class Problem implements AutoCloseable {
    private static final class RowSpec {
        final Expr e;
        double lo, hi;

        RowSpec(Expr e, double lo, double hi) {
            this.e = e;
            this.lo = lo;
            this.hi = hi;
        }
    }

    private final List<Double> lower = new ArrayList<>(), upper = new ArrayList<>();
    private final List<Boolean> integer = new ArrayList<>(), semi = new ArrayList<>();
    private final List<RowSpec> rows = new ArrayList<>();
    private final TreeMap<Long, Double> quad = new TreeMap<>();
    private final TreeMap<Integer, TreeMap<Long, Double>> rowQuad = new TreeMap<>();
    private final List<Cone> cones = new ArrayList<>();
    private final List<Sos> sets = new ArrayList<>();
    private final TreeMap<Integer, Indicator> indicators = new TreeMap<>();
    private TreeMap<Integer, Double> cost = new TreeMap<>();
    private double constant;
    private Sense sense = Sense.MINIMIZE;
    private final Model model = new Model();
    private Solution solution;
    private boolean loaded, structural, objectiveMoved;
    private final TreeSet<Integer> colsMoved = new TreeSet<>(), costsMoved = new TreeSet<>(),
                                   rowsMoved = new TreeSet<>();

    public Var addVar(double lb, double ub, boolean isInteger) {
        return addVar(lb, ub, isInteger, false);
    }

    /** A semi-continuous variable rests at zero or inside [lb, ub]. */
    public Var addVar(double lb, double ub, boolean isInteger, boolean semicontinuous) {
        lower.add(lb);
        upper.add(ub);
        integer.add(isInteger);
        semi.add(semicontinuous);
        touch();
        return new Var(lower.size() - 1);
    }

    /** A row {@code lo <= e <= hi}, the constant of {@code e} moved to its sides; returns its index. */
    public int addRange(Expr e, double lo, double hi) {
        rows.add(new RowSpec(e.copy(), lo - e.constant, hi - e.constant));
        touch();
        return rows.size() - 1;
    }

    public int addLe(Expr e, double rhs) { return addRange(e, Double.NEGATIVE_INFINITY, rhs); }
    public int addGe(Expr e, double rhs) { return addRange(e, rhs, Double.POSITIVE_INFINITY); }
    public int addEq(Expr e, double rhs) { return addRange(e, rhs, rhs); }

    private static long key(Var a, Var b) {
        int i = Math.max(a.index, b.index), j = Math.min(a.index, b.index);
        return ((long) i << 32) | j;
    }

    /** Adds {@code coef * a * b} to the objective, {@code coef * a²} when they are one variable. */
    public void addQuadratic(Var a, Var b, double coef) {
        quad.merge(key(a, b), a.index == b.index ? 2.0 * coef : coef, Double::sum);
        touch();
    }

    /** Adds {@code coef * a * b} to row {@code row}'s activity; the row then keeps one finite side. */
    public void addRowQuadratic(int row, Var a, Var b, double coef) {
        rowQuad.computeIfAbsent(row, r -> new TreeMap<>())
               .merge(key(a, b), a.index == b.index ? 2.0 * coef : coef, Double::sum);
        touch();
    }

    public void addCone(ConeType type, Var... vars) {
        long[] cols = new long[vars.length];
        for (int k = 0; k < vars.length; k++)
            cols[k] = vars[k].index;
        cones.add(new Cone(type, cols));
        touch();
    }

    /** A special ordered set of type 1 or 2 over {@code vars}, ordered by {@code weights}. */
    public void addSos(int type, Var[] vars, double[] weights) {
        if (weights.length != vars.length)
            throw new IllegalArgumentException("one weight per variable");
        long[] cols = new long[vars.length];
        for (int k = 0; k < vars.length; k++)
            cols[k] = vars[k].index;
        sets.add(new Sos(type, cols, weights.clone()));
        touch();
    }

    /** A special ordered set weighted 1, 2, 3, ... in the order given. */
    public void addSos(int type, Var... vars) {
        double[] w = new double[vars.length];
        for (int k = 0; k < w.length; k++)
            w[k] = k + 1;
        addSos(type, vars, w);
    }

    /** Row {@code row} holds only while integer variable {@code z} equals {@code value}, 0 or 1. */
    public void setIndicator(int row, Var z, int value) {
        indicators.put(row, new Indicator(z.index, value));
        touch();
    }

    public void minimize(Expr e) { setObjective(e, Sense.MINIMIZE); }
    public void maximize(Expr e) { setObjective(e, Sense.MAXIMIZE); }

    private void setObjective(Expr e, Sense s) {
        TreeMap<Integer, Double> next = new TreeMap<>();
        for (int k = 0; k < e.cols.size(); k++)
            next.merge(e.cols.get(k), e.coefs.get(k), Double::sum);
        TreeSet<Integer> all = new TreeSet<>(cost.keySet());
        all.addAll(next.keySet());
        for (int j : all)
            if (cost.getOrDefault(j, 0.0).doubleValue() != next.getOrDefault(j, 0.0).doubleValue())
                moved(costsMoved, j);
        cost = next;
        setSense(s);
        setConstant(e.constant);
    }

    /** One variable's objective coefficient. */
    public void setCost(Var v, double c) {
        if (cost.getOrDefault(v.index, 0.0).doubleValue() != c)
            moved(costsMoved, v.index);
        cost.put(v.index, c);
    }

    public void setSense(Sense s) {
        if (s != sense) {
            sense = s;
            objectiveMoved();
        }
    }

    /** The objective's constant term. */
    public void setConstant(double c) {
        if (c != constant) {
            constant = c;
            objectiveMoved();
        }
    }

    /** New bounds for {@code v}. */
    public void setBounds(Var v, double lb, double ub) {
        lower.set(v.index, lb);
        upper.set(v.index, ub);
        moved(colsMoved, v.index);
    }

    /** New sides for row {@code row}, its expression's constant moved to them as {@link #addRange} does. */
    public void setRowBounds(int row, double lo, double hi) {
        RowSpec r = rows.get(row);
        r.lo = lo - r.e.constant;
        r.hi = hi - r.e.constant;
        moved(rowsMoved, row);
    }

    public double lower(Var v) { return lower.get(v.index); }
    public double upper(Var v) { return upper.get(v.index); }

    private void touch() {
        solution = null;
        if (loaded)
            structural = true;
    }

    private void moved(TreeSet<Integer> set, int k) {
        solution = null;
        if (loaded && !structural)
            set.add(k);
    }

    private void objectiveMoved() {
        solution = null;
        if (loaded && !structural)
            objectiveMoved = true;
    }

    private boolean pending() {
        return !loaded || structural || objectiveMoved || !costsMoved.isEmpty()
            || !colsMoved.isEmpty() || !rowsMoved.isEmpty();
    }

    private void build() {
        int nc = lower.size(), nr = rows.size();
        double[] c = new double[nc];
        for (Map.Entry<Integer, Double> t : cost.entrySet())
            c[t.getKey()] = t.getValue();
        List<List<double[]>> byCol = new ArrayList<>();
        for (int j = 0; j < nc; j++)
            byCol.add(new ArrayList<>());
        for (int i = 0; i < nr; i++) {
            TreeMap<Integer, Double> merged = new TreeMap<>();
            Expr e = rows.get(i).e;
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
            rl[i] = rows.get(i).lo;
            ru[i] = rows.get(i).hi;
        }
        model.loadLp(sense, constant, c, cl, cu, rl, ru, start, index, value);
        for (int j = 0; j < nc; j++) {
            if (integer.get(j))
                model.setColInteger(j, true);
            if (semi.get(j))
                model.setColSemicontinuous(j, true);
        }
        if (!quad.isEmpty())
            model.setQuadratic(qRows(quad), qCols(quad), qValues(quad));
        for (Map.Entry<Integer, TreeMap<Long, Double>> t : rowQuad.entrySet())
            model.setRowQuadratic(t.getKey(), qRows(t.getValue()), qCols(t.getValue()),
                                  qValues(t.getValue()));
        for (Sos s : sets)
            model.addSos(s.type(), s.cols(), s.weights());
        for (Map.Entry<Integer, Indicator> t : indicators.entrySet())
            model.setRowIndicator(t.getKey(), t.getValue().col(), t.getValue().value());
        for (Cone k : cones)
            model.addCone(k.type(), k.cols());
        model.restoreMipStart();
        loaded = true;
        structural = false;
        objectiveMoved = false;
        costsMoved.clear();
        colsMoved.clear();
        rowsMoved.clear();
    }

    private static long[] qRows(TreeMap<Long, Double> q) {
        return q.keySet().stream().mapToLong(k -> k >>> 32).toArray();
    }

    private static long[] qCols(TreeMap<Long, Double> q) {
        return q.keySet().stream().mapToLong(k -> k & 0xffffffffL).toArray();
    }

    private static double[] qValues(TreeMap<Long, Double> q) {
        return q.values().stream().mapToDouble(Double::doubleValue).toArray();
    }

    private void load() {
        if (!loaded || structural) {
            build();
            return;
        }
        if (objectiveMoved) {
            model.setSense(sense);
            model.setObjectiveOffset(constant);
        }
        for (int j : costsMoved)
            model.setColCost(j, cost.getOrDefault(j, 0.0));
        for (int j : colsMoved)
            model.setColBounds(j, lower.get(j), upper.get(j));
        for (int i : rowsMoved)
            model.setRowBounds(i, rows.get(i).lo, rows.get(i).hi);
        objectiveMoved = false;
        costsMoved.clear();
        colsMoved.clear();
        rowsMoved.clear();
    }

    /** Loads what changed and solves: warm after bound, cost, sense and constant changes, cold after the rest. */
    public SolveStatus solve() {
        solution = null;
        load();
        SolveStatus s = model.solve();
        if (s == SolveStatus.OPTIMAL)
            solution = model.solution();
        return s;
    }

    /** The model underneath, for every other call; the same one for the life of the problem. */
    public Model model() { return model; }

    public SolveStatus status() { return model.status(); }

    private void settled() {
        if (pending())
            throw new IllegalStateException(
                "the problem changed since the last solve; call solve() before reading values");
    }

    public double objective() {
        settled();
        return model.objective();
    }

    private Solution answer() {
        settled();
        if (solution == null)
            throw new IllegalStateException(
                "the last solve did not end optimal, so there are no values");
        return solution;
    }

    public double value(Var v) { return answer().x()[v.index]; }

    /** The expression at the held solution. */
    public double value(Expr e) {
        double[] x = answer().x();
        double s = e.constant;
        for (int k = 0; k < e.cols.size(); k++)
            s += e.coefs.get(k) * x[e.cols.get(k)];
        return s;
    }

    public double reducedCost(Var v) { return answer().reducedCost()[v.index]; }

    public double dual(int row) { return answer().rowDual()[row]; }

    /** The row's left-hand side at the held solution. */
    public double activity(int row) { return answer().rowActivity()[row]; }

    /** The tree's pool after the last solve, best first, values by variable index. */
    public List<Incumbent> mipPool() {
        settled();
        return model.mipPool();
    }

    @Override
    public void close() { model.close(); }
}
