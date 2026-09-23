// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_mip_report}, all twelve fields. */
public record MipReport(long nodes, long lpSolves, boolean hasIncumbent,
                        double incumbent, double bound, long cuts,
                        long heuristicPoints, long firstIncumbentNode,
                        long fixedCols, long tightened, long symmetryGenerators,
                        long symmetryOrbits) {}
