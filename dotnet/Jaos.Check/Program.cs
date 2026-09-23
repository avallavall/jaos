// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Jaos;

int failed = 0, passed = 0;

void Check(bool ok, string what)
{
    if (ok)
    {
        passed++;
        Console.WriteLine($"ok   {what}");
    }
    else
    {
        failed++;
        Console.WriteLine($"FAIL {what}");
    }
}

bool Near(double a, double b, double tol = 1e-6) => Math.Abs(a - b) <= tol;

string data = Path.GetFullPath(Path.Combine(
    AppContext.BaseDirectory, "..", "..", "..", "..", "..", "tests", "data"));

Check(Model.Version.StartsWith("0.", StringComparison.Ordinal),
      $"the library answers with its version {Model.Version}");

using (var m = Model.Read(Path.Combine(data, "g1.lp")))
{
    m.Solve();
    Check(m.Status == SolveStatus.Optimal, "g1.lp solves optimal");
    Check(Near(m.Objective, -5.0), "to objective -5");
    var s = m.Solution();
    Check(Near(s.X[0], 0.0) && Near(s.X[1], -1.0) && Near(s.X[2], 8.0),
          "at (0, -1, 8)");
    Check(Model.StatusString(m.Status) == "optimal", "and says 'optimal'");
}

try
{
    using var m = Model.Read(Path.Combine(data, "no_such_file.lp"));
    Check(false, "a missing file throws");
}
catch (JaosException e)
{
    Check(e.Status == Status.Io && e.Message.Length > 0,
          $"a missing file throws with its message: {e.Message}");
}

using (var m = new Model())
{
    try
    {
        m.SetOption("no_such_option", "1");
        Check(false, "an unknown option throws");
    }
    catch (JaosException e)
    {
        Check(e.Status == Status.InvalidInput, "an unknown option throws");
    }
    m.SetOption("mip_gap", "0.001");
    Check(m.GetOption("mip_gap").StartsWith("0.001", StringComparison.Ordinal),
          "an option reads back as it was set");
}

using (var p = new Problem())
{
    var w = new[] { 2.0, 3.0, 1.0, 4.0, 5.0 };
    var v = new[] { 5.0, 4.0, 3.0, 7.0, 6.0 };
    var x = Enumerable.Range(0, 5).Select(_ => p.AddVar(0, 1, true)).ToArray();
    Expr weight = 0.0, value = 0.0;
    for (int j = 0; j < 5; j++)
    {
        weight += w[j] * x[j];
        value += v[j] * x[j];
    }
    p.AddLe(weight, 9);
    p.Maximize(value);
    Check(p.Solve() == SolveStatus.Optimal, "a knapsack solves optimal");
    Check(Near(p.Objective, 16.0), "to 16");
    Check(Near(p.Value(x[0]) + p.Value(x[1]) + p.Value(x[3]), 3.0),
          "with the first, second and fourth items");
    var r = p.Model.MipResult();
    Check(r.HasIncumbent && Near(r.Bound, 16.0, 1e-4),
          "and the tree's report agrees");

    var lines = new List<string>();
    p.Model.SetOption("mip_gap", "0.25");
    p.Model.SetLog(LogLevel.Summary, lines.Add);
    p.AddLe(x[0] + x[1], 1);
    p.Solve();
    Check(p.Model.GetOption("mip_gap") == "0.25" && lines.Count > 0,
          "a solve after an edit keeps the options and the log set on its model");
    Check(Model.OptionNames().Contains("mip_gap"), "the option list names mip_gap");
}

using (var p = new Problem())
{
    var y1 = p.AddVar();
    var y2 = p.AddVar();
    var row = p.AddLe(y1 + y2, 0.5);
    p.AddQuadratic(y1, y1, 1.0);
    p.AddQuadratic(y2, y2, 1.0);
    p.Minimize(-1.0 * y1 - y2);
    Check(p.Solve() == SolveStatus.Optimal, "a QP solves optimal");
    Check(Near(p.Objective, -0.375), "to -0.375");
    Check(Near(p.Value(y1), 0.25) && Near(p.Value(y2), 0.25), "at (0.25, 0.25)");
    Check(Near(p.Dual(row), -0.5), "with the row's dual -0.5");
}

using (var p = new Problem())
{
    var t = p.AddVar(double.NegativeInfinity);
    var a = p.AddVar(double.NegativeInfinity);
    var b = p.AddVar(double.NegativeInfinity);
    p.AddEq(a, 3);
    p.AddEq(b, 4);
    p.AddCone(ConeType.Quadratic, t, a, b);
    p.Minimize(t);
    Check(p.Solve() == SolveStatus.Optimal, "a cone model solves optimal");
    Check(Near(p.Objective, 5.0), "to 5");
    var z = p.Model.ConeDual(0);
    Check(z.Length == 3 && Near(z[0], 1.0) && Near(z[1], -0.6) &&
          Near(z[2], -0.8), "with the cone's dual (1, -0.6, -0.8)");
}

using (var p = new Problem())
{
    var x = p.AddVar(2.5);
    p.AddLe(1.0 * x, 1.4);
    p.Minimize(x);
    Check(p.Solve() == SolveStatus.Infeasible, "an infeasible LP says so");
    var y = p.Model.Certificate();
    Check(y != null && y.Length == 1 && y[0] < 0.0,
          "with a Farkas multiplier on its upper side");
}

using (var m = new Model())
{
    var lines = new List<string>();
    m.ReadFile(Path.Combine(data, "g1.lp"));
    m.SetLog(LogLevel.Summary, lines.Add);
    m.Solve();
    Check(lines.Count > 0, $"the log reaches the sink, {lines.Count} lines");
    m.SetLog(LogLevel.Off, null);
    lines.Clear();
    m.SetColCost(0, 3.5);
    m.Solve();
    Check(lines.Count == 0, "and stops when it is turned off");
}

Console.WriteLine($"{passed} passed, {failed} failed");
return failed == 0 ? 0 : 1;
