// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_incumbent}: a new incumbent of a branch and bound. */
public record IncumbentEvent(long node, double objective, double bound, double[] x,
                             boolean byRounding) {}
