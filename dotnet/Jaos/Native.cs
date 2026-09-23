// SPDX-License-Identifier: Apache-2.0
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

namespace Jaos;

internal sealed class ModelHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    public ModelHandle() : base(true) { }

    protected override bool ReleaseHandle()
    {
        Native.jaos_model_free(handle);
        return true;
    }
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeMipReport
{
    public long Nodes;
    public long LpSolves;
    [MarshalAs(UnmanagedType.U1)] public bool HasIncumbent;
    public double Incumbent;
    public double Bound;
    public long Cuts;
    public long HeuristicPoints;
    public long FirstIncumbentNode;
    public long FixedCols;
    public long Tightened;
    public long SymmetryGenerators;
    public long SymmetryOrbits;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeModelStats
{
    public long NumRow, NumCol, NumNz;
    public long IntegerCol;
    public long BinaryCol;
    public long EqualityRow, RangedRow, OneSidedRow, FreeRow;
    public long FixedCol, RangedCol, OneSidedCol, FreeCol;
    public long EmptyRow, EmptyCol;
    public long ObjNz;
    public double MinAbs, MaxAbs;
    public double ObjMinAbs, ObjMaxAbs;
    public long SemicontinuousCol, SosSet, IndicatorRow;
    public long QuadraticCol;
    public long ConeSet, QuadraticRow;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativePresolveReport
{
    public long NumRow, NumCol, NumNz;
    public long Rounds;
    public long FixedCol;
    public long EmptyRow, EmptyCol;
    public long SingletonRow, SingletonCol;
    public long FreeColSingleton;
    public long ForcingRow, RedundantRow;
    public long ImpliedFreeCol;
    public long TightenedBound;
    public long DuplicateRow, DuplicateCol, DominatedCol;
    public long AggregatedCol;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeProgress
{
    public long Iterations;
    public long WorkUnits;
    public double PrimalInfeasibility;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeIncumbent
{
    public long Node;
    public double Objective;
    public double Bound;
    public IntPtr ColValue;
    public long NumCol;
    [MarshalAs(UnmanagedType.U1)] public bool ByRounding;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeNode
{
    public long Node;
    public long Depth;
    public double Objective;
    public double Bound;
    public IntPtr ColValue;
    public long NumCol;
    [MarshalAs(UnmanagedType.U1)] public bool Integral;
    public long BranchCol;
    public IntPtr Internal;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeCheckReport
{
    public double MaxColViolation;
    public double MaxRowViolation;
    public double MaxRowViolationRelative;
    public double MaxDualViolation;
    public double PrimalObjective;
    public double DualObjective;
    public double ObjectiveGap;
    public double GapPositive;
    public double GapNegative;
    public double MaxDroppedMultiplier;
    public long DroppedTerms;
    public double CertifiedSuboptimality;
    public long UnquantifiedRays;
    public double RelativeSuboptimality;
    [MarshalAs(UnmanagedType.U1)] public bool PrimalFeasible;
    [MarshalAs(UnmanagedType.U1)] public bool DualFeasible;
    [MarshalAs(UnmanagedType.U1)] public bool CheckedDuals;
    [MarshalAs(UnmanagedType.U1)] public bool GapCertified;
    public double MaxIntegralityViolation;
    public double MaxConeViolation;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeCertificateReport
{
    public double SupColumns;
    public double InfRows;
    public double Gap;
    [MarshalAs(UnmanagedType.U1)] public bool Certified;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRayReport
{
    public double Rate;
    public double MaxColEscape;
    public double MaxRowEscape;
    public double Curvature;
    [MarshalAs(UnmanagedType.U1)] public bool Certified;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeIisReport
{
    public long Members;
    public long Candidates;
    public long Solves;
    public long WorkUnits;
    [MarshalAs(UnmanagedType.U1)] public bool FromCertificate;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeRelaxReport
{
    public double Total;
    public long RowsMoved;
    public long ColsMoved;
    public long AtRow;
    public long AtCol;
    public double Largest;
    public long WorkUnits;
    public int Status;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeVerifyReport
{
    public int Status;
    public int Stage;
    public double BoundBits;
    public double CapacityBits;
    public long Blocks;
    public long LargestBlock;
    public long AtRow;
    public long AtCol;
    public double Violation;
    public long BytesHeld;
    public long Terms;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeExactRayReport
{
    [MarshalAs(UnmanagedType.U1)] public bool Derived;
    public double BoundBits;
    public double CapacityBits;
    public long Blocks;
    public long LargestBlock;
    public long AtRow;
    public long BytesHeld;
    public long Terms;
}

[StructLayout(LayoutKind.Sequential)]
internal struct NativeProofReport
{
    [MarshalAs(UnmanagedType.U1)] public bool Primal;
    [MarshalAs(UnmanagedType.U1)] public bool Dual;
    [MarshalAs(UnmanagedType.U1)] public bool Objective;
    public long BadRow;
    public long BadCol;
    public long Terms;
    public int Kind;
    [MarshalAs(UnmanagedType.U1)] public bool Certified;
}

internal static class Native
{
    internal const string Lib = "jaos";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void LogFn(IntPtr user, int level, IntPtr line);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int ProgressFn(IntPtr progress, IntPtr user);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int IncumbentFn(IntPtr incumbent, IntPtr user);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate int NodeFn(IntPtr node, IntPtr user);

    static Native()
    {
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, Resolve);
    }

    internal static void Ensure() { }

    internal static T[] Pad<T>(T[] a) => a.Length > 0 ? a : new T[1];

    private static IntPtr Resolve(string name, Assembly assembly,
                                  DllImportSearchPath? searchPath)
    {
        if (name != Lib)
            return IntPtr.Zero;
        string? env = Environment.GetEnvironmentVariable("JAOS_LIBRARY");
        if (!string.IsNullOrEmpty(env))
        {
            if (!File.Exists(env))
                throw new DllNotFoundException(
                    $"JAOS_LIBRARY is set to '{env}', which does not exist");
            return NativeLibrary.Load(env);
        }
        string file = OperatingSystem.IsWindows() ? "jaos.dll"
                    : OperatingSystem.IsMacOS() ? "libjaos.dylib"
                    : "libjaos.so";
        for (string? dir = AppContext.BaseDirectory; dir != null;
             dir = Path.GetDirectoryName(dir))
        {
            string candidate = Path.Combine(dir, "build", "release", file);
            if (File.Exists(candidate))
                return NativeLibrary.Load(candidate);
        }
        return NativeLibrary.TryLoad(file, assembly, searchPath, out IntPtr h)
            ? h : IntPtr.Zero;
    }

    [DllImport(Lib)] internal static extern IntPtr jaos_version();
    [DllImport(Lib)] internal static extern int jaos_model_new(out ModelHandle m);
    [DllImport(Lib)] internal static extern void jaos_model_free(IntPtr m);
    [DllImport(Lib)] internal static extern IntPtr jaos_model_error(ModelHandle m);
    [DllImport(Lib)] internal static extern IntPtr jaos_status_str(int s);
    [DllImport(Lib)] internal static extern IntPtr jaos_solve_status_str(int s);
    [DllImport(Lib)] internal static extern double jaos_infinity();

    [DllImport(Lib)]
    internal static extern int jaos_load_lp(ModelHandle m, long numCol,
        long numRow, int sense, double offset, double[] cost,
        double[] colLower, double[] colUpper, double[] rowLower,
        double[] rowUpper, long numNz, long[]? aStart, long[]? aIndex,
        double[]? aValue);

    [DllImport(Lib)] internal static extern int jaos_model_copy(ModelHandle m, out ModelHandle copy);

    [DllImport(Lib)] internal static extern int jaos_read_mps(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_lp(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_nl(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_qplib(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_osil(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_cbf(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_mps(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_lp(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_nl(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_qplib(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_cbf(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_osil(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_solution(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_sol_ampl(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, [MarshalAs(UnmanagedType.LPUTF8Str)] string? message);

    [DllImport(Lib)] internal static extern int jaos_read_solution(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, out double objective, double[] colValue, double[] colDual, int[] colStatus, double[] rowActivity, double[] rowDual, int[] rowStatus);
    [DllImport(Lib)] internal static extern int jaos_read_certificate(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, out int status, double[] rowRay, double[] colRay);
    [DllImport(Lib)] internal static extern int jaos_read_cone_duals(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, double[] coneDual);
    [DllImport(Lib)] internal static extern int jaos_read_basis(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, int[] colStatus, int[] rowStatus);
    [DllImport(Lib)] internal static extern int jaos_write_mps_basis(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_mps_basis(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, int[] colStatus, int[] rowStatus);
    [DllImport(Lib)] internal static extern int jaos_write_point(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_point_values(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, double[] colValue);
    [DllImport(Lib)] internal static extern int jaos_write_duals(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_dual_values(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, double[] rowDual);
    [DllImport(Lib)] internal static extern int jaos_read_point(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, double[] colValue);
    [DllImport(Lib)] internal static extern int jaos_read_duals(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, double[] rowDual);
    [DllImport(Lib)] internal static extern int jaos_solution_file_status(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, out int status);

    [DllImport(Lib)] internal static extern long jaos_num_col(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_row(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_nz(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_cones(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_sos(ModelHandle m);
    [DllImport(Lib)] [return: MarshalAs(UnmanagedType.U1)] internal static extern bool jaos_model_has_integer(ModelHandle m);

    [DllImport(Lib)] internal static extern int jaos_col_cost(ModelHandle m, long col, out double cost);
    [DllImport(Lib)] internal static extern int jaos_col_bounds(ModelHandle m, long col, out double lower, out double upper);
    [DllImport(Lib)] internal static extern int jaos_row_bounds(ModelHandle m, long row, out double lower, out double upper);
    [DllImport(Lib)] internal static extern int jaos_set_col_cost(ModelHandle m, long col, double cost);
    [DllImport(Lib)] internal static extern int jaos_set_col_bounds(ModelHandle m, long col, double lower, double upper);
    [DllImport(Lib)] internal static extern int jaos_set_row_bounds(ModelHandle m, long row, double lower, double upper);
    [DllImport(Lib)] internal static extern int jaos_objective_sense(ModelHandle m, out int sense);
    [DllImport(Lib)] internal static extern int jaos_objective_offset(ModelHandle m, out double offset);
    [DllImport(Lib)] internal static extern int jaos_set_objective_sense(ModelHandle m, int sense);
    [DllImport(Lib)] internal static extern int jaos_set_objective_offset(ModelHandle m, double offset);
    [DllImport(Lib)] internal static extern int jaos_col_entries(ModelHandle m, long col, out long count, long[]? index, double[]? value);
    [DllImport(Lib)] internal static extern int jaos_row_entries(ModelHandle m, long row, out long count, long[]? index, double[]? value);
    [DllImport(Lib)] internal static extern int jaos_coefficient(ModelHandle m, long row, long col, out double value);
    [DllImport(Lib)] internal static extern int jaos_set_coefficient(ModelHandle m, long row, long col, double value);
    [DllImport(Lib)] internal static extern int jaos_add_cols(ModelHandle m, long numNew, double[] cost, double[] lower, double[] upper, long numNz, long[]? aStart, long[]? aIndex, double[]? aValue);
    [DllImport(Lib)] internal static extern int jaos_add_rows(ModelHandle m, long numNew, double[] lower, double[] upper, long numNz, long[]? aStart, long[]? aIndex, double[]? aValue);
    [DllImport(Lib)] internal static extern int jaos_delete_cols(ModelHandle m, long numDel, long[] cols);
    [DllImport(Lib)] internal static extern int jaos_delete_rows(ModelHandle m, long numDel, long[] rows);

    [DllImport(Lib)] internal static extern int jaos_col_name(ModelHandle m, long col, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_row_name(ModelHandle m, long row, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_objective_name(ModelHandle m, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_model_name(ModelHandle m, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_set_col_name(ModelHandle m, long col, [MarshalAs(UnmanagedType.LPUTF8Str)] string? name);
    [DllImport(Lib)] internal static extern int jaos_set_row_name(ModelHandle m, long row, [MarshalAs(UnmanagedType.LPUTF8Str)] string? name);
    [DllImport(Lib)] internal static extern int jaos_set_objective_name(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string? name);
    [DllImport(Lib)] internal static extern int jaos_set_model_name(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string? name);
    [DllImport(Lib)] internal static extern int jaos_col_index(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out long col);
    [DllImport(Lib)] internal static extern int jaos_row_index(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, out long row);

    [DllImport(Lib)] internal static extern int jaos_set_col_integer(ModelHandle m, long col, [MarshalAs(UnmanagedType.U1)] bool on);
    [DllImport(Lib)] internal static extern int jaos_col_integer(ModelHandle m, long col, [MarshalAs(UnmanagedType.U1)] out bool on);
    [DllImport(Lib)] internal static extern int jaos_set_col_semicontinuous(ModelHandle m, long col, [MarshalAs(UnmanagedType.U1)] bool on);
    [DllImport(Lib)] internal static extern int jaos_col_semicontinuous(ModelHandle m, long col, [MarshalAs(UnmanagedType.U1)] out bool on);
    [DllImport(Lib)] internal static extern int jaos_add_sos(ModelHandle m, int type, long n, long[] cols, double[] weights);
    [DllImport(Lib)] internal static extern int jaos_sos(ModelHandle m, long k, out int type, out long n, long[]? cols, double[]? weights);
    [DllImport(Lib)] internal static extern int jaos_set_row_indicator(ModelHandle m, long row, long col, int value);
    [DllImport(Lib)] internal static extern int jaos_row_indicator(ModelHandle m, long row, out long col, out int value);

    [DllImport(Lib)] internal static extern int jaos_set_col_quadratic(ModelHandle m, long col, double q);
    [DllImport(Lib)] internal static extern int jaos_col_quadratic(ModelHandle m, long col, out double q);
    [DllImport(Lib)] internal static extern int jaos_set_quadratic(ModelHandle m, long numNz, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern long jaos_quadratic_nz(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_quadratic(ModelHandle m, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern int jaos_set_row_quadratic(ModelHandle m, long row, long numNz, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern long jaos_row_quadratic_nz(ModelHandle m, long row);
    [DllImport(Lib)] internal static extern int jaos_row_quadratic(ModelHandle m, long row, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern int jaos_add_cone(ModelHandle m, int type, long n, long[] cols);
    [DllImport(Lib)] internal static extern int jaos_cone(ModelHandle m, long k, out int type, out long n, long[]? cols);
    [DllImport(Lib)] internal static extern int jaos_delete_cones(ModelHandle m, long numDel, long[] cones);
    [DllImport(Lib)] internal static extern int jaos_cone_dual(ModelHandle m, long k, double[] z);

    [DllImport(Lib)] internal static extern int jaos_set_option(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    [DllImport(Lib)] internal static extern int jaos_get_option(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_read_options(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern long jaos_num_options();
    [DllImport(Lib)] internal static extern IntPtr jaos_option_name(long k);
    [DllImport(Lib)] internal static extern int jaos_set_time_limit(ModelHandle m, double seconds);
    [DllImport(Lib)] internal static extern int jaos_set_work_limit(ModelHandle m, long units);
    [DllImport(Lib)] internal static extern int jaos_set_threads(ModelHandle m, long threads);
    [DllImport(Lib)] internal static extern long jaos_threads_of(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_set_primal_tolerance(ModelHandle m, double tol);
    [DllImport(Lib)] internal static extern int jaos_set_dual_tolerance(ModelHandle m, double tol);
    [DllImport(Lib)] internal static extern int jaos_set_algorithm(ModelHandle m, int alg);
    [DllImport(Lib)] internal static extern int jaos_algorithm_of(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_set_log_callback(ModelHandle m, LogFn? cb, IntPtr user);
    [DllImport(Lib)] internal static extern int jaos_set_log_level(ModelHandle m, int level);
    [DllImport(Lib)] internal static extern int jaos_set_progress_callback(ModelHandle m, ProgressFn? cb, IntPtr user);
    [DllImport(Lib)] internal static extern int jaos_set_incumbent_callback(ModelHandle m, IncumbentFn? cb, IntPtr user);
    [DllImport(Lib)] internal static extern int jaos_set_node_callback(ModelHandle m, NodeFn? cb, IntPtr user);
    [DllImport(Lib)] internal static extern int jaos_node_add_row(IntPtr node, long nnz, long[] index, double[] value, double lower, double upper);

    [DllImport(Lib)] internal static extern int jaos_set_mip_start(ModelHandle m, double[]? x);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cutoff(ModelHandle m, double cutoff);
    [DllImport(Lib)] internal static extern int jaos_set_mip_pool_size(ModelHandle m, long size);
    [DllImport(Lib)] internal static extern int jaos_set_mip_tree_batch(ModelHandle m, long nodes);
    [DllImport(Lib)] internal static extern int jaos_set_mip_gap(ModelHandle m, double gap);
    [DllImport(Lib)] internal static extern int jaos_set_mip_node_limit(ModelHandle m, long nodes);
    [DllImport(Lib)] internal static extern int jaos_set_mip_branching(ModelHandle m, int rule);
    [DllImport(Lib)] internal static extern int jaos_set_mip_reliability(ModelHandle m, long branches);
    [DllImport(Lib)] internal static extern int jaos_set_mip_probe_cap(ModelHandle m, double multiple);
    [DllImport(Lib)] internal static extern int jaos_set_mip_probe_depth(ModelHandle m, long depth);
    [DllImport(Lib)] internal static extern int jaos_set_mip_node_select(ModelHandle m, long rule);
    [DllImport(Lib)] internal static extern int jaos_set_mip_restart(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_conflicts(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_symmetry(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_orbital(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_propagate(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_propagate_depth(ModelHandle m, long depth);
    [DllImport(Lib)] internal static extern int jaos_set_mip_rcfix(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_tighten(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_probing(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_probing_cap(ModelHandle m, double multiple);
    [DllImport(Lib)] internal static extern int jaos_set_mip_clique_fix(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cut_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cut_depth(ModelHandle m, long depth);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cut_drop(ModelHandle m, [MarshalAs(UnmanagedType.U1)] bool on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_node_cut_cap(ModelHandle m, long cap);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cover_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_clique_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_zero_half_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_flow_cover_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cut_stall(ModelHandle m, double fraction);
    [DllImport(Lib)] internal static extern int jaos_set_mip_node_cut_stall(ModelHandle m, double fraction);
    [DllImport(Lib)] internal static extern int jaos_set_mip_root_cut_drop(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_cover_lift(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_mir_rounds(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_node_mir(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_mir_aggregate(ModelHandle m, long rows);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive(ModelHandle m, [MarshalAs(UnmanagedType.U1)] bool on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_backtrack(ModelHandle m, long times);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_gap(ModelHandle m, double fraction);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_heuristic(ModelHandle m, long solves);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_heuristic_depth(ModelHandle m, long depth);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_child(ModelHandle m, int rule);
    [DllImport(Lib)] internal static extern int jaos_set_mip_dive_degrade(ModelHandle m, double fraction);
    [DllImport(Lib)] internal static extern int jaos_set_mip_rins(ModelHandle m, long solves);
    [DllImport(Lib)] internal static extern int jaos_set_mip_local_branching(ModelHandle m, long size);
    [DllImport(Lib)] internal static extern int jaos_set_mip_feaspump(ModelHandle m, long rounds);
    [DllImport(Lib)] internal static extern int jaos_set_mip_pump_general(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_pump_obj(ModelHandle m, double decay);
    [DllImport(Lib)] internal static extern int jaos_set_mip_pump_always(ModelHandle m, int on);
    [DllImport(Lib)] internal static extern int jaos_set_mip_heuristics(ModelHandle m, [MarshalAs(UnmanagedType.U1)] bool on);

    [DllImport(Lib)] internal static extern int jaos_solve(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_status_of(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_objective(ModelHandle m, out double value);
    [DllImport(Lib)] internal static extern int jaos_solution(ModelHandle m, double[] x, double[] act, double[] y, double[] d);
    [DllImport(Lib)] internal static extern int jaos_basis(ModelHandle m, int[] colStatus, int[] rowStatus);
    [DllImport(Lib)] internal static extern int jaos_set_basis(ModelHandle m, int[] colStatus, int[] rowStatus);
    [DllImport(Lib)] internal static extern void jaos_clear_basis(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_certificate(ModelHandle m, double[] y);
    [DllImport(Lib)] internal static extern int jaos_unbounded_ray(ModelHandle m, double[] d);
    [DllImport(Lib)] internal static extern int jaos_mip_result(ModelHandle m, out NativeMipReport r);
    [DllImport(Lib)] internal static extern int jaos_mip_incumbent(ModelHandle m, double[] x, out double objective);
    [DllImport(Lib)] internal static extern int jaos_mip_pool_count(ModelHandle m, out long count);
    [DllImport(Lib)] internal static extern int jaos_mip_pool_solution(ModelHandle m, long k, double[] x, out double objective);
    [DllImport(Lib)] internal static extern long jaos_iterations(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_work_units(ModelHandle m);
    [DllImport(Lib)] internal static extern double jaos_solve_time(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_model_statistics(ModelHandle m, out NativeModelStats s);
    [DllImport(Lib)] internal static extern int jaos_presolve_result(ModelHandle m, out NativePresolveReport r);

    [DllImport(Lib)] internal static extern int jaos_check_solution(ModelHandle m, double[] x, double[]? y, double tol, out NativeCheckReport r);
    [DllImport(Lib)] internal static extern int jaos_check_conic_solution(ModelHandle m, double[] x, double[]? y, double[] z, double tol, out NativeCheckReport r);
    [DllImport(Lib)] internal static extern int jaos_check_certificate(ModelHandle m, double[] y, double tol, out NativeCertificateReport r);
    [DllImport(Lib)] internal static extern int jaos_check_conic_certificate(ModelHandle m, double[] y, double[] z, double tol, out NativeCertificateReport r);
    [DllImport(Lib)] internal static extern int jaos_check_ray(ModelHandle m, double[] d, double tol, out NativeRayReport r);

    [DllImport(Lib)] internal static extern int jaos_iis(ModelHandle m, int[] rowSide, int[] colSide, out NativeIisReport r);
    [DllImport(Lib)] internal static extern int jaos_iis_model(ModelHandle m, int[] rowSide, int[] colSide, out ModelHandle sub);
    [DllImport(Lib)] internal static extern int jaos_feasrelax(ModelHandle m, int scope, double[] rowMove, double[] colMove, out NativeRelaxReport r);
    [DllImport(Lib)] internal static extern int jaos_cost_ranging(ModelHandle m, double[] lower, double[] upper);
    [DllImport(Lib)] internal static extern int jaos_rhs_ranging(ModelHandle m, double[] lowerLo, double[] lowerHi, double[] upperLo, double[] upperHi);
    [DllImport(Lib)] internal static extern int jaos_bound_ranging(ModelHandle m, double[] lowerLo, double[] lowerHi, double[] upperLo, double[] upperHi);

    [DllImport(Lib)] internal static extern int jaos_verify(ModelHandle m, out NativeVerifyReport r);
    [DllImport(Lib)] internal static extern int jaos_verify_basis(ModelHandle m, int[] colStatus, int[] rowStatus, out NativeVerifyReport r);
    [DllImport(Lib)] internal static extern int jaos_exact_col_value(ModelHandle m, long col, out IntPtr text);
    [DllImport(Lib)] internal static extern int jaos_exact_row_dual(ModelHandle m, long row, out IntPtr text);
    [DllImport(Lib)] internal static extern int jaos_exact_objective(ModelHandle m, out IntPtr text);
    [DllImport(Lib)] internal static extern int jaos_exact_certificate(ModelHandle m, out NativeExactRayReport r);
    [DllImport(Lib)] internal static extern int jaos_exact_row_multiplier(ModelHandle m, long row, out IntPtr text);
    [DllImport(Lib)] internal static extern int jaos_exact_unbounded_ray(ModelHandle m, out NativeExactRayReport r);
    [DllImport(Lib)] internal static extern int jaos_exact_col_direction(ModelHandle m, long col, out IntPtr text);
    [DllImport(Lib)] internal static extern int jaos_write_proof(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_check_proof(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path, out NativeProofReport r);
}
