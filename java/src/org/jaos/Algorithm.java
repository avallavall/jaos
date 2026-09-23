// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** What solves an LP: the C {@code jaos_algorithm}, in its order. */
public enum Algorithm {
    DUAL, PRIMAL, BARRIER, PDLP, CONCURRENT
}
