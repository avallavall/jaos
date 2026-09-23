// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** An irreducible infeasible subsystem: one side per row and per column, and its cost. */
public record Iis(IisSide[] rowSide, IisSide[] colSide, IisReport report) {}
