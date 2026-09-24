// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/**
 * The C {@code jaos_progress}: counts so far and the primal infeasibility. In
 * a branch and bound, the node count, the bound and the incumbent are the
 * tree's; elsewhere the node count is 0 and the bound is the infinity on the
 * sense's far side.
 */
public record Progress(long iterations, long workUnits, double primalInfeasibility,
                       long nodes, double bound, boolean hasIncumbent,
                       double incumbent) {}
