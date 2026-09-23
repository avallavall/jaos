// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A special ordered set of type 1 or 2, members in weight order. */
public record Sos(int type, long[] cols, double[] weights) {}
