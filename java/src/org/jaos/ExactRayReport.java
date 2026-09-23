// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** The C {@code jaos_exact_ray_report}. */
public record ExactRayReport(boolean derived, double boundBits, double capacityBits,
                             long blocks, long largestBlock, long atRow,
                             long bytesHeld, long terms) {}
