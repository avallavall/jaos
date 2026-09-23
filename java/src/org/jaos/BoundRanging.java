// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** How far each lower and each upper bound may move, one entry per row or column. */
public record BoundRanging(double[] lowerLo, double[] lowerHi, double[] upperLo,
                           double[] upperHi) {}
