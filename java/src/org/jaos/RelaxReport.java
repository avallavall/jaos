// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_relax_report}. */
public record RelaxReport(double total, long rowsMoved, long colsMoved, long atRow,
                          long atCol, double largest, long workUnits,
                          SolveStatus status) {}
