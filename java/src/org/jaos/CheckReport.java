// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_check_report}, field for field; jaos.h says what each means. */
public record CheckReport(double maxColViolation, double maxRowViolation,
                          double maxRowViolationRelative, double maxDualViolation,
                          double primalObjective, double dualObjective,
                          double objectiveGap, double gapPositive,
                          double gapNegative, double maxDroppedMultiplier,
                          long droppedTerms, double certifiedSuboptimality,
                          long unquantifiedRays, double relativeSuboptimality,
                          boolean primalFeasible, boolean dualFeasible,
                          boolean checkedDuals, boolean gapCertified,
                          double maxIntegralityViolation, double maxConeViolation) {}
