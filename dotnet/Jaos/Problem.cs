// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Linq;

namespace Jaos;

/// <summary>A linear expression over the variables of one <see cref="Problem"/>, with squares and products of two variables.</summary>
public class Expr
{
    internal readonly List<(int Col, double Coef)> Terms = new();
    internal readonly List<(int A, int B, double Coef)> Quad = new();
    internal double Constant;

    public static implicit operator Expr(double c) => new() { Constant = c };

    private static Expr Combine(Expr a, double sa, Expr b, double sb)
    {
        var e = new Expr { Constant = sa * a.Constant + sb * b.Constant };
        foreach (var (c, v) in a.Terms)
            e.Terms.Add((c, sa * v));
        foreach (var (c, v) in b.Terms)
            e.Terms.Add((c, sb * v));
        foreach (var (i, j, v) in a.Quad)
            e.Quad.Add((i, j, sa * v));
        foreach (var (i, j, v) in b.Quad)
            e.Quad.Add((i, j, sb * v));
        return e;
    }

    private bool IsConstant => Terms.Count == 0 && Quad.Count == 0;

    private bool OneVariable(out int col, out double coef)
    {
        col = -1;
        coef = 0.0;
        if (Constant != 0.0 || Quad.Count > 0)
            return false;
        foreach (var (c, v) in Terms)
        {
            if (col >= 0 && c != col)
                return false;
            col = c;
            coef += v;
        }
        return col >= 0;
    }

    public static Expr operator +(Expr a, Expr b) => Combine(a, 1.0, b, 1.0);
    public static Expr operator -(Expr a, Expr b) => Combine(a, 1.0, b, -1.0);
    public static Expr operator -(Expr a) => Combine(a, -1.0, 0.0, 0.0);
    public static Expr operator *(double s, Expr a) => Combine(a, s, 0.0, 0.0);
    public static Expr operator *(Expr a, double s) => Combine(a, s, 0.0, 0.0);

    /// <summary>A number times an expression, or a product of two terms of one variable each: <c>x * y</c>, <c>3 * x * x</c>.</summary>
    public static Expr operator *(Expr a, Expr b)
    {
        if (a.IsConstant)
            return Combine(b, a.Constant, 0.0, 0.0);
        if (b.IsConstant)
            return Combine(a, b.Constant, 0.0, 0.0);
        if (!a.OneVariable(out int i, out double ci) ||
            !b.OneVariable(out int j, out double cj))
            throw new InvalidOperationException(
                "a product is quadratic only between two terms of one variable each");
        var e = new Expr();
        e.Quad.Add((i, j, ci * cj));
        return e;
    }
}

/// <summary>
/// A variable: an expression of one term. A bound moved after a solve
/// reaches the model through its setter, so the next solve starts warm.
/// </summary>
public sealed class Var : Expr
{
    private readonly Problem owner;
    private double lb, ub;

    internal Var(Problem owner, int index, double lb, double ub, bool integer,
                 bool semicontinuous, string? name)
    {
        this.owner = owner;
        this.lb = lb;
        this.ub = ub;
        Index = index;
        Integer = integer;
        Semicontinuous = semicontinuous;
        Name = name;
        Terms.Add((index, 1.0));
    }

    public int Index { get; }
    public string? Name { get; }
    public bool Integer { get; }
    public bool Semicontinuous { get; }

    public double Lb
    {
        get => lb;
        set
        {
            lb = value;
            owner.ColMoved(Index);
        }
    }

    public double Ub
    {
        get => ub;
        set
        {
            ub = value;
            owner.ColMoved(Index);
        }
    }
}

/// <summary>
/// A row of a <see cref="Problem"/>, <c>Lb &lt;= e &lt;= Ub</c> with the
/// constant of <c>e</c> moved to the sides. A side moved after a solve
/// reaches the model through its setter, so the next solve starts warm.
/// </summary>
public sealed class Row
{
    private readonly Problem owner;
    private double lb, ub;
    internal readonly Expr E;

    internal Row(Problem owner, int index, Expr e, double lb, double ub,
                 string? name)
    {
        this.owner = owner;
        this.lb = lb;
        this.ub = ub;
        E = e;
        Index = index;
        Name = name;
    }

    public int Index { get; }
    public string? Name { get; }

    public double Lb
    {
        get => lb;
        set
        {
            lb = value;
            owner.RowMoved(Index);
        }
    }

    public double Ub
    {
        get => ub;
        set
        {
            ub = value;
            owner.RowMoved(Index);
        }
    }
}

/// <summary>
/// A model written in variables and expressions and loaded into
/// <see cref="Model"/> at the first <see cref="Solve"/>:
/// <code>
/// var p = new Problem();
/// var x = p.AddVar(ub: 4);
/// var y = p.AddVar(integer: true);
/// p.AddLe(x + y, 4);
/// p.Maximize(x + 2 * y);
/// p.Solve();
/// </code>
/// From then on a moved bound, cost, sense or constant goes through the C
/// setters and the next solve starts warm; a new variable, row, cone, set
/// or indicator loads the problem again and the next solve starts cold.
/// A value read while the problem is ahead of its last solve throws.
/// </summary>
public sealed class Problem : IDisposable
{
    private readonly List<Var> variables = new();
    private readonly List<Row> constraints = new();
    private readonly List<(int A, int B, double Coef)> quadTerms = new();
    private readonly List<(ConeType Type, long[] Cols)> cones = new();
    private readonly List<(int Type, long[] Cols, double[] Weights)> sets = new();
    private readonly List<(int Row, int Col, int Value)> indicators = new();
    private readonly SortedSet<int> colsMoved = new(), costsMoved = new(),
                                    rowsMoved = new();
    private Expr objective = 0.0;
    private Sense sense = Sense.Minimize;
    private Solution? solution;
    private bool loaded, structural, objectiveMoved, quadMoved;

    /// <summary>The model underneath, for every other call; <see cref="Load"/> brings it up to date without a solve.</summary>
    public Model Model { get; } = new();

    public IReadOnlyList<Var> Variables => variables;
    public IReadOnlyList<Row> Rows => constraints;

    /// <summary>A new variable. A semi-continuous one rests at zero or inside its bounds.</summary>
    public Var AddVar(double lb = 0.0, double ub = double.PositiveInfinity,
                      bool integer = false, string? name = null,
                      bool semicontinuous = false)
    {
        var v = new Var(this, variables.Count, lb, ub, integer, semicontinuous,
                        name);
        variables.Add(v);
        Touch();
        return v;
    }

    /// <summary>A row <c>lo &lt;= e &lt;= hi</c>, the constant of <paramref name="e"/> moved to its sides. Squares or products in <paramref name="e"/> make it a quadratic row, which takes one finite side.</summary>
    public Row AddRange(Expr e, double lo, double hi, string? name = null)
    {
        var r = new Row(this, constraints.Count, e, lo - e.Constant,
                        hi - e.Constant, name);
        constraints.Add(r);
        Touch();
        return r;
    }

    public Row AddLe(Expr e, double rhs, string? name = null) =>
        AddRange(e, double.NegativeInfinity, rhs, name);
    public Row AddGe(Expr e, double rhs, string? name = null) =>
        AddRange(e, rhs, double.PositiveInfinity, name);
    public Row AddEq(Expr e, double rhs, string? name = null) =>
        AddRange(e, rhs, rhs, name);

    /// <summary>Adds <c>coef * a * b</c> to the objective, <c>coef * a²</c> when they are one variable.</summary>
    public void AddQuadratic(Var a, Var b, double coef)
    {
        quadTerms.Add((a.Index, b.Index, coef));
        solution = null;
        if (loaded && !structural)
            quadMoved = true;
    }

    public void AddCone(ConeType type, params Var[] members)
    {
        cones.Add((type, members.Select(v => (long)v.Index).ToArray()));
        Touch();
    }

    /// <summary>A special ordered set of type 1 or 2 over <paramref name="members"/>; the weights order them and are 1, 2, 3, ... by default.</summary>
    public void AddSos(int type, Var[] members, double[]? weights = null)
    {
        weights ??= Enumerable.Range(1, members.Length).Select(k => (double)k).ToArray();
        if (weights.Length != members.Length)
            throw new ArgumentException("one weight per variable");
        sets.Add((type, members.Select(v => (long)v.Index).ToArray(),
                  (double[])weights.Clone()));
        Touch();
    }

    /// <summary><paramref name="row"/> holds only while the integer variable <paramref name="indicator"/> equals <paramref name="value"/>.</summary>
    public void SetIndicator(Row row, Var indicator, int value = 1)
    {
        indicators.Add((row.Index, indicator.Index, value));
        Touch();
    }

    public void Minimize(Expr e) => SetObjective(e, Sense.Minimize);
    public void Maximize(Expr e) => SetObjective(e, Sense.Maximize);

    private void SetObjective(Expr e, Sense s)
    {
        solution = null;
        if (loaded && !structural)
        {
            if (s != sense || e.Constant != objective.Constant)
                objectiveMoved = true;
            var before = Costs(objective);
            var after = Costs(e);
            for (int j = 0; j < after.Length; j++)
                if (before[j] != after[j])
                    costsMoved.Add(j);
            if (!Same(ObjectiveQ(objective), ObjectiveQ(e)))
                quadMoved = true;
        }
        objective = e;
        sense = s;
    }

    private void Touch()
    {
        solution = null;
        if (loaded)
            structural = true;
    }

    internal void ColMoved(int j)
    {
        solution = null;
        if (loaded && !structural)
            colsMoved.Add(j);
    }

    internal void RowMoved(int i)
    {
        solution = null;
        if (loaded && !structural)
            rowsMoved.Add(i);
    }

    private bool Pending() =>
        !loaded || structural || objectiveMoved || quadMoved ||
        colsMoved.Count > 0 || costsMoved.Count > 0 || rowsMoved.Count > 0;

    private double[] Costs(Expr e)
    {
        var c = new double[variables.Count];
        foreach (var (j, v) in e.Terms)
            c[j] += v;
        return c;
    }

    private static SortedDictionary<(int, int), double> QMatrix(
        IEnumerable<(int A, int B, double Coef)> terms)
    {
        var q = new SortedDictionary<(int, int), double>();
        foreach (var (a, b, c) in terms)
        {
            var key = a >= b ? (a, b) : (b, a);
            q[key] = q.GetValueOrDefault(key) + (a == b ? 2.0 * c : c);
        }
        foreach (var k in q.Where(kv => kv.Value == 0.0).Select(kv => kv.Key).ToList())
            q.Remove(k);
        return q;
    }

    private SortedDictionary<(int, int), double> ObjectiveQ(Expr e) =>
        QMatrix(quadTerms.Concat(e.Quad));

    private static bool Same(SortedDictionary<(int, int), double> a,
                             SortedDictionary<(int, int), double> b) =>
        a.Count == b.Count &&
        a.All(kv => b.TryGetValue(kv.Key, out double v) && v == kv.Value);

    private static (long[] Rows, long[] Cols, double[] Values) Triplets(
        SortedDictionary<(int, int), double> q) =>
        (q.Keys.Select(k => (long)k.Item1).ToArray(),
         q.Keys.Select(k => (long)k.Item2).ToArray(),
         q.Values.ToArray());

    /// <summary>
    /// Puts every change onto <see cref="Model"/>: bounds, costs, the sense,
    /// the constant and the quadratic objective through the setters,
    /// anything else by loading the whole problem again.
    /// </summary>
    public void Load()
    {
        if (!loaded || structural)
        {
            Build();
            return;
        }
        if (objectiveMoved)
        {
            Model.SetSense(sense);
            Model.SetObjOffset(objective.Constant);
        }
        if (costsMoved.Count > 0)
        {
            var c = Costs(objective);
            foreach (int j in costsMoved)
                Model.SetColCost(j, c[j]);
        }
        if (quadMoved)
        {
            var (r, c, v) = Triplets(ObjectiveQ(objective));
            Model.SetQuadratic(r, c, v);
        }
        foreach (int j in colsMoved)
            Model.SetColBounds(j, variables[j].Lb, variables[j].Ub);
        foreach (int i in rowsMoved)
            Model.SetRowBounds(i, constraints[i].Lb, constraints[i].Ub);
        Settle();
    }

    private void Settle()
    {
        objectiveMoved = false;
        quadMoved = false;
        costsMoved.Clear();
        colsMoved.Clear();
        rowsMoved.Clear();
        structural = false;
        loaded = true;
    }

    private void Build()
    {
        int nc = variables.Count, nr = constraints.Count;
        var byCol = new List<(int Row, double Coef)>[nc];
        for (int j = 0; j < nc; j++)
            byCol[j] = new List<(int, double)>();
        for (int i = 0; i < nr; i++)
        {
            var merged = new SortedDictionary<int, double>();
            foreach (var (c, v) in constraints[i].E.Terms)
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
        Model.Reload(sense, objective.Constant, Costs(objective),
                     variables.Select(v => v.Lb).ToArray(),
                     variables.Select(v => v.Ub).ToArray(),
                     constraints.Select(r => r.Lb).ToArray(),
                     constraints.Select(r => r.Ub).ToArray(),
                     start, index, value);
        foreach (var v in variables)
        {
            if (v.Integer)
                Model.SetColInteger(v.Index);
            if (v.Semicontinuous)
                Model.SetColSemicontinuous(v.Index);
            if (v.Name != null)
                Model.SetColName(v.Index, v.Name);
        }
        var q = ObjectiveQ(objective);
        if (q.Count > 0)
        {
            var (qr, qc, qv) = Triplets(q);
            Model.SetQuadratic(qr, qc, qv);
        }
        foreach (var (t, cols, w) in sets)
            Model.AddSos(t, cols, w);
        foreach (var (r, c, v) in indicators)
            Model.SetRowIndicator(r, c, v);
        foreach (var r in constraints)
        {
            if (r.Name != null)
                Model.SetRowName(r.Index, r.Name);
            if (r.E.Quad.Count > 0)
            {
                var (qr, qc, qv) = Triplets(QMatrix(r.E.Quad));
                Model.SetRowQuadratic(r.Index, qr, qc, qv);
            }
        }
        foreach (var (t, cols) in cones)
            Model.AddCone(t, cols);
        Settle();
    }

    /// <summary>Loads what changed and solves it.</summary>
    public SolveStatus Solve()
    {
        solution = null;
        Load();
        Model.Solve();
        if (Model.Status == SolveStatus.Optimal)
            solution = Model.Solution();
        return Model.Status;
    }

    public SolveStatus Status => Model.Status;

    private void Settled()
    {
        if (Pending())
            throw new InvalidOperationException(
                "the problem changed since the last solve; call Solve before reading values");
    }

    public double Objective
    {
        get
        {
            Settled();
            return Model.Objective;
        }
    }

    private Solution Answer
    {
        get
        {
            Settled();
            return solution ?? throw new InvalidOperationException(
                "the last solve did not end optimal, so there are no values");
        }
    }

    public double Value(Var v) => Answer.X[v.Index];

    public double Value(Expr e)
    {
        var x = Answer.X;
        return e.Constant + e.Terms.Sum(t => t.Coef * x[t.Col]) +
               e.Quad.Sum(t => t.Coef * x[t.A] * x[t.B]);
    }

    public double ReducedCost(Var v) => Answer.ReducedCost[v.Index];
    public double Dual(Row r) => Answer.RowDual[r.Index];

    /// <summary>The row's left-hand side at the answer, without the constant moved to its sides.</summary>
    public double Activity(Row r) => Answer.RowActivity[r.Index];

    /// <summary>The solution pool of the last branch and bound, best first, values by <see cref="Var.Index"/>.</summary>
    public (double[] X, double Objective)[] MipPool()
    {
        Settled();
        return Model.MipPool();
    }

    /// <summary>The library's independent checker on the answer, with the cones' duals when there are cones.</summary>
    public CheckReport Check(double tol = 1e-7)
    {
        var s = Answer;
        if (cones.Count == 0)
            return Model.CheckSolution(s.X, s.RowDual, tol);
        var z = new double[cones.Count][];
        for (int k = 0; k < z.Length; k++)
            z[k] = Model.ConeDual(k);
        return Model.CheckConicSolution(s.X, s.RowDual, z, tol);
    }

    public void Dispose() => Model.Dispose();
}
