// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/**
 * How the MIP gap is measured: the C {@code jaos_gap_rule}. SHIFTED stops when
 * no node beats the incumbent by more than gap * (1 + |incumbent|), RELATIVE
 * by more than gap * |incumbent|.
 */
public enum GapRule {
    SHIFTED, RELATIVE
}
