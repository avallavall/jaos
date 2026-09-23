// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A {@code ½ x'Qx} as triplets: the lower triangle with the diagonal, in column order. */
public record QuadraticTerms(long[] rows, long[] cols, double[] values) {}
