// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemorySegment;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.ref.Cleaner;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.BiConsumer;
import java.util.function.Consumer;
import java.util.function.Function;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

/**
 * A {@code jaos_model}: every call of the C library that builds, reads,
 * writes, solves and reads back a model, one method each. {@link #close}
 * frees it, and so does the garbage collector when it is not closed.
 */
public final class Model implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();
    private static final long NAME_CAP = 256;

    private static final class Failure {
        volatile Throwable error;

        void set(Throwable t) {
            if (error == null)
                error = t;
        }
    }

    private final MemorySegment h;
    private final Cleaner.Cleanable cleanable;
    private final Failure failure;
    private MemorySegment logStub = MemorySegment.NULL;
    private MemorySegment progressStub = MemorySegment.NULL;
    private MemorySegment incumbentStub = MemorySegment.NULL;
    private MemorySegment nodeStub = MemorySegment.NULL;
    private double[] mipStart;

    public Model() {
        this(create(), new Failure());
    }

    private Model(MemorySegment handle, Failure failure) {
        h = handle;
        this.failure = failure;
        cleanable = CLEANER.register(this, () -> call(Native.MODEL_FREE, handle));
    }

    private static MemorySegment create() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(ADDRESS);
            int st = (int) call(Native.MODEL_NEW, out);
            if (st != 0)
                throw new JaosException(Status.values()[st], "jaos_model_new failed");
            return out.get(ADDRESS, 0);
        }
    }

    private Model adopt(MemorySegment handle) {
        Model m = new Model(handle, failure);
        m.logStub = logStub;
        m.progressStub = progressStub;
        m.incumbentStub = incumbentStub;
        m.nodeStub = nodeStub;
        return m;
    }

    @Override
    public void close() {
        cleanable.clean();
    }

    static Object call(MethodHandle mh, Object... args) {
        try {
            return mh.invokeWithArguments(args);
        } catch (RuntimeException | Error e) {
            throw e;
        } catch (Throwable t) {
            throw new IllegalStateException(t);
        }
    }

    private void check(Object status) {
        int st = (int) status;
        if (st != 0)
            throw new JaosException(Status.values()[st], error());
    }

    private int guarded(MethodHandle mh, Object... args) {
        failure.error = null;
        int st = (int) call(mh, args);
        Throwable t = failure.error;
        failure.error = null;
        if (t instanceof RuntimeException r)
            throw r;
        if (t instanceof Error e)
            throw e;
        if (t != null)
            throw new IllegalStateException(t);
        return st;
    }

    static MemorySegment doubles(Arena a, double[] v) {
        return v.length == 0 ? a.allocate(JAVA_DOUBLE)
                             : a.allocateFrom(JAVA_DOUBLE, v);
    }

    static MemorySegment longs(Arena a, long[] v) {
        return v.length == 0 ? a.allocate(JAVA_LONG)
                             : a.allocateFrom(JAVA_LONG, v);
    }

    private static MemorySegment ints(Arena a, int[] v) {
        return v.length == 0 ? a.allocate(JAVA_INT)
                             : a.allocateFrom(JAVA_INT, v);
    }

    private static MemorySegment text(Arena a, String s) {
        return s == null ? MemorySegment.NULL : a.allocateFrom(s);
    }

    private static MemorySegment doublesOut(Arena a, long n) {
        return a.allocate(JAVA_DOUBLE, Math.max(n, 1));
    }

    private static MemorySegment intsOut(Arena a, long n) {
        return a.allocate(JAVA_INT, Math.max(n, 1));
    }

    private static double[] prefix(MemorySegment s, long n) {
        return s.asSlice(0, n * Double.BYTES).toArray(JAVA_DOUBLE);
    }

    private static long[] longPrefix(MemorySegment s, long n) {
        return s.asSlice(0, n * Long.BYTES).toArray(JAVA_LONG);
    }

    private static int[] intPrefix(MemorySegment s, long n) {
        return s.asSlice(0, n * Integer.BYTES).toArray(JAVA_INT);
    }

    private static BasisStatus[] statuses(MemorySegment s, long n) {
        int[] v = intPrefix(s, n);
        BasisStatus[] out = new BasisStatus[v.length];
        for (int k = 0; k < v.length; k++)
            out[k] = BasisStatus.values()[v[k]];
        return out;
    }

    private static int[] codes(Enum<?>[] v) {
        int[] out = new int[v.length];
        for (int k = 0; k < v.length; k++)
            out[k] = v[k].ordinal();
        return out;
    }

    private static IisSide[] sides(MemorySegment s, long n) {
        int[] v = intPrefix(s, n);
        IisSide[] out = new IisSide[v.length];
        for (int k = 0; k < v.length; k++)
            out[k] = IisSide.values()[v[k]];
        return out;
    }

    private static boolean flag(MemorySegment s, long offset) {
        return s.get(JAVA_BYTE, offset) != 0;
    }

    private static void need(int length, long n, String what) {
        if (length != n)
            throw new IllegalArgumentException(
                what + " needs " + n + " values, and got " + length);
    }

    public static String version() {
        return Native.string((MemorySegment) call(Native.VERSION));
    }

    /** The git commit the library was built from, empty when the build had none. */
    public static String buildCommit() {
        return Native.string((MemorySegment) call(Native.BUILD_COMMIT));
    }

    public static String statusString(SolveStatus s) {
        return Native.string((MemorySegment) call(Native.STATUS_STR, s.ordinal()));
    }

    /** The library's name for what a call returned. */
    public static String statusString(Status s) {
        return Native.string((MemorySegment) call(Native.STATUS_NAME, s.ordinal()));
    }

    /** The library's infinity, the value of a missing bound. */
    public static double infinity() {
        return (double) call(Native.INFINITY);
    }

    /** The model's last message. */
    public String error() {
        return Native.string((MemorySegment) call(Native.MODEL_ERROR, h));
    }

    /**
     * Reads {@code path} with the reader its name selects, as {@code jaos
     * solve} does: MPS for a name with no known extension.
     */
    public static Model read(String path) {
        Model m = new Model();
        try {
            m.readFile(path);
        } catch (RuntimeException e) {
            m.close();
            throw e;
        }
        return m;
    }

    public void readFile(String path) {
        String p = path.endsWith(".gz") ? path.substring(0, path.length() - 3) : path;
        MethodHandle reader = p.endsWith(".lp") ? Native.READ_LP
            : p.endsWith(".nl") ? Native.READ_NL
            : p.endsWith(".qplib") ? Native.READ_QPLIB
            : p.endsWith(".osil") ? Native.READ_OSIL
            : p.endsWith(".cbf") ? Native.READ_CBF
            : Native.READ_MPS;
        withPath(reader, path);
    }

    public void readMps(String path) { withPath(Native.READ_MPS, path); }
    public void readLp(String path) { withPath(Native.READ_LP, path); }
    public void readNl(String path) { withPath(Native.READ_NL, path); }
    public void readQplib(String path) { withPath(Native.READ_QPLIB, path); }
    public void readCbf(String path) { withPath(Native.READ_CBF, path); }
    public void readOsil(String path) { withPath(Native.READ_OSIL, path); }

    public void writeMps(String path) { withPath(Native.WRITE_MPS, path); }
    public void writeLp(String path) { withPath(Native.WRITE_LP, path); }
    /** An AMPL .nl file with its .col and .row name files beside it. */
    public void writeNl(String path) { withPath(Native.WRITE_NL, path); }
    public void writeQplib(String path) { withPath(Native.WRITE_QPLIB, path); }
    public void writeCbf(String path) { withPath(Native.WRITE_CBF, path); }
    public void writeOsil(String path) { withPath(Native.WRITE_OSIL, path); }
    public void writeSolution(String path) { withPath(Native.WRITE_SOLUTION, path); }

    /** The last solve as the .sol file an AMPL solver hands back; null writes the status and objective as the message. */
    public void writeSolAmpl(String path, String message) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.WRITE_SOL_AMPL, h, a.allocateFrom(path), text(a, message)));
        }
    }

    private void withPath(MethodHandle mh, String path) {
        try (Arena a = Arena.ofConfined()) {
            check(call(mh, h, a.allocateFrom(path)));
        }
    }

    /** Options from a file, one {@code name value} per line, {@code #} for a comment. */
    public void readOptions(String path) { withPath(Native.READ_OPTIONS, path); }

    /** Reads back a file {@link #writeSolution} wrote for an optimum; installs nothing. */
    public SolutionFile readSolution(String path) {
        long nc = numCol(), nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment obj = a.allocate(JAVA_DOUBLE);
            MemorySegment cv = doublesOut(a, nc), cd = doublesOut(a, nc);
            MemorySegment ra = doublesOut(a, nr), rd = doublesOut(a, nr);
            MemorySegment cs = intsOut(a, nc), rs = intsOut(a, nr);
            check(call(Native.READ_SOLUTION, h, a.allocateFrom(path), obj, cv, cd, cs,
                       ra, rd, rs));
            return new SolutionFile(obj.get(JAVA_DOUBLE, 0),
                new Solution(prefix(cv, nc), prefix(ra, nr), prefix(rd, nr), prefix(cd, nc)),
                new Basis(statuses(cs, nc), statuses(rs, nr)));
        }
    }

    /** Reads back the certificate {@link #writeSolution} wrote for INFEASIBLE or UNBOUNDED. */
    public Certificate readCertificate(String path) {
        long nc = numCol(), nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment st = a.allocate(JAVA_INT);
            MemorySegment rr = doublesOut(a, nr), cr = doublesOut(a, nc);
            check(call(Native.READ_CERTIFICATE, h, a.allocateFrom(path), st, rr, cr));
            SolveStatus s = SolveStatus.values()[st.get(JAVA_INT, 0)];
            return new Certificate(s, s == SolveStatus.INFEASIBLE ? prefix(rr, nr)
                                                                  : prefix(cr, nc));
        }
    }

    /** The cone records of a solution file, one array per cone. */
    public double[][] readConeDuals(String path) {
        int nk = (int) numCones();
        int[] sizes = new int[nk];
        long total = 0;
        for (int k = 0; k < nk; k++) {
            sizes[k] = cone(k).cols().length;
            total += sizes[k];
        }
        try (Arena a = Arena.ofConfined()) {
            MemorySegment z = doublesOut(a, total);
            check(call(Native.READ_CONE_DUALS, h, a.allocateFrom(path), z));
            double[] flat = prefix(z, total);
            double[][] out = new double[nk][];
            for (int k = 0, at = 0; k < nk; at += sizes[k], k++)
                out[k] = Arrays.copyOfRange(flat, at, at + sizes[k]);
            return out;
        }
    }

    private Basis basisFrom(MethodHandle mh, String path) {
        long nc = numCol(), nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment cs = intsOut(a, nc), rs = intsOut(a, nr);
            check(call(mh, h, a.allocateFrom(path), cs, rs));
            return new Basis(statuses(cs, nc), statuses(rs, nr));
        }
    }

    /** The basis in a solution file of either kind; installs nothing. */
    public Basis readBasis(String path) { return basisFrom(Native.READ_BASIS, path); }

    /** The last solve's basis in the MPS basis file format. */
    public void writeMpsBasis(String path) { withPath(Native.WRITE_MPS_BASIS, path); }

    /** The basis in an MPS basis file, written here or by another solver; installs nothing. */
    public Basis readMpsBasis(String path) { return basisFrom(Native.READ_MPS_BASIS, path); }

    /** The last optimum's point: one {@code NAME VALUE} line per column. */
    public void writePoint(String path) { withPath(Native.WRITE_POINT, path); }

    /** The same file from values the caller has, one per column; needs no solve. */
    public void writePointValues(String path, double[] x) {
        need(x.length, numCol(), "a point");
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.WRITE_POINT_VALUES, h, a.allocateFrom(path), doubles(a, x)));
        }
    }

    /** The last optimum's row duals, in the point file's shape. */
    public void writeDuals(String path) { withPath(Native.WRITE_DUALS, path); }

    /** The same file from duals the caller has, one per row; needs no solve. */
    public void writeDualValues(String path, double[] y) {
        need(y.length, numRow(), "a duals file");
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.WRITE_DUAL_VALUES, h, a.allocateFrom(path), doubles(a, y)));
        }
    }

    private double[] valuesFrom(MethodHandle mh, String path, long n) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = doublesOut(a, n);
            check(call(mh, h, a.allocateFrom(path), v));
            return prefix(v, n);
        }
    }

    /** The column values in a point file, in index order; every column must appear once. */
    public double[] readPoint(String path) { return valuesFrom(Native.READ_POINT, path, numCol()); }

    /** The row duals in a file of the point file's shape, in index order. */
    public double[] readDuals(String path) { return valuesFrom(Native.READ_DUALS, path, numRow()); }

    /** OPTIMAL, INFEASIBLE or UNBOUNDED: what a solution file holds. */
    public SolveStatus solutionFileStatus(String path) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment st = a.allocate(JAVA_INT);
            check(call(Native.SOLUTION_FILE_STATUS, h, a.allocateFrom(path), st));
            return SolveStatus.values()[st.get(JAVA_INT, 0)];
        }
    }

    /**
     * The whole model in one call, the matrix column-wise with 0-based
     * starts (one more than the columns) and row indices.
     */
    public void loadLp(Sense sense, double offset, double[] cost,
                       double[] colLower, double[] colUpper, double[] rowLower,
                       double[] rowUpper, long[] aStart, long[] aIndex,
                       double[] aValue) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.LOAD_LP, h, (long) cost.length, (long) rowLower.length,
                       sense.ordinal(), offset, doubles(a, cost), doubles(a, colLower),
                       doubles(a, colUpper), doubles(a, rowLower), doubles(a, rowUpper),
                       (long) aValue.length, longs(a, aStart), longs(a, aIndex),
                       doubles(a, aValue)));
        }
    }

    /** A new model holding this one's problem, names, settings, callbacks and start basis, and not its answer. */
    public Model copy() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(ADDRESS);
            check(call(Native.MODEL_COPY, h, out));
            return adopt(out.get(ADDRESS, 0));
        }
    }

    public long numCol() { return (long) call(Native.NUM_COL, h); }
    public long numRow() { return (long) call(Native.NUM_ROW, h); }
    public long numNz() { return (long) call(Native.NUM_NZ, h); }
    public long numCones() { return (long) call(Native.NUM_CONES, h); }
    public long numSos() { return (long) call(Native.NUM_SOS, h); }
    public boolean hasInteger() { return (boolean) call(Native.HAS_INTEGER, h); }

    private double doubleAt(MethodHandle mh, long index) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(mh, h, index, v));
            return v.get(JAVA_DOUBLE, 0);
        }
    }

    private boolean flagAt(MethodHandle mh, long index) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_BYTE);
            check(call(mh, h, index, v));
            return v.get(JAVA_BYTE, 0) != 0;
        }
    }

    private Bounds boundsAt(MethodHandle mh, long index) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment lo = a.allocate(JAVA_DOUBLE), hi = a.allocate(JAVA_DOUBLE);
            check(call(mh, h, index, lo, hi));
            return new Bounds(lo.get(JAVA_DOUBLE, 0), hi.get(JAVA_DOUBLE, 0));
        }
    }

    public double colCost(long col) { return doubleAt(Native.COL_COST, col); }
    public Bounds colBounds(long col) { return boundsAt(Native.COL_BOUNDS, col); }
    public Bounds rowBounds(long row) { return boundsAt(Native.ROW_BOUNDS, row); }

    public void setColCost(long col, double cost) {
        check(call(Native.SET_COL_COST, h, col, cost));
    }

    public void setColBounds(long col, double lower, double upper) {
        check(call(Native.SET_COL_BOUNDS, h, col, lower, upper));
    }

    public void setRowBounds(long row, double lower, double upper) {
        check(call(Native.SET_ROW_BOUNDS, h, row, lower, upper));
    }

    public Sense sense() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_INT);
            check(call(Native.OBJECTIVE_SENSE, h, v));
            return Sense.values()[v.get(JAVA_INT, 0)];
        }
    }

    public double objectiveOffset() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(Native.OBJECTIVE_OFFSET, h, v));
            return v.get(JAVA_DOUBLE, 0);
        }
    }

    /** Minimize or maximize; discards the answer, keeps the basis. */
    public void setSense(Sense sense) {
        check(call(Native.SET_OBJECTIVE_SENSE, h, sense.ordinal()));
    }

    /** The objective's constant; discards the answer, keeps the basis. */
    public void setObjectiveOffset(double offset) {
        check(call(Native.SET_OBJECTIVE_OFFSET, h, offset));
    }

    private Entries entriesAt(MethodHandle mh, long k) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment n = a.allocate(JAVA_LONG);
            check(call(mh, h, k, n, MemorySegment.NULL, MemorySegment.NULL));
            long len = n.get(JAVA_LONG, 0);
            MemorySegment idx = a.allocate(JAVA_LONG, Math.max(len, 1));
            MemorySegment val = doublesOut(a, len);
            check(call(mh, h, k, n, idx, val));
            return new Entries(longPrefix(idx, len), prefix(val, len));
        }
    }

    /** One column of the matrix: row indices ascending, no explicit zeros. */
    public Entries colEntries(long col) { return entriesAt(Native.COL_ENTRIES, col); }

    /** One row of the matrix: column indices ascending. */
    public Entries rowEntries(long row) { return entriesAt(Native.ROW_ENTRIES, row); }

    /** One entry; 0 where the model holds none. */
    public double coefficient(long row, long col) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(Native.COEFFICIENT, h, row, col, v));
            return v.get(JAVA_DOUBLE, 0);
        }
    }

    /** Sets one entry; zero deletes it. */
    public void setCoefficient(long row, long col, double value) {
        check(call(Native.SET_COEFFICIENT, h, row, col, value));
    }

    /**
     * Appends columns: {@code start} (one more than the new columns),
     * {@code index} (rows) and {@code value} give their entries, or all
     * three are null for none.
     */
    public void addCols(double[] cost, double[] lower, double[] upper, long[] start,
                        long[] index, double[] value) {
        need(lower.length, cost.length, "the new columns' lower bounds");
        need(upper.length, cost.length, "the new columns' upper bounds");
        try (Arena a = Arena.ofConfined()) {
            boolean some = value != null && value.length > 0;
            check(call(Native.ADD_COLS, h, (long) cost.length, doubles(a, cost),
                       doubles(a, lower), doubles(a, upper),
                       some ? (long) value.length : 0L,
                       some ? longs(a, start) : MemorySegment.NULL,
                       some ? longs(a, index) : MemorySegment.NULL,
                       some ? doubles(a, value) : MemorySegment.NULL));
        }
    }

    /**
     * Appends rows: {@code start} (one more than the new rows), {@code index}
     * (columns) and {@code value} give their entries row by row, or all three
     * are null for none.
     */
    public void addRows(double[] lower, double[] upper, long[] start, long[] index,
                        double[] value) {
        need(upper.length, lower.length, "the new rows' upper bounds");
        try (Arena a = Arena.ofConfined()) {
            boolean some = value != null && value.length > 0;
            check(call(Native.ADD_ROWS, h, (long) lower.length, doubles(a, lower),
                       doubles(a, upper), some ? (long) value.length : 0L,
                       some ? longs(a, start) : MemorySegment.NULL,
                       some ? longs(a, index) : MemorySegment.NULL,
                       some ? doubles(a, value) : MemorySegment.NULL));
        }
    }

    private void deleteSet(MethodHandle mh, long[] which) {
        try (Arena a = Arena.ofConfined()) {
            check(call(mh, h, (long) which.length, longs(a, which)));
        }
    }

    /** Removes a set of columns; the rest keep their order and are numbered from 0 again. */
    public void deleteCols(long... cols) { deleteSet(Native.DELETE_COLS, cols); }
    public void deleteRows(long... rows) { deleteSet(Native.DELETE_ROWS, rows); }
    public void deleteCones(long... cones) { deleteSet(Native.DELETE_CONES, cones); }

    private String nameOf(MethodHandle mh, Object... index) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment buf = a.allocate(NAME_CAP);
            Object[] args = new Object[index.length + 3];
            args[0] = h;
            System.arraycopy(index, 0, args, 1, index.length);
            args[index.length + 1] = buf;
            args[index.length + 2] = NAME_CAP;
            check(call(mh, args));
            return buf.getString(0);
        }
    }

    public String colName(long col) { return nameOf(Native.COL_NAME, col); }
    public String rowName(long row) { return nameOf(Native.ROW_NAME, row); }
    public String objectiveName() { return nameOf(Native.OBJECTIVE_NAME); }

    /** The model's own name: an MPS file's NAME, {@code JAOS} until one is given. */
    public String name() { return nameOf(Native.MODEL_NAME); }

    /** A null name restores the positional one. */
    public void setColName(long col, String name) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_COL_NAME, h, col, text(a, name)));
        }
    }

    /** A null name restores the positional one. */
    public void setRowName(long row, String name) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_ROW_NAME, h, row, text(a, name)));
        }
    }

    public void setObjectiveName(String name) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_OBJECTIVE_NAME, h, text(a, name)));
        }
    }

    /** A null name restores {@code JAOS}. */
    public void setName(String name) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_MODEL_NAME, h, text(a, name)));
        }
    }

    private long indexOf(MethodHandle mh, String name) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_LONG);
            check(call(mh, h, a.allocateFrom(name), v));
            return v.get(JAVA_LONG, 0);
        }
    }

    /** The column called {@code name}, positional names included. */
    public long colIndex(String name) { return indexOf(Native.COL_INDEX, name); }
    public long rowIndex(String name) { return indexOf(Native.ROW_INDEX, name); }

    public void setColInteger(long col, boolean on) {
        check(call(Native.SET_COL_INTEGER, h, col, on));
    }

    public boolean colInteger(long col) { return flagAt(Native.COL_INTEGER, col); }

    /** A semi-continuous column rests at zero or inside its own bounds. */
    public void setColSemicontinuous(long col, boolean on) {
        check(call(Native.SET_COL_SEMICONTINUOUS, h, col, on));
    }

    public boolean colSemicontinuous(long col) {
        return flagAt(Native.COL_SEMICONTINUOUS, col);
    }

    /** A special ordered set of type 1 or 2 over {@code cols}, ordered by {@code weights}. */
    public void addSos(int type, long[] cols, double[] weights) {
        need(weights.length, cols.length, "an SOS set's weights");
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.ADD_SOS, h, type, (long) cols.length, longs(a, cols),
                       doubles(a, weights)));
        }
    }

    public Sos sos(long k) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment t = a.allocate(JAVA_INT), n = a.allocate(JAVA_LONG);
            check(call(Native.SOS, h, k, t, n, MemorySegment.NULL, MemorySegment.NULL));
            long len = n.get(JAVA_LONG, 0);
            MemorySegment cols = a.allocate(JAVA_LONG, Math.max(len, 1));
            MemorySegment w = doublesOut(a, len);
            check(call(Native.SOS, h, k, MemorySegment.NULL, MemorySegment.NULL, cols, w));
            return new Sos(t.get(JAVA_INT, 0), longPrefix(cols, len), prefix(w, len));
        }
    }

    /** Row {@code row} holds only while integer column {@code col} equals {@code value}; col -1 clears it. */
    public void setRowIndicator(long row, long col, int value) {
        check(call(Native.SET_ROW_INDICATOR, h, row, col, value));
    }

    /** The row's indicator, or null. */
    public Indicator rowIndicator(long row) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment c = a.allocate(JAVA_LONG), v = a.allocate(JAVA_INT);
            check(call(Native.ROW_INDICATOR, h, row, c, v));
            long col = c.get(JAVA_LONG, 0);
            return col < 0 ? null : new Indicator(col, v.get(JAVA_INT, 0));
        }
    }

    /** The diagonal entry of the objective's {@code ½ x'Qx}. */
    public void setColQuadratic(long col, double q) {
        check(call(Native.SET_COL_QUADRATIC, h, col, q));
    }

    public double colQuadratic(long col) { return doubleAt(Native.COL_QUADRATIC, col); }

    /** The objective's {@code ½ x'Qx} as triplets, one per diagonal entry and per pair. */
    public void setQuadratic(long[] rows, long[] cols, double[] values) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_QUADRATIC, h, (long) values.length, longs(a, rows),
                       longs(a, cols), doubles(a, values)));
        }
    }

    public long quadraticNz() { return (long) call(Native.QUADRATIC_NZ, h); }

    private QuadraticTerms terms(long n, MethodHandle mh, Object... lead) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(JAVA_LONG, Math.max(n, 1));
            MemorySegment c = a.allocate(JAVA_LONG, Math.max(n, 1));
            MemorySegment v = doublesOut(a, n);
            Object[] args = new Object[lead.length + 4];
            args[0] = h;
            System.arraycopy(lead, 0, args, 1, lead.length);
            args[lead.length + 1] = r;
            args[lead.length + 2] = c;
            args[lead.length + 3] = v;
            if (n > 0)
                check(call(mh, args));
            return new QuadraticTerms(longPrefix(r, n), longPrefix(c, n), prefix(v, n));
        }
    }

    /** The objective's Q back: the lower triangle with the diagonal, in column order. */
    public QuadraticTerms quadratic() { return terms(quadraticNz(), Native.QUADRATIC); }

    /** Row {@code row}'s part {@code ½ x'Qx}. */
    public void setRowQuadratic(long row, long[] rows, long[] cols, double[] values) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_ROW_QUADRATIC, h, row, (long) values.length,
                       longs(a, rows), longs(a, cols), doubles(a, values)));
        }
    }

    public long rowQuadraticNz(long row) {
        return (long) call(Native.ROW_QUADRATIC_NZ, h, row);
    }

    public QuadraticTerms rowQuadratic(long row) {
        return terms(rowQuadraticNz(row), Native.ROW_QUADRATIC, row);
    }

    public void addCone(ConeType type, long[] cols) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.ADD_CONE, h, type.code, (long) cols.length, longs(a, cols)));
        }
    }

    public Cone cone(long k) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment t = a.allocate(JAVA_INT), n = a.allocate(JAVA_LONG);
            check(call(Native.CONE, h, k, t, n, MemorySegment.NULL));
            long len = n.get(JAVA_LONG, 0);
            MemorySegment cols = a.allocate(JAVA_LONG, Math.max(len, 1));
            check(call(Native.CONE, h, k, MemorySegment.NULL, MemorySegment.NULL, cols));
            ConeType type = t.get(JAVA_INT, 0) == ConeType.ROTATED.code
                ? ConeType.ROTATED : ConeType.QUADRATIC;
            return new Cone(type, longPrefix(cols, len));
        }
    }

    public void setOption(String name, String value) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_OPTION, h, a.allocateFrom(name), a.allocateFrom(value)));
        }
    }

    public String getOption(String name) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment buf = a.allocate(512);
            check(call(Native.GET_OPTION, h, a.allocateFrom(name), buf, 512L));
            return buf.getString(0);
        }
    }

    /** The name of every option {@link #setOption} takes. */
    public static String[] optionNames() {
        long n = (long) call(Native.NUM_OPTIONS);
        String[] names = new String[(int) n];
        for (int k = 0; k < n; k++)
            names[k] = Native.string((MemorySegment) call(Native.OPTION_NAME, (long) k));
        return names;
    }

    public void setTimeLimit(double seconds) { check(call(Native.SET_TIME_LIMIT, h, seconds)); }
    public void setWorkLimit(long units) { check(call(Native.SET_WORK_LIMIT, h, units)); }
    /** The thread count; 0 takes every core the machine has. */
    public void setThreads(long threads) { check(call(Native.SET_THREADS, h, threads)); }
    public long threads() { return (long) call(Native.THREADS_OF, h); }
    public void setPrimalTolerance(double tol) { check(call(Native.SET_PRIMAL_TOLERANCE, h, tol)); }
    public void setDualTolerance(double tol) { check(call(Native.SET_DUAL_TOLERANCE, h, tol)); }
    public void setAlgorithm(Algorithm alg) { check(call(Native.SET_ALGORITHM, h, alg.ordinal())); }
    public Algorithm algorithm() { return Algorithm.values()[(int) call(Native.ALGORITHM_OF, h)]; }

    /**
     * The tree's starting point, one value per column; a NaN leaves its column
     * open, and a small tree completes the start with the given integer
     * columns fixed. Null clears it.
     */
    public void setMipStart(double[] x) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_MIP_START, h, x == null ? MemorySegment.NULL
                : doubles(a, Arrays.copyOf(x, (int) numCol()))));
        }
        mipStart = x == null ? null : x.clone();
    }

    void restoreMipStart() {
        if (mipStart != null)
            setMipStart(mipStart);
    }

    /** An objective the tree need not beat; an infinity removes it. */
    public void setMipCutoff(double cutoff) { check(call(Native.SET_MIP_CUTOFF, h, cutoff)); }
    /** The gap that closes the tree; 0 means zero. */
    public void setMipGap(double gap) { check(call(Native.SET_MIP_GAP, h, gap)); }
    public void setMipGapRule(GapRule rule) { check(call(Native.SET_MIP_GAP_RULE, h, rule.ordinal())); }
    public void setMipNodeLimit(long nodes) { check(call(Native.SET_MIP_NODE_LIMIT, h, nodes)); }
    public void setMipPoolSize(long size) { check(call(Native.SET_MIP_POOL_SIZE, h, size)); }
    public void setMipTreeBatch(long nodes) { check(call(Native.SET_MIP_TREE_BATCH, h, nodes)); }
    public void setMipBranching(Branching rule) { check(call(Native.SET_MIP_BRANCHING, h, rule.ordinal())); }
    public void setMipReliability(long branches) { check(call(Native.SET_MIP_RELIABILITY, h, branches)); }
    public void setMipProbeCap(double multiple) { check(call(Native.SET_MIP_PROBE_CAP, h, multiple)); }
    public void setMipProbeDepth(long depth) { check(call(Native.SET_MIP_PROBE_DEPTH, h, depth)); }
    public void setMipNodeSelect(long rule) { check(call(Native.SET_MIP_NODE_SELECT, h, rule)); }
    public void setMipPropagate(long rounds) { check(call(Native.SET_MIP_PROPAGATE, h, rounds)); }
    public void setMipPropagateDepth(long depth) { check(call(Native.SET_MIP_PROPAGATE_DEPTH, h, depth)); }
    public void setMipProbingCap(double multiple) { check(call(Native.SET_MIP_PROBING_CAP, h, multiple)); }
    public void setMipCutRounds(long rounds) { check(call(Native.SET_MIP_CUT_ROUNDS, h, rounds)); }
    public void setMipCutDepth(long depth) { check(call(Native.SET_MIP_CUT_DEPTH, h, depth)); }
    public void setMipCutDrop(boolean on) { check(call(Native.SET_MIP_CUT_DROP, h, on)); }
    public void setMipNodeCutCap(long cap) { check(call(Native.SET_MIP_NODE_CUT_CAP, h, cap)); }
    public void setMipCoverRounds(long rounds) { check(call(Native.SET_MIP_COVER_ROUNDS, h, rounds)); }
    public void setMipCliqueRounds(long rounds) { check(call(Native.SET_MIP_CLIQUE_ROUNDS, h, rounds)); }
    public void setMipZeroHalfRounds(long rounds) { check(call(Native.SET_MIP_ZERO_HALF_ROUNDS, h, rounds)); }
    public void setMipFlowCoverRounds(long rounds) { check(call(Native.SET_MIP_FLOW_COVER_ROUNDS, h, rounds)); }
    public void setMipCutStall(double fraction) { check(call(Native.SET_MIP_CUT_STALL, h, fraction)); }
    public void setMipNodeCutStall(double fraction) { check(call(Native.SET_MIP_NODE_CUT_STALL, h, fraction)); }
    public void setMipMirRounds(long rounds) { check(call(Native.SET_MIP_MIR_ROUNDS, h, rounds)); }
    public void setMipMirAggregate(long rows) { check(call(Native.SET_MIP_MIR_AGGREGATE, h, rows)); }
    public void setMipDive(boolean on) { check(call(Native.SET_MIP_DIVE, h, on)); }
    public void setMipDiveBacktrack(long times) { check(call(Native.SET_MIP_DIVE_BACKTRACK, h, times)); }
    public void setMipDiveGap(double fraction) { check(call(Native.SET_MIP_DIVE_GAP, h, fraction)); }
    public void setMipDiveHeuristic(long solves) { check(call(Native.SET_MIP_DIVE_HEURISTIC, h, solves)); }
    public void setMipDiveHeuristicDepth(long depth) { check(call(Native.SET_MIP_DIVE_HEURISTIC_DEPTH, h, depth)); }
    public void setMipDiveChild(DiveChild rule) { check(call(Native.SET_MIP_DIVE_CHILD, h, rule.ordinal())); }
    public void setMipDiveDegrade(double fraction) { check(call(Native.SET_MIP_DIVE_DEGRADE, h, fraction)); }
    public void setMipRins(long solves) { check(call(Native.SET_MIP_RINS, h, solves)); }
    public void setMipLocalBranching(long size) { check(call(Native.SET_MIP_LOCAL_BRANCHING, h, size)); }
    public void setMipFeaspump(long rounds) { check(call(Native.SET_MIP_FEASPUMP, h, rounds)); }
    public void setMipPumpObj(double decay) { check(call(Native.SET_MIP_PUMP_OBJ, h, decay)); }
    public void setMipHeuristics(boolean on) { check(call(Native.SET_MIP_HEURISTICS, h, on)); }

    /** The switches below take 1 for on, 0 for off and a negative value for the default. */
    public void setMipRestart(int on) { check(call(Native.SET_MIP_RESTART, h, on)); }
    public void setMipConflicts(int on) { check(call(Native.SET_MIP_CONFLICTS, h, on)); }
    public void setMipSymmetry(int on) { check(call(Native.SET_MIP_SYMMETRY, h, on)); }
    public void setMipOrbital(int on) { check(call(Native.SET_MIP_ORBITAL, h, on)); }
    public void setMipRcfix(int on) { check(call(Native.SET_MIP_RCFIX, h, on)); }
    public void setMipTighten(int on) { check(call(Native.SET_MIP_TIGHTEN, h, on)); }
    public void setMipProbing(int on) { check(call(Native.SET_MIP_PROBING, h, on)); }
    public void setMipCliqueFix(int on) { check(call(Native.SET_MIP_CLIQUE_FIX, h, on)); }
    public void setMipRootCutDrop(int on) { check(call(Native.SET_MIP_ROOT_CUT_DROP, h, on)); }
    public void setMipCoverLift(int on) { check(call(Native.SET_MIP_COVER_LIFT, h, on)); }
    public void setMipNodeMir(int on) { check(call(Native.SET_MIP_NODE_MIR, h, on)); }
    public void setMipPumpGeneral(int on) { check(call(Native.SET_MIP_PUMP_GENERAL, h, on)); }
    public void setMipPumpAlways(int on) { check(call(Native.SET_MIP_PUMP_ALWAYS, h, on)); }

    private MemorySegment stub(String method, FunctionDescriptor fd, Object fn,
                               Class<?> fnType) {
        try {
            MethodHandle target = MethodHandles.lookup().findStatic(Model.class, method,
                fd.toMethodType().insertParameterTypes(0, Failure.class, fnType));
            target = MethodHandles.insertArguments(target, 0, failure, fn);
            return Native.LINKER.upcallStub(target, fd, Arena.ofAuto());
        } catch (ReflectiveOperationException e) {
            throw new IllegalStateException(e);
        }
    }

    private static int action(CallbackAction a) {
        return a == null ? CallbackAction.CONTINUE.ordinal() : a.ordinal();
    }

    private static void logLine(Failure failure, BiConsumer<LogLevel, String> sink,
                                MemorySegment user, int level, MemorySegment line) {
        try {
            sink.accept(LogLevel.values()[level], Native.string(line));
        } catch (Throwable t) {
            failure.set(t);
        }
    }

    private static int progressEvent(Failure failure, Function<Progress, CallbackAction> fn,
                                     MemorySegment p, MemorySegment user) {
        try {
            MemorySegment s = p.reinterpret(56);
            return action(fn.apply(new Progress(s.get(JAVA_LONG, 0), s.get(JAVA_LONG, 8),
                                                s.get(JAVA_DOUBLE, 16), s.get(JAVA_LONG, 24),
                                                s.get(JAVA_DOUBLE, 32), flag(s, 40),
                                                s.get(JAVA_DOUBLE, 48))));
        } catch (Throwable t) {
            failure.set(t);
            return CallbackAction.STOP.ordinal();
        }
    }

    private static int incumbentEvent(Failure failure,
                                      Function<IncumbentEvent, CallbackAction> fn,
                                      MemorySegment p, MemorySegment user) {
        try {
            MemorySegment s = p.reinterpret(48);
            long n = s.get(JAVA_LONG, 32);
            double[] x = s.get(ADDRESS, 24).reinterpret(n * Double.BYTES).toArray(JAVA_DOUBLE);
            return action(fn.apply(new IncumbentEvent(s.get(JAVA_LONG, 0),
                s.get(JAVA_DOUBLE, 8), s.get(JAVA_DOUBLE, 16), x, flag(s, 40))));
        } catch (Throwable t) {
            failure.set(t);
            return CallbackAction.STOP.ordinal();
        }
    }

    private static int nodeEvent(Failure failure, Function<NodeEvent, CallbackAction> fn,
                                 MemorySegment p, MemorySegment user) {
        NodeEvent ev = null;
        try {
            ev = new NodeEvent(p.reinterpret(72));
            return action(fn.apply(ev));
        } catch (Throwable t) {
            failure.set(t);
            return CallbackAction.STOP.ordinal();
        } finally {
            if (ev != null)
                ev.end();
        }
    }

    /** Sends the solver's log to {@code sink} at {@code level}; null or OFF stops it. */
    public void setLog(LogLevel level, Consumer<String> sink) {
        setLogCallback(level, sink == null ? null : (lvl, line) -> sink.accept(line));
    }

    /** Sends each log line with its level to {@code sink}; null or OFF stops it. */
    public void setLogCallback(LogLevel level, BiConsumer<LogLevel, String> sink) {
        if (sink == null || level == LogLevel.OFF) {
            check(call(Native.SET_LOG_CALLBACK, h, MemorySegment.NULL, MemorySegment.NULL));
            logStub = MemorySegment.NULL;
            check(call(Native.SET_LOG_LEVEL, h, LogLevel.OFF.ordinal()));
            return;
        }
        MemorySegment s = stub("logLine", Native.LOG_FN, sink, BiConsumer.class);
        check(call(Native.SET_LOG_CALLBACK, h, s, MemorySegment.NULL));
        logStub = s;
        check(call(Native.SET_LOG_LEVEL, h, level.ordinal()));
    }

    /**
     * Calls {@code fn} as the solve runs; STOP ends it INTERRUPTED, null or
     * CONTINUE lets it run on. An exception in {@code fn} stops the solve and
     * is rethrown by {@link #solve}. Null removes the callback.
     */
    public void setProgressCallback(Function<Progress, CallbackAction> fn) {
        MemorySegment s = fn == null ? MemorySegment.NULL
            : stub("progressEvent", Native.EVENT_FN, fn, Function.class);
        check(call(Native.SET_PROGRESS_CALLBACK, h, s, MemorySegment.NULL));
        progressStub = s;
    }

    /** Calls {@code fn} for each new incumbent of a branch and bound, under the progress callback's rules. */
    public void setIncumbentCallback(Function<IncumbentEvent, CallbackAction> fn) {
        MemorySegment s = fn == null ? MemorySegment.NULL
            : stub("incumbentEvent", Native.EVENT_FN, fn, Function.class);
        check(call(Native.SET_INCUMBENT_CALLBACK, h, s, MemorySegment.NULL));
        incumbentStub = s;
    }

    /**
     * Calls {@code fn} at every node once its relaxation is solved, and at
     * every point a heuristic would make an incumbent; the event may add rows
     * and choose the branching column. Same rules as the progress callback.
     */
    public void setNodeCallback(Function<NodeEvent, CallbackAction> fn) {
        MemorySegment s = fn == null ? MemorySegment.NULL
            : stub("nodeEvent", Native.EVENT_FN, fn, Function.class);
        check(call(Native.SET_NODE_CALLBACK, h, s, MemorySegment.NULL));
        nodeStub = s;
    }

    /** Solves the model; returns where the solve stopped. */
    public SolveStatus solve() {
        check(guarded(Native.SOLVE, h));
        return status();
    }

    public SolveStatus status() {
        return SolveStatus.values()[(int) call(Native.STATUS_OF, h)];
    }

    public double objective() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(Native.OBJECTIVE, h, v));
            return v.get(JAVA_DOUBLE, 0);
        }
    }

    public Solution solution() {
        long nc = numCol(), nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment x = doublesOut(a, nc), act = doublesOut(a, nr);
            MemorySegment y = doublesOut(a, nr), d = doublesOut(a, nc);
            check(call(Native.SOLUTION, h, x, act, y, d));
            return new Solution(prefix(x, nc), prefix(act, nr), prefix(y, nr),
                                prefix(d, nc));
        }
    }

    /** Where each variable rests in the basis behind the answer. */
    public Basis basis() {
        long nc = numCol(), nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment cs = intsOut(a, nc), rs = intsOut(a, nr);
            check(call(Native.BASIS, h, cs, rs));
            return new Basis(statuses(cs, nc), statuses(rs, nr));
        }
    }

    /** The next solve's starting basis, one status per column and per row. */
    public void setBasis(Basis b) {
        need(b.colStatus().length, numCol(), "a basis's column statuses");
        need(b.rowStatus().length, numRow(), "a basis's row statuses");
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_BASIS, h, ints(a, codes(b.colStatus())),
                       ints(a, codes(b.rowStatus()))));
        }
    }

    /** The next solve starts cold. */
    public void clearBasis() { call(Native.CLEAR_BASIS, h); }

    /** The dual of cone {@code k}, one entry per member. */
    public double[] coneDual(long k) {
        long len = cone(k).cols().length;
        try (Arena a = Arena.ofConfined()) {
            MemorySegment z = doublesOut(a, len);
            check(call(Native.CONE_DUAL, h, k, z));
            return prefix(z, len);
        }
    }

    /** The Farkas multipliers of an infeasible model, or null. */
    public double[] certificate() {
        long nr = numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment y = doublesOut(a, nr);
            return (int) call(Native.CERTIFICATE, h, y) == 0 ? prefix(y, nr) : null;
        }
    }

    /** The improving direction of an unbounded model, or null. */
    public double[] unboundedRay() {
        long nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment d = doublesOut(a, nc);
            return (int) call(Native.UNBOUNDED_RAY, h, d) == 0 ? prefix(d, nc) : null;
        }
    }

    private static CheckReport checkReport(MemorySegment r) {
        return new CheckReport(r.get(JAVA_DOUBLE, 0), r.get(JAVA_DOUBLE, 8),
            r.get(JAVA_DOUBLE, 16), r.get(JAVA_DOUBLE, 24), r.get(JAVA_DOUBLE, 32),
            r.get(JAVA_DOUBLE, 40), r.get(JAVA_DOUBLE, 48), r.get(JAVA_DOUBLE, 56),
            r.get(JAVA_DOUBLE, 64), r.get(JAVA_DOUBLE, 72), r.get(JAVA_LONG, 80),
            r.get(JAVA_DOUBLE, 88), r.get(JAVA_LONG, 96), r.get(JAVA_DOUBLE, 104),
            flag(r, 112), flag(r, 113), flag(r, 114), flag(r, 115),
            r.get(JAVA_DOUBLE, 120), r.get(JAVA_DOUBLE, 128));
    }

    private static CertificateReport certificateReport(MemorySegment r) {
        return new CertificateReport(r.get(JAVA_DOUBLE, 0), r.get(JAVA_DOUBLE, 8),
                                     r.get(JAVA_DOUBLE, 16), flag(r, 24));
    }

    private MemorySegment coneVector(Arena a, double[][] parts, String what) {
        int nk = (int) numCones();
        if (parts.length != nk)
            throw new IllegalArgumentException(
                what + " has " + parts.length + " cones, expected " + nk);
        List<Double> flat = new ArrayList<>();
        for (int k = 0; k < nk; k++) {
            need(parts[k].length, cone(k).cols().length, what + "[" + k + "]");
            for (double v : parts[k])
                flat.add(v);
        }
        double[] v = new double[flat.size()];
        for (int k = 0; k < v.length; k++)
            v[k] = flat.get(k);
        return doubles(a, v);
    }

    /**
     * The library's independent checker on a candidate answer against the
     * model as loaded; a null {@code rowDual} checks the primal side only.
     */
    public CheckReport checkSolution(double[] x, double[] rowDual, double tol) {
        need(x.length, numCol(), "col_value");
        if (rowDual != null)
            need(rowDual.length, numRow(), "row_dual");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(136, 8);
            check(call(Native.CHECK_SOLUTION, h, doubles(a, x),
                       rowDual == null ? MemorySegment.NULL : doubles(a, rowDual), tol, r));
            return checkReport(r);
        }
    }

    /** {@link #checkSolution} for a model with cones: one dual array per cone. */
    public CheckReport checkConicSolution(double[] x, double[] rowDual, double[][] coneDual,
                                          double tol) {
        need(x.length, numCol(), "col_value");
        if (rowDual != null)
            need(rowDual.length, numRow(), "row_dual");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(136, 8);
            check(call(Native.CHECK_CONIC_SOLUTION, h, doubles(a, x),
                       rowDual == null ? MemorySegment.NULL : doubles(a, rowDual),
                       coneVector(a, coneDual, "cone_dual"), tol, r));
            return checkReport(r);
        }
    }

    /** Judges a claimed Farkas certificate from the model alone. */
    public CertificateReport checkCertificate(double[] rowRay, double tol) {
        need(rowRay.length, numRow(), "row_ray");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(32, 8);
            check(call(Native.CHECK_CERTIFICATE, h, doubles(a, rowRay), tol, r));
            return certificateReport(r);
        }
    }

    /** {@link #checkCertificate} for a model with cones: one ray array per cone. */
    public CertificateReport checkConicCertificate(double[] rowRay, double[][] coneRay,
                                                   double tol) {
        need(rowRay.length, numRow(), "row_ray");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(32, 8);
            check(call(Native.CHECK_CONIC_CERTIFICATE, h, doubles(a, rowRay),
                       coneVector(a, coneRay, "cone_ray"), tol, r));
            return certificateReport(r);
        }
    }

    /** Judges a claimed unbounded ray from the model alone. */
    public RayReport checkRay(double[] colRay, double tol) {
        need(colRay.length, numCol(), "col_ray");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(40, 8);
            check(call(Native.CHECK_RAY, h, doubles(a, colRay), tol, r));
            return new RayReport(r.get(JAVA_DOUBLE, 0), r.get(JAVA_DOUBLE, 8),
                                 r.get(JAVA_DOUBLE, 16), r.get(JAVA_DOUBLE, 24), flag(r, 32));
        }
    }

    /** An irreducible infeasible subsystem of the last INFEASIBLE answer, found on a private copy. */
    public Iis iis() {
        long nr = numRow(), nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment rs = intsOut(a, nr), cs = intsOut(a, nc);
            MemorySegment r = a.allocate(40, 8);
            check(guarded(Native.IIS, h, rs, cs, r));
            return new Iis(sides(rs, nr), sides(cs, nc),
                new IisReport(r.get(JAVA_LONG, 0), r.get(JAVA_LONG, 8), r.get(JAVA_LONG, 16),
                              r.get(JAVA_LONG, 24), flag(r, 32)));
        }
    }

    /** The subsystem {@code iis} describes, as a model of its own. */
    public Model iisModel(Iis iis) { return iisModel(iis.rowSide(), iis.colSide()); }

    /** The subsystem these sides describe, one per row and per column, as a model of its own. */
    public Model iisModel(IisSide[] rowSide, IisSide[] colSide) {
        need(rowSide.length, numRow(), "an IIS's row sides");
        need(colSide.length, numCol(), "an IIS's column sides");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(ADDRESS);
            check(call(Native.IIS_MODEL, h, ints(a, codes(rowSide)), ints(a, codes(colSide)),
                       out));
            return adopt(out.get(ADDRESS, 0));
        }
    }

    /** The smallest total move of the bounds {@code scope} allows that makes the model feasible. */
    public Relaxation feasrelax(RelaxScope scope) {
        long nr = numRow(), nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment rm = doublesOut(a, nr), cm = doublesOut(a, nc);
            MemorySegment r = a.allocate(64, 8);
            check(guarded(Native.FEASRELAX, h, scope.code, rm, cm, r));
            return new Relaxation(prefix(rm, nr), prefix(cm, nc),
                new RelaxReport(r.get(JAVA_DOUBLE, 0), r.get(JAVA_LONG, 8),
                                r.get(JAVA_LONG, 16), r.get(JAVA_LONG, 24),
                                r.get(JAVA_LONG, 32), r.get(JAVA_DOUBLE, 40),
                                r.get(JAVA_LONG, 48),
                                SolveStatus.values()[r.get(JAVA_INT, 56)]));
        }
    }

    /** How far each cost may move with the basis behind the optimum staying optimal. */
    public CostRanging costRanging() {
        long nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment lo = doublesOut(a, nc), hi = doublesOut(a, nc);
            check(call(Native.COST_RANGING, h, lo, hi));
            return new CostRanging(prefix(lo, nc), prefix(hi, nc));
        }
    }

    private BoundRanging ranging(MethodHandle mh, long n) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment ll = doublesOut(a, n), lh = doublesOut(a, n);
            MemorySegment ul = doublesOut(a, n), uh = doublesOut(a, n);
            check(call(mh, h, ll, lh, ul, uh));
            return new BoundRanging(prefix(ll, n), prefix(lh, n), prefix(ul, n), prefix(uh, n));
        }
    }

    /** How far each row's two bounds may move. */
    public BoundRanging rhsRanging() { return ranging(Native.RHS_RANGING, numRow()); }

    /** How far each column's two bounds may move. */
    public BoundRanging boundRanging() { return ranging(Native.BOUND_RANGING, numCol()); }

    /** Sizes, row and column kinds and magnitude ranges, counted in one pass. */
    public ModelStats statistics() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(208, 8);
            check(call(Native.MODEL_STATISTICS, h, r));
            long[] v = new long[26];
            for (int k = 0; k < 26; k++)
                v[k] = r.get(JAVA_LONG, 8L * k);
            return new ModelStats(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8],
                v[9], v[10], v[11], v[12], v[13], v[14], v[15], r.get(JAVA_DOUBLE, 128),
                r.get(JAVA_DOUBLE, 136), r.get(JAVA_DOUBLE, 144), r.get(JAVA_DOUBLE, 152),
                v[20], v[21], v[22], v[23], v[24], v[25]);
        }
    }

    /** What presolve did on the last solve. */
    public PresolveReport presolveReport() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(144, 8);
            check(call(Native.PRESOLVE_RESULT, h, r));
            long[] v = r.toArray(JAVA_LONG);
            return new PresolveReport(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8],
                v[9], v[10], v[11], v[12], v[13], v[14], v[15], v[16], v[17]);
        }
    }

    public MipReport mipResult() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(104, 8);
            check(call(Native.MIP_RESULT, h, r));
            return new MipReport(r.get(JAVA_LONG, 0), r.get(JAVA_LONG, 8), flag(r, 16),
                                 r.get(JAVA_DOUBLE, 24), r.get(JAVA_DOUBLE, 32),
                                 r.get(JAVA_LONG, 40), r.get(JAVA_LONG, 48),
                                 r.get(JAVA_LONG, 56), r.get(JAVA_LONG, 64),
                                 r.get(JAVA_LONG, 72), r.get(JAVA_LONG, 80),
                                 r.get(JAVA_LONG, 88), flag(r, 96));
        }
    }

    /** The incumbent's values and objective of a tree that stopped with one. */
    public Incumbent mipIncumbent() {
        long nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment x = doublesOut(a, nc);
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(Native.MIP_INCUMBENT, h, x, v));
            return new Incumbent(prefix(x, nc), v.get(JAVA_DOUBLE, 0));
        }
    }

    /** The solution pool after a branch and bound, best first; empty when no integer point was found. */
    public List<Incumbent> mipPool() {
        long nc = numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment n = a.allocate(JAVA_LONG);
            check(call(Native.MIP_POOL_COUNT, h, n));
            List<Incumbent> out = new ArrayList<>();
            MemorySegment x = doublesOut(a, nc);
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            for (long k = 0; k < n.get(JAVA_LONG, 0); k++) {
                check(call(Native.MIP_POOL_SOLUTION, h, k, x, v));
                out.add(new Incumbent(prefix(x, nc), v.get(JAVA_DOUBLE, 0)));
            }
            return out;
        }
    }

    private static VerifyReport verifyReport(MemorySegment r) {
        return new VerifyReport(Proof.values()[r.get(JAVA_INT, 0)],
            ProofStage.values()[r.get(JAVA_INT, 4)], r.get(JAVA_DOUBLE, 8),
            r.get(JAVA_DOUBLE, 16), r.get(JAVA_LONG, 24), r.get(JAVA_LONG, 32),
            r.get(JAVA_LONG, 40), r.get(JAVA_LONG, 48), r.get(JAVA_DOUBLE, 56),
            r.get(JAVA_LONG, 64), r.get(JAVA_LONG, 72));
    }

    /** Proves, or refuses to prove, over the rationals that the basis behind the optimum certifies it. */
    public VerifyReport verify() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(80, 8);
            check(call(Native.VERIFY, h, r));
            return verifyReport(r);
        }
    }

    /** The same proof over a basis handed in, with no solve. */
    public VerifyReport verifyBasis(Basis b) {
        need(b.colStatus().length, numCol(), "a basis's column statuses");
        need(b.rowStatus().length, numRow(), "a basis's row statuses");
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(80, 8);
            check(call(Native.VERIFY_BASIS, h, ints(a, codes(b.colStatus())),
                       ints(a, codes(b.rowStatus())), r));
            return verifyReport(r);
        }
    }

    private String exact(MethodHandle mh, Object... index) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(ADDRESS);
            Object[] args = new Object[index.length + 2];
            args[0] = h;
            System.arraycopy(index, 0, args, 1, index.length);
            args[index.length + 1] = out;
            check(call(mh, args));
            return Native.string(out.get(ADDRESS, 0));
        }
    }

    /** A column's exact value at the proved optimum, as a rational like {@code 1/3}. */
    public String exactColValue(long col) { return exact(Native.EXACT_COL_VALUE, col); }
    public String exactRowDual(long row) { return exact(Native.EXACT_ROW_DUAL, row); }
    public String exactObjective() { return exact(Native.EXACT_OBJECTIVE); }
    public String exactRowMultiplier(long row) { return exact(Native.EXACT_ROW_MULTIPLIER, row); }
    public String exactColDirection(long col) { return exact(Native.EXACT_COL_DIRECTION, col); }

    private ExactRayReport exactRay(MethodHandle mh) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(64, 8);
            check(call(mh, h, r));
            return new ExactRayReport(flag(r, 0), r.get(JAVA_DOUBLE, 8),
                r.get(JAVA_DOUBLE, 16), r.get(JAVA_LONG, 24), r.get(JAVA_LONG, 32),
                r.get(JAVA_LONG, 40), r.get(JAVA_LONG, 48), r.get(JAVA_LONG, 56));
        }
    }

    /** Derives the Farkas multipliers behind INFEASIBLE exactly, from the basis the solve stopped on. */
    public ExactRayReport exactCertificate() { return exactRay(Native.EXACT_CERTIFICATE); }

    /** Derives the direction behind UNBOUNDED exactly. */
    public ExactRayReport exactUnboundedRay() { return exactRay(Native.EXACT_UNBOUNDED_RAY); }

    /** Writes the answer's exact proof: an optimum needs a verify that proved it. */
    public void writeProof(String path) { withPath(Native.WRITE_PROOF, path); }

    /** Judges a proof file from this model alone, over the rationals. */
    public ProofReport checkProof(String path) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(40, 8);
            check(call(Native.CHECK_PROOF, h, a.allocateFrom(path), r));
            return new ProofReport(flag(r, 0), flag(r, 1), flag(r, 2), r.get(JAVA_LONG, 8),
                                   r.get(JAVA_LONG, 16), r.get(JAVA_LONG, 24),
                                   ProofKind.values()[r.get(JAVA_INT, 32)], flag(r, 36));
        }
    }

    public long iterations() { return (long) call(Native.ITERATIONS, h); }
    public long workUnits() { return (long) call(Native.WORK_UNITS, h); }

    /** Seconds the last solve took: a development number that does not repeat. */
    public double solveTime() { return (double) call(Native.SOLVE_TIME, h); }
}
