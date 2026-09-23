// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** What the solution file of an optimum holds. */
public record SolutionFile(double objective, Solution solution, Basis basis) {}
