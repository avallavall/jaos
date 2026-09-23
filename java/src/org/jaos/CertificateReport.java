// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_certificate_report}: the two halves, their gap and the verdict. */
public record CertificateReport(double supColumns, double infRows, double gap,
                                boolean certified) {}
