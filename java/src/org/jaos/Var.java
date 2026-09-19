// SPDX-License-Identifier: Apache-2.0
package org.jaos;

/** A variable of one {@link Problem}. */
public final class Var {
    final int index;

    Var(int index) {
        this.index = index;
    }

    public int index() {
        return index;
    }
}
