// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_model_stats}, field for field. */
public record ModelStats(long numRow, long numCol, long numNz, long integerCol,
                         long binaryCol, long equalityRow, long rangedRow,
                         long oneSidedRow, long freeRow, long fixedCol,
                         long rangedCol, long oneSidedCol, long freeCol,
                         long emptyRow, long emptyCol, long objNz, double minAbs,
                         double maxAbs, double objMinAbs, double objMaxAbs,
                         long semicontinuousCol, long sosSet, long indicatorRow,
                         long quadraticCol, long coneSet, long quadraticRow) {}
