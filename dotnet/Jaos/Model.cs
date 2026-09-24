// SPDX-License-Identifier: Apache-2.0
using System;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

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
                               long HeuristicPoints, long FirstIncumbentNode,
                               long FixedCols, long Tightened,
                               long SymmetryGenerators, long SymmetryOrbits,
                               bool StartAccepted);

/// <summary>
/// A <c>jaos_model</c>: every call of the C library that builds, reads,
/// writes, solves and reads back a model, one method each. The handle is
/// freed by <see cref="Dispose"/> or by the finaliser.
/// </summary>
public sealed class Model : IDisposable
{
    private const int NameCap = 256;

    private readonly ModelHandle h;
    private Native.LogFn? logFn;
    private Action<LogLevel, string>? logSink;
    private LogLevel logLevel;
    private Native.ProgressFn? progressFn;
    private Func<Progress, CallbackAction>? progressSink;
    private Native.IncumbentFn? incumbentFn;
    private Func<Incumbent, CallbackAction>? incumbentSink;
    private Native.NodeFn? nodeFn;
    private Func<NodeEvent, CallbackAction>? nodeSink;
    private Exception? pending;
    private double[]? mipStart;

    private static readonly int BranchColOffset =
        (int)Marshal.OffsetOf<NativeNode>(nameof(NativeNode.BranchCol));

    static Model() => Native.Ensure();

    public Model()
    {
        int st = Native.jaos_model_new(out h);
        if (st != 0)
            throw new JaosException((Status)st, "jaos_model_new failed");
    }

    private Model(ModelHandle handle) => h = handle;

    public void Dispose() => h.Dispose();

    public static string Version =>
        Marshal.PtrToStringUTF8(Native.jaos_version()) ?? "";

    /// <summary>The git commit the library was built from, empty when the build had none.</summary>
    public static string BuildCommit =>
        Marshal.PtrToStringUTF8(Native.jaos_build_commit()) ?? "";

    public static string StatusString(SolveStatus s) =>
        Marshal.PtrToStringUTF8(Native.jaos_solve_status_str((int)s)) ?? "";

    public static string StatusString(Status s) =>
        Marshal.PtrToStringUTF8(Native.jaos_status_str((int)s)) ?? "";

    /// <summary>The library's infinity, which is the IEEE one.</summary>
    public static double Infinity => Native.jaos_infinity();

    /// <summary>The model's last message.</summary>
    public string Error =>
        Marshal.PtrToStringUTF8(Native.jaos_model_error(h)) ?? "";

    private void Check(int st)
    {
        var e = Interlocked.Exchange(ref pending, null);
        if (e != null)
            ExceptionDispatchInfo.Capture(e).Throw();
        if (st != 0)
            throw new JaosException((Status)st, Error);
    }

    private void Hold(Exception e) =>
        Interlocked.CompareExchange(ref pending, e, null);

    private static void Want(Array a, long n, string what)
    {
        if (a.Length != n)
            throw new ArgumentException(
                $"{what} has {a.Length} entries, expected {n}");
    }

    private static BasisStatus[] ToBasis(int[] s, long n)
    {
        var b = new BasisStatus[n];
        for (long k = 0; k < n; k++)
            b[k] = (BasisStatus)s[k];
        return b;
    }

    private static int[] FromEnum<T>(T[] s) where T : Enum
    {
        var a = new int[Math.Max(s.Length, 1)];
        for (int k = 0; k < s.Length; k++)
            a[k] = Convert.ToInt32(s[k]);
        return a;
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
        mipStart = null;
        Check(st);
    }

    public void WriteMps(string path) => Check(Native.jaos_write_mps(h, path));
    public void WriteLp(string path) => Check(Native.jaos_write_lp(h, path));

    /// <summary>An AMPL .nl file in text form, with the names in .col and .row files beside it.</summary>
    public void WriteNl(string path) => Check(Native.jaos_write_nl(h, path));
    public void WriteQplib(string path) => Check(Native.jaos_write_qplib(h, path));
    public void WriteCbf(string path) => Check(Native.jaos_write_cbf(h, path));
    public void WriteOsil(string path) => Check(Native.jaos_write_osil(h, path));
    public void WriteSolution(string path) => Check(Native.jaos_write_solution(h, path));

    /// <summary>The last solve as the .sol file an AMPL solver hands back; null writes the status and objective as the message.</summary>
    public void WriteSolAmpl(string path, string? message = null) =>
        Check(Native.jaos_write_sol_ampl(h, path, message));

    /// <summary>
    /// The whole model in one call, the matrix column-wise with 0-based
    /// starts (one more than the columns) and row indices.
    /// </summary>
    public void LoadLp(Sense sense, double offset, double[] cost,
                       double[] colLower, double[] colUpper,
                       double[] rowLower, double[] rowUpper,
                       long[] aStart, long[] aIndex, double[] aValue)
    {
        mipStart = null;
        Check(Native.jaos_load_lp(h, cost.Length, rowLower.Length, (int)sense,
                                  offset, Native.Pad(cost),
                                  Native.Pad(colLower), Native.Pad(colUpper),
                                  Native.Pad(rowLower), Native.Pad(rowUpper),
                                  aValue.Length, Native.Pad(aStart),
                                  Native.Pad(aIndex), Native.Pad(aValue)));
    }

    internal void Reload(Sense sense, double offset, double[] cost,
                         double[] colLower, double[] colUpper,
                         double[] rowLower, double[] rowUpper,
                         long[] aStart, long[] aIndex, double[] aValue)
    {
        var start = mipStart;
        LoadLp(sense, offset, cost, colLower, colUpper, rowLower, rowUpper,
               aStart, aIndex, aValue);
        if (start != null)
        {
            var x = new double[NumCol];
            Array.Copy(start, x, Math.Min(start.Length, x.Length));
            SetMipStart(x);
        }
    }

    /// <summary>A new model holding this one's problem, names, settings, callbacks and starting basis, and not its answer.</summary>
    public Model Copy()
    {
        Check(Native.jaos_model_copy(h, out ModelHandle c));
        var m = Adopt(c);
        m.mipStart = mipStart;
        return m;
    }

    private Model Adopt(ModelHandle c)
    {
        var m = new Model(c);
        if (logSink != null)
            m.SetLogCallback(logSink, logLevel);
        if (progressSink != null)
            m.SetProgressCallback(progressSink);
        if (incumbentSink != null)
            m.SetIncumbentCallback(incumbentSink);
        if (nodeSink != null)
            m.SetNodeCallback(nodeSink);
        return m;
    }

    public long NumCol => Native.jaos_num_col(h);
    public long NumRow => Native.jaos_num_row(h);
    public long NumNz => Native.jaos_num_nz(h);
    public long NumCones => Native.jaos_num_cones(h);
    public long NumSos => Native.jaos_num_sos(h);
    public bool HasInteger => Native.jaos_model_has_integer(h);

    public double ColCost(long col)
    {
        Check(Native.jaos_col_cost(h, col, out double v));
        return v;
    }

    public (double Lower, double Upper) ColBounds(long col)
    {
        Check(Native.jaos_col_bounds(h, col, out double lo, out double hi));
        return (lo, hi);
    }

    public (double Lower, double Upper) RowBounds(long row)
    {
        Check(Native.jaos_row_bounds(h, row, out double lo, out double hi));
        return (lo, hi);
    }

    public Sense Sense
    {
        get
        {
            Check(Native.jaos_objective_sense(h, out int s));
            return (Sense)s;
        }
    }

    public double ObjOffset
    {
        get
        {
            Check(Native.jaos_objective_offset(h, out double v));
            return v;
        }
    }

    /// <summary>Minimize or maximize. Discards the answer and keeps the basis.</summary>
    public void SetSense(Sense sense) =>
        Check(Native.jaos_set_objective_sense(h, (int)sense));

    /// <summary>The objective's constant. Discards the answer and keeps the basis.</summary>
    public void SetObjOffset(double offset) =>
        Check(Native.jaos_set_objective_offset(h, offset));

    public void SetColInteger(long col, bool on = true) =>
        Check(Native.jaos_set_col_integer(h, col, on));
    public void SetColBounds(long col, double lower, double upper) =>
        Check(Native.jaos_set_col_bounds(h, col, lower, upper));
    public void SetRowBounds(long row, double lower, double upper) =>
        Check(Native.jaos_set_row_bounds(h, row, lower, upper));
    public void SetColCost(long col, double cost) =>
        Check(Native.jaos_set_col_cost(h, col, cost));

    private (long[] Index, double[] Value) Entries(bool byCol, long k)
    {
        long n;
        Check(byCol ? Native.jaos_col_entries(h, k, out n, null, null)
                    : Native.jaos_row_entries(h, k, out n, null, null));
        var idx = new long[Math.Max(n, 1)];
        var val = new double[Math.Max(n, 1)];
        Check(byCol ? Native.jaos_col_entries(h, k, out n, idx, val)
                    : Native.jaos_row_entries(h, k, out n, idx, val));
        return (idx[..(int)n], val[..(int)n]);
    }

    /// <summary>One column of the matrix: row indices ascending, no explicit zeros.</summary>
    public (long[] Index, double[] Value) ColEntries(long col) => Entries(true, col);

    /// <summary>One row of the matrix: column indices ascending.</summary>
    public (long[] Index, double[] Value) RowEntries(long row) => Entries(false, row);

    /// <summary>One entry; 0 where the model holds none.</summary>
    public double Coefficient(long row, long col)
    {
        Check(Native.jaos_coefficient(h, row, col, out double v));
        return v;
    }

    /// <summary>Sets one entry. Zero deletes it; a new index inserts one.</summary>
    public void SetCoefficient(long row, long col, double value) =>
        Check(Native.jaos_set_coefficient(h, row, col, value));

    private static (long Nz, long[]? Start, long[]? Index, double[]? Value)
        Matrix(long[]? start, long[]? index, double[]? value, int major)
    {
        if (value == null || value.Length == 0)
            return (0, null, null, null);
        if (index == null || index.Length != value.Length)
            throw new ArgumentException("aIndex must have one entry per value");
        if (start == null || start.Length != major + 1)
            throw new ArgumentException($"aStart must have {major + 1} entries");
        return (value.Length, start, index, value);
    }

    /// <summary>
    /// Appends columns. The matrix follows <see cref="LoadLp"/>'s layout
    /// over the new columns, with row indices into the existing rows; null
    /// appends columns with no entries. The basis survives.
    /// </summary>
    public void AddCols(double[] cost, double[] lower, double[] upper,
                        long[]? aStart = null, long[]? aIndex = null,
                        double[]? aValue = null)
    {
        Want(lower, cost.Length, "lower");
        Want(upper, cost.Length, "upper");
        var (nz, s, i, v) = Matrix(aStart, aIndex, aValue, cost.Length);
        Check(Native.jaos_add_cols(h, cost.Length, Native.Pad(cost),
                                   Native.Pad(lower), Native.Pad(upper),
                                   nz, s, i, v));
    }

    /// <summary>
    /// Appends rows. The matrix is row-wise over the new rows, with column
    /// indices; the new rows arrive basic, so the basis survives.
    /// </summary>
    public void AddRows(double[] lower, double[] upper,
                        long[]? aStart = null, long[]? aIndex = null,
                        double[]? aValue = null)
    {
        Want(upper, lower.Length, "upper");
        var (nz, s, i, v) = Matrix(aStart, aIndex, aValue, lower.Length);
        Check(Native.jaos_add_rows(h, lower.Length, Native.Pad(lower),
                                   Native.Pad(upper), nz, s, i, v));
    }

    /// <summary>Removes columns in one call; the rest keep their order and are renumbered from zero.</summary>
    public void DeleteCols(params long[] cols) =>
        Check(Native.jaos_delete_cols(h, cols.Length, Native.Pad(cols)));

    public void DeleteRows(params long[] rows) =>
        Check(Native.jaos_delete_rows(h, rows.Length, Native.Pad(rows)));

    private static string Text(byte[] buf)
    {
        int end = Array.IndexOf(buf, (byte)0);
        return Encoding.UTF8.GetString(buf, 0, end < 0 ? buf.Length : end);
    }

    /// <summary>The column's name: the file's, one set here, or its position (C1, C2, ...).</summary>
    public string ColName(long col)
    {
        var buf = new byte[NameCap];
        Check(Native.jaos_col_name(h, col, buf, buf.Length));
        return Text(buf);
    }

    public string RowName(long row)
    {
        var buf = new byte[NameCap];
        Check(Native.jaos_row_name(h, row, buf, buf.Length));
        return Text(buf);
    }

    public string ObjectiveName
    {
        get
        {
            var buf = new byte[NameCap];
            Check(Native.jaos_objective_name(h, buf, buf.Length));
            return Text(buf);
        }
    }

    /// <summary>The model's own name: an MPS file's NAME, "JAOS" until one is given.</summary>
    public string Name
    {
        get
        {
            var buf = new byte[NameCap];
            Check(Native.jaos_model_name(h, buf, buf.Length));
            return Text(buf);
        }
    }

    /// <summary>Null restores the positional name.</summary>
    public void SetColName(long col, string? name) =>
        Check(Native.jaos_set_col_name(h, col, name));
    public void SetRowName(long row, string? name) =>
        Check(Native.jaos_set_row_name(h, row, name));
    public void SetObjectiveName(string? name) =>
        Check(Native.jaos_set_objective_name(h, name));
    public void SetName(string? name) =>
        Check(Native.jaos_set_model_name(h, name));

    /// <summary>The column called <paramref name="name"/>, positional names included.</summary>
    public long ColIndex(string name)
    {
        Check(Native.jaos_col_index(h, name, out long k));
        return k;
    }

    public long RowIndex(string name)
    {
        Check(Native.jaos_row_index(h, name, out long k));
        return k;
    }

    public bool ColInteger(long col)
    {
        Check(Native.jaos_col_integer(h, col, out bool on));
        return on;
    }

    /// <summary>The column rests at zero or inside its own bounds; the model solves by branch and bound.</summary>
    public void SetColSemicontinuous(long col, bool on = true) =>
        Check(Native.jaos_set_col_semicontinuous(h, col, on));

    public bool ColSemicontinuous(long col)
    {
        Check(Native.jaos_col_semicontinuous(h, col, out bool on));
        return on;
    }

    /// <summary>A special ordered set of type 1 or 2 over <paramref name="cols"/>, ordered by <paramref name="weights"/>.</summary>
    public void AddSos(int type, long[] cols, double[] weights)
    {
        Want(weights, cols.Length, "weights");
        Check(Native.jaos_add_sos(h, type, cols.Length, Native.Pad(cols),
                                  Native.Pad(weights)));
    }

    /// <summary>Type, members and weights of set <paramref name="k"/>, members in weight order.</summary>
    public (int Type, long[] Cols, double[] Weights) Sos(long k)
    {
        Check(Native.jaos_sos(h, k, out int type, out long n, null, null));
        var cols = new long[Math.Max(n, 1)];
        var w = new double[Math.Max(n, 1)];
        Check(Native.jaos_sos(h, k, out type, out n, cols, w));
        return (type, cols[..(int)n], w[..(int)n]);
    }

    /// <summary>Row <paramref name="row"/> holds only while integer column <paramref name="col"/> equals <paramref name="value"/>; a null column makes it an ordinary row.</summary>
    public void SetRowIndicator(long row, long? col, int value = 1) =>
        Check(Native.jaos_set_row_indicator(h, row, col ?? -1, value));

    public (long? Col, int Value) RowIndicator(long row)
    {
        Check(Native.jaos_row_indicator(h, row, out long col, out int v));
        return (col < 0 ? (long?)null : col, v);
    }

    /// <summary>The diagonal entry of the objective's <c>½ x'Qx</c>.</summary>
    public void SetColQuadratic(long col, double q) =>
        Check(Native.jaos_set_col_quadratic(h, col, q));

    public double ColQuadratic(long col)
    {
        Check(Native.jaos_col_quadratic(h, col, out double q));
        return q;
    }

    /// <summary>
    /// The objective's <c>½ x'Qx</c> as triplets, one per diagonal entry
    /// and one per off-diagonal pair.
    /// </summary>
    public void SetQuadratic(long[] rows, long[] cols, double[] values)
    {
        Want(rows, values.Length, "rows");
        Want(cols, values.Length, "cols");
        Check(Native.jaos_set_quadratic(h, values.Length, Native.Pad(rows),
                                        Native.Pad(cols), Native.Pad(values)));
    }

    public long QuadraticNz => Native.jaos_quadratic_nz(h);

    /// <summary>Q back as triplets: the lower triangle with the diagonal, in column order.</summary>
    public (long[] Rows, long[] Cols, double[] Values) Quadratic()
    {
        long n = QuadraticNz;
        var r = new long[Math.Max(n, 1)];
        var c = new long[Math.Max(n, 1)];
        var v = new double[Math.Max(n, 1)];
        if (n > 0)
            Check(Native.jaos_quadratic(h, r, c, v));
        return (r[..(int)n], c[..(int)n], v[..(int)n]);
    }

    /// <summary>Row <paramref name="row"/>'s part <c>½ x'Qx</c>; empty arrays clear it.</summary>
    public void SetRowQuadratic(long row, long[] rows, long[] cols,
                                double[] values)
    {
        Want(rows, values.Length, "rows");
        Want(cols, values.Length, "cols");
        Check(Native.jaos_set_row_quadratic(h, row, values.Length,
                                            Native.Pad(rows), Native.Pad(cols),
                                            Native.Pad(values)));
    }

    public long RowQuadraticNz(long row) => Native.jaos_row_quadratic_nz(h, row);

    public (long[] Rows, long[] Cols, double[] Values) RowQuadratic(long row)
    {
        long n = RowQuadraticNz(row);
        var r = new long[Math.Max(n, 1)];
        var c = new long[Math.Max(n, 1)];
        var v = new double[Math.Max(n, 1)];
        if (n > 0)
            Check(Native.jaos_row_quadratic(h, row, r, c, v));
        return (r[..(int)n], c[..(int)n], v[..(int)n]);
    }

    public void AddCone(ConeType type, long[] cols) =>
        Check(Native.jaos_add_cone(h, (int)type, cols.Length, Native.Pad(cols)));

    public (ConeType Type, long[] Cols) Cone(long k)
    {
        Check(Native.jaos_cone(h, k, out int type, out long n, null));
        var cols = new long[Math.Max(n, 1)];
        Check(Native.jaos_cone(h, k, out type, out n, cols));
        return ((ConeType)type, cols[..(int)n]);
    }

    public void DeleteCones(params long[] cones) =>
        Check(Native.jaos_delete_cones(h, cones.Length, Native.Pad(cones)));

    public void SetOption(string name, string value) =>
        Check(Native.jaos_set_option(h, name, value));

    public string GetOption(string name)
    {
        var buf = new byte[512];
        Check(Native.jaos_get_option(h, name, buf, buf.Length));
        return Text(buf);
    }

    /// <summary>Options from a file, one <c>name value</c> per line, <c>#</c> for a comment.</summary>
    public void ReadOptions(string path) => Check(Native.jaos_read_options(h, path));

    /// <summary>The name of every option <see cref="SetOption"/> takes.</summary>
    public static string[] OptionNames()
    {
        long n = Native.jaos_num_options();
        var names = new string[n];
        for (long k = 0; k < n; k++)
            names[k] = Marshal.PtrToStringUTF8(Native.jaos_option_name(k)) ?? "";
        return names;
    }

    public void SetTimeLimit(double seconds) =>
        Check(Native.jaos_set_time_limit(h, seconds));
    public void SetWorkLimit(long units) =>
        Check(Native.jaos_set_work_limit(h, units));
    /// <summary>The thread count; 0 takes every core the machine has.</summary>
    public void SetThreads(long threads) =>
        Check(Native.jaos_set_threads(h, threads));
    public long Threads => Native.jaos_threads_of(h);
    public void SetPrimalTolerance(double tol) =>
        Check(Native.jaos_set_primal_tolerance(h, tol));
    public void SetDualTolerance(double tol) =>
        Check(Native.jaos_set_dual_tolerance(h, tol));
    public void SetAlgorithm(Algorithm alg) =>
        Check(Native.jaos_set_algorithm(h, (int)alg));
    public Algorithm Algorithm => (Algorithm)Native.jaos_algorithm_of(h);

    /// <summary>
    /// An integer point for the tree to start from, checked at the root; a
    /// NaN leaves its column open, and a small tree completes the start
    /// with the given integer columns fixed. Null clears it.
    /// </summary>
    public void SetMipStart(double[]? x)
    {
        if (x == null)
        {
            Check(Native.jaos_set_mip_start(h, null));
            mipStart = null;
            return;
        }
        Want(x, NumCol, "the start");
        Check(Native.jaos_set_mip_start(h, Native.Pad(x)));
        mipStart = (double[])x.Clone();
    }

    public void SetMipCutoff(double cutoff) => Check(Native.jaos_set_mip_cutoff(h, cutoff));
    public void SetMipPoolSize(long size) => Check(Native.jaos_set_mip_pool_size(h, size));
    public void SetMipTreeBatch(long nodes) => Check(Native.jaos_set_mip_tree_batch(h, nodes));
    /// <summary>The gap that closes the tree; 0 means zero.</summary>
    public void SetMipGap(double gap) => Check(Native.jaos_set_mip_gap(h, gap));
    public void SetMipGapRule(GapRule rule) => Check(Native.jaos_set_mip_gap_rule(h, (int)rule));
    public void SetMipNodeLimit(long nodes) => Check(Native.jaos_set_mip_node_limit(h, nodes));
    public void SetMipBranching(Branching rule) => Check(Native.jaos_set_mip_branching(h, (int)rule));
    public void SetMipReliability(long branches) => Check(Native.jaos_set_mip_reliability(h, branches));
    public void SetMipProbeCap(double multiple) => Check(Native.jaos_set_mip_probe_cap(h, multiple));
    public void SetMipProbeDepth(long depth) => Check(Native.jaos_set_mip_probe_depth(h, depth));
    public void SetMipNodeSelect(long rule) => Check(Native.jaos_set_mip_node_select(h, rule));
    public void SetMipRestart(int on) => Check(Native.jaos_set_mip_restart(h, on));
    public void SetMipConflicts(int on) => Check(Native.jaos_set_mip_conflicts(h, on));
    public void SetMipSymmetry(int on) => Check(Native.jaos_set_mip_symmetry(h, on));
    public void SetMipOrbital(int on) => Check(Native.jaos_set_mip_orbital(h, on));
    public void SetMipPropagate(long rounds) => Check(Native.jaos_set_mip_propagate(h, rounds));
    public void SetMipPropagateDepth(long depth) => Check(Native.jaos_set_mip_propagate_depth(h, depth));
    public void SetMipRcfix(int on) => Check(Native.jaos_set_mip_rcfix(h, on));
    public void SetMipTighten(int on) => Check(Native.jaos_set_mip_tighten(h, on));
    public void SetMipProbing(int on) => Check(Native.jaos_set_mip_probing(h, on));
    public void SetMipProbingCap(double multiple) => Check(Native.jaos_set_mip_probing_cap(h, multiple));
    public void SetMipCliqueFix(int on) => Check(Native.jaos_set_mip_clique_fix(h, on));
    public void SetMipCutRounds(long rounds) => Check(Native.jaos_set_mip_cut_rounds(h, rounds));
    public void SetMipCutDepth(long depth) => Check(Native.jaos_set_mip_cut_depth(h, depth));
    public void SetMipCutDrop(bool on) => Check(Native.jaos_set_mip_cut_drop(h, on));
    public void SetMipNodeCutCap(long cap) => Check(Native.jaos_set_mip_node_cut_cap(h, cap));
    public void SetMipCoverRounds(long rounds) => Check(Native.jaos_set_mip_cover_rounds(h, rounds));
    public void SetMipCliqueRounds(long rounds) => Check(Native.jaos_set_mip_clique_rounds(h, rounds));
    public void SetMipZeroHalfRounds(long rounds) => Check(Native.jaos_set_mip_zero_half_rounds(h, rounds));
    public void SetMipFlowCoverRounds(long rounds) => Check(Native.jaos_set_mip_flow_cover_rounds(h, rounds));
    public void SetMipCutStall(double fraction) => Check(Native.jaos_set_mip_cut_stall(h, fraction));
    public void SetMipNodeCutStall(double fraction) => Check(Native.jaos_set_mip_node_cut_stall(h, fraction));
    public void SetMipRootCutDrop(int on) => Check(Native.jaos_set_mip_root_cut_drop(h, on));
    public void SetMipCoverLift(int on) => Check(Native.jaos_set_mip_cover_lift(h, on));
    public void SetMipMirRounds(long rounds) => Check(Native.jaos_set_mip_mir_rounds(h, rounds));
    public void SetMipNodeMir(int on) => Check(Native.jaos_set_mip_node_mir(h, on));
    public void SetMipMirAggregate(long rows) => Check(Native.jaos_set_mip_mir_aggregate(h, rows));
    public void SetMipDive(bool on) => Check(Native.jaos_set_mip_dive(h, on));
    public void SetMipDiveBacktrack(long times) => Check(Native.jaos_set_mip_dive_backtrack(h, times));
    public void SetMipDiveGap(double fraction) => Check(Native.jaos_set_mip_dive_gap(h, fraction));
    public void SetMipDiveHeuristic(long solves) => Check(Native.jaos_set_mip_dive_heuristic(h, solves));
    public void SetMipDiveHeuristicDepth(long depth) => Check(Native.jaos_set_mip_dive_heuristic_depth(h, depth));
    public void SetMipDiveChild(DiveChild rule) => Check(Native.jaos_set_mip_dive_child(h, (int)rule));
    public void SetMipDiveDegrade(double fraction) => Check(Native.jaos_set_mip_dive_degrade(h, fraction));
    public void SetMipRins(long solves) => Check(Native.jaos_set_mip_rins(h, solves));
    public void SetMipLocalBranching(long size) => Check(Native.jaos_set_mip_local_branching(h, size));
    public void SetMipFeaspump(long rounds) => Check(Native.jaos_set_mip_feaspump(h, rounds));
    public void SetMipPumpGeneral(int on) => Check(Native.jaos_set_mip_pump_general(h, on));
    public void SetMipPumpObj(double decay) => Check(Native.jaos_set_mip_pump_obj(h, decay));
    public void SetMipPumpAlways(int on) => Check(Native.jaos_set_mip_pump_always(h, on));
    public void SetMipHeuristics(bool on) => Check(Native.jaos_set_mip_heuristics(h, on));

    /// <summary>
    /// Sends the solver's log to <paramref name="sink"/> at
    /// <paramref name="level"/>; null or <see cref="LogLevel.Off"/> stops it.
    /// </summary>
    public void SetLog(LogLevel level, Action<string>? sink)
    {
        Action<LogLevel, string>? fn = null;
        if (sink != null)
            fn = (_, line) => sink(line);
        SetLogCallback(fn, level);
    }

    /// <summary>
    /// Sends the solver's log to <c>fn(level, line)</c>; null stops it. An
    /// exception in <paramref name="fn"/> is thrown again by the call that
    /// was running when it happened.
    /// </summary>
    public void SetLogCallback(Action<LogLevel, string>? fn,
                               LogLevel level = LogLevel.Summary)
    {
        if (fn == null || level == LogLevel.Off)
        {
            Check(Native.jaos_set_log_callback(h, null, IntPtr.Zero));
            logFn = null;
            logSink = null;
            level = LogLevel.Off;
        }
        else
        {
            logFn = (user, lvl, line) =>
            {
                try
                {
                    fn((LogLevel)lvl, Marshal.PtrToStringUTF8(line) ?? "");
                }
                catch (Exception e)
                {
                    Hold(e);
                }
            };
            Check(Native.jaos_set_log_callback(h, logFn, IntPtr.Zero));
            logSink = fn;
        }
        logLevel = level;
        Check(Native.jaos_set_log_level(h, (int)level));
    }

    /// <summary>
    /// Calls <paramref name="fn"/> as the solve runs; <see cref="CallbackAction.Stop"/>
    /// ends it as <see cref="SolveStatus.Interrupted"/>. An exception in
    /// <paramref name="fn"/> stops the solve and <see cref="Solve"/> throws it.
    /// Null removes the callback.
    /// </summary>
    public void SetProgressCallback(Func<Progress, CallbackAction>? fn)
    {
        Native.ProgressFn? cb = null;
        if (fn != null)
            cb = (p, user) =>
            {
                try
                {
                    var c = Marshal.PtrToStructure<NativeProgress>(p);
                    return (int)fn(new Progress(c.Iterations, c.WorkUnits,
                                                c.PrimalInfeasibility, c.Nodes,
                                                c.Bound, c.HasIncumbent,
                                                c.Incumbent));
                }
                catch (Exception e)
                {
                    Hold(e);
                    return (int)CallbackAction.Stop;
                }
            };
        Check(Native.jaos_set_progress_callback(h, cb, IntPtr.Zero));
        progressFn = cb;
        progressSink = fn;
    }

    /// <summary>
    /// Calls <paramref name="fn"/> for each new incumbent of a branch and
    /// bound; the rules are <see cref="SetProgressCallback"/>'s.
    /// </summary>
    public void SetIncumbentCallback(Func<Incumbent, CallbackAction>? fn)
    {
        Native.IncumbentFn? cb = null;
        if (fn != null)
            cb = (p, user) =>
            {
                try
                {
                    var c = Marshal.PtrToStructure<NativeIncumbent>(p);
                    var x = new double[c.NumCol];
                    if (c.NumCol > 0)
                        Marshal.Copy(c.ColValue, x, 0, (int)c.NumCol);
                    return (int)fn(new Incumbent(c.Node, c.Objective, c.Bound,
                                                 x, c.ByRounding));
                }
                catch (Exception e)
                {
                    Hold(e);
                    return (int)CallbackAction.Stop;
                }
            };
        Check(Native.jaos_set_incumbent_callback(h, cb, IntPtr.Zero));
        incumbentFn = cb;
        incumbentSink = fn;
    }

    /// <summary>
    /// Calls <paramref name="fn"/> at every node of a branch and bound once
    /// its relaxation is solved, and for every point about to become an
    /// incumbent. The callback may add rows and choose the branching column
    /// through the <see cref="NodeEvent"/>; the other rules are
    /// <see cref="SetProgressCallback"/>'s.
    /// </summary>
    public void SetNodeCallback(Func<NodeEvent, CallbackAction>? fn)
    {
        Native.NodeFn? cb = null;
        if (fn != null)
            cb = (p, user) =>
            {
                NodeEvent? ev = null;
                try
                {
                    ev = new NodeEvent(p, Marshal.PtrToStructure<NativeNode>(p));
                    var r = fn(ev);
                    Marshal.WriteInt64(p, BranchColOffset, ev.BranchCol);
                    return (int)r;
                }
                catch (Exception e)
                {
                    Hold(e);
                    return (int)CallbackAction.Stop;
                }
                finally
                {
                    ev?.Close();
                }
            };
        Check(Native.jaos_set_node_callback(h, cb, IntPtr.Zero));
        nodeFn = cb;
        nodeSink = fn;
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
        var x = new double[Math.Max(nc, 1)];
        var act = new double[Math.Max(nr, 1)];
        var y = new double[Math.Max(nr, 1)];
        var d = new double[Math.Max(nc, 1)];
        Check(Native.jaos_solution(h, x, act, y, d));
        return new Solution(x[..(int)nc], act[..(int)nr], y[..(int)nr],
                            d[..(int)nc]);
    }

    /// <summary>Where each variable rests in the basis behind the answer.</summary>
    public Basis Basis()
    {
        long nc = NumCol, nr = NumRow;
        var cs = new int[Math.Max(nc, 1)];
        var rs = new int[Math.Max(nr, 1)];
        Check(Native.jaos_basis(h, cs, rs));
        return new Basis(ToBasis(cs, nc), ToBasis(rs, nr));
    }

    /// <summary>The next solve's starting basis, one status per column and per row.</summary>
    public void SetBasis(BasisStatus[] colStatus, BasisStatus[] rowStatus)
    {
        Want(colStatus, NumCol, "colStatus");
        Want(rowStatus, NumRow, "rowStatus");
        Check(Native.jaos_set_basis(h, FromEnum(colStatus), FromEnum(rowStatus)));
    }

    /// <summary>Asks the next solve to start cold.</summary>
    public void ClearBasis() => Native.jaos_clear_basis(h);

    /// <summary>The dual of cone <paramref name="k"/>, one entry per member.</summary>
    public double[] ConeDual(long k)
    {
        Check(Native.jaos_cone(h, k, out _, out long n, null));
        var z = new double[Math.Max(n, 1)];
        Check(Native.jaos_cone_dual(h, k, z));
        return z[..(int)n];
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
                             r.FirstIncumbentNode, r.FixedCols, r.Tightened,
                             r.SymmetryGenerators, r.SymmetryOrbits,
                             r.StartAccepted);
    }

    /// <summary>The incumbent of a tree that stopped with one.</summary>
    public (double[] X, double Objective) MipIncumbent()
    {
        var x = new double[Math.Max(NumCol, 1)];
        Check(Native.jaos_mip_incumbent(h, x, out double v));
        return (x[..(int)NumCol], v);
    }

    /// <summary>The solution pool of the last branch and bound, best first.</summary>
    public (double[] X, double Objective)[] MipPool()
    {
        Check(Native.jaos_mip_pool_count(h, out long n));
        long nc = NumCol;
        var pool = new (double[] X, double Objective)[n];
        for (long k = 0; k < n; k++)
        {
            var x = new double[Math.Max(nc, 1)];
            Check(Native.jaos_mip_pool_solution(h, k, x, out double v));
            pool[k] = (x[..(int)nc], v);
        }
        return pool;
    }

    public long Iterations => Native.jaos_iterations(h);
    public long WorkUnits => Native.jaos_work_units(h);

    /// <summary>Seconds the last solve took; it does not repeat, so nothing should depend on it.</summary>
    public double SolveTime => Native.jaos_solve_time(h);

    /// <summary>Sizes, row and column kinds and magnitude ranges, counted without a solve.</summary>
    public ModelStats Statistics()
    {
        Check(Native.jaos_model_statistics(h, out NativeModelStats s));
        return new ModelStats(s);
    }

    /// <summary>What presolve did on the last solve.</summary>
    public PresolveReport PresolveReport()
    {
        Check(Native.jaos_presolve_result(h, out NativePresolveReport r));
        return new PresolveReport(r);
    }

    private double[] ConeVector(double[][] parts, string what)
    {
        long nk = NumCones;
        if (parts.Length != nk)
            throw new ArgumentException($"{what} has {parts.Length} cones, expected {nk}");
        int total = 0;
        for (int k = 0; k < parts.Length; k++)
        {
            Want(parts[k], Cone(k).Cols.Length, $"{what}[{k}]");
            total += parts[k].Length;
        }
        var flat = new double[Math.Max(total, 1)];
        int at = 0;
        foreach (var part in parts)
        {
            part.CopyTo(flat, at);
            at += part.Length;
        }
        return flat;
    }

    /// <summary>
    /// The independent checker on a candidate answer against the model as
    /// loaded. A null <paramref name="rowDual"/> skips the dual conditions.
    /// </summary>
    public CheckReport CheckSolution(double[] x, double[]? rowDual = null,
                                     double tol = 1e-7)
    {
        Want(x, NumCol, "x");
        if (rowDual != null)
            Want(rowDual, NumRow, "rowDual");
        Check(Native.jaos_check_solution(h, Native.Pad(x),
                                         rowDual == null ? null : Native.Pad(rowDual),
                                         tol, out NativeCheckReport r));
        return new CheckReport(r);
    }

    /// <summary><see cref="CheckSolution"/> for a model with cones, one dual per cone.</summary>
    public CheckReport CheckConicSolution(double[] x, double[]? rowDual,
                                          double[][] coneDual,
                                          double tol = 1e-7)
    {
        Want(x, NumCol, "x");
        if (rowDual != null)
            Want(rowDual, NumRow, "rowDual");
        var z = ConeVector(coneDual, "coneDual");
        Check(Native.jaos_check_conic_solution(
            h, Native.Pad(x), rowDual == null ? null : Native.Pad(rowDual), z,
            tol, out NativeCheckReport r));
        return new CheckReport(r);
    }

    /// <summary>Judges a claimed Farkas certificate from the model alone.</summary>
    public CertificateReport CheckCertificate(double[] rowRay, double tol = 1e-7)
    {
        Want(rowRay, NumRow, "rowRay");
        Check(Native.jaos_check_certificate(h, Native.Pad(rowRay), tol,
                                            out NativeCertificateReport r));
        return new CertificateReport(r.SupColumns, r.InfRows, r.Gap, r.Certified);
    }

    public CertificateReport CheckConicCertificate(double[] rowRay,
                                                   double[][] coneRay,
                                                   double tol = 1e-7)
    {
        Want(rowRay, NumRow, "rowRay");
        var z = ConeVector(coneRay, "coneRay");
        Check(Native.jaos_check_conic_certificate(h, Native.Pad(rowRay), z, tol,
                                                  out NativeCertificateReport r));
        return new CertificateReport(r.SupColumns, r.InfRows, r.Gap, r.Certified);
    }

    /// <summary>Judges a claimed unbounded ray from the model alone.</summary>
    public RayReport CheckRay(double[] colRay, double tol = 1e-7)
    {
        Want(colRay, NumCol, "colRay");
        Check(Native.jaos_check_ray(h, Native.Pad(colRay), tol,
                                    out NativeRayReport r));
        return new RayReport(r.Rate, r.MaxColEscape, r.MaxRowEscape,
                             r.Curvature, r.Certified);
    }

    /// <summary>An irreducible infeasible subsystem of the last infeasible answer, found on a private copy.</summary>
    public Iis Iis()
    {
        long nr = NumRow, nc = NumCol;
        var rs = new int[Math.Max(nr, 1)];
        var cs = new int[Math.Max(nc, 1)];
        Check(Native.jaos_iis(h, rs, cs, out NativeIisReport r));
        var rows = new IisSide[nr];
        var cols = new IisSide[nc];
        for (long i = 0; i < nr; i++)
            rows[i] = (IisSide)rs[i];
        for (long j = 0; j < nc; j++)
            cols[j] = (IisSide)cs[j];
        return new Iis(rows, cols, new IisReport(r.Members, r.Candidates,
                                                 r.Solves, r.WorkUnits,
                                                 r.FromCertificate));
    }

    /// <summary>The subsystem as a model of its own, costs zeroed and names kept.</summary>
    public Model IisModel(Iis iis) => IisModel(iis.RowSide, iis.ColSide);

    public Model IisModel(IisSide[] rowSide, IisSide[] colSide)
    {
        Want(rowSide, NumRow, "rowSide");
        Want(colSide, NumCol, "colSide");
        Check(Native.jaos_iis_model(h, FromEnum(rowSide), FromEnum(colSide),
                                    out ModelHandle sub));
        return Adopt(sub);
    }

    /// <summary>
    /// The smallest total move of the bounds that makes the model feasible,
    /// found on a private copy: below zero a lower side comes down, above
    /// zero an upper side goes up.
    /// </summary>
    public Relaxation FeasRelax(RelaxScope scope = RelaxScope.Both)
    {
        long nr = NumRow, nc = NumCol;
        var rm = new double[Math.Max(nr, 1)];
        var cm = new double[Math.Max(nc, 1)];
        Check(Native.jaos_feasrelax(h, (int)scope, rm, cm, out NativeRelaxReport r));
        return new Relaxation(rm[..(int)nr], cm[..(int)nc],
                              new RelaxReport(r.Total, r.RowsMoved, r.ColsMoved,
                                              r.AtRow, r.AtCol, r.Largest,
                                              r.WorkUnits, (SolveStatus)r.Status));
    }

    /// <summary>How far each cost may move with the optimal basis staying optimal.</summary>
    public CostRanging CostRanging()
    {
        long nc = NumCol;
        var lo = new double[Math.Max(nc, 1)];
        var hi = new double[Math.Max(nc, 1)];
        Check(Native.jaos_cost_ranging(h, lo, hi));
        return new CostRanging(lo[..(int)nc], hi[..(int)nc]);
    }

    /// <summary>How far each row's two bounds may move.</summary>
    public BoundRanging RhsRanging() => Ranging(true, NumRow);

    /// <summary>How far each column's two bounds may move.</summary>
    public BoundRanging BoundRanging() => Ranging(false, NumCol);

    private BoundRanging Ranging(bool rows, long n)
    {
        var a = new double[Math.Max(n, 1)];
        var b = new double[Math.Max(n, 1)];
        var c = new double[Math.Max(n, 1)];
        var d = new double[Math.Max(n, 1)];
        Check(rows ? Native.jaos_rhs_ranging(h, a, b, c, d)
                   : Native.jaos_bound_ranging(h, a, b, c, d));
        return new BoundRanging(a[..(int)n], b[..(int)n], c[..(int)n], d[..(int)n]);
    }

    /// <summary>
    /// Proves, or refuses to prove, over the rationals that the basis behind
    /// the last optimum certifies it. Refused is an answer, not a failure.
    /// </summary>
    public VerifyReport Verify()
    {
        Check(Native.jaos_verify(h, out NativeVerifyReport r));
        return new VerifyReport(r);
    }

    /// <summary>The same proof over a basis handed in, with no solve.</summary>
    public VerifyReport VerifyBasis(BasisStatus[] colStatus, BasisStatus[] rowStatus)
    {
        Want(colStatus, NumCol, "colStatus");
        Want(rowStatus, NumRow, "rowStatus");
        Check(Native.jaos_verify_basis(h, FromEnum(colStatus), FromEnum(rowStatus),
                                       out NativeVerifyReport r));
        return new VerifyReport(r);
    }

    /// <summary>A column's exact value at the proved optimum, as <c>p/q</c>.</summary>
    public string ExactColValue(long col)
    {
        Check(Native.jaos_exact_col_value(h, col, out IntPtr s));
        return Marshal.PtrToStringUTF8(s) ?? "";
    }

    public string ExactRowDual(long row)
    {
        Check(Native.jaos_exact_row_dual(h, row, out IntPtr s));
        return Marshal.PtrToStringUTF8(s) ?? "";
    }

    public string ExactObjective()
    {
        Check(Native.jaos_exact_objective(h, out IntPtr s));
        return Marshal.PtrToStringUTF8(s) ?? "";
    }

    /// <summary>Derives the Farkas multipliers of an infeasible answer exactly from its basis.</summary>
    public ExactRayReport ExactCertificate()
    {
        Check(Native.jaos_exact_certificate(h, out NativeExactRayReport r));
        return new ExactRayReport(r);
    }

    public string ExactRowMultiplier(long row)
    {
        Check(Native.jaos_exact_row_multiplier(h, row, out IntPtr s));
        return Marshal.PtrToStringUTF8(s) ?? "";
    }

    /// <summary>Derives the direction of an unbounded answer exactly from its basis.</summary>
    public ExactRayReport ExactUnboundedRay()
    {
        Check(Native.jaos_exact_unbounded_ray(h, out NativeExactRayReport r));
        return new ExactRayReport(r);
    }

    public string ExactColDirection(long col)
    {
        Check(Native.jaos_exact_col_direction(h, col, out IntPtr s));
        return Marshal.PtrToStringUTF8(s) ?? "";
    }

    /// <summary>Writes the answer's exact proof; an optimum needs a <see cref="Verify"/> that proved it.</summary>
    public void WriteProof(string path) => Check(Native.jaos_write_proof(h, path));

    /// <summary>Judges a proof file from this model alone, over the rationals.</summary>
    public ProofReport CheckProof(string path)
    {
        Check(Native.jaos_check_proof(h, path, out NativeProofReport r));
        return new ProofReport(r.Primal, r.Dual, r.Objective, r.BadRow,
                               r.BadCol, r.Terms, (ProofKind)r.Kind,
                               r.Certified);
    }

    /// <summary>Reads back a file <see cref="WriteSolution"/> wrote for an optimum. Nothing is installed.</summary>
    public (double Objective, Solution Solution, Basis Basis) ReadSolution(string path)
    {
        long nc = NumCol, nr = NumRow;
        var x = new double[Math.Max(nc, 1)];
        var d = new double[Math.Max(nc, 1)];
        var cs = new int[Math.Max(nc, 1)];
        var act = new double[Math.Max(nr, 1)];
        var y = new double[Math.Max(nr, 1)];
        var rs = new int[Math.Max(nr, 1)];
        Check(Native.jaos_read_solution(h, path, out double obj, x, d, cs, act,
                                        y, rs));
        return (obj,
                new Solution(x[..(int)nc], act[..(int)nr], y[..(int)nr],
                             d[..(int)nc]),
                new Basis(ToBasis(cs, nc), ToBasis(rs, nr)));
    }

    /// <summary>The certificate a solution file holds: one multiplier per row when infeasible, one direction per column when unbounded.</summary>
    public (SolveStatus Status, double[] Ray) ReadCertificate(string path)
    {
        long nc = NumCol, nr = NumRow;
        var rr = new double[Math.Max(nr, 1)];
        var cr = new double[Math.Max(nc, 1)];
        Check(Native.jaos_read_certificate(h, path, out int st, rr, cr));
        var status = (SolveStatus)st;
        return (status, status == SolveStatus.Infeasible ? rr[..(int)nr]
                                                         : cr[..(int)nc]);
    }

    /// <summary>The cone records of a solution file, one array per cone.</summary>
    public double[][] ReadConeDuals(string path)
    {
        long nk = NumCones;
        var sizes = new int[nk];
        int total = 0;
        for (long k = 0; k < nk; k++)
        {
            sizes[k] = Cone(k).Cols.Length;
            total += sizes[k];
        }
        var z = new double[Math.Max(total, 1)];
        Check(Native.jaos_read_cone_duals(h, path, z));
        var parts = new double[nk][];
        for (int k = 0, at = 0; k < nk; at += sizes[k], k++)
            parts[k] = z[at..(at + sizes[k])];
        return parts;
    }

    /// <summary>The basis in a solution file of either kind. Nothing is installed.</summary>
    public Basis ReadBasis(string path)
    {
        long nc = NumCol, nr = NumRow;
        var cs = new int[Math.Max(nc, 1)];
        var rs = new int[Math.Max(nr, 1)];
        Check(Native.jaos_read_basis(h, path, cs, rs));
        return new Basis(ToBasis(cs, nc), ToBasis(rs, nr));
    }

    /// <summary>The last solve's basis in the MPS basis file format.</summary>
    public void WriteMpsBasis(string path) => Check(Native.jaos_write_mps_basis(h, path));

    public Basis ReadMpsBasis(string path)
    {
        long nc = NumCol, nr = NumRow;
        var cs = new int[Math.Max(nc, 1)];
        var rs = new int[Math.Max(nr, 1)];
        Check(Native.jaos_read_mps_basis(h, path, cs, rs));
        return new Basis(ToBasis(cs, nc), ToBasis(rs, nr));
    }

    /// <summary>The last optimum's point, one <c>NAME VALUE</c> line per column.</summary>
    public void WritePoint(string path) => Check(Native.jaos_write_point(h, path));

    /// <summary>The same file from values the caller has; no solve is needed.</summary>
    public void WritePointValues(string path, double[] x)
    {
        Want(x, NumCol, "x");
        Check(Native.jaos_write_point_values(h, path, Native.Pad(x)));
    }

    /// <summary>The last optimum's row multipliers, in the point file's shape.</summary>
    public void WriteDuals(string path) => Check(Native.jaos_write_duals(h, path));

    public void WriteDualValues(string path, double[] y)
    {
        Want(y, NumRow, "y");
        Check(Native.jaos_write_dual_values(h, path, Native.Pad(y)));
    }

    /// <summary>The column values in a point file, by index; every column must appear once.</summary>
    public double[] ReadPoint(string path)
    {
        long nc = NumCol;
        var x = new double[Math.Max(nc, 1)];
        Check(Native.jaos_read_point(h, path, x));
        return x[..(int)nc];
    }

    public double[] ReadDuals(string path)
    {
        long nr = NumRow;
        var y = new double[Math.Max(nr, 1)];
        Check(Native.jaos_read_duals(h, path, y));
        return y[..(int)nr];
    }

    /// <summary>Which answer a solution file holds: optimal, infeasible or unbounded.</summary>
    public SolveStatus SolutionFileStatus(string path)
    {
        Check(Native.jaos_solution_file_status(h, path, out int st));
        return (SolveStatus)st;
    }
}
