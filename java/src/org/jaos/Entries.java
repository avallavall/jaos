// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** One column or row of the matrix: indices ascending, values beside them. */
public record Entries(long[] index, double[] value) {}
