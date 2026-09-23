// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** Which bounds a feasibility relaxation may move: the C {@code jaos_relax_scope}. */
public enum RelaxScope {
    ROWS(1), COLS(2), BOTH(3);

    final int code;

    RelaxScope(int code) {
        this.code = code;
    }
}
