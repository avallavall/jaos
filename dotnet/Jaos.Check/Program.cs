// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Numerics;
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

bool Throws<T>(Action a) where T : Exception
{
    try
    {
        a();
    }
    catch (T)
    {
        return true;
    }
    return false;
}

bool Rat(string s, long p, long q)
{
    var parts = s.Split('/');
    var num = BigInteger.Parse(parts[0], CultureInfo.InvariantCulture);
    var den = parts.Length > 1
        ? BigInteger.Parse(parts[1], CultureInfo.InvariantCulture) : BigInteger.One;
    return num * q == p * den;
}

double inf = double.PositiveInfinity;

string data = Path.GetFullPath(Path.Combine(
    AppContext.BaseDirectory, "..", "..", "..", "..", "..", "tests", "data"));
string D(string name) => Path.Combine(data, name);
string tmp = Directory.CreateTempSubdirectory("jaos-dotnet-").FullName;
string P(string name) => Path.Combine(tmp, name);

Model Golden()
{
    var m = new Model();
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0, -2.0 }, new[] { 0.0, 0.0 },
             new[] { inf, inf }, new[] { -inf }, new[] { 4.0 },
             new long[] { 0, 1, 2 }, new long[] { 0, 0 }, new[] { 1.0, 1.0 });
    return m;
}

Model NormModel()
{
    var m = new Model();
    m.LoadLp(Sense.Minimize, 0.0, new[] { 1.0, 0.0, 0.0 },
             new[] { -inf, -inf, -inf }, new[] { inf, inf, inf },
             new[] { 3.0, 4.0 }, new[] { 3.0, 4.0 }, new long[] { 0, 0, 1, 2 },
             new long[] { 0, 1 }, new[] { 1.0, 1.0 });
    m.AddCone(ConeType.Quadratic, new long[] { 0, 1, 2 });
    return m;
}

void Plain(Model m)
{
    m.SetMipCutRounds(0);
    m.SetMipCoverRounds(0);
    m.SetMipMirRounds(0);
    m.SetMipCliqueRounds(0);
    m.SetMipZeroHalfRounds(0);
    m.SetMipFlowCoverRounds(0);
    m.SetMipCutDepth(0);
    m.SetMipHeuristics(false);
    m.SetMipDiveHeuristic(0);
    m.SetMipFeaspump(0);
    m.SetMipTighten(0);
}

Check(Model.Version.StartsWith("0.", StringComparison.Ordinal),
      $"the library answers with its version {Model.Version}");
Check(Model.StatusString(Status.Io).Length > 0 && Model.Infinity == inf,
      $"a status reads as '{Model.StatusString(Status.Io)}' and the infinity is IEEE's");

using (var m = Model.Read(D("g1.lp")))
{
    m.Solve();
    Check(m.Status == SolveStatus.Optimal, "g1.lp solves optimal");
    Check(Near(m.Objective, -5.0), "to objective -5");
    var s = m.Solution();
    Check(Near(s.X[0], 0.0) && Near(s.X[1], -1.0) && Near(s.X[2], 8.0),
          "at (0, -1, 8)");
    Check(Model.StatusString(m.Status) == "optimal", "and says 'optimal'");
    Check(m.SolveTime >= 0.0 && m.WorkUnits > 0 && m.Iterations >= 0,
          "the solve time, the work and the iterations read back");
}

using (var m = Model.Read(D("g1.lp.gz")))
{
    m.Solve();
    Check(Near(m.Objective, -5.0), "the gzip file reads as the plain one");
}

try
{
    using var m = Model.Read(D("no_such_file.lp"));
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
    File.WriteAllText(P("opts.txt"), "# options\nmip_gap 0.25\nalgorithm primal\n");
    m.ReadOptions(P("opts.txt"));
    Check(m.GetOption("mip_gap") == "0.25" && m.Algorithm == Algorithm.Primal,
          "an options file sets what it names");
    File.WriteAllText(P("bad.txt"), "no_such_option 1\n");
    Check(Throws<JaosException>(() => m.ReadOptions(P("bad.txt"))),
          "an options file with an unknown name throws");
}

using (var m = new Model())
{
    var bad = new List<string>();
    void Is(string option, string want)
    {
        string got = m.GetOption(option);
        if (got != want)
            bad.Add($"{option}={got}");
    }
    void Flip(string option, Action<bool> set)
    {
        bool on = m.GetOption(option) == "true";
        set(!on);
        Is(option, on ? "false" : "true");
    }
    void Num(string option, double want)
    {
        if (double.Parse(m.GetOption(option), CultureInfo.InvariantCulture) != want)
            bad.Add($"{option}={m.GetOption(option)}");
    }
    m.SetMipGap(0.25); Is("mip_gap", "0.25");
    m.SetMipNodeLimit(7); Is("mip_node_limit", "7");
    m.SetMipTreeBatch(3); Is("mip_tree_batch", "3");
    m.SetMipBranching(Branching.MostFractional); Is("mip_branching", "most-fractional");
    m.SetMipReliability(4); Is("mip_reliability", "4");
    m.SetMipProbeCap(2.0); Is("mip_probe_cap", "2");
    m.SetMipProbeDepth(5); Is("mip_probe_depth", "5");
    m.SetMipNodeSelect(0); Is("mip_node_select", "0");
    Flip("mip_restart", on => m.SetMipRestart(on ? 1 : 0));
    Flip("mip_conflicts", on => m.SetMipConflicts(on ? 1 : 0));
    Flip("mip_symmetry", on => m.SetMipSymmetry(on ? 1 : 0));
    Flip("mip_orbital", on => m.SetMipOrbital(on ? 1 : 0));
    m.SetMipPropagate(3); Is("mip_propagate", "3");
    m.SetMipPropagateDepth(4); Is("mip_propagate_depth", "4");
    Flip("mip_rcfix", on => m.SetMipRcfix(on ? 1 : 0));
    Flip("mip_tighten", on => m.SetMipTighten(on ? 1 : 0));
    Flip("mip_probing", on => m.SetMipProbing(on ? 1 : 0));
    m.SetMipProbingCap(2.0); Is("mip_probing_cap", "2");
    Flip("mip_clique_fix", on => m.SetMipCliqueFix(on ? 1 : 0));
    m.SetMipCutRounds(2); Is("mip_cut_rounds", "2");
    m.SetMipCutDepth(1); Is("mip_cut_depth", "1");
    Flip("mip_cut_drop", on => m.SetMipCutDrop(on));
    m.SetMipNodeCutCap(9); Is("mip_node_cut_cap", "9");
    m.SetMipCoverRounds(2); Is("mip_cover_rounds", "2");
    m.SetMipCliqueRounds(2); Is("mip_clique_rounds", "2");
    m.SetMipZeroHalfRounds(2); Is("mip_zero_half_rounds", "2");
    m.SetMipFlowCoverRounds(2); Is("mip_flow_cover_rounds", "2");
    m.SetMipCutStall(0.25); Is("mip_cut_stall", "0.25");
    m.SetMipNodeCutStall(0.5); Is("mip_node_cut_stall", "0.5");
    Flip("mip_root_cut_drop", on => m.SetMipRootCutDrop(on ? 1 : 0));
    Flip("mip_cover_lift", on => m.SetMipCoverLift(on ? 1 : 0));
    m.SetMipMirRounds(2); Is("mip_mir_rounds", "2");
    Flip("mip_node_mir", on => m.SetMipNodeMir(on ? 1 : 0));
    m.SetMipMirAggregate(3); Is("mip_mir_aggregate", "3");
    Flip("mip_dive", on => m.SetMipDive(on));
    m.SetMipDiveBacktrack(2); Is("mip_dive_backtrack", "2");
    m.SetMipDiveGap(0.25); Is("mip_dive_gap", "0.25");
    m.SetMipDiveHeuristic(7); Is("mip_dive_heuristic", "7");
    m.SetMipDiveHeuristicDepth(2); Is("mip_dive_heuristic_depth", "2");
    m.SetMipDiveChild(DiveChild.Down); Is("mip_dive_child", "down");
    m.SetMipDiveDegrade(0.25); Is("mip_dive_degrade", "0.25");
    m.SetMipRins(5); Is("mip_rins", "5");
    m.SetMipLocalBranching(5); Is("mip_local_branching", "5");
    m.SetMipFeaspump(3); Is("mip_feaspump", "3");
    Flip("mip_pump_general", on => m.SetMipPumpGeneral(on ? 1 : 0));
    m.SetMipPumpObj(0.25); Is("mip_pump_obj", "0.25");
    Flip("mip_pump_always", on => m.SetMipPumpAlways(on ? 1 : 0));
    Flip("mip_heuristics", on => m.SetMipHeuristics(on));
    m.SetMipPoolSize(4); Is("mip_pool_size", "4");
    m.SetMipCutoff(100.5); Is("mip_cutoff", "100.5");
    m.SetPrimalTolerance(1e-8); Num("primal_tolerance", 1e-8);
    m.SetDualTolerance(1e-8); Num("dual_tolerance", 1e-8);
    m.SetAlgorithm(Algorithm.Barrier); Is("algorithm", "barrier");
    m.SetThreads(3); Is("threads", "3");
    m.SetWorkLimit(1000); Is("work_limit", "1000");
    m.SetTimeLimit(2.5); Is("time_limit", "2.5");
    Check(bad.Count == 0 && m.Algorithm == Algorithm.Barrier && m.Threads == 3,
          "every typed setter lands on its option" +
          (bad.Count > 0 ? ": " + string.Join(", ", bad) : ""));
    Check(Throws<JaosException>(() => m.SetMipPoolSize(0)) &&
          Throws<JaosException>(() => m.SetThreads(0)),
          "a setter refuses a value out of its range");
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
    var ck = p.Check();
    Check(ck.PrimalFeasible && ck.DualFeasible && ck.MaxConeViolation <= 1e-7,
          "and the checker accepts the problem's own answer with its cone");
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
    m.ReadFile(D("g1.lp"));
    m.SetLog(LogLevel.Summary, lines.Add);
    m.Solve();
    Check(lines.Count > 0, $"the log reaches the sink, {lines.Count} lines");
    m.SetLog(LogLevel.Off, null);
    lines.Clear();
    m.SetColCost(0, 3.5);
    m.Solve();
    Check(lines.Count == 0, "and stops when it is turned off");
    var levels = new List<LogLevel>();
    m.SetLogCallback((lvl, line) => levels.Add(lvl), LogLevel.Detail);
    m.SetColCost(0, 3.0);
    m.Solve();
    Check(levels.Count > 0 && levels.All(l => l >= LogLevel.Summary && l <= LogLevel.Detail),
          "the log callback receives each line's level");
    levels.Clear();
    using (var copy = m.Copy())
        copy.Solve();
    Check(levels.Count > 0, "a copy logs to the same callback");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 1.0, 1.0, 1.0 }, new[] { 0.0, 0.0, 0.0 },
             new[] { 10.0, 10.0, 10.0 }, new[] { 0.0, 0.0 }, new[] { 5.0, 5.0 },
             new long[] { 0, 2, 3, 5 }, new long[] { 0, 1, 1, 1, 0 },
             new[] { 1.0, 0.0, 3.0, 4.0, 2.0 });
    var c2 = m.ColEntries(2);
    var r1 = m.RowEntries(1);
    Check(m.NumNz == 4 && c2.Index.SequenceEqual(new long[] { 0, 1 }) &&
          c2.Value.SequenceEqual(new[] { 2.0, 4.0 }) &&
          r1.Index.SequenceEqual(new long[] { 1, 2 }) &&
          r1.Value.SequenceEqual(new[] { 3.0, 4.0 }) &&
          m.ColEntries(0).Index.SequenceEqual(new long[] { 0 }) &&
          m.Coefficient(1, 2) == 4.0 && m.Coefficient(1, 0) == 0.0,
          "the matrix reads back by column, by row and by entry");
    m.SetCoefficient(1, 0, 7.0);
    m.SetCoefficient(0, 0, 0.0);
    Check(m.RowEntries(1).Index.SequenceEqual(new long[] { 0, 1, 2 }) &&
          m.RowEntries(1).Value[0] == 7.0 && m.ColEntries(0).Index.SequenceEqual(new long[] { 1 }),
          "a coefficient set or zeroed reads back");
    Check(Throws<JaosException>(() => m.ColEntries(3)) &&
          Throws<JaosException>(() => m.Coefficient(2, 0)),
          "an index out of range throws");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Maximize, 10.0, new[] { 1.0 }, new[] { 0.0 }, new[] { 3.0 },
             new[] { -inf }, new[] { 10.0 }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 1.0 });
    Check(m.Sense == Sense.Maximize && m.ObjOffset == 10.0 && m.ColCost(0) == 1.0 &&
          m.ColBounds(0) == (0.0, 3.0) && m.RowBounds(0) == (-inf, 10.0),
          "the sense, constant, cost and bounds read back");
    m.Solve();
    double a = m.Objective;
    m.SetSense(Sense.Minimize);
    bool reset = m.Status == SolveStatus.NotRun && m.Sense == Sense.Minimize;
    m.Solve();
    double b = m.Objective;
    m.SetObjOffset(0.0);
    m.Solve();
    double c = m.Objective;
    Check(Near(a, 13.0) && reset && Near(b, 10.0) && Near(c, 0.0) && m.ObjOffset == 0.0,
          "the sense and the constant move through their setters: 13, 10, 0");
    Check(Throws<JaosException>(() => m.SetObjOffset(double.NaN)),
          "a NaN constant throws");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0 }, new[] { 0.0 }, new[] { 3.0 },
             new[] { -inf }, new[] { 10.0 }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 1.0 });
    m.Solve();
    double a = m.Objective;
    m.SetColBounds(0, 0.0, 7.0);
    m.Solve();
    double b = m.Objective;
    m.SetRowBounds(0, -inf, 5.0);
    m.Solve();
    double c = m.Objective;
    m.SetColCost(0, 1.0);
    m.Solve();
    Check(Near(a, -3.0) && Near(b, -7.0) && Near(c, -5.0) && Near(m.Objective, 0.0),
          "a moved bound, row side and cost move the answer: -3, -7, -5, 0");
}

using (var m = Golden())
{
    m.Solve();
    double a = m.Objective;
    m.AddRows(new[] { -inf }, new[] { 3.0 }, new long[] { 0, 1 }, new long[] { 1 },
              new[] { 1.0 });
    bool grew = m.NumRow == 2 && m.NumNz == 3;
    m.Solve();
    double b = m.Objective;
    m.DeleteRows(1);
    m.Solve();
    double c = m.Objective;
    m.AddCols(new[] { -3.0 }, new[] { 0.0 }, new[] { 2.0 }, new long[] { 0, 1 },
              new long[] { 0 }, new[] { 1.0 });
    m.Solve();
    double d = m.Objective;
    m.DeleteCols(1, 2);
    m.Solve();
    Check(Near(a, -8.0) && grew && Near(b, -7.0) && Near(c, -8.0) && Near(d, -10.0) &&
          Near(m.Objective, -4.0) && m.NumCol == 1,
          "rows and columns added and deleted move the optimum: -8, -7, -8, -10, -4");
    Check(Throws<JaosException>(() => m.DeleteCols(0, 0)) &&
          Throws<ArgumentException>(() => m.AddRows(new[] { 0.0 }, new[] { 1.0 },
                                                    new long[] { 0 }, new long[] { 0 },
                                                    new[] { 1.0 })),
          "a repeated delete index and a short start array throw");
}

using (var m = new Model())
{
    m.AddCols(new[] { 1.0, 2.0 }, new[] { 0.0, 0.0 }, new[] { 10.0, 10.0 });
    m.AddRows(new[] { 3.0 }, new[] { inf }, new long[] { 0, 2 }, new long[] { 0, 1 },
              new[] { 1.0, 1.0 });
    m.Solve();
    Check(m.Status == SolveStatus.Optimal && Near(m.Objective, 3.0),
          "a model built one call at a time, columns first, solves to 3");
}

using (var m = Model.Read(D("t1.mps")))
{
    Check(m.ColName(0) == "X1" && m.ColName(2) == "X3" && m.RowName(0) == "LIM1" &&
          m.RowName(2) == "EQ1" && m.ObjectiveName == "COST" &&
          m.ColIndex("X2") == 1 && m.RowIndex("EQ1") == 2 &&
          Throws<JaosException>(() => m.RowIndex("COST")),
          "a file's names come through and look up");
    Check(m.Name == "T1", "the model's name is the file's");
    m.SetName("renamed");
    bool refused = Throws<JaosException>(() => m.SetName("two words"));
    m.Solve();
    using var c = m.Copy();
    bool fresh = c.Status == SolveStatus.NotRun;
    c.Solve();
    c.SetColName(0, "mine");
    Check(refused && m.Name == "renamed" && c.Name == "renamed" && c.ColName(2) == "X3" &&
          c.NumNz == m.NumNz && fresh && c.Status == m.Status && m.ColName(0) == "X1",
          "a copy keeps the problem and the names, not the answer, and stands alone");
}

using (var m = Golden())
{
    bool positional = m.ColName(0) == "C1" && m.RowName(0) == "R1";
    m.SetColName(1, "y");
    m.SetRowName(0, "cap");
    m.SetObjectiveName("profit");
    bool set = m.ColName(1) == "y" && m.RowName(0) == "cap" &&
               m.ObjectiveName == "profit" && m.ColIndex("y") == 1 &&
               m.RowIndex("cap") == 0 && m.ColIndex("C1") == 0;
    m.SetColName(1, null);
    Check(positional && set && m.ColName(1) == "C2" &&
          Throws<JaosException>(() => m.SetColName(0, "a b")),
          "names set here read back, look up and go back to positional");
}

using (var m = Model.Read(D("g1.lp")))
{
    m.SetRowName(1, "second");
    m.WriteMps(P("named.mps"));
    using var back = Model.Read(P("named.mps"));
    m.SetColName(0, "z");
    Check(back.RowName(1) == "second" && back.ColName(2) == "z" &&
          back.ObjectiveName == "obj" && Throws<JaosException>(() => m.WriteLp(P("dup.lp"))),
          "names go through an MPS file, and a duplicate name stops the LP writer");
}

using (var p = new Problem())
{
    var x = p.AddVar(ub: 4, name: "apples");
    var y = p.AddVar();
    p.AddLe(x + y, 4, "crate");
    p.Maximize(x + 2 * y);
    p.Solve();
    p.Model.WriteLp(P("p.lp"));
    string text = File.ReadAllText(P("p.lp"));
    Check(p.Model.ColName(0) == "apples" && p.Model.ColName(1) == "C2" &&
          p.Model.RowName(0) == "crate" && text.Contains("apples") && text.Contains("crate:"),
          "a problem's names reach the library and its LP file");
}

using (var m = Model.Read(D("t_lin.nl")))
{
    Check(m.NumCol == 3 && m.NumRow == 3 && m.NumNz == 6 && m.ColName(2) == "y" &&
          m.ColInteger(2), "an NL file reads with its names and integers");
    m.WriteNl(P("out.nl"));
    using var back = Model.Read(P("out.nl"));
    back.Solve();
    Check(File.Exists(P("out.col")) && File.Exists(P("out.row")) && back.NumNz == 6 &&
          back.ColName(1) == "z" && back.RowName(1) == "c2" && back.ObjectiveName == "obj" &&
          back.ColInteger(2) && Near(back.Objective, -4.0),
          "an NL file written here reads back with its names and solves to -4");
}

using (var m = Model.Read(D("g_quad.qplib")))
{
    m.Solve();
    m.WriteQplib(P("out.qplib"));
    m.WriteOsil(P("out.osil"));
    using var q = Model.Read(P("out.qplib"));
    using var o = Model.Read(P("out.osil"));
    q.Solve();
    o.Solve();
    Check(Near(m.Objective, 4.0) && q.RowName(0) == "c1" && q.ColQuadratic(1) == 2.0 &&
          Near(q.Objective, 4.0), "a QPLIB file written here reads back and solves to 4");
    Check(File.ReadAllText(P("out.osil")).Contains("<var name=\"x\"") &&
          o.ColName(0) == "x" && o.RowName(0) == "c1" && o.ColQuadratic(0) == 2.0 &&
          Near(o.Objective, 4.0), "an OSiL file written here reads back and solves to 4");
}

using (var m = Model.Read(D("g_cone.mps")))
{
    m.WriteCbf(P("cone.cbf"));
    using var b = Model.Read(P("cone.cbf"));
    var (t, cols) = b.Cone(0);
    b.Solve();
    Check(t == ConeType.Quadratic && cols.SequenceEqual(new long[] { 0, 1, 2 }) &&
          Near(b.Objective, 5.0), "a CBF file written here reads back with its cone and solves to 5");
}

using (var m = Model.Read(D("t_ampl_lp.nl")))
{
    m.Solve();
    m.WriteSolAmpl(P("amp.sol"));
    var lines = File.ReadAllText(P("amp.sol")).Split('\n');
    bool first = lines[0] == $"JAOS {Model.Version}: optimal; objective -7" &&
                 lines[2] == "Options" && lines[16] == "objno 0 0";
    m.WriteSolAmpl(P("amp.sol"), "solved by hand");
    Check(first && File.ReadAllLines(P("amp.sol"))[0] == "solved by hand",
          "the AMPL .sol file carries the status, the objective and a given message");
}

using (var m = Model.Read(D("solve1.mps")))
{
    m.Solve();
    var want = m.Solution();
    var wb = m.Basis();
    double wobj = m.Objective;
    Check(wb.ColStatus.Length == m.NumCol && wb.RowStatus.Length == m.NumRow,
          "the basis comes back one status per variable");
    m.WriteSolution(P("s.sol"));
    var (obj, sol, basis) = m.ReadSolution(P("s.sol"));
    Check(obj == wobj && sol.X.SequenceEqual(want.X) && sol.RowDual.SequenceEqual(want.RowDual) &&
          sol.ReducedCost.SequenceEqual(want.ReducedCost) &&
          sol.RowActivity.SequenceEqual(want.RowActivity) &&
          basis.ColStatus.SequenceEqual(wb.ColStatus) && basis.RowStatus.SequenceEqual(wb.RowStatus),
          "a solution file reads back as the same answer and basis");
    Check(m.SolutionFileStatus(P("s.sol")) == SolveStatus.Optimal &&
          m.ReadBasis(P("s.sol")).RowStatus.SequenceEqual(wb.RowStatus),
          "the file's status and basis read on their own");
    m.WriteMpsBasis(P("s.bas"));
    var mb = m.ReadMpsBasis(P("s.bas"));
    Check(mb.ColStatus.SequenceEqual(wb.ColStatus) && mb.RowStatus.SequenceEqual(wb.RowStatus) &&
          File.ReadAllText(P("s.bas")).Contains("ENDATA"), "an MPS basis file round trips");
    m.WritePoint(P("p.txt"));
    m.WriteDuals(P("d.txt"));
    var px = m.ReadPoint(P("p.txt"));
    var dy = m.ReadDuals(P("d.txt"));
    var ck = m.CheckSolution(px, dy);
    Check(px.SequenceEqual(want.X) && dy.SequenceEqual(want.RowDual) && ck.CheckedDuals &&
          ck.DualFeasible, "point and duals files round trip and check out");
    m.WritePointValues(P("g.txt"), new[] { 1.5, -2.0, 0.0 });
    m.WriteDualValues(P("z.txt"), new double[m.NumRow]);
    Check(m.ReadPoint(P("g.txt")).SequenceEqual(new[] { 1.5, -2.0, 0.0 }) &&
          m.ReadDuals(P("z.txt")).All(v => v == 0.0) &&
          Throws<ArgumentException>(() => m.WritePointValues(P("g.txt"), new[] { 1.0 })),
          "point and duals files are written from given values");
}

using (var n = Model.Read(D("solve1.mps")))
{
    var b = n.ReadMpsBasis(P("s.bas"));
    n.SetBasis(b.ColStatus, b.RowStatus);
    n.Solve();
    bool warm = n.Status == SolveStatus.Optimal && Near(n.Objective, 29.0) && n.Iterations == 0;
    n.ClearBasis();
    n.SetColCost(0, n.ColCost(0));
    n.Solve();
    Check(warm && Near(n.Objective, 29.0) && n.Iterations > 0 &&
          Throws<ArgumentException>(() => n.SetBasis(new[] { BasisStatus.Basic }, new BasisStatus[0])),
          "a basis handed back starts at the optimum, and a cleared one starts cold");
}

using (var m = Model.Read(D("t1.mps")))
{
    m.Solve();
    var want = m.Certificate()!;
    m.WriteSolution(P("t1.sol"));
    var (st, ray) = m.ReadCertificate(P("t1.sol"));
    Check(m.Status == SolveStatus.Infeasible &&
          m.SolutionFileStatus(P("t1.sol")) == SolveStatus.Infeasible &&
          st == SolveStatus.Infeasible && ray.SequenceEqual(want) &&
          m.CheckCertificate(ray).Certified,
          "a certificate file reads back as the certificate and certifies");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0, -1.0 }, new[] { 0.0, 0.0 },
             new[] { inf, inf }, new[] { -inf }, new[] { 1.0 }, new long[] { 0, 1, 2 },
             new long[] { 0, 0 }, new[] { 1.0, -1.0 });
    m.Solve();
    m.WriteSolution(P("ray.sol"));
    var (st, ray) = m.ReadCertificate(P("ray.sol"));
    Check(st == SolveStatus.Unbounded && ray.SequenceEqual(m.UnboundedRay()!) &&
          m.CheckRay(ray).Certified, "an unbounded answer's file reads back as its ray");
}

using (var m = Model.Read(D("solve1.mps")))
{
    m.Solve();
    var s = m.Solution();
    var r = m.CheckSolution(s.X, s.RowDual);
    Check(r.PrimalFeasible && r.DualFeasible && r.CheckedDuals && Near(r.PrimalObjective, 29.0, 1e-9),
          "the checker accepts the true solution");
    var wrong = m.CheckSolution(s.X.Select(v => v + 100.0).ToArray(), s.RowDual);
    Check(!wrong.PrimalFeasible && Math.Max(wrong.MaxColViolation, wrong.MaxRowViolation) > 1.0,
          "and flags a corrupted one");
    Check(!m.CheckSolution(s.X).CheckedDuals, "no duals means no dual verdict");
    Check(m.Certificate() == null && m.UnboundedRay() == null &&
          !m.CheckCertificate(Enumerable.Repeat(1.0, (int)m.NumRow).ToArray()).Certified,
          "an optimum has no certificate and a made-up one is refused");
    var into = m.CheckRay(new[] { 1.0, 0.0, 0.0 });
    Check(!into.Certified && into.MaxColEscape > 0.0, "a ray into a bound is refused");
    Check(Throws<ArgumentException>(() => m.CheckCertificate(new[] { 1.0, 2.0, 3.0, 4.0 })) &&
          Throws<ArgumentException>(() => m.CheckRay(new[] { 1.0 })),
          "a wrong length never reaches C");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 1.0 }, new[] { 0.0 }, new[] { 2.0 },
             new[] { 4.0 }, new[] { inf }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 1.0 });
    m.Solve();
    var r = m.CheckCertificate(m.Certificate()!);
    Check(m.Status == SolveStatus.Infeasible && r.Certified && Near(r.InfRows, 4.0, 1e-9) &&
          Near(r.SupColumns, 2.0, 1e-9) && Near(r.Gap, 2.0, 1e-9),
          "an infeasible model's certificate is checked: 4 against 2");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0 }, new[] { 0.0 }, new[] { inf },
             new[] { -inf }, new[] { inf }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 1.0 });
    m.Solve();
    var d = m.UnboundedRay()!;
    var r = m.CheckRay(d);
    Check(m.Status == SolveStatus.Unbounded && d.Length == 1 && d[0] > 0.0 && r.Certified &&
          r.Rate < 0.0 && r.MaxColEscape == 0.0, "an unbounded model's ray is checked");
}

using (var m = Model.Read(D("g_qp_unbounded.lp")))
{
    m.Solve();
    var d = m.UnboundedRay()!;
    var r = m.CheckRay(d);
    var flat = m.CheckRay(new[] { 0.0, 1.0 });
    Check(r.Certified && r.Curvature == 0.0 && !flat.Certified && flat.Curvature == 1.0,
          "a QP's ray is judged by its curvature too");
}

using (var m = Model.Read(D("t4_int.mps")))
{
    Check(m.ColInteger(0) && !m.ColInteger(1), "integer marks read back");
    m.Solve();
    var ck = m.CheckSolution(m.Solution().X);
    Check(Near(m.Objective, 3.5, 1e-9) && ck.PrimalFeasible && ck.MaxIntegralityViolation == 0.0,
          "a MIP's answer checks with no integrality violation");
}

using (var m = Model.Read(D("solve1.mps")))
{
    Check(Throws<JaosException>(() => m.Verify()), "verify needs an optimum");
    m.Solve();
    var r = m.Verify();
    Check(r.CapacityBits == 4096.0 && r.Status == Proof.Optimal && r.Stage == ProofStage.None &&
          r.AtRow == -1 && r.AtCol == -1 && r.Violation == 0.0 && r.Blocks >= 1 &&
          r.LargestBlock <= m.NumRow, "a small optimum is proved, and the report's layout holds");
    Check(Throws<JaosException>(() => m.ExactCertificate()),
          "an optimum has no exact certificate");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.5, new[] { 1.0 }, new[] { 0.0 }, new[] { inf },
             new[] { 1.0 }, new[] { inf }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 3.0 });
    bool early = Throws<JaosException>(() => m.ExactColValue(0));
    m.Solve();
    m.Verify();
    Check(early && Rat(m.ExactColValue(0), 1, 3) && Rat(m.ExactRowDual(0), 1, 3) &&
          Rat(m.ExactObjective(), 5, 6), $"a third comes back as {m.ExactColValue(0)}");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0, -1.0 }, new[] { 0.0, 0.0 },
             new[] { 3.0, inf }, new[] { -inf }, new[] { 4.0 }, new long[] { 0, 1, 2 },
             new long[] { 0, 0 }, new[] { 1.0, 1.0 });
    var r = m.VerifyBasis(new[] { BasisStatus.AtUpper, BasisStatus.Basic },
                          new[] { BasisStatus.AtUpper });
    bool proved = r.Status == Proof.Optimal && m.Status == SolveStatus.NotRun &&
                  Rat(m.ExactColValue(0), 3, 1) && Rat(m.ExactObjective(), -4, 1);
    var broken = m.VerifyBasis(new[] { BasisStatus.AtLower, BasisStatus.AtLower },
                               new[] { BasisStatus.Basic });
    Check(proved && broken.Status == Proof.Broken && broken.Stage == ProofStage.Dual &&
          Throws<JaosException>(() => m.VerifyBasis(new[] { BasisStatus.Basic, BasisStatus.Basic },
                                                    new[] { BasisStatus.Basic })) &&
          Throws<ArgumentException>(() => m.VerifyBasis(new[] { BasisStatus.Basic },
                                                        new[] { BasisStatus.AtUpper })),
          "a basis from outside is proved or broken with no solve");
}

using (var p = new Problem())
{
    var x = p.AddVar(name: "x");
    p.AddGe(3 * x, 1);
    p.Minimize(x);
    p.Solve();
    var v = p.Model.Verify();
    p.Model.WriteProof(P("x.proof"));
    string body = File.ReadAllText(P("x.proof"));
    using var q = new Problem();
    var y = q.AddVar(name: "x");
    q.AddGe(3 * y, 1);
    q.Minimize(y);
    q.Load();
    var rep = q.Model.CheckProof(P("x.proof"));
    Check(v.Status == Proof.Optimal && body.Contains("proof optimal") && body.Contains("1/3") &&
          rep.Primal && rep.Dual && rep.Objective && rep.BadRow == -1 && rep.Terms > 0 &&
          rep.Kind == ProofKind.Optimal && rep.Certified,
          "an optimum's proof file is written and judged from the model alone");
    File.WriteAllText(P("x.proof"), body.Replace("col x 1/3", "col x 1/4"));
    var bad = q.Model.CheckProof(P("x.proof"));
    Check(!bad.Primal && !bad.Certified && bad.Kind == ProofKind.Optimal,
          "and a tampered one is refused");
}

using (var p = new Problem())
{
    var x = p.AddVar(name: "x");
    var y = p.AddVar(name: "y");
    p.AddLe(x + y, 1, "low");
    p.AddGe(x + y, 2, "high");
    p.Minimize(x + y);
    p.Solve();
    var rep = p.Model.ExactCertificate();
    p.Model.WriteProof(P("cert.proof"));
    var chk = p.Model.CheckProof(P("cert.proof"));
    Check(p.Status == SolveStatus.Infeasible && rep.Derived && rep.BoundBits <= rep.CapacityBits &&
          Rat(p.Model.ExactRowMultiplier(0), -1, 1) && Rat(p.Model.ExactRowMultiplier(1), 1, 1) &&
          chk.Kind == ProofKind.Infeasible && chk.Certified,
          "the Farkas multipliers are derived exactly and their proof certifies");
}

using (var p = new Problem())
{
    var x = p.AddVar(name: "x");
    var y = p.AddVar(name: "y");
    p.AddLe(x - y, 1);
    p.Minimize(-1.0 * x);
    p.Solve();
    var rep = p.Model.ExactUnboundedRay();
    string dx = p.Model.ExactColDirection(0), dy = p.Model.ExactColDirection(1);
    p.Model.WriteProof(P("u.proof"));
    var chk = p.Model.CheckProof(P("u.proof"));
    Check(p.Status == SolveStatus.Unbounded && rep.Derived && rep.AtRow == -1 && dx == dy &&
          !dx.StartsWith("-", StringComparison.Ordinal) && dx != "0" &&
          chk.Kind == ProofKind.Unbounded && chk.Certified,
          $"the unbounded direction is derived exactly ({dx}, {dy}) and its proof certifies");
}

Model TwoRows()
{
    var m = new Model();
    m.LoadLp(Sense.Minimize, 0.0, new[] { 1.0 }, new[] { 0.0 }, new[] { inf },
             new[] { 1.0, -inf }, new[] { inf, 0.0 }, new long[] { 0, 2 },
             new long[] { 0, 1 }, new[] { 1.0, 1.0 });
    return m;
}

using (var m = TwoRows())
{
    m.Solve();
    var found = m.Iis();
    Check(found.RowSide.SequenceEqual(new[] { IisSide.Lower, IisSide.Upper }) &&
          found.ColSide.SequenceEqual(new[] { IisSide.None }) && found.Report.Members == 2 &&
          found.Report.Solves == 3 && found.Report.FromCertificate &&
          m.Status == SolveStatus.Infeasible && m.Certificate()!.Length == 2,
          "the IIS is the two rows and not the column bound");
    using var sub = m.IisModel(found);
    sub.Solve();
    var loose = found.RowSide.ToArray();
    loose[0] = IisSide.None;
    using var looser = m.IisModel(loose, found.ColSide);
    looser.Solve();
    Check(sub.Status == SolveStatus.Infeasible && sub.NumRow == 2 && sub.NumCol == 1 &&
          looser.Status == SolveStatus.Optimal &&
          Throws<ArgumentException>(() => m.IisModel(new[] { IisSide.None }, found.ColSide)),
          "the subsystem comes back as an infeasible model, and one member less is feasible");
}

using (var m = Model.Read(D("solve1.mps")))
{
    m.Solve();
    Check(Throws<JaosException>(() => m.Iis()), "a feasible model has no IIS");
}

Problem AsksTooMuch()
{
    var p = new Problem();
    var x = p.AddVar(0, 10, name: "x");
    var y = p.AddVar(0, 10, name: "y");
    p.AddGe(x + y, 30, "big");
    p.Minimize(x + y);
    p.Load();
    return p;
}

using (var p = AsksTooMuch())
{
    var r = p.Model.FeasRelax();
    var rows = p.Model.FeasRelax(RelaxScope.Rows);
    var cols = p.Model.FeasRelax(RelaxScope.Cols);
    Check(Near(r.Report.Total, 10.0, 1e-9) && r.RowMove.SequenceEqual(new[] { -10.0 }) &&
          r.ColMove.All(v => v == 0.0) && rows.Report.ColsMoved == 0 &&
          rows.Report.RowsMoved == 1 && cols.Report.RowsMoved == 0 &&
          cols.Report.ColsMoved == 1 && Near(rows.Report.Total, cols.Report.Total, 1e-9) &&
          p.Model.Status == SolveStatus.NotRun,
          "the feasibility relaxation moves the row by 10, or the columns when told to");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 0.0, 0.0 }, new[] { 0.0, 0.0 },
             new[] { 0.0, 0.0 }, new[] { 1.0 }, new[] { 1.0 }, new long[] { 0, 1, 2 },
             new long[] { 0, 0 }, new[] { 5.0, -7.0 });
    m.SetColInteger(0);
    m.SetColInteger(1);
    var r = m.FeasRelax(RelaxScope.Cols);
    Check(r.Report.Status == SolveStatus.Optimal && Near(r.Report.Total, 5.0, 1e-9) &&
          r.ColMove.SequenceEqual(new[] { 3.0, 2.0 }),
          "freed integer columns are boxed and the box grows: 3 and 2");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 0.0 }, new[] { 5.0 }, new[] { 3.0 },
             new[] { 0.0 }, new[] { inf }, new long[] { 0, 1 }, new long[] { 0 },
             new[] { 1.0 });
    Check(Throws<JaosException>(() => m.FeasRelax()), "a model with no relaxation throws");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 10);
    p.AddGe(x, 3.0);
    p.Minimize(x);
    p.Load();
    var r = p.Model.FeasRelax();
    Check(r.Report.Total == 0.0 && r.Report.AtRow == -1 && r.Report.AtCol == -1,
          "a feasible problem moves nothing");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Maximize, 0.0, new[] { 1.0, 1.0 }, new[] { 0.0, 0.0 },
             new[] { inf, inf }, new[] { -inf, -inf }, new[] { 4.0, 6.0 },
             new long[] { 0, 2, 4 }, new long[] { 0, 1, 0, 1 }, new[] { 1.0, 3.0, 2.0, 1.0 });
    Check(Throws<JaosException>(() => m.CostRanging()), "ranging needs an optimum");
    m.Solve();
    var c = m.CostRanging();
    var rhs = m.RhsRanging();
    var b = m.BoundRanging();
    Check(Near(c.Lower[0], 0.5, 1e-12) && Near(c.Upper[0], 3.0, 1e-12) &&
          Near(c.Lower[1], 1.0 / 3.0, 1e-12) && Near(c.Upper[1], 2.0, 1e-12),
          "cost ranging reads the textbook intervals");
    Check(Near(rhs.UpperLo[0], 2.0, 1e-12) && Near(rhs.UpperHi[0], 12.0, 1e-12) &&
          rhs.LowerLo[1] == -inf && Near(rhs.LowerHi[1], 6.0, 1e-12) &&
          b.LowerLo[0] == -inf && Near(b.LowerHi[0], 1.6, 1e-12) &&
          Near(b.UpperLo[1], 1.2, 1e-12) && b.UpperHi[1] == inf,
          "rhs and bound ranging come back four arrays each");
}

using (var m = Model.Read(D("g_quad.lp")))
{
    Check(m.ColQuadratic(0) == 2.0 && m.Statistics().QuadraticCol == 2,
          "a separable QP's diagonal reads back");
    m.Solve();
    double a = m.Objective;
    m.SetColQuadratic(0, -1.0);
    bool refused = false;
    try
    {
        m.Solve();
    }
    catch (JaosException e)
    {
        refused = e.Message.Contains("convex");
    }
    Check(Near(a, 4.0) && refused, "a nonconvex diagonal is refused at the solve");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -3.0, -3.0 }, new[] { 0.0, 0.0 },
             new[] { 3.0, 3.0 }, new[] { -inf }, new[] { 4.0 }, new long[] { 0, 1, 2 },
             new long[] { 0, 0 }, new[] { 1.0, 1.0 });
    m.SetQuadratic(new long[] { 0, 1, 1 }, new long[] { 0, 1, 0 }, new[] { 2.0, 2.0, 1.0 });
    var q = m.Quadratic();
    bool back = m.QuadraticNz == 3 && q.Rows.Zip(q.Cols).All(rc => rc.First >= rc.Second) &&
                Enumerable.Range(0, 3).Any(k => q.Rows[k] == 1 && q.Cols[k] == 0 && q.Values[k] == 1.0);
    m.Solve();
    double a = m.Objective;
    m.SetQuadratic(new long[0], new long[0], new double[0]);
    m.Solve();
    Check(back && Near(a, -3.0, 1e-5) && m.QuadraticNz == 0 && Near(m.Objective, -12.0),
          "a paired Q reads back as its lower triangle, solves to -3, and clears to -12");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { -1.0, -1.0 }, new[] { -inf, -inf },
             new[] { inf, inf }, new[] { -inf }, new[] { 2.0 }, new long[] { 0, 0, 0 },
             new long[0], new double[0]);
    m.SetRowQuadratic(0, new long[] { 0, 1 }, new long[] { 0, 1 }, new[] { 2.0, 2.0 });
    var rq = m.RowQuadratic(0);
    bool back = m.RowQuadraticNz(0) == 2 && rq.Rows.SequenceEqual(new long[] { 0, 1 }) &&
                rq.Cols.SequenceEqual(new long[] { 0, 1 }) && rq.Values.SequenceEqual(new[] { 2.0, 2.0 }) &&
                m.Statistics().QuadraticRow == 1;
    m.Solve();
    var s = m.Solution();
    var ck = m.CheckConicSolution(s.X, s.RowDual, new double[0][]);
    Check(back && Near(s.X[0], 1.0) && Near(s.RowActivity[0], 2.0) && Near(s.RowDual[0], -0.5) &&
          ck.DualFeasible, "a quadratic row reads back, solves and checks");
    m.SetRowQuadratic(0, new long[0], new long[0], new double[0]);
    Check(m.RowQuadratic(0).Rows.Length == 0, "and clears");
}

using (var m = NormModel())
{
    bool shape = m.NumCones == 1 && m.Cone(0).Type == ConeType.Quadratic &&
                 m.Cone(0).Cols.SequenceEqual(new long[] { 0, 1, 2 }) && m.Statistics().ConeSet == 1;
    m.Solve();
    var z = m.ConeDual(0);
    var s = m.Solution();
    var ck = m.CheckConicSolution(s.X, s.RowDual, new[] { z });
    Check(shape && Near(m.Objective, 5.0) && ck.PrimalFeasible && ck.CheckedDuals &&
          ck.DualFeasible && ck.MaxConeViolation <= 1e-7 &&
          Throws<ArgumentException>(() => m.CheckConicSolution(s.X, s.RowDual, new[] { z[..2] })),
          "a cone reads back, gives the norm, and its dual checks");
    m.WriteSolution(P("norm.sol"));
    var zs = m.ReadConeDuals(P("norm.sol"));
    Check(zs.Length == 1 && zs[0].SequenceEqual(z), "the cone duals read back from the solution file");
    bool held = false;
    try
    {
        m.DeleteCols(2);
    }
    catch (JaosException e)
    {
        held = e.Message.Contains("cone");
    }
    m.DeleteCones(0);
    Check(held && m.NumCones == 0 && Throws<JaosException>(() => m.Cone(0)),
          "a cone's column cannot go while the cone is there, and the cone can");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 0.0, 0.0 }, new[] { -inf, 2.0 },
             new[] { 1.0, 2.0 }, new double[0], new double[0], new long[] { 0, 0, 0 },
             new long[0], new double[0]);
    m.AddCone(ConeType.Quadratic, new long[] { 0, 1 });
    m.Solve();
    var z = m.ConeDual(0);
    Check(m.Status == SolveStatus.Infeasible &&
          m.CheckConicCertificate(new double[0], new[] { z }).Certified &&
          !m.CheckConicCertificate(new double[0], new[] { new[] { -1.0, 0.0 } }).Certified,
          "an infeasible cone is certified by its dual");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 10, name: "x");
    var z = p.AddVar(0, 1, true, "z");
    p.Maximize(x - 3 * z);
    var c = p.AddLe(x, 2, "c1");
    p.SetIndicator(c, z, 1);
    p.Solve();
    var (col, val) = p.Model.RowIndicator(c.Index);
    Check(Near(p.Objective, 10.0) && p.Value(x) == 10.0 && p.Value(z) == 0.0 &&
          col == z.Index && val == 1, "an indicator row holds only while its variable is 1");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 10, name: "x");
    var z = p.AddVar(0, 1, true, "z");
    p.Maximize(x - 3 * z);
    p.SetIndicator(p.AddLe(x, 2), z, 0);
    p.Solve();
    Check(Near(p.Objective, 7.0) && p.Value(x) == 10.0 && p.Value(z) == 1.0,
          "and one on 0 holds while its variable is 0");
}

using (var m = new Model())
{
    m.LoadLp(Sense.Minimize, 0.0, new[] { 1.0, 1.0, 0.0 }, new[] { 0.0, 0.0, 0.0 },
             new[] { 4.0, 4.0, 1.0 }, new[] { 6.0 }, new[] { 8.0 }, new long[] { 0, 1, 2, 2 },
             new long[] { 0, 0 }, new[] { 1.0, 1.0 });
    m.SetColInteger(2);
    m.SetRowIndicator(0, 2, 1);
    bool held = Throws<JaosException>(() => m.SetColInteger(2, false));
    m.SetRowIndicator(0, null);
    m.SetColInteger(2, false);
    Check(held && m.RowIndicator(0).Col == null && m.RowIndicator(0).Value == 0 && !m.ColInteger(2),
          "an indicator's switch stays integer until the indicator is cleared");
}

foreach (var (type, want) in new[] { (1, 1.0), (2, 2.0) })
{
    using var p = new Problem();
    var xs = Enumerable.Range(0, 3).Select(k => p.AddVar(0, 1, name: $"x{k}")).ToArray();
    p.AddLe(xs[0] + xs[1] + xs[2], 10);
    p.Maximize(xs[0] + xs[1] + xs[2]);
    p.AddSos(type, xs);
    p.Solve();
    var nz = Enumerable.Range(0, 3).Where(k => Math.Abs(p.Value(xs[k])) > 1e-9).ToArray();
    var (t, cols, ws) = p.Model.Sos(0);
    Check(Near(p.Objective, want) && nz.Length == (int)want &&
          (type == 1 || nz[1] - nz[0] == 1) && p.Model.NumSos == 1 && t == type &&
          cols.SequenceEqual(new long[] { 0, 1, 2 }) && ws.SequenceEqual(new[] { 1.0, 2.0, 3.0 }),
          $"a type {type} set lets {want} members be nonzero, and reads back");
    if (type == 2)
        Check(Throws<JaosException>(() => p.Model.AddSos(1, new long[] { 0, 0 }, new[] { 1.0, 2.0 })),
              "a set with a repeated member throws");
}

using (var p = new Problem())
{
    var x = p.AddVar(2, 10, name: "x", semicontinuous: true);
    var y = p.AddVar(0, 1, name: "y");
    p.AddGe(x + y, 1);
    p.Minimize(x + 5 * y);
    p.Solve();
    bool low = Near(p.Objective, 2.0) && p.Value(x) == 2.0 && p.Value(y) == 0.0;
    p.Minimize(10 * x + 5 * y);
    p.Solve();
    var m = p.Model;
    Check(low && Near(p.Objective, 5.0) && p.Value(x) == 0.0 && p.Value(y) == 1.0 &&
          m.ColSemicontinuous(0) && !m.ColSemicontinuous(1) && !m.ColInteger(0),
          "a semi-continuous variable rests at zero or above its floor");
    m.SetColSemicontinuous(0, false);
    Check(!m.ColSemicontinuous(0), "and its mark comes off");
}

using (var p = new Problem())
{
    var x = p.AddVar(2, 10, name: "x", semicontinuous: true);
    var z = p.AddVar(0, 1, true, "z");
    var y = p.AddVar(0, 1, name: "y");
    p.SetIndicator(p.AddLe(x + y, 5), z, 1);
    p.AddSos(1, new[] { x, y });
    p.Minimize(x + y + z);
    p.Load();
    var st = p.Model.Statistics();
    Check(st.SemicontinuousCol == 1 && st.SosSet == 1 && st.IndicatorRow == 1 &&
          st.IntegerCol == 1 && p.Model.HasInteger, "the statistics count the new kinds");
}

using (var p = new Problem())
{
    var a = p.AddVar(0, 1, true, "a");
    var b = p.AddVar(0, 1, true, "b");
    var c = p.AddVar(0, 1, true, "c");
    p.AddLe(3 * a + 5 * b + 2 * c, 8);
    p.Maximize(10 * a + 13 * b + 7 * c);
    p.Load();
    var st = p.Model.Statistics();
    Check(st.NumCol == 3 && st.NumRow == 1 && st.IntegerCol == 3 && st.BinaryCol == 3 &&
          st.OneSidedRow == 1 && st.ObjNz == 3 &&
          st.NumRow == st.EqualityRow + st.RangedRow + st.OneSidedRow + st.FreeRow &&
          st.NumCol == st.FixedCol + st.RangedCol + st.OneSidedCol + st.FreeCol &&
          st.MinAbs == 2.0 && st.MaxAbs == 5.0 && st.ObjMinAbs == 7.0 && st.ObjMaxAbs == 13.0,
          "the statistics count the model and its magnitude ranges");
    p.Solve();
    double best = p.Objective;
    p.Model.SetMipStart(new[] { 1.0, 1.0, 0.0 });
    p.Solve();
    bool started = Near(p.Objective, best);
    p.Model.SetMipStart(new[] { 9.0, 9.0, 9.0 });
    p.Solve();
    bool refused = Near(p.Objective, best);
    p.Model.SetMipStart(null);
    p.Model.SetMipCutoff(best + 1000.0);
    var cut = p.Solve();
    p.Model.SetMipCutoff(inf);
    Check(started && refused && cut == SolveStatus.Infeasible && p.Solve() == SolveStatus.Optimal &&
          Throws<ArgumentException>(() => p.Model.SetMipStart(new[] { 1.0 })),
          "a start point, a wrong one and a cutoff keep the answer honest");
    p.Model.SetMipStart(new[] { 1.0, 1.0, 0.0 });
    p.AddVar(0, 1, true, "d");
    Check(p.Solve() == SolveStatus.Optimal && Near(p.Objective, best),
          "a start point is padded to the new column count when a new variable reloads the problem");
}

using (var p = new Problem())
{
    var x = Enumerable.Range(0, 4).Select(k => p.AddVar(name: $"x{k}")).ToArray();
    p.AddEq(x[0] - x[1], 1);
    p.AddGe(x[0] + x[2], 3);
    p.AddLe(x[0] + x[3], 10);
    p.AddGe(x[1] + x[2] + x[3], 2);
    p.Minimize(2 * x[0] + x[1] + 3 * x[2] + x[3]);
    p.Solve();
    var rep = p.Model.PresolveReport();
    Check(Near(p.Objective, 8.0, 1e-12) && rep.AggregatedCol == 1 && rep.Rounds >= 0 &&
          rep.NumRow <= 4, "the presolve report reaches its last field: one aggregated column");
}

using (var p = new Problem())
{
    var a = p.AddVar(0, 1, true, "a");
    var b = p.AddVar(0, 1, true, "b");
    var c = p.AddVar(0, 1, true, "c");
    p.AddLe(2 * a + 3 * b + c, 5);
    p.Maximize(5 * a + 4 * b + 3 * c);
    p.Model.SetMipTighten(0);
    p.Solve();
    var rep = p.Model.MipResult();
    var (ix, iobj) = p.Model.MipIncumbent();
    bool root = Near(p.Objective, 9.0) && rep.HasIncumbent && rep.Cuts >= 1 && rep.Nodes == 1 &&
                Near(iobj, 9.0) && ix.SequenceEqual(new[] { 1.0, 1.0, 0.0 });
    p.Model.SetMipCutRounds(0);
    p.Model.SetMipCoverRounds(0);
    p.Model.SetMipMirRounds(0);
    p.Model.SetMipCutDepth(0);
    p.Model.SetMipDive(true);
    p.Model.SetMipHeuristics(false);
    p.Model.SetMipDiveHeuristic(0);
    p.Model.SetMipFeaspump(0);
    p.Solve();
    rep = p.Model.MipResult();
    Check(root && Near(p.Objective, 9.0) && rep.Cuts == 0 && rep.HeuristicPoints == 0 &&
          rep.Nodes >= 2, "the knapsack closes at the root with cuts and branches without them");
}

{
    var moved = new List<long>();
    foreach (int rounds in new[] { 0, 4 })
    {
        using var p = new Problem();
        var x = p.AddVar(0, 10, true, "x");
        var y = p.AddVar(0, 10, true, "y");
        var z = p.AddVar(0, 10, true, "z");
        p.AddLe(x + y + z, 3);
        p.AddLe(2 * x + y, 3);
        p.Maximize(3 * x + 2.4 * y + 2 * z);
        p.Model.SetMipPropagate(rounds);
        p.Model.SetMipRcfix(1);
        p.Model.SetMipPropagateDepth(-1);
        p.Model.SetMipPumpAlways(1);
        p.Solve();
        var rep = p.Model.MipResult();
        if (Near(p.Objective, 7.4, 1e-9) && rep.FixedCols >= 0)
            moved.Add(rep.Tightened);
    }
    Check(moved.Count == 2 && moved[0] == 0 && moved[1] >= 4,
          "the report counts the bounds propagation tightened");
}

{
    var orbits = new List<long>();
    foreach (int on in new[] { 1, 0 })
    {
        using var p = new Problem();
        var xs = Enumerable.Range(0, 3).Select(k => p.AddVar(0, 1, true, $"x{k}")).ToArray();
        p.AddGe(xs[0] + xs[1] + xs[2], 1);
        p.Minimize(xs[0] + xs[1] + xs[2]);
        p.Model.SetMipSymmetry(on);
        p.Model.SetMipOrbital(on);
        p.Solve();
        var rep = p.Model.MipResult();
        orbits.Add(rep.SymmetryOrbits);
        if (on == 1)
            orbits.Add(rep.SymmetryGenerators);
    }
    Check(orbits[0] == 1 && orbits[1] >= 1 && orbits[2] == 0,
          $"the report counts the symmetry found: {orbits[0]} orbit, {orbits[1]} generators");
}

using (var p = new Problem())
{
    var names = "abcde";
    var v = names.Select(n => p.AddVar(0, 1, true, n.ToString())).ToArray();
    p.AddLe(3 * v[0] + 5 * v[1] + 2 * v[2] + 4 * v[3] + 2 * v[4], 8);
    p.Maximize(10 * v[0] + 13 * v[1] + 7 * v[2] + 9 * v[3] + 5 * v[4]);
    p.Model.SetMipCutRounds(0);
    p.Model.SetMipCoverRounds(0);
    p.Model.SetMipMirRounds(0);
    p.Model.SetMipCutDepth(0);
    p.Model.SetMipPoolSize(3);
    p.Solve();
    var pool = p.MipPool();
    var objs = pool.Select(e => e.Objective).ToArray();
    Check(pool.Length >= 1 && pool.Length <= 3 && Near(pool[0].Objective, 23.0, 1e-9) &&
          pool[0].X[v[0].Index] == 1.0 && pool[0].X[v[1].Index] == 1.0 &&
          objs.SequenceEqual(objs.OrderByDescending(o => o)) &&
          pool.Select(e => string.Join(",", e.X)).Distinct().Count() == pool.Length,
          $"the solution pool holds {pool.Length} distinct points, best first");
}

using (var m = Model.Read(D("solve1.mps")))
{
    var seen = new List<Progress>();
    m.SetProgressCallback(pr =>
    {
        seen.Add(pr);
        return CallbackAction.Continue;
    });
    m.Solve();
    using var quiet = Model.Read(D("solve1.mps"));
    quiet.Solve();
    Check(m.Status == SolveStatus.Optimal && seen.Count > 0 && seen.All(s => s.Iterations >= 0) &&
          quiet.Objective == m.Objective && quiet.WorkUnits == m.WorkUnits,
          $"the progress callback sees the solve {seen.Count} times and changes nothing");
}

using (var m = Model.Read(D("solve1.mps")))
{
    m.SetProgressCallback(_ => CallbackAction.Stop);
    m.Solve();
    bool stopped = m.Status == SolveStatus.Interrupted &&
                   Throws<JaosException>(() => { _ = m.Objective; });
    m.SetProgressCallback(null);
    m.Solve();
    Check(stopped && m.Status == SolveStatus.Optimal,
          "a progress callback stops the solve, and the next solve after its removal finishes");
}

using (var m = Model.Read(D("solve1.mps")))
{
    m.SetProgressCallback(_ => throw new InvalidOperationException("boom"));
    bool boom = false;
    try
    {
        m.Solve();
    }
    catch (InvalidOperationException e)
    {
        boom = e.Message == "boom";
    }
    Check(boom && m.Status == SolveStatus.Interrupted,
          "an exception in a callback stops the solve and Solve throws it");
}

using (var p = new Problem())
{
    var x = p.AddVar(integer: true, name: "x");
    var y = p.AddVar(integer: true, name: "y");
    p.AddLe(x + y, 3.6);
    p.AddLe(1.0 * x, 2.2);
    p.AddLe(1.0 * y, 1.4);
    p.Maximize(x + y);
    var pm = p.Model;
    pm.SetMipCutRounds(0);
    pm.SetMipCoverRounds(0);
    pm.SetMipMirRounds(0);
    pm.SetMipCutDepth(0);
    pm.SetMipNodeLimit(1);
    var seen = new List<Incumbent>();
    pm.SetIncumbentCallback(inc =>
    {
        seen.Add(inc);
        return CallbackAction.Continue;
    });
    var st = p.Solve();
    var (_, iobj) = pm.MipIncumbent();
    Check(st == SolveStatus.NodeLimit && seen.Count == 1 && seen[0].Node == 1 &&
          seen[0].ByRounding && seen[0].Values[x.Index] == 2.0 &&
          seen[0].Values[y.Index] == 1.0 && Near(iobj, 3.0),
          "the incumbent callback sees the rounded point a node limit keeps");
    pm.SetMipNodeLimit(0);
    pm.SetIncumbentCallback(_ => CallbackAction.Stop);
    Check(p.Solve() == SolveStatus.Interrupted && pm.MipResult().HasIncumbent,
          "and stopping the tree there keeps the incumbent");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 1, true, "x");
    var y = p.AddVar(0, 1, true, "y");
    var z = p.AddVar(0, 1, true, "z");
    p.AddLe(x + y + z, 2);
    p.Maximize(2 * x + 2 * y + z);
    bool integral = false;
    p.Model.SetNodeCallback(ev =>
    {
        if (ev.Integral)
        {
            integral = true;
            if (ev.Values[x.Index] + ev.Values[y.Index] > 1.5)
                ev.AddRow(x + y, -inf, 1);
        }
        return CallbackAction.Continue;
    });
    p.Solve();
    Check(Near(p.Objective, 3.0) && p.Value(x) + p.Value(y) <= 1.0 + 1e-9 && integral,
          "the node callback adds a lazy row against an integral point");
    p.Model.SetNodeCallback(null);
    p.Solve();
    Check(Near(p.Objective, 4.0), "and without it the search reaches 4");
}

using (var q = new Problem())
{
    var a = q.AddVar(0, 1, true, "a");
    var b = q.AddVar(0, 1, true, "b");
    var c = q.AddVar(0, 1, true, "c");
    q.AddLe(2 * a + 2 * b + 2 * c, 3);
    q.Maximize(3 * a + 2.5 * b + 2 * c);
    var qm = q.Model;
    Plain(qm);
    q.Solve();
    long plain = qm.MipResult().Nodes;
    var choices = new List<long>();
    qm.SetNodeCallback(ev =>
    {
        if (ev.Node == 1 && !ev.Integral)
        {
            choices.Add(ev.BranchCol);
            ev.AddRow(a + b + c, -inf, 1);
        }
        return CallbackAction.Continue;
    });
    q.Solve();
    Check(plain > 1 && Near(q.Objective, 3.0) && qm.MipResult().Nodes == 1 &&
          choices.Count > 0 && choices[0] == b.Index,
          "a user cut at the root closes the tree, and the event names the branching column");
    qm.SetNodeCallback(ev =>
    {
        ev.AddRow(new long[] { 7 }, new[] { 1.0 }, 0.0, 1.0);
        return CallbackAction.Continue;
    });
    bool threw = Throws<JaosException>(() => q.Solve());
    Check(threw && q.Status == SolveStatus.Interrupted,
          "a row the node may not take stops the search, and Solve throws");
    qm.SetNodeCallback(_ => CallbackAction.Stop);
    Check(q.Solve() == SolveStatus.Interrupted, "a node callback's Stop interrupts the search");
}

using (var s = new Problem())
{
    var a = s.AddVar(0, 1, true, "a");
    var b = s.AddVar(0, 1, true, "b");
    var c = s.AddVar(0, 1, true, "c");
    s.AddLe(2 * a + 2 * b, 3);
    s.AddLe(2 * a + 2 * c, 3);
    s.Maximize(3 * a + b + c);
    Plain(s.Model);
    var depth1 = new List<double>();
    var steered = new List<int>();
    s.Model.SetNodeCallback(ev =>
    {
        if (ev.Depth == 0 && !ev.Integral)
        {
            int other = ev.BranchCol == b.Index ? c.Index : b.Index;
            steered.Add(other);
            ev.BranchCol = other;
        }
        if (ev.Depth == 1 && steered.Count > 0)
            depth1.Add(ev.Values[steered[0]]);
        return CallbackAction.Continue;
    });
    s.Solve();
    Check(Near(s.Objective, 3.0) && steered.Count > 0 && depth1.Count > 0 &&
          depth1.All(v => v == 0.0 || v == 1.0),
          "the node callback chooses the column the node branches on");
}

using (var p = new Problem())
{
    var x = p.AddVar();
    var y = p.AddVar();
    var c = p.AddLe(x + y, 4);
    p.Minimize(-1.0 * x - 2 * y);
    p.Solve();
    double a = p.Objective;
    p.Model.SetObjectiveName("kept");
    c.Ub = 3;
    bool stale = Throws<InvalidOperationException>(() => p.Value(y)) &&
                 Throws<InvalidOperationException>(() => { _ = p.Objective; });
    p.Solve();
    double b = p.Objective;
    x.Lb = 1;
    p.Solve();
    double d = p.Objective;
    p.Minimize(-3.0 * x - 2 * y);
    p.Solve();
    double e = p.Objective;
    bool costs = p.Model.ColCost(0) == -3.0;
    p.Maximize(-1.0 * x - 2 * y);
    p.Solve();
    double f = p.Objective;
    p.Minimize(-1.0 * x - 2 * y + 100);
    p.Solve();
    double g = p.Objective;
    bool warm = p.Model.ObjectiveName == "kept" && p.Model.ObjOffset == 100.0 &&
                p.Model.RowBounds(0) == (-inf, 3.0) && p.Model.ColBounds(0) == (1.0, inf);
    Check(Near(a, -8.0) && stale && Near(b, -6.0) && Near(d, -5.0) && Near(e, -9.0) && costs &&
          Near(f, -1.0) && Near(g, 95.0) && warm,
          "a moved side, bound, cost, sense and constant reach the model through its setters: -8, -6, -5, -9, -1, 95");
    var z = p.AddVar(0, 2);
    p.Minimize(-1.0 * x - 2 * y - 3 * z);
    p.Solve();
    Check(Near(p.Objective, -11.0) && Near(p.Value(z), 2.0) && p.Model.ObjectiveName != "kept",
          "a new variable loads the problem again");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 10, name: "x");
    var y = p.AddVar(0, 10, name: "y");
    p.AddGe(x + y, 2, "c1");
    p.Minimize(x + y + x * x + y * y);
    p.Solve();
    bool qp = Near(p.Objective, 4.0) && Near(p.Value(x), 1.0, 1e-5) && p.Model.ColQuadratic(0) == 2.0;
    p.Minimize(x + y);
    p.Solve();
    Check(qp && p.Model.ColQuadratic(0) == 0.0 && Near(p.Objective, 2.0) &&
          Throws<InvalidOperationException>(() => { _ = (x + y) * (x + y); }),
          "squares in the objective solve, a new objective clears them, and a product of sums throws");
}

using (var q = new Problem())
{
    var a = q.AddVar(0, 5, true, "a");
    var b = q.AddVar(0, 5, true, "b");
    q.AddLe(a + b, 3);
    q.Minimize(a * a + b * b - 5.2 * a - 2.6 * b);
    q.Solve();
    Check(Near(q.Objective, -8.0) && Near(q.Value(a), 2.0) && Near(q.Value(b), 1.0),
          "an integer QP through expressions solves to -8");
}

using (var p = new Problem())
{
    var x = p.AddVar(-inf, name: "x");
    var y = p.AddVar(-inf, name: "y");
    var ball = p.AddLe(x * x + y * y, 2, "ball");
    p.Minimize(-1.0 * x - y);
    p.Solve();
    Check(Near(p.Objective, -2.0) && Near(p.Value(x), 1.0) && Near(p.Activity(ball), 2.0) &&
          Near(p.Dual(ball), -0.5) && p.Check().DualFeasible &&
          p.Model.RowQuadraticNz(ball.Index) == 2 && Near(p.Value(x * x + y * y), 2.0),
          "a quadratic row in a problem solves on the ball");
}

using (var p = new Problem())
{
    var x = p.AddVar(0, 5, true, "x");
    p.AddGe(x, 1.5, "floor");
    p.Minimize(x);
    p.Load();
    p.Model.WriteQplib(P("pq.qplib"));
    p.Model.WriteOsil(P("pq.osil"));
    using var q = Model.Read(P("pq.qplib"));
    using var o = Model.Read(P("pq.osil"));
    q.Solve();
    o.Solve();
    Check(q.ColInteger(0) && o.ColInteger(0) && q.Objective == 2.0 && o.Objective == 2.0,
          "a problem's integer column goes through QPLIB and OSiL");
}

Directory.Delete(tmp, true);
Console.WriteLine($"{passed} passed, {failed} failed");
return failed == 0 ? 0 : 1;
