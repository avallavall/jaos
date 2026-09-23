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

internal static class Native
{
    internal const string Lib = "jaos";

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    internal delegate void LogFn(IntPtr user, int level, IntPtr line);

    static Native()
    {
        NativeLibrary.SetDllImportResolver(typeof(Native).Assembly, Resolve);
    }

    internal static void Ensure() { }

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
    [DllImport(Lib)] internal static extern IntPtr jaos_solve_status_str(int s);

    [DllImport(Lib)]
    internal static extern int jaos_load_lp(ModelHandle m, long numCol,
        long numRow, int sense, double offset, double[] cost,
        double[] colLower, double[] colUpper, double[] rowLower,
        double[] rowUpper, long numNz, long[] aStart, long[] aIndex,
        double[] aValue);

    [DllImport(Lib)] internal static extern int jaos_read_mps(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_lp(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_nl(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_qplib(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_osil(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_read_cbf(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_mps(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_lp(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int jaos_write_solution(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    [DllImport(Lib)] internal static extern long jaos_num_col(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_row(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_num_cones(ModelHandle m);
    [DllImport(Lib)] [return: MarshalAs(UnmanagedType.U1)] internal static extern bool jaos_model_has_integer(ModelHandle m);

    [DllImport(Lib)] internal static extern int jaos_set_col_integer(ModelHandle m, long col, [MarshalAs(UnmanagedType.U1)] bool on);
    [DllImport(Lib)] internal static extern int jaos_set_col_bounds(ModelHandle m, long col, double lower, double upper);
    [DllImport(Lib)] internal static extern int jaos_set_row_bounds(ModelHandle m, long row, double lower, double upper);
    [DllImport(Lib)] internal static extern int jaos_set_col_cost(ModelHandle m, long col, double cost);
    [DllImport(Lib)] internal static extern int jaos_set_col_quadratic(ModelHandle m, long col, double q);
    [DllImport(Lib)] internal static extern int jaos_set_quadratic(ModelHandle m, long numNz, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern int jaos_set_row_quadratic(ModelHandle m, long row, long numNz, long[] rows, long[] cols, double[] values);
    [DllImport(Lib)] internal static extern int jaos_add_cone(ModelHandle m, int type, long n, long[] cols);
    [DllImport(Lib)] internal static extern int jaos_cone_dual(ModelHandle m, long k, double[] z);
    [DllImport(Lib)] internal static extern int jaos_cone(ModelHandle m, long k, out int type, out long n, long[]? cols);

    [DllImport(Lib)] internal static extern int jaos_set_option(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, [MarshalAs(UnmanagedType.LPUTF8Str)] string value);
    [DllImport(Lib)] internal static extern int jaos_get_option(ModelHandle m, [MarshalAs(UnmanagedType.LPUTF8Str)] string name, byte[] buf, long cap);
    [DllImport(Lib)] internal static extern int jaos_set_time_limit(ModelHandle m, double seconds);
    [DllImport(Lib)] internal static extern int jaos_set_work_limit(ModelHandle m, long units);
    [DllImport(Lib)] internal static extern int jaos_set_threads(ModelHandle m, long threads);
    [DllImport(Lib)] internal static extern int jaos_set_mip_start(ModelHandle m, double[] x);
    [DllImport(Lib)] internal static extern long jaos_num_options();
    [DllImport(Lib)] internal static extern IntPtr jaos_option_name(long k);
    [DllImport(Lib)] internal static extern int jaos_set_log_callback(ModelHandle m, LogFn? cb, IntPtr user);
    [DllImport(Lib)] internal static extern int jaos_set_log_level(ModelHandle m, int level);

    [DllImport(Lib)] internal static extern int jaos_solve(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_status_of(ModelHandle m);
    [DllImport(Lib)] internal static extern int jaos_objective(ModelHandle m, out double value);
    [DllImport(Lib)] internal static extern int jaos_solution(ModelHandle m, double[] x, double[] act, double[] y, double[] d);
    [DllImport(Lib)] internal static extern int jaos_certificate(ModelHandle m, double[] y);
    [DllImport(Lib)] internal static extern int jaos_unbounded_ray(ModelHandle m, double[] d);
    [DllImport(Lib)] internal static extern int jaos_mip_result(ModelHandle m, out NativeMipReport r);
    [DllImport(Lib)] internal static extern int jaos_mip_incumbent(ModelHandle m, double[] x, out double objective);
    [DllImport(Lib)] internal static extern long jaos_iterations(ModelHandle m);
    [DllImport(Lib)] internal static extern long jaos_work_units(ModelHandle m);
}
