// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Linq;

namespace Jaos;

/// <summary>A linear expression over the variables of one <see cref="Problem"/>.</summary>
public class Expr
{
    internal readonly List<(int Col, double Coef)> Terms = new();
    internal double Constant;

    public static implicit operator Expr(double c) => new() { Constant = c };

    private static Expr Combine(Expr a, double sa, Expr b, double sb)
    {
        var e = new Expr { Constant = sa * a.Constant + sb * b.Constant };
        foreach (var (c, v) in a.Terms)
            e.Terms.Add((c, sa * v));
        foreach (var (c, v) in b.Terms)
            e.Terms.Add((c, sb * v));
        return e;
    }

    public static Expr operator +(Expr a, Expr b) => Combine(a, 1.0, b, 1.0);
    public static Expr operator -(Expr a, Expr b) => Combine(a, 1.0, b, -1.0);
    public static Expr operator -(Expr a) => Combine(a, -1.0, 0.0, 0.0);
    public static Expr operator *(double s, Expr a) => Combine(a, s, 0.0, 0.0);
    public static Expr operator *(Expr a, double s) => Combine(a, s, 0.0, 0.0);
}

/// <summary>A variable: an expression of one term.</summary>
public sealed class Var : Expr
{
    public int Index { get; }

    internal Var(int index)
    {
        Index = index;
        Terms.Add((index, 1.0));
    }
}

/// <summary>A row of a <see cref="Problem"/>, to read its dual back.</summary>
public readonly record struct Row(int Index);

/// <summary>
/// A model written in variables and expressions and loaded into a
/// <see cref="Model"/> whole at <see cref="Solve"/>:
/// <code>
/// var p = new Problem();
/// var x = p.AddVar(ub: 4);
/// var y = p.AddVar(integer: true);
/// p.AddLe(x + y, 4);
/// p.Maximize(x + 2 * y);
/// p.Solve();
/// </code>
/// </summary>
public sealed class Problem : IDisposable
{
    private readonly List<double> lower = new(), upper = new();
    private readonly List<bool> integer = new();
    private readonly List<(Expr E, double Lo, double Hi)> rows = new();
    private readonly Dictionary<(int, int), double> quad = new();
    private readonly List<(ConeType Type, long[] Cols)> cones = new();
    private Expr objective = 0.0;
    private Sense sense = Sense.Minimize;
    private Solution? solution;

    /// <summary>The model the last <see cref="Solve"/> loaded, for every other call.</summary>
    public Model Model { get; private set; } = new();

    public Var AddVar(double lb = 0.0, double ub = double.PositiveInfinity,
                      bool integer = false)
    {
        lower.Add(lb);
        upper.Add(ub);
        this.integer.Add(integer);
        return new Var(lower.Count - 1);
    }

    /// <summary>A row <c>lo &lt;= e &lt;= hi</c>, the constant of <paramref name="e"/> moved to its sides.</summary>
    public Row AddRange(Expr e, double lo, double hi)
    {
        rows.Add((e, lo - e.Constant, hi - e.Constant));
        return new Row(rows.Count - 1);
    }

    public Row AddLe(Expr e, double rhs) => AddRange(e, double.NegativeInfinity, rhs);
    public Row AddGe(Expr e, double rhs) => AddRange(e, rhs, double.PositiveInfinity);
    public Row AddEq(Expr e, double rhs) => AddRange(e, rhs, rhs);

    /// <summary>Adds <c>coef * a * b</c> to the objective, <c>coef * a²</c> when they are one variable.</summary>
    public void AddQuadratic(Var a, Var b, double coef)
    {
        var key = a.Index >= b.Index ? (a.Index, b.Index) : (b.Index, a.Index);
        double v = a.Index == b.Index ? 2.0 * coef : coef;
        quad[key] = quad.GetValueOrDefault(key) + v;
    }

    public void AddCone(ConeType type, params Var[] vars) =>
        cones.Add((type, vars.Select(v => (long)v.Index).ToArray()));

    public void Minimize(Expr e)
    {
        objective = e;
        sense = Sense.Minimize;
    }

    public void Maximize(Expr e)
    {
        objective = e;
        sense = Sense.Maximize;
    }

    /// <summary>Loads the problem into a fresh <see cref="Model"/> and solves it.</summary>
    public SolveStatus Solve()
    {
        int nc = lower.Count, nr = rows.Count;
        var cost = new double[nc];
        foreach (var (c, v) in objective.Terms)
            cost[c] += v;
        var byCol = new List<(int Row, double Coef)>[nc];
        for (int j = 0; j < nc; j++)
            byCol[j] = new List<(int, double)>();
        for (int i = 0; i < nr; i++)
        {
            var merged = new SortedDictionary<int, double>();
            foreach (var (c, v) in rows[i].E.Terms)
                merged[c] = merged.GetValueOrDefault(c) + v;
            foreach (var (c, v) in merged)
                if (v != 0.0)
                    byCol[c].Add((i, v));
        }
        var start = new long[nc + 1];
        for (int j = 0; j < nc; j++)
            start[j + 1] = start[j] + byCol[j].Count;
        var index = new long[start[nc]];
        var value = new double[start[nc]];
        for (int j = 0, p = 0; j < nc; j++)
            foreach (var (r, v) in byCol[j])
            {
                index[p] = r;
                value[p++] = v;
            }
        var m = new Model();
        m.LoadLp(sense, objective.Constant, cost, lower.ToArray(),
                 upper.ToArray(), rows.Select(r => r.Lo).ToArray(),
                 rows.Select(r => r.Hi).ToArray(), start, index, value);
        for (int j = 0; j < nc; j++)
            if (integer[j])
                m.SetColInteger(j);
        if (quad.Count > 0)
        {
            var keys = quad.Keys.OrderBy(k => k).ToArray();
            m.SetQuadratic(keys.Select(k => (long)k.Item1).ToArray(),
                           keys.Select(k => (long)k.Item2).ToArray(),
                           keys.Select(k => quad[k]).ToArray());
        }
        foreach (var (t, cols) in cones)
            m.AddCone(t, cols);
        Model.CopySettingsTo(m);
        Model.Dispose();
        Model = m;
        solution = null;
        m.Solve();
        if (m.Status == SolveStatus.Optimal)
            solution = m.Solution();
        return m.Status;
    }

    public SolveStatus Status => Model.Status;
    public double Objective => Model.Objective;

    private Solution Answer =>
        solution ?? throw new InvalidOperationException(
            "the last solve did not end optimal, so there are no values");

    public double Value(Var v) => Answer.X[v.Index];
    public double Value(Expr e) =>
        e.Constant + e.Terms.Sum(t => t.Coef * Answer.X[t.Col]);
    public double Dual(Row r) => Answer.RowDual[r.Index];

    public void Dispose() => Model.Dispose();
}
