// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** How far each column's cost may move with the basis staying optimal. */
public record CostRanging(double[] lower, double[] upper) {}
