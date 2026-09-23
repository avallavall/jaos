// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_ray_report}. */
public record RayReport(double rate, double maxColEscape, double maxRowEscape,
                        double curvature, boolean certified) {}
