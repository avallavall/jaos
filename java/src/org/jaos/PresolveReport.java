// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_presolve_report}, field for field. */
public record PresolveReport(long numRow, long numCol, long numNz, long rounds,
                             long fixedCol, long emptyRow, long emptyCol,
                             long singletonRow, long singletonCol,
                             long freeColSingleton, long forcingRow,
                             long redundantRow, long impliedFreeCol,
                             long tightenedBound, long duplicateRow,
                             long duplicateCol, long dominatedCol,
                             long aggregatedCol) {}
