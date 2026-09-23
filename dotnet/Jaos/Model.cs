// SPDX-License-Identifier: Apache-2.0
using System;
using System.Runtime.InteropServices;
using System.Text;

namespace Jaos;

/// <summary>What a C call returned when it was not <c>JAOS_OK</c>.</summary>
public enum Status
{
    Ok = 0,
    InvalidInput,
    OutOfMemory,
    Io,
    Numerical,
}

/// <summary>Where the last solve stopped: the C <c>jaos_solve_status</c>.</summary>
public enum SolveStatus
{
    NotRun = 0,
    Optimal,
    Infeasible,
    Unbounded,
    WorkLimit,
    TimeLimit,
    NumericalError,
    Interrupted,
    NodeLimit,
}

public enum Sense
{
    Minimize = 0,
    Maximize = 1,
}

public enum ConeType
{
    Quadratic = 1,
    Rotated = 2,
}

public enum LogLevel
{
    Off = 0,
    Summary,
    Progress,
    Detail,
}

/// <summary>A C call that failed, with the model's message.</summary>
public sealed class JaosException : Exception
{
    public Status Status { get; }

    public JaosException(Status status, string message)
        : base(message.Length > 0 ? message : status.ToString())
    {
        Status = status;
    }
}

/// <summary>The answer of an optimal solve, in the model's own signs.</summary>
public sealed record Solution(double[] X, double[] RowActivity,
                              double[] RowDual, double[] ReducedCost);

/// <summary>The C <c>jaos_mip_report</c>.</summary>
public sealed record MipReport(long Nodes, long LpSolves, bool HasIncumbent,
                               double Incumbent, double Bound, long Cuts,
                               long HeuristicPoints, long FirstIncumbentNode);

/// <summary>
/// A <c>jaos_model</c>: every call of the C library that builds, reads,
/// writes, solves and reads back a model, one method each. The handle is
/// freed by <see cref="Dispose"/> or by the finaliser.
/// </summary>
public sealed class Model : IDisposable
{
    private readonly ModelHandle h;
    private Native.LogFn? logFn;
    private Action<string>? logSink;
    private LogLevel logLevel;
    private double[]? mipStart;

    static Model() => Native.Ensure();

    public Model()
    {
        int st = Native.jaos_model_new(out h);
        if (st != 0)
            throw new JaosException((Status)st, "jaos_model_new failed");
    }

    public void Dispose() => h.Dispose();

    public static string Version =>
        Marshal.PtrToStringUTF8(Native.jaos_version()) ?? "";

    public static string StatusString(SolveStatus s) =>
        Marshal.PtrToStringUTF8(Native.jaos_solve_status_str((int)s)) ?? "";

    /// <summary>The model's last message.</summary>
    public string Error =>
        Marshal.PtrToStringUTF8(Native.jaos_model_error(h)) ?? "";

    private void Check(int st)
    {
        if (st != 0)
            throw new JaosException((Status)st, Error);
    }

    /// <summary>
    /// Reads <paramref name="path"/> with the reader its name selects, as
    /// <c>jaos solve</c> does: MPS for a name with no known extension.
    /// </summary>
    public static Model Read(string path)
    {
        var m = new Model();
        try
        {
            m.ReadFile(path);
        }
        catch
        {
            m.Dispose();
            throw;
        }
        return m;
    }

    public void ReadFile(string path)
    {
        string p = path.EndsWith(".gz", StringComparison.Ordinal)
            ? path[..^3] : path;
        int st = p.EndsWith(".lp", StringComparison.Ordinal) ? Native.jaos_read_lp(h, path)
            : p.EndsWith(".nl", StringComparison.Ordinal) ? Native.jaos_read_nl(h, path)
            : p.EndsWith(".qplib", StringComparison.Ordinal) ? Native.jaos_read_qplib(h, path)
            : p.EndsWith(".osil", StringComparison.Ordinal) ? Native.jaos_read_osil(h, path)
            : p.EndsWith(".cbf", StringComparison.Ordinal) ? Native.jaos_read_cbf(h, path)
            : Native.jaos_read_mps(h, path);
        Check(st);
    }

    public void WriteMps(string path) => Check(Native.jaos_write_mps(h, path));
    public void WriteLp(string path) => Check(Native.jaos_write_lp(h, path));
    public void WriteSolution(string path) => Check(Native.jaos_write_solution(h, path));

    /// <summary>
    /// The whole model in one call, the matrix column-wise with 0-based
    /// starts (one more than the columns) and row indices.
    /// </summary>
    public void LoadLp(Sense sense, double offset, double[] cost,
                       double[] colLower, double[] colUpper,
                       double[] rowLower, double[] rowUpper,
                       long[] aStart, long[] aIndex, double[] aValue) =>
        Check(Native.jaos_load_lp(h, cost.Length, rowLower.Length, (int)sense,
                                  offset, cost, colLower, colUpper, rowLower,
                                  rowUpper, aValue.Length, aStart, aIndex,
                                  aValue));

    public long NumCol => Native.jaos_num_col(h);
    public long NumRow => Native.jaos_num_row(h);
    public long NumCones => Native.jaos_num_cones(h);
    public bool HasInteger => Native.jaos_model_has_integer(h);

    public void SetColInteger(long col, bool on = true) =>
        Check(Native.jaos_set_col_integer(h, col, on));
    public void SetColBounds(long col, double lower, double upper) =>
        Check(Native.jaos_set_col_bounds(h, col, lower, upper));
    public void SetRowBounds(long row, double lower, double upper) =>
        Check(Native.jaos_set_row_bounds(h, row, lower, upper));
    public void SetColCost(long col, double cost) =>
        Check(Native.jaos_set_col_cost(h, col, cost));

    /// <summary>The diagonal entry of the objective's <c>½ x'Qx</c>.</summary>
    public void SetColQuadratic(long col, double q) =>
        Check(Native.jaos_set_col_quadratic(h, col, q));

    /// <summary>
    /// The objective's <c>½ x'Qx</c> as triplets, one per diagonal entry
    /// and one per off-diagonal pair.
    /// </summary>
    public void SetQuadratic(long[] rows, long[] cols, double[] values) =>
        Check(Native.jaos_set_quadratic(h, values.Length, rows, cols, values));

    /// <summary>Row <paramref name="row"/>'s part <c>½ x'Qx</c>.</summary>
    public void SetRowQuadratic(long row, long[] rows, long[] cols,
                                double[] values) =>
        Check(Native.jaos_set_row_quadratic(h, row, values.Length, rows, cols,
                                            values));

    public void AddCone(ConeType type, long[] cols) =>
        Check(Native.jaos_add_cone(h, (int)type, cols.Length, cols));

    public void SetOption(string name, string value) =>
        Check(Native.jaos_set_option(h, name, value));

    public string GetOption(string name)
    {
        var buf = new byte[512];
        Check(Native.jaos_get_option(h, name, buf, buf.Length));
        int end = Array.IndexOf(buf, (byte)0);
        return Encoding.UTF8.GetString(buf, 0, end < 0 ? buf.Length : end);
    }

    public void SetTimeLimit(double seconds) =>
        Check(Native.jaos_set_time_limit(h, seconds));
    public void SetWorkLimit(long units) =>
        Check(Native.jaos_set_work_limit(h, units));
    public void SetThreads(long threads) =>
        Check(Native.jaos_set_threads(h, threads));
    public void SetMipStart(double[] x)
    {
        Check(Native.jaos_set_mip_start(h, x));
        mipStart = (double[])x.Clone();
    }

    /// <summary>The name of every option <see cref="SetOption"/> takes.</summary>
    public static string[] OptionNames()
    {
        long n = Native.jaos_num_options();
        var names = new string[n];
        for (long k = 0; k < n; k++)
            names[k] = Marshal.PtrToStringUTF8(Native.jaos_option_name(k)) ?? "";
        return names;
    }

    internal void CopySettingsTo(Model other)
    {
        foreach (var name in OptionNames())
            other.SetOption(name, GetOption(name));
        if (logSink != null)
            other.SetLog(logLevel, logSink);
        if (mipStart != null)
        {
            var x = new double[other.NumCol];
            Array.Copy(mipStart, x, Math.Min(mipStart.Length, x.Length));
            other.SetMipStart(x);
        }
    }

    /// <summary>
    /// Sends the solver's log to <paramref name="sink"/> at
    /// <paramref name="level"/>; null or <see cref="LogLevel.Off"/> stops it.
    /// </summary>
    public void SetLog(LogLevel level, Action<string>? sink)
    {
        if (sink == null || level == LogLevel.Off)
        {
            Check(Native.jaos_set_log_callback(h, null, IntPtr.Zero));
            logFn = null;
            logSink = null;
        }
        else
        {
            logFn = (user, lvl, line) =>
                sink(Marshal.PtrToStringUTF8(line) ?? "");
            Check(Native.jaos_set_log_callback(h, logFn, IntPtr.Zero));
            logSink = sink;
        }
        logLevel = level;
        Check(Native.jaos_set_log_level(h, (int)level));
    }

    public void Solve() => Check(Native.jaos_solve(h));

    public SolveStatus Status => (SolveStatus)Native.jaos_status_of(h);

    public double Objective
    {
        get
        {
            Check(Native.jaos_objective(h, out double v));
            return v;
        }
    }

    public Solution Solution()
    {
        long nc = NumCol, nr = NumRow;
        var x = new double[nc];
        var act = new double[nr];
        var y = new double[nr];
        var d = new double[nc];
        Check(Native.jaos_solution(h, x, act, y, d));
        return new Solution(x, act, y, d);
    }

    /// <summary>The dual of cone <paramref name="k"/>, one entry per member.</summary>
    public double[] ConeDual(long k)
    {
        Check(Native.jaos_cone(h, k, out _, out long n, null));
        var z = new double[n];
        Check(Native.jaos_cone_dual(h, k, z));
        return z;
    }

    /// <summary>The Farkas multipliers of an infeasible model, or null.</summary>
    public double[]? Certificate()
    {
        var y = new double[Math.Max(NumRow, 1)];
        return Native.jaos_certificate(h, y) == 0 ? y[..(int)NumRow] : null;
    }

    /// <summary>The improving direction of an unbounded model, or null.</summary>
    public double[]? UnboundedRay()
    {
        var d = new double[Math.Max(NumCol, 1)];
        return Native.jaos_unbounded_ray(h, d) == 0 ? d[..(int)NumCol] : null;
    }

    public MipReport MipResult()
    {
        Check(Native.jaos_mip_result(h, out NativeMipReport r));
        return new MipReport(r.Nodes, r.LpSolves, r.HasIncumbent, r.Incumbent,
                             r.Bound, r.Cuts, r.HeuristicPoints,
                             r.FirstIncumbentNode);
    }

    /// <summary>The incumbent of a tree that stopped with one.</summary>
    public (double[] X, double Objective) MipIncumbent()
    {
        var x = new double[NumCol];
        Check(Native.jaos_mip_incumbent(h, x, out double v));
        return (x, v);
    }

    public long Iterations => Native.jaos_iterations(h);
    public long WorkUnits => Native.jaos_work_units(h);
}
