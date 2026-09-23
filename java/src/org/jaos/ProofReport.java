// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_proof_report}. */
public record ProofReport(boolean primal, boolean dual, boolean objective, long badRow,
                          long badCol, long terms, ProofKind kind, boolean certified) {}
