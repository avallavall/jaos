// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** One signed move per row and per column, and the report behind them. */
public record Relaxation(double[] rowMove, double[] colMove, RelaxReport report) {}
