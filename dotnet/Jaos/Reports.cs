// SPDX-License-Identifier: Apache-2.0
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Jaos;

/// <summary>Where a variable rests in a basis: the C <c>jaos_basis_status</c>.</summary>
public enum BasisStatus
{
    Basic = 0,
    AtLower = 1,
    AtUpper = 2,
    Free = 3,
}

/// <summary>What a progress, incumbent or node callback asks of the solve.</summary>
public enum CallbackAction
{
    Continue = 0,
    Stop = 1,
}

/// <summary>What solves an LP: the C <c>jaos_algorithm</c>.</summary>
public enum Algorithm
{
    Dual = 0,
    Primal = 1,
    Barrier = 2,
    Pdlp = 3,
    Concurrent = 4,
}

/// <summary>Which column a fractional node branches on.</summary>
public enum Branching
{
    Pseudocost = 0,
    MostFractional = 1,
}

/// <summary>Which child a dive solves first.</summary>
public enum DiveChild
{
    Nearer = 0,
    Up = 1,
    Down = 2,
    Pseudocost = 3,
}

/// <summary>The sides of a bound that belong to an irreducible infeasible subsystem.</summary>
[Flags]
public enum IisSide
{
    None = 0,
    Lower = 1,
    Upper = 2,
    Both = 3,
}

/// <summary>Which bounds a feasibility relaxation may move.</summary>
public enum RelaxScope
{
    Rows = 1,
    Cols = 2,
    Both = 3,
}

/// <summary>What <see cref="Model.Verify"/> concluded.</summary>
public enum Proof
{
    Optimal = 0,
    Broken = 1,
    Refused = 2,
}

/// <summary>Which check a <see cref="Proof.Broken"/> verdict came from.</summary>
public enum ProofStage
{
    None = 0,
    Rank = 1,
    Primal = 2,
    Dual = 3,
}

/// <summary>Which of the three answers a proof file claims.</summary>
public enum ProofKind
{
    Optimal = 0,
    Infeasible = 1,
    Unbounded = 2,
}

/// <summary>One status per column and one per row.</summary>
public sealed record Basis(BasisStatus[] ColStatus, BasisStatus[] RowStatus);

/// <summary>What the progress callback sees: the C <c>jaos_progress</c>.</summary>
public sealed record Progress(long Iterations, long WorkUnits,
                              double PrimalInfeasibility);

/// <summary>What the incumbent callback sees: the C <c>jaos_incumbent</c>.</summary>
public sealed record Incumbent(long Node, double Objective, double Bound,
                               double[] Values, bool ByRounding);

/// <summary>The C <c>jaos_check_report</c>; jaos.h says what each field decides.</summary>
public sealed record CheckReport(
    double MaxColViolation, double MaxRowViolation,
    double MaxRowViolationRelative, double MaxDualViolation,
    double PrimalObjective, double DualObjective, double ObjectiveGap,
    double GapPositive, double GapNegative, double MaxDroppedMultiplier,
    long DroppedTerms, double CertifiedSuboptimality, long UnquantifiedRays,
    double RelativeSuboptimality, bool PrimalFeasible, bool DualFeasible,
    bool CheckedDuals, bool GapCertified, double MaxIntegralityViolation,
    double MaxConeViolation)
{
    internal CheckReport(in NativeCheckReport r) : this(
        r.MaxColViolation, r.MaxRowViolation, r.MaxRowViolationRelative,
        r.MaxDualViolation, r.PrimalObjective, r.DualObjective,
        r.ObjectiveGap, r.GapPositive, r.GapNegative, r.MaxDroppedMultiplier,
        r.DroppedTerms, r.CertifiedSuboptimality, r.UnquantifiedRays,
        r.RelativeSuboptimality, r.PrimalFeasible, r.DualFeasible,
        r.CheckedDuals, r.GapCertified, r.MaxIntegralityViolation,
        r.MaxConeViolation) { }
}

/// <summary>The C <c>jaos_certificate_report</c>.</summary>
public sealed record CertificateReport(double SupColumns, double InfRows,
                                       double Gap, bool Certified);

/// <summary>The C <c>jaos_ray_report</c>.</summary>
public sealed record RayReport(double Rate, double MaxColEscape,
                               double MaxRowEscape, double Curvature,
                               bool Certified);

/// <summary>The C <c>jaos_iis_report</c>.</summary>
public sealed record IisReport(long Members, long Candidates, long Solves,
                               long WorkUnits, bool FromCertificate);

/// <summary>An irreducible infeasible subsystem: one side per row and per column.</summary>
public sealed record Iis(IisSide[] RowSide, IisSide[] ColSide, IisReport Report);

/// <summary>The C <c>jaos_relax_report</c>.</summary>
public sealed record RelaxReport(double Total, long RowsMoved, long ColsMoved,
                                 long AtRow, long AtCol, double Largest,
                                 long WorkUnits, SolveStatus Status);

/// <summary>One signed move per row and per column, below zero on the lower side.</summary>
public sealed record Relaxation(double[] RowMove, double[] ColMove,
                                RelaxReport Report);

/// <summary>How far each column's cost may move with the basis staying optimal.</summary>
public sealed record CostRanging(double[] Lower, double[] Upper);

/// <summary>How far each lower and upper bound may move, one entry per row or column.</summary>
public sealed record BoundRanging(double[] LowerLo, double[] LowerHi,
                                  double[] UpperLo, double[] UpperHi);

/// <summary>The C <c>jaos_verify_report</c>.</summary>
public sealed record VerifyReport(Proof Status, ProofStage Stage,
                                  double BoundBits, double CapacityBits,
                                  long Blocks, long LargestBlock, long AtRow,
                                  long AtCol, double Violation, long BytesHeld,
                                  long Terms)
{
    internal VerifyReport(in NativeVerifyReport r) : this(
        (Proof)r.Status, (ProofStage)r.Stage, r.BoundBits, r.CapacityBits,
        r.Blocks, r.LargestBlock, r.AtRow, r.AtCol, r.Violation, r.BytesHeld,
        r.Terms) { }
}

/// <summary>The C <c>jaos_exact_ray_report</c>.</summary>
public sealed record ExactRayReport(bool Derived, double BoundBits,
                                    double CapacityBits, long Blocks,
                                    long LargestBlock, long AtRow,
                                    long BytesHeld, long Terms)
{
    internal ExactRayReport(in NativeExactRayReport r) : this(
        r.Derived, r.BoundBits, r.CapacityBits, r.Blocks, r.LargestBlock,
        r.AtRow, r.BytesHeld, r.Terms) { }
}

/// <summary>The C <c>jaos_proof_report</c>.</summary>
public sealed record ProofReport(bool Primal, bool Dual, bool Objective,
                                 long BadRow, long BadCol, long Terms,
                                 ProofKind Kind, bool Certified);

/// <summary>The C <c>jaos_model_stats</c>.</summary>
public sealed record ModelStats(
    long NumRow, long NumCol, long NumNz, long IntegerCol, long BinaryCol,
    long EqualityRow, long RangedRow, long OneSidedRow, long FreeRow,
    long FixedCol, long RangedCol, long OneSidedCol, long FreeCol,
    long EmptyRow, long EmptyCol, long ObjNz, double MinAbs, double MaxAbs,
    double ObjMinAbs, double ObjMaxAbs, long SemicontinuousCol, long SosSet,
    long IndicatorRow, long QuadraticCol, long ConeSet, long QuadraticRow)
{
    internal ModelStats(in NativeModelStats s) : this(
        s.NumRow, s.NumCol, s.NumNz, s.IntegerCol, s.BinaryCol,
        s.EqualityRow, s.RangedRow, s.OneSidedRow, s.FreeRow, s.FixedCol,
        s.RangedCol, s.OneSidedCol, s.FreeCol, s.EmptyRow, s.EmptyCol,
        s.ObjNz, s.MinAbs, s.MaxAbs, s.ObjMinAbs, s.ObjMaxAbs,
        s.SemicontinuousCol, s.SosSet, s.IndicatorRow, s.QuadraticCol,
        s.ConeSet, s.QuadraticRow) { }
}

/// <summary>The C <c>jaos_presolve_report</c>.</summary>
public sealed record PresolveReport(
    long NumRow, long NumCol, long NumNz, long Rounds, long FixedCol,
    long EmptyRow, long EmptyCol, long SingletonRow, long SingletonCol,
    long FreeColSingleton, long ForcingRow, long RedundantRow,
    long ImpliedFreeCol, long TightenedBound, long DuplicateRow,
    long DuplicateCol, long DominatedCol, long AggregatedCol)
{
    internal PresolveReport(in NativePresolveReport r) : this(
        r.NumRow, r.NumCol, r.NumNz, r.Rounds, r.FixedCol, r.EmptyRow,
        r.EmptyCol, r.SingletonRow, r.SingletonCol, r.FreeColSingleton,
        r.ForcingRow, r.RedundantRow, r.ImpliedFreeCol, r.TightenedBound,
        r.DuplicateRow, r.DuplicateCol, r.DominatedCol, r.AggregatedCol) { }
}

/// <summary>
/// What the node callback sees. <see cref="BranchCol"/> is the solver's
/// choice and may be set to any integer column fractional at the point;
/// <see cref="AddRow(long[], double[], double, double)"/> adds a row every
/// solution of the model must satisfy. Live only while the callback runs.
/// </summary>
public sealed class NodeEvent
{
    private IntPtr ev;

    internal NodeEvent(IntPtr ev, in NativeNode c)
    {
        this.ev = ev;
        Node = c.Node;
        Depth = c.Depth;
        Objective = c.Objective;
        Bound = c.Bound;
        Values = new double[c.NumCol];
        if (c.NumCol > 0)
            Marshal.Copy(c.ColValue, Values, 0, (int)c.NumCol);
        Integral = c.Integral;
        BranchCol = c.BranchCol;
    }

    public long Node { get; }
    public long Depth { get; }

    /// <summary>The node's relaxation.</summary>
    public double Objective { get; }

    /// <summary>The tree's bound.</summary>
    public double Bound { get; }

    public double[] Values { get; }

    /// <summary>The point is integer feasible and becomes an incumbent unless a row added here cuts it.</summary>
    public bool Integral { get; }

    public long BranchCol { get; set; }

    /// <summary>Adds <c>lower &lt;= sum(value[k] * x[index[k]]) &lt;= upper</c>.</summary>
    public void AddRow(long[] index, double[] value,
                       double lower = double.NegativeInfinity,
                       double upper = double.PositiveInfinity)
    {
        if (ev == IntPtr.Zero)
            throw new JaosException(Status.InvalidInput, "this node event is over");
        if (index.Length != value.Length)
            throw new ArgumentException("index and value differ in length");
        int st = Native.jaos_node_add_row(ev, index.Length, Native.Pad(index),
                                          Native.Pad(value), lower, upper);
        if (st != 0)
            throw new JaosException((Status)st,
                "the row is not one the node callback may add: a column " +
                "index out of range, a non-finite coefficient, or bounds " +
                "that cross");
    }

    /// <summary>Adds <c>lower &lt;= e &lt;= upper</c> over a <see cref="Problem"/>'s variables, the constant of <paramref name="e"/> moved to the sides.</summary>
    public void AddRow(Expr e, double lower, double upper)
    {
        if (e.Quad.Count > 0)
            throw new ArgumentException("a row added at a node is linear");
        var merged = new SortedDictionary<int, double>();
        foreach (var (c, v) in e.Terms)
            merged[c] = merged.GetValueOrDefault(c) + v;
        var index = new List<long>();
        var value = new List<double>();
        foreach (var (c, v) in merged)
            if (v != 0.0)
            {
                index.Add(c);
                value.Add(v);
            }
        AddRow(index.ToArray(), value.ToArray(), lower - e.Constant,
               upper - e.Constant);
    }

    /// <summary>Hands the tree a point, one value per column: before its next node the tree rounds the integer columns and takes the point as its incumbent when it meets every bound and row and beats the incumbent it has. A later call replaces an earlier one.</summary>
    public void AddSolution(double[] values)
    {
        if (ev == IntPtr.Zero)
            throw new JaosException(Status.InvalidInput, "this node event is over");
        int st = Native.jaos_node_add_solution(ev, values.Length, Native.Pad(values));
        if (st != 0)
            throw new JaosException((Status)st,
                "the point is not one the node callback may hand: one " +
                "value per column, every value finite");
    }

    internal void Close() => ev = IntPtr.Zero;
}
