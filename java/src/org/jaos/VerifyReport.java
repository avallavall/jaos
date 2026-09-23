// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_verify_report}. */
public record VerifyReport(Proof status, ProofStage stage, double boundBits,
                           double capacityBits, long blocks, long largestBlock,
                           long atRow, long atCol, double violation, long bytesHeld,
                           long terms) {}
