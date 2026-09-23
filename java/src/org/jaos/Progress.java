// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_progress}: counts so far and the primal infeasibility. */
public record Progress(long iterations, long workUnits, double primalInfeasibility) {}
