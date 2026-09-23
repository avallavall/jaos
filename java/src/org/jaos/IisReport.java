// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_iis_report}. */
public record IisReport(long members, long candidates, long solves, long workUnits,
                        boolean fromCertificate) {}
