// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The answer of an optimal solve, in the model's own signs. */
public record Solution(double[] x, double[] rowActivity, double[] rowDual,
                       double[] reducedCost) {}
