// SPDX-License-Identifier: Apache-2.0
package org.jaos;

import java.lang.foreign.Arena;
import java.lang.foreign.MemorySegment;

import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_LONG;

/**
 * The C {@code jaos_node} a node callback receives: the node, its depth, the
 * relaxation's objective, the tree's bound, the point, whether the point is
 * integral, and the column the tree will branch on. Live only while the
 * callback runs.
 */
public final class NodeEvent {
    private MemorySegment ev;
    private final long node, depth;
    private final double objective, bound;
    private final double[] x;
    private final boolean integral;

    NodeEvent(MemorySegment ev) {
        this.ev = ev;
        node = ev.get(JAVA_LONG, 0);
        depth = ev.get(JAVA_LONG, 8);
        objective = ev.get(JAVA_DOUBLE, 16);
        bound = ev.get(JAVA_DOUBLE, 24);
        long n = ev.get(JAVA_LONG, 40);
        x = ev.get(ADDRESS, 32).reinterpret(n * Double.BYTES).toArray(JAVA_DOUBLE);
        integral = ev.get(JAVA_BYTE, 48) != 0;
    }

    void end() { ev = null; }

    private MemorySegment live() {
        if (ev == null)
            throw new IllegalStateException("this node event is over");
        return ev;
    }

    public long node() { return node; }
    public long depth() { return depth; }
    public double objective() { return objective; }
    public double bound() { return bound; }
    public double[] x() { return x.clone(); }
    public boolean integral() { return integral; }

    /** The column the tree will branch on, or -1. */
    public long branchCol() { return live().get(JAVA_LONG, 56); }

    /** Branches on {@code col} instead: an integer column fractional at the point. */
    public void setBranchCol(long col) { live().set(JAVA_LONG, 56, col); }

    /** Adds {@code lower <= sum value[k] * x[index[k]] <= upper}, a row every solution must satisfy. */
    public void addRow(long[] index, double[] value, double lower, double upper) {
        MemorySegment e = live();
        if (index.length != value.length)
            throw new IllegalArgumentException("index and value differ in length");
        try (Arena a = Arena.ofConfined()) {
            int st = (int) Model.call(Native.NODE_ADD_ROW, e, (long) index.length,
                                      Model.longs(a, index), Model.doubles(a, value),
                                      lower, upper);
            if (st != 0)
                throw new JaosException(Status.values()[st],
                    "the row is not one the node callback may add: a column index "
                    + "out of range, a non-finite coefficient, or bounds that cross");
        }
    }
}
