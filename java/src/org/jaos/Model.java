// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.ref.Cleaner;
import java.util.function.Consumer;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

/**
 * A {@code jaos_model}: every call of the C library that builds, reads,
 * writes, solves and reads back a model, one method each. {@link #close}
 * frees it, and so does the garbage collector when it is not closed.
 */
public final class Model implements AutoCloseable {
    private static final Cleaner CLEANER = Cleaner.create();
    private final MemorySegment h;
    private final Cleaner.Cleanable cleanable;
    private MemorySegment logStub = MemorySegment.NULL;
    private Consumer<String> logSink;
    private LogLevel logLevel = LogLevel.OFF;
    private double[] mipStart;

    public Model() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment out = a.allocate(ADDRESS);
            int st = (int) call(Native.MODEL_NEW, out);
            if (st != 0)
                throw new JaosException(Status.values()[st], "jaos_model_new failed");
            h = out.get(ADDRESS, 0);
        }
        MemorySegment handle = h;
        cleanable = CLEANER.register(this, () -> call(Native.MODEL_FREE, handle));
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

    private static MemorySegment doubles(Arena a, double[] v) {
        return v.length == 0 ? a.allocate(JAVA_DOUBLE)
                             : a.allocateFrom(JAVA_DOUBLE, v);
    }

    private static MemorySegment longs(Arena a, long[] v) {
        return v.length == 0 ? a.allocate(JAVA_LONG)
                             : a.allocateFrom(JAVA_LONG, v);
    }

    public static String version() {
        return Native.string((MemorySegment) call(Native.VERSION));
    }

    public static String statusString(SolveStatus s) {
        return Native.string((MemorySegment) call(Native.STATUS_STR, s.ordinal()));
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

    public void writeMps(String path) { withPath(Native.WRITE_MPS, path); }
    public void writeLp(String path) { withPath(Native.WRITE_LP, path); }
    public void writeSolution(String path) { withPath(Native.WRITE_SOLUTION, path); }

    private void withPath(MethodHandle mh, String path) {
        try (Arena a = Arena.ofConfined()) {
            check(call(mh, h, a.allocateFrom(path)));
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

    public long numCol() { return (long) call(Native.NUM_COL, h); }
    public long numRow() { return (long) call(Native.NUM_ROW, h); }
    public long numCones() { return (long) call(Native.NUM_CONES, h); }
    public boolean hasInteger() { return (boolean) call(Native.HAS_INTEGER, h); }

    public void setColInteger(long col, boolean on) {
        check(call(Native.SET_COL_INTEGER, h, col, on));
    }

    public void setColBounds(long col, double lower, double upper) {
        check(call(Native.SET_COL_BOUNDS, h, col, lower, upper));
    }

    public void setRowBounds(long row, double lower, double upper) {
        check(call(Native.SET_ROW_BOUNDS, h, row, lower, upper));
    }

    public void setColCost(long col, double cost) {
        check(call(Native.SET_COL_COST, h, col, cost));
    }

    /** The diagonal entry of the objective's {@code ½ x'Qx}. */
    public void setColQuadratic(long col, double q) {
        check(call(Native.SET_COL_QUADRATIC, h, col, q));
    }

    /** The objective's {@code ½ x'Qx} as triplets, one per diagonal entry and per pair. */
    public void setQuadratic(long[] rows, long[] cols, double[] values) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_QUADRATIC, h, (long) values.length, longs(a, rows),
                       longs(a, cols), doubles(a, values)));
        }
    }

    /** Row {@code row}'s part {@code ½ x'Qx}. */
    public void setRowQuadratic(long row, long[] rows, long[] cols, double[] values) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_ROW_QUADRATIC, h, row, (long) values.length,
                       longs(a, rows), longs(a, cols), doubles(a, values)));
        }
    }

    public void addCone(ConeType type, long[] cols) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.ADD_CONE, h, type.code, (long) cols.length, longs(a, cols)));
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

    public void setTimeLimit(double seconds) { check(call(Native.SET_TIME_LIMIT, h, seconds)); }
    public void setWorkLimit(long units) { check(call(Native.SET_WORK_LIMIT, h, units)); }
    public void setThreads(long threads) { check(call(Native.SET_THREADS, h, threads)); }

    public void setMipStart(double[] x) {
        try (Arena a = Arena.ofConfined()) {
            check(call(Native.SET_MIP_START, h, doubles(a, x)));
        }
        mipStart = x.clone();
    }

    /** The name of every option {@link #setOption} takes. */
    public static String[] optionNames() {
        long n = (long) call(Native.NUM_OPTIONS);
        String[] names = new String[(int) n];
        for (int k = 0; k < n; k++)
            names[k] = Native.string((MemorySegment) call(Native.OPTION_NAME, (long) k));
        return names;
    }

    void copySettingsTo(Model other) {
        for (String name : optionNames())
            other.setOption(name, getOption(name));
        if (logSink != null)
            other.setLog(logLevel, logSink);
        if (mipStart != null)
            other.setMipStart(java.util.Arrays.copyOf(mipStart, (int) other.numCol()));
    }

    private static void logLine(Consumer<String> sink, MemorySegment user,
                                int level, MemorySegment line) {
        sink.accept(Native.string(line));
    }

    /** Sends the solver's log to {@code sink} at {@code level}; null or OFF stops it. */
    public void setLog(LogLevel level, Consumer<String> sink) {
        logLevel = level;
        logSink = sink == null || level == LogLevel.OFF ? null : sink;
        if (sink == null || level == LogLevel.OFF) {
            check(call(Native.SET_LOG_CALLBACK, h, MemorySegment.NULL, MemorySegment.NULL));
            logStub = MemorySegment.NULL;
        } else {
            MethodHandle target;
            try {
                target = MethodHandles.lookup().findStatic(Model.class, "logLine",
                    MethodType.methodType(void.class, Consumer.class,
                                          MemorySegment.class, int.class,
                                          MemorySegment.class)).bindTo(sink);
            } catch (ReflectiveOperationException e) {
                throw new IllegalStateException(e);
            }
            logStub = Native.LINKER.upcallStub(target, Native.LOG_FN, Arena.ofAuto());
            check(call(Native.SET_LOG_CALLBACK, h, logStub, MemorySegment.NULL));
        }
        check(call(Native.SET_LOG_LEVEL, h, level.ordinal()));
    }

    public void solve() { check(call(Native.SOLVE, h)); }

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
        int nc = (int) numCol(), nr = (int) numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment x = a.allocate(JAVA_DOUBLE, Math.max(nc, 1));
            MemorySegment act = a.allocate(JAVA_DOUBLE, Math.max(nr, 1));
            MemorySegment y = a.allocate(JAVA_DOUBLE, Math.max(nr, 1));
            MemorySegment d = a.allocate(JAVA_DOUBLE, Math.max(nc, 1));
            check(call(Native.SOLUTION, h, x, act, y, d));
            return new Solution(prefix(x, nc), prefix(act, nr), prefix(y, nr),
                                prefix(d, nc));
        }
    }

    private static double[] prefix(MemorySegment s, int n) {
        return s.asSlice(0, (long) n * Double.BYTES).toArray(JAVA_DOUBLE);
    }

    /** The dual of cone {@code k}, one entry per member. */
    public double[] coneDual(long k) {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment n = a.allocate(JAVA_LONG);
            check(call(Native.CONE, h, k, MemorySegment.NULL, n, MemorySegment.NULL));
            int len = (int) n.get(JAVA_LONG, 0);
            MemorySegment z = a.allocate(JAVA_DOUBLE, Math.max(len, 1));
            check(call(Native.CONE_DUAL, h, k, z));
            return prefix(z, len);
        }
    }

    /** The Farkas multipliers of an infeasible model, or null. */
    public double[] certificate() {
        int nr = (int) numRow();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment y = a.allocate(JAVA_DOUBLE, Math.max(nr, 1));
            return (int) call(Native.CERTIFICATE, h, y) == 0 ? prefix(y, nr) : null;
        }
    }

    /** The improving direction of an unbounded model, or null. */
    public double[] unboundedRay() {
        int nc = (int) numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment d = a.allocate(JAVA_DOUBLE, Math.max(nc, 1));
            return (int) call(Native.UNBOUNDED_RAY, h, d) == 0 ? prefix(d, nc) : null;
        }
    }

    public MipReport mipResult() {
        try (Arena a = Arena.ofConfined()) {
            MemorySegment r = a.allocate(96, 8);
            check(call(Native.MIP_RESULT, h, r));
            return new MipReport(r.get(JAVA_LONG, 0), r.get(JAVA_LONG, 8),
                                 r.get(JAVA_BYTE, 16) != 0, r.get(JAVA_DOUBLE, 24),
                                 r.get(JAVA_DOUBLE, 32), r.get(JAVA_LONG, 40),
                                 r.get(JAVA_LONG, 48), r.get(JAVA_LONG, 56));
        }
    }

    /** The incumbent's values and objective of a tree that stopped with one. */
    public Incumbent mipIncumbent() {
        int nc = (int) numCol();
        try (Arena a = Arena.ofConfined()) {
            MemorySegment x = a.allocate(JAVA_DOUBLE, Math.max(nc, 1));
            MemorySegment v = a.allocate(JAVA_DOUBLE);
            check(call(Native.MIP_INCUMBENT, h, x, v));
            return new Incumbent(prefix(x, nc), v.get(JAVA_DOUBLE, 0));
        }
    }

    public long iterations() { return (long) call(Native.ITERATIONS, h); }
    public long workUnits() { return (long) call(Native.WORK_UNITS, h); }
}
