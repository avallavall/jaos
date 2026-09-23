// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A certificate file: one multiplier per row when INFEASIBLE, one step per column when UNBOUNDED. */
public record Certificate(SolveStatus status, double[] ray) {}
